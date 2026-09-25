"""The debug adapter, driven the way an editor drives it over the Debug Adapter Protocol.

Breakpoints (moved off blank lines, conditional, by hit count, logpoints), stepping in,
over and out, the call stack, locals and globals with arrays, changing a variable,
evaluating expressions and statements, stopping on a runtime error, pausing a busy
loop, and both transports: stdio with captured output, and a TCP connection that
leaves the program its own terminal for input and output.
"""
from pathlib import Path
import json
import os
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time

binary = str(Path(sys.argv[1]).resolve())
TIMEOUT = 20


class Dap:
    def __init__(self, reader, writer):
        self.write = writer
        self.seq = 1
        self.messages = queue.Queue()
        self.events = []
        self.pending = []
        threading.Thread(target=self._read, args=(reader,), daemon=True).start()

    def _read(self, read):
        buffer = b''
        while True:
            while b'\r\n\r\n' not in buffer:
                chunk = read(65536)
                if not chunk:
                    self.messages.put(None)
                    return
                buffer += chunk
            header, buffer = buffer.split(b'\r\n\r\n', 1)
            length = int(header.split(b':')[1])
            while len(buffer) < length:
                chunk = read(65536)
                if not chunk:
                    self.messages.put(None)
                    return
                buffer += chunk
            body, buffer = buffer[:length], buffer[length:]
            self.messages.put(json.loads(body.decode('utf-8')))

    def _next(self):
        message = self.messages.get(timeout=TIMEOUT)
        if message is None:
            raise AssertionError('adapter closed the connection; events: ' + repr(self.events[-5:]))
        if message['type'] == 'event':
            self.events.append(message)
        return message

    def send(self, command, arguments=None):
        seq = self.seq
        self.seq += 1
        data = json.dumps({'seq': seq, 'type': 'request', 'command': command, 'arguments': arguments or {}}).encode('utf-8')
        self.write(b'Content-Length: ' + str(len(data)).encode() + b'\r\n\r\n' + data)
        return seq

    def request(self, command, arguments=None, ok=True):
        seq = self.send(command, arguments)
        while True:
            message = self._next()
            if message['type'] == 'response' and message['request_seq'] == seq:
                assert message['success'] == ok, (command, message)
                return message.get('body', {}) if ok else message
            if message['type'] == 'event':
                self.pending.append(message)

    def event(self, name):
        for index, message in enumerate(self.pending):
            if message['event'] == name:
                return self.pending.pop(index)
        while True:
            message = self._next()
            if message['type'] == 'event' and message['event'] == name:
                return message
            if message['type'] == 'event':
                self.pending.append(message)

    def stopped(self, reason, line):
        body = self.event('stopped')['body']
        assert body['reason'] == reason, body
        frames = self.request('stackTrace', {'threadId': 1})['stackFrames']
        assert frames[0]['line'] == line, (reason, line, frames)
        return body, frames

    def output(self):
        return ''.join(e['body']['output'] for e in self.events if e['event'] == 'output')

    def locals(self, frame_id):
        scopes = self.request('scopes', {'frameId': frame_id})['scopes']
        found = {}
        for scope in scopes:
            for item in self.request('variables', {'variablesReference': scope['variablesReference']})['variables']:
                found.setdefault(scope['name'], {})[item['name']] = item
        return found

    def evaluate(self, expression, frame_id, context='watch'):
        return self.request('evaluate', {'expression': expression, 'frameId': frame_id, 'context': context})['result']


def stdio_session(workdir):
    process = subprocess.Popen([binary, 'debug-adapter'], cwd=workdir, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    def write(data):
        process.stdin.write(data)
        process.stdin.flush()
    return process, Dap(process.stdout.read1 if hasattr(process.stdout, 'read1') else process.stdout.read, write)


def start(dap, program, breakpoints, stop_on_entry=False, errors=True, filters=None):
    caps = dap.request('initialize', {'adapterID': 'foxlang', 'linesStartAt1': True, 'columnsStartAt1': True})
    assert caps['supportsConditionalBreakpoints'] and caps['supportsLogPoints'] and caps['supportsSetVariable']
    dap.event('initialized')
    dap.request('launch', {'program': str(program), 'cwd': str(program.parent), 'stopOnEntry': stop_on_entry})
    placed = dap.request('setBreakpoints', {'source': {'path': str(program)}, 'breakpoints': breakpoints})['breakpoints']
    if filters is None:
        filters = ['uncaught'] if errors else []
    dap.request('setExceptionBreakpoints', {'filters': filters})
    dap.request('configurationDone')
    return placed


def finish(process, dap, code):
    assert dap.event('exited')['body']['exitCode'] == code
    dap.event('terminated')
    dap.request('disconnect', {})
    process.wait(timeout=TIMEOUT)


STEPPING = '''using string;

int square(int n) {
    int result = n * n;
    return result;
}

int total = 0;
array items = [1, 2, 3];
for (int i = 0; i < 5; i++) {
    total = total + square(i);
}
print("total " + total);
string name = "лиса";
print(name);
'''

ERRORS = '''int boom(int x) {
    array a = [1, 2];
    return a[x];
}
for (int i = 0; i < 3; i++) {
    print("i=" + i);
    int twice = i * 2;
}
boom(5);
'''

PAUSE = '''string line = input();
int count = 0;
while (count >= 0) {
    count = count + 1;
}
print("got " + line);
'''

SCOPES = '''int check(int v) {
    return v;
}
void run() {
    int k = 0;
    while (check(k) < 3) {
        int inner = k;
        k = k + 1;
    }
}
run();
'''

with tempfile.TemporaryDirectory(prefix='fox-debug-') as directory:
    workdir = Path(directory)

    # Breakpoints, the stack, variables and stepping, over stdio.
    program = workdir / 'main.fox'
    program.write_text(STEPPING, encoding='utf-8')
    process, dap = stdio_session(workdir)
    placed = start(dap, program, [{'line': 4, 'condition': 'n == 3'}, {'line': 7}, {'line': 13}])
    assert [b['line'] for b in placed] == [4, 8, 13] and all(b['verified'] for b in placed), placed
    assert dap.request('threads')['threads'] == [{'id': 1, 'name': 'FoxLang'}]
    dap.stopped('breakpoint', 8)

    dap.request('continue', {'threadId': 1})
    _, frames = dap.stopped('breakpoint', 4)
    assert [f['name'] for f in frames] == ['square', 'программа'], frames
    assert frames[1]['line'] == 11 and frames[0]['source']['path'].endswith('main.fox'), frames
    scopes = dap.locals(frames[0]['id'])
    assert scopes['Локальные']['n']['value'] == '3' and 'result' not in scopes['Локальные'], scopes
    assert dap.evaluate('n * 10', frames[0]['id']) == '30'
    assert dap.evaluate('total', frames[1]['id']) == '5'
    globals_ = dap.locals(frames[1]['id'])['Глобальные']
    assert globals_['items']['value'] == '[1, 2, 3]' and globals_['items']['indexedVariables'] == 3, globals_['items']
    elements = dap.request('variables', {'variablesReference': globals_['items']['variablesReference']})['variables']
    assert [e['name'] for e in elements] == ['[0]', '[1]', '[2]'] and elements[2]['value'] == '3', elements
    locals_ref = dap.request('scopes', {'frameId': frames[0]['id']})['scopes'][0]['variablesReference']
    changed = dap.request('setVariable', {'variablesReference': locals_ref, 'name': 'n', 'value': '4'})
    assert changed['value'] == '4', changed

    dap.request('next', {'threadId': 1})
    _, frames = dap.stopped('step', 5)
    assert dap.evaluate('result', frames[0]['id']) == '16'
    dap.request('stepOut', {'threadId': 1})
    _, frames = dap.stopped('step', 11)
    assert len(frames) == 1 and dap.evaluate('total', frames[0]['id']) == '21', frames
    dap.request('stepIn', {'threadId': 1})
    dap.stopped('step', 4)
    dap.request('continue', {'threadId': 1})
    _, frames = dap.stopped('breakpoint', 13)
    assert dap.evaluate('total', frames[0]['id']) == '37'
    assert dap.evaluate('"имя: " + "лиса"', frames[0]['id']) == '"имя: лиса"'
    dap.request('evaluate', {'expression': 'total = 100', 'frameId': frames[0]['id'], 'context': 'repl'})
    failure = dap.request('evaluate', {'expression': 'missing + 1', 'frameId': frames[0]['id'], 'context': 'watch'}, ok=False)
    assert 'missing' in failure['message'], failure
    dap.request('continue', {'threadId': 1})
    finish(process, dap, 0)
    assert 'total 100\n' in dap.output() and 'лиса\n' in dap.output(), dap.output()

    # Stop on entry, a logpoint, a hit count and a runtime error.
    program = workdir / 'errors.fox'
    program.write_text(ERRORS, encoding='utf-8')
    process, dap = stdio_session(workdir)
    start(dap, program, [{'line': 6, 'logMessage': 'лог {i}'}, {'line': 7, 'hitCondition': '2'}], stop_on_entry=True)
    dap.stopped('entry', 5)
    dap.request('continue', {'threadId': 1})
    _, frames = dap.stopped('breakpoint', 7)
    assert dap.evaluate('i', frames[0]['id']) == '1'
    dap.request('continue', {'threadId': 1})
    body, frames = dap.stopped('exception', 3)
    assert 'index' in body['text'].lower(), body
    assert frames[0]['name'] == 'boom' and frames[1]['line'] == 9, frames
    info = dap.request('exceptionInfo', {'threadId': 1})
    assert info['description'] == body['text'], info
    dap.request('continue', {'threadId': 1})
    finish(process, dap, 1)
    output = dap.output()
    for expected in ('лог 0\n', 'лог 1\n', 'лог 2\n', 'i=2\n', 'FoxLang: '):
        assert expected in output, (expected, output)

    # A call from a loop condition: the loop body's scope is gone, the caller's frame
    # shows the function's own variables.
    program = workdir / 'scopes.fox'
    program.write_text(SCOPES, encoding='utf-8')
    process, dap = stdio_session(workdir)
    start(dap, program, [{'line': 2, 'hitCondition': '2'}])
    _, frames = dap.stopped('breakpoint', 2)
    assert [(f['name'], f['line']) for f in frames] == [('check', 2), ('run', 6), ('программа', 11)], frames
    caller = dap.locals(frames[1]['id'])['Локальные']
    assert caller['k']['value'] == '1' and 'inner' not in caller, caller
    dap.request('continue', {'threadId': 1})
    finish(process, dap, 0)

    # Stepping into a function of a standard module: its text comes from the adapter
    # when the module is built into foxlang.
    program = workdir / 'module.fox'
    program.write_text('using fs;\nstring s = format_bytes(2048);\nprint(s);\n', encoding='utf-8')
    process, dap = stdio_session(workdir)
    start(dap, program, [], stop_on_entry=True)
    dap.stopped('entry', 1)
    dap.request('stepIn', {'threadId': 1})
    dap.stopped('step', 2)
    dap.request('stepIn', {'threadId': 1})
    dap.event('stopped')
    frames = dap.request('stackTrace', {'threadId': 1})['stackFrames']
    assert frames[0]['name'] == 'format_bytes' and frames[0]['source']['name'].endswith('fs.fox'), frames
    where = frames[0]['source']
    text = dap.request('source', {'sourceReference': where['sourceReference']})['content'] if where.get('sourceReference') \
        else Path(where['path']).read_text(encoding='utf-8')
    assert 'format_bytes' in text, text[:200]
    dap.request('stepOut', {'threadId': 1})
    dap.stopped('step', 3)
    dap.request('continue', {'threadId': 1})
    finish(process, dap, 0)
    assert '2 KB' in dap.output(), dap.output()

    # An error inside try stops only with the "caught" filter; a map and a struct
    # open up in the variables view.
    program = workdir / 'caught.fox'
    program.write_text('struct Point {\n    int x;\n    int y;\n}\nmap m = {"a": [1, 2]};\nPoint p = Point(3, 4);\n'
                       'try {\n    int z = 0;\n    int r = 1 / z;\n} catch (e) {\n    print("caught " + e);\n}\n', encoding='utf-8')
    for filters, stops in ((['uncaught'], False), (['uncaught', 'caught'], True)):
        process, dap = stdio_session(workdir)
        start(dap, program, [], filters=filters)
        if stops:
            _, frames = dap.stopped('exception', 9)
            globals_ = dap.locals(frames[0]['id'])['Глобальные']
            assert globals_['m']['value'] == '{a: [1, 2]}' and globals_['m']['type'] == 'map (1)', globals_['m']
            assert globals_['p']['value'] == 'Point{x: 3, y: 4}', globals_['p']
            fields = dap.request('variables', {'variablesReference': globals_['p']['variablesReference']})['variables']
            assert [(f['name'], f['value']) for f in fields] == [('x', '3'), ('y', '4')], fields
            changed = dap.request('setVariable', {'variablesReference': globals_['p']['variablesReference'], 'name': 'x', 'value': '30'})
            assert changed['value'] == '30', changed
            assert dap.evaluate('p.x + m["a"][1]', frames[0]['id']) == '32'
            dap.request('continue', {'threadId': 1})
        finish(process, dap, 0)
        assert 'caught Runtime Error: Division by zero' in dap.output(), dap.output()

    # Over TCP the program keeps its terminal: input() reads it, print() writes to it.
    program = workdir / 'pause.fox'
    program.write_text(PAUSE, encoding='utf-8')
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(TIMEOUT)
    port = listener.getsockname()[1]
    process = subprocess.Popen([binary, 'debug-adapter', '--connect', '127.0.0.1:' + str(port)], cwd=workdir,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    connection, _ = listener.accept()
    connection.settimeout(None)
    dap = Dap(connection.recv, connection.sendall)
    process.stdin.write('привет\n'.encode('utf-8'))
    process.stdin.flush()
    start(dap, program, [])
    # Let the program read its line and get into the loop before pausing it.
    time.sleep(0.5)
    dap.request('pause', {'threadId': 1})
    _, frames = dap.stopped('pause', 4)
    locals_ref = dap.request('scopes', {'frameId': frames[0]['id']})['scopes'][1]['variablesReference']
    dap.request('setVariable', {'variablesReference': locals_ref, 'name': 'count', 'value': '-100'})
    dap.request('continue', {'threadId': 1})
    finish(process, dap, 0)
    connection.close()
    listener.close()
    stdout = process.stdout.read().decode('utf-8')
    assert 'got привет' in stdout, stdout

print('DEBUGGER_OK')
