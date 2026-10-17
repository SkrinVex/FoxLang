"""Public environment plans in an isolated user home; Windows registry snapshot fidelity."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid

binary = str(Path(sys.argv[1]).resolve())
q = lambda value: json.dumps(str(value), ensure_ascii=False)
with tempfile.TemporaryDirectory(prefix='skrumi environment ') as temp:
    root = Path(temp)
    env = dict(os.environ, HOME=str(root), SHELL='/bin/sh', XDG_DATA_HOME='relative-invalid')
    source = root/'test.skm'
    def run(code, ok=True):
        source.write_text('using env;\n' + code, encoding='utf-8')
        p = subprocess.run([binary, str(source)], cwd=root, env=env, capture_output=True, text=True, encoding='utf-8', timeout=15)
        assert (p.returncode == 0) == ok, (p.stdout, p.stderr, code)
        return p
    if os.name == 'nt':
        variable = 'SKRUMI_TEST_'+uuid.uuid4().hex
        run(f'''array before = env_user_snapshot({q(variable)});
            if (before[0]) {{ fail("test key collision"); }}
            try {{
                env_user_compare_exchange({q(variable)}, before, [true, true, "Юникод %TEMP%"]);
                array after = env_user_snapshot({q(variable)});
                if (after[2] != "Юникод %TEMP%" || !after[1]) {{ fail("registry mismatch"); }}
                bool conflict = false;
                try {{ env_user_compare_exchange({q(variable)}, before, before); }} catch (string e) {{ conflict = true; }}
                if (!conflict) {{ fail("snapshot conflict ignored"); }}
            }} finally {{ env_user_compare_exchange({q(variable)}, env_user_snapshot({q(variable)}), before); }}''')
    else:
        profile = root/'.profile'
        original = '# original user profile\nexport USER_SETTING="keep"\n'
        profile.write_text(original, encoding='utf-8'); profile.chmod(0o640)
        target = root/"путь ' literal $(touch INJECTED) [x]"
        target.mkdir()
        assert run('print(os_user_dir("data"));').stdout.strip() == str(root/'.local/share')
        code = f'array plan = user_path_plan({q(target)}, true);'
        run(code + 'fs_write_atomic("plan.json", json_value(plan)); user_path_apply(plan, false); user_path_apply(plan, false);')
        installed = profile.read_text()
        assert installed.startswith(original) and installed.count('# Skrumi PATH ') == 2
        assert profile.stat().st_mode & 0o777 == 0o640
        out = subprocess.check_output(['/bin/sh', '-c', '. "$1"; printf "%s" "$PATH"', 'sh', str(profile)], env=env, text=True)
        assert out.split(':').count(str(target)) == 1 and not (root/'INJECTED').exists()
        # Existing marker causes an exact no-op plan; no duplicate shell block.
        run(code + 'user_path_apply(plan, false);')
        assert profile.read_text() == installed
        run('array p = json_decode(read_file("plan.json")); user_path_apply(p, true); user_path_apply(p, true);')
        assert profile.read_text() == original
        # Refuse a stale rollback, then remove only our block preserving later edits.
        run('array p = json_decode(read_file("plan.json")); user_path_apply(p, false);')
        profile.write_text(profile.read_text()+'# later user edit\n')
        run('array p = json_decode(read_file("plan.json")); user_path_apply(p, true);', False)
        run(f'array p = json_decode(read_file("plan.json")); user_path_apply(user_path_removal(p, {q(target)}), false);')
        assert profile.read_text() == original+'# later user edit\n'
        run('env_user_snapshot("Path");', False)
print('PERSISTENT_ENVIRONMENT_OK')
