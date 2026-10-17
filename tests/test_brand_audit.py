"""Keep retired brand identifiers out of active code, paths and release inputs."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
retired = re.compile(r'fox[ _-]?lang|\.fox', re.I)
allowed = {
    'CHANGELOG.md': 'Historical entries and the breaking-release record.',
    'README.md': 'History paragraph only.',
    'docs/MIGRATION.md': 'Historical names needed for manual migration.',
    'tests/test_brand_audit.py': 'The audit pattern and its documented exceptions.',
    'tests/test_rebranding.py': 'Negative CLI/configuration compatibility regression cases.',
    'tests/test_module_resolution.py': 'Negative fallback and collision regression fixtures.',
    'tests/test_android_packaging.py': 'Negative source staging regression fixtures.',
}
paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard'], cwd=root, text=True).splitlines()
for name in sorted(set(paths)):
    path = root / name
    if not path.is_file(): continue
    assert not retired.search(name), f'Retired path: {name}'
    try: text = path.read_text(encoding='utf-8')
    except UnicodeError: continue
    matches = list(retired.finditer(text))
    if matches:
        assert name in allowed, f'Unexplained retired identifier: {name}'
        if name == 'README.md':
            assert not retired.search(text.split('## History')[0])
        print(f'{name}: {len(matches)} matches — {allowed[name]}')
print('BRAND_AUDIT_OK')
