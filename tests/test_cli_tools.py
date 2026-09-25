"""foxlang test and foxlang fmt on a small project in a temporary directory."""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())

TESTS = '''include("lib/math.fox");

void test_add() {
    assert_equal(add(2, 3), 5);
    counter += 1;
    assert_equal(counter, 1);
}

void test_fresh_globals() {
    counter += 1;
    assert_equal(counter, 1, "globals start over in every test");
}

void test_wrong_sum() {
    assert_equal(add(2, 2), 5, "sum");
}

void test_error() {
    array items = [1];
    int x = items[3];
}

void test_caught() {
    bool thrown = false;
    try {
        throw "boom";
    } catch (e) {
        thrown = e == "boom";
    }
    assert(thrown, "throw is caught");
}

void helper(int x) {
}
'''


def run(*args, cwd):
    result = subprocess.run([binary, *args], cwd=cwd, capture_output=True, text=True, encoding='utf-8')
    return result.returncode, result.stdout + result.stderr


with tempfile.TemporaryDirectory(prefix='fox-tools-') as directory:
    root = Path(directory)
    (root / 'lib').mkdir()
    (root / 'lib' / 'math.fox').write_text('int add(int a, int b) {\n    return a + b;\n}\nint counter = 0;\n', encoding='utf-8')
    (root / 'math_test.fox').write_text(TESTS, encoding='utf-8')
    (root / 'build').mkdir()
    (root / 'build' / 'ignored_test.fox').write_text('void test_never() { assert(false); }\n', encoding='utf-8')

    code, out = run('test', cwd=root)
    assert code == 1, out
    for line in ('ok   test_add', 'ok   test_fresh_globals', 'ok   test_caught',
                 'FAIL test_wrong_sum: math_test.fox:15: Assertion failed: sum: expected 5, got 4',
                 'FAIL test_error: math_test.fox:20: Runtime Error: Array index out of bounds',
                 '5 tests: 3 passed, 2 failed'):
        assert line in out, (line, out)
    assert 'helper' not in out and 'test_never' not in out, out

    code, out = run('test', '--filter', 'add', 'math_test.fox', cwd=root)
    assert code == 0 and '1 tests: 1 passed, 0 failed' in out, out

    messy = 'int x=1;\r\n\r\n\r\nif (x > 0) {\r\n\tprint(x);   \r\n}'
    (root / 'lib' / 'messy.fox').write_bytes(messy.encode('utf-8'))
    code, out = run('fmt', '--check', cwd=root)
    assert code == 1 and 'messy.fox: not formatted' in out and 'math_test.fox' not in out, out
    code, out = run('fmt', 'lib', cwd=root)
    assert code == 0 and 'formatted' in out, out
    assert (root / 'lib' / 'messy.fox').read_bytes() == b'int x=1;\n\nif (x > 0) {\n    print(x);\n}\n'
    code, out = run('fmt', '--check', cwd=root)
    assert code == 0 and 'all files are formatted' in out, out

print('CLI_TOOLS_OK')
