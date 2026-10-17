"""CI-only: obtain the previous stable CLI for a same-runner benchmark comparison."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile


def prepare(destination):
    root = Path(destination).resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = os.environ.get('GITHUB_REPOSITORY', 'SkrinVex/Skrumi')
    # Network/authentication failures are errors, never a fabricated first release.
    releases = json.loads(subprocess.check_output([
        'gh', 'api', f'repos/{repo}/releases', '--paginate', '--slurp'], text=True))
    stable = [release for page in releases for release in page
              if not release['draft'] and not release['prerelease']]
    if not stable:
        print('No previous stable release; no baseline is available.')
        return
    release = stable[0]
    assets = {asset['name']: asset for asset in release['assets']}
    installer = 'Skrumi-linux-x86_64.run'
    if installer in assets:
        subprocess.run(['gh', 'release', 'download', release['tag_name'], '--repo', repo,
                        '--pattern', installer, '--dir', str(root)], check=True)
        setup = root / installer
        setup.chmod(0o700)
        target = root / 'runtime'
        subprocess.run([str(setup), '--headless', 'install', str(target)], check=True,
                       timeout=180)
        binary = target / 'skrumi'
    else:
        # Historical releases remain supported. Never extract an archive tree:
        # copy only the unique regular CLI member to a controlled filename.
        archives = [name for name in assets if name.endswith('-linux-x86_64.tar.gz')]
        if len(archives) != 1:
            raise RuntimeError('Previous stable release has no supported Linux baseline')
        subprocess.run(['gh', 'release', 'download', release['tag_name'], '--repo', repo,
                        '--pattern', archives[0], '--dir', str(root)], check=True)
        binary = root / 'skrumi'
        with tarfile.open(root / archives[0], 'r:gz') as archive:
            candidates = [m for m in archive.getmembers() if m.isfile()
                          and Path(m.name).name in ('skrumi.bin', 'foxlang.bin')]
            primary = [m for m in candidates if Path(m.name).name == 'skrumi.bin']
            candidates = primary or candidates
            if len(candidates) != 1 or not 0 < candidates[0].size <= 64 * 1024 * 1024:
                raise RuntimeError('Ambiguous or oversized historical CLI')
            with archive.extractfile(candidates[0]) as source:
                binary.write_bytes(source.read())
        binary.chmod(0o700)
    subprocess.run([str(binary), '--version'], check=True, timeout=15)
    (root / 'baseline-path').write_text(str(binary), encoding='utf-8')


if __name__ == '__main__':
    prepare(sys.argv[1])
