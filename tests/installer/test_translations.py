"""Check catalog completeness, real Skrumi translation and stable dynamic values."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
repo = Path(__file__).resolve().parents[2]
q = lambda s: json.dumps(str(s), ensure_ascii=False)
source = (repo/'installer/i18n.skm').read_text(encoding='utf-8')
rows = [json.loads('['+m+']') for m in re.findall(r'^    \[(.*)\],?$', source, re.M)]
translations = dict(rows)
assert len(rows) == len(translations)
for path in (repo/'installer').glob('*.skm'):
    for literal in re.findall(r'setup_tr\(("(?:[^"\\]|\\.)*")\)', path.read_text(encoding='utf-8')):
        assert json.loads(literal) in translations, (path, literal)
with tempfile.TemporaryDirectory(prefix='skrumi-i18n-') as temp:
    script = Path(temp)/'test.skm'
    script.write_text(f'''include({q(repo/'installer/i18n.skm')});
for (int i = 0; i < size(setup_translations); i++) {{
    setup_language = "en";
    if (setup_tr(setup_translations[i][0]) != setup_translations[i][0]) {{ fail("English changed"); }}
    setup_language = "ru";
    if (setup_tr(setup_translations[i][0]) != setup_translations[i][1]) {{ fail("Russian missing"); }}
}}
string path = "C:/Users/Verified Home/Установка/Completed successfully.skm";
if (setup_tr("Verified ") + path != "Проверен файл: " + path) {{ fail("Dynamic path translated"); }}
using graphics;
int font = load_font(fs_read_checked({q(repo/'installer/assets/DejaVuSans.ttf')}));
if (font_text_width(font, setup_tr("Recover interrupted setup"), 14) > 192) {{ fail("Recovery button clips"); }}
if (font_text_width(font, setup_tr("Native tools, scripting and graphical applications."), 18) > 704) {{ fail("Welcome clips"); }}
free_font(font);
print("INSTALLER_TRANSLATIONS_OK");
''', encoding='utf-8')
    subprocess.run([sys.argv[1], str(script)], check=True)
