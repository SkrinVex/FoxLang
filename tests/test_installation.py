"""Stage CMake and Linux installer in isolated directories; never alter the host profile."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

source = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='skrumi-install-') as temp:
    root = Path(temp)
    prefix = root / 'cmake prefix'
    subprocess.run(['cmake', '--install', str(binary.parent), '--prefix', str(prefix)], check=True, capture_output=True)
    suffix = binary.suffix
    for name in ('skrumi', 'foxlang', 'skrumi-lsp', 'foxlang-lsp'):
        subprocess.run([str(prefix / 'bin' / (name + suffix)), '--version'], check=True, capture_output=True)
    assert (prefix / 'include/skrumi/Skrumi.h').exists()
    assert (prefix / 'include/foxlang/FoxLang.h').exists()
    if os.name != 'nt':
        package = root / 'package'; package.mkdir()
        for name in ('skrumi', 'skrumi-lsp'):
            shutil.copy2(binary.parent / name, package / (name + '.bin'))
        shutil.copy2(source / 'VERSION', package / 'VERSION')
        shutil.copy2(source / 'packaging/linux/install.sh', package / 'install.sh')
        shutil.copytree(source / 'std', package / 'std')
        home = root / 'profile'; home.mkdir()
        prefix = root / 'space $dollar "quoted" prefix'
        env = dict(os.environ, HOME=str(home), SKRUMI_PREFIX=str(prefix))
        def install(ok=True):
            result = subprocess.run(['bash', str(package / 'install.sh')], env=env, capture_output=True, text=True)
            assert (result.returncode == 0) == ok, result.stderr
        install()
        install() # update owned launchers, including escaped paths
        for name in ('skrumi', 'foxlang', 'skrumi-lsp', 'foxlang-lsp'):
            subprocess.run([str(prefix / 'bin' / name), '--version'], check=True, capture_output=True)
        personal = prefix / 'share/skrumi/personal.skm'
        personal.write_text('print("keep me");')
        install(False)
        refused = subprocess.run(['bash', str(prefix / 'share/skrumi/uninstall.sh')], capture_output=True)
        assert refused.returncode != 0 and personal.read_text() == 'print("keep me");'
        personal.unlink()
        foreign = prefix / 'bin/foxlang' 
        foreign.write_text('#!/bin/sh\necho unrelated\n')
        install(False)
        subprocess.run(['bash', str(prefix / 'share/skrumi/uninstall.sh')], check=True, capture_output=True)
        assert foreign.read_text() == '#!/bin/sh\necho unrelated\n'
        assert not (prefix / 'bin/skrumi').exists()
print('INSTALLATION_OK')
