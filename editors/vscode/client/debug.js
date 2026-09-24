// Debugging FoxLang programs: F5, the Run and Debug view, breakpoints in .fox files.
// The program runs in a terminal of its own, so input(), colours and graphics work as
// in a normal run, and talks the Debug Adapter Protocol to VS Code over a local
// connection that this adapter relays.
const net = require('net');
const path = require('path');
const vscode = require('vscode');
const run = require('./run');

const DEBUG_TYPE = 'foxlang';
const CONNECT_TIMEOUT_MS = 15000;
let terminal = null;

class ProgramAdapter {
    constructor(configuration) {
        this.configuration = configuration;
        this.emitter = new vscode.EventEmitter();
        this.onDidSendMessage = this.emitter.event;
        this.queue = [];
        this.buffer = Buffer.alloc(0);
        this.socket = null;
        this.done = false;
        this.listen();
    }

    listen() {
        this.server = net.createServer(socket => this.connected(socket));
        this.server.on('error', error => this.fail('Отладчик FoxLang не смог открыть порт: ' + error.message));
        this.server.listen(0, '127.0.0.1', () => {
            const config = this.configuration;
            // One debug terminal at a time: the previous run's output is no longer needed.
            if (terminal) terminal.dispose();
            terminal = vscode.window.createTerminal({
                name: `FoxLang: отладка ${path.basename(config.program)}`,
                shellPath: run.resolveExecutable(),
                shellArgs: ['debug-adapter', '--connect', `127.0.0.1:${this.server.address().port}`],
                cwd: config.cwd,
                env: Object.assign({ FOXLANG_ABSOLUTE_PATHS: '1' }, config.env || {})
            });
            terminal.show(true);
            this.timer = setTimeout(() => {
                if (!this.socket) {
                    this.fail('FoxLang не подключился к отладчику. Проверьте настройку foxlang.run.executablePath: нужен FoxLang 6.4 или новее.');
                }
            }, CONNECT_TIMEOUT_MS);
        });
    }

    connected(socket) {
        if (this.socket || this.done) {
            socket.destroy();
            return;
        }
        this.socket = socket;
        clearTimeout(this.timer);
        this.server.close();
        socket.on('data', data => this.receive(data));
        socket.on('close', () => this.end());
        socket.on('error', () => this.end());
        for (const message of this.queue) this.send(message);
        this.queue = [];
    }

    receive(data) {
        this.buffer = Buffer.concat([this.buffer, data]);
        for (;;) {
            const header = this.buffer.indexOf('\r\n\r\n');
            if (header < 0) return;
            const match = /Content-Length:\s*(\d+)/i.exec(this.buffer.subarray(0, header).toString('ascii'));
            if (!match) {
                this.end();
                return;
            }
            const start = header + 4;
            const length = Number(match[1]);
            if (this.buffer.length < start + length) return;
            const body = this.buffer.subarray(start, start + length).toString('utf8');
            this.buffer = this.buffer.subarray(start + length);
            try {
                this.emitter.fire(JSON.parse(body));
            } catch (error) {
                // A broken message is dropped; the next one starts after it.
            }
        }
    }

    send(message) {
        const body = Buffer.from(JSON.stringify(message), 'utf8');
        this.socket.write(`Content-Length: ${body.length}\r\n\r\n`);
        this.socket.write(body);
    }

    handleMessage(message) {
        if (this.done) return;
        if (this.socket) this.send(message);
        else this.queue.push(message);
    }

    fail(text) {
        for (const message of this.queue) {
            if (message.type === 'request') {
                this.emitter.fire({ seq: 0, type: 'response', request_seq: message.seq, command: message.command, success: false, message: text });
            }
        }
        this.queue = [];
        vscode.window.showErrorMessage(text);
        this.end();
    }

    // The program ended or its terminal was closed: the session is over.
    end() {
        if (this.done) return;
        this.done = true;
        clearTimeout(this.timer);
        this.emitter.fire({ seq: 0, type: 'event', event: 'terminated' });
        try { this.server.close(); } catch (error) { /* already closed */ }
        if (this.socket) this.socket.destroy();
    }

    dispose() {
        this.end();
    }
}

// F5 without launch.json debugs the main file (foxlang.run.entryFile) or the open one.
const configurationProvider = {
    provideDebugConfigurations() {
        return [{ type: DEBUG_TYPE, request: 'launch', name: 'FoxLang: текущий файл', program: '${file}' }];
    },
    resolveDebugConfiguration(folder, config) {
        if (!config.type && !config.request && !config.name) {
            config = { type: DEBUG_TYPE, request: 'launch', name: 'FoxLang: отладка' };
        }
        if (!config.program) config.program = run.entryFile();
        if (!config.program) {
            vscode.window.showWarningMessage('Откройте файл .fox или укажите главный файл командой «FoxLang: сделать этот файл главным».');
            return undefined;
        }
        if (!path.isAbsolute(config.program) && folder) config.program = path.join(folder.uri.fsPath, config.program);
        if (!config.cwd) config.cwd = path.dirname(config.program);
        if (!config.args) config.args = run.config().get('run.arguments') || [];
        return config;
    }
};

async function startDebugging() {
    const program = run.entryFile();
    if (!program) {
        vscode.window.showWarningMessage('Откройте файл .fox или укажите главный файл командой «FoxLang: сделать этот файл главным».');
        return;
    }
    const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(program));
    await vscode.debug.startDebugging(folder, { type: DEBUG_TYPE, request: 'launch', name: `FoxLang: отладка ${path.basename(program)}`, program });
}

function activate(context) {
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(DEBUG_TYPE, configurationProvider));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(DEBUG_TYPE, {
        createDebugAdapterDescriptor(session) {
            return new vscode.DebugAdapterInlineImplementation(new ProgramAdapter(session.configuration));
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.debug', startDebugging));
}

module.exports = { activate };
