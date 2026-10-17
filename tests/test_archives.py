"""USTAR extraction rejects paths/links/corruption and verifies actual file bytes."""
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='skrumi-archive-') as temp:
    root = Path(temp)
    script = root/'main.skm'
    def archive(name='folder/file.txt', kind=tarfile.REGTYPE):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode='w', format=tarfile.USTAR_FORMAT) as tar:
            entry = tarfile.TarInfo(name); entry.type = kind; entry.mode = 0o640
            data = b'contents\0bytes'
            if kind == tarfile.REGTYPE: entry.size = len(data)
            else: entry.linkname = '../outside'
            tar.addfile(entry, io.BytesIO(data))
        return output.getvalue()
    def run(data, name, ok):
        (root/'data.tar').write_bytes(data)
        destination = root/name
        script.write_text('using fs; print(json_value(extract_archive(read_checked("data.tar"), '+json.dumps(str(destination))+')));')
        p = subprocess.run([binary,str(script)], cwd=root, capture_output=True, text=True, encoding='utf-8', timeout=10)
        assert (p.returncode==0)==ok, (p.stdout,p.stderr)
        return destination
    destination = run(archive(), 'success', True)
    assert (destination/'folder/file.txt').read_bytes() == b'contents\0bytes'
    if os.name != 'nt': assert (destination/'folder/file.txt').stat().st_mode & 0o777 == 0o640
    run(archive(), 'success', False)
    cases = [archive('../escape'), archive('/absolute'), archive('C:drive'), archive('link', tarfile.SYMTYPE), archive('hardlink',tarfile.LNKTYPE), archive('device',tarfile.CHRTYPE)]
    corrupted = bytearray(archive()); corrupted[20] ^= 1; cases += [corrupted, archive()[:600]]
    for i,data in enumerate(cases):
        destination = run(data,'invalid'+str(i),False)
        assert not destination.exists(), 'malformed archive must be validated before any output'
    assert not (root/'escape').exists()
print('ARCHIVES_OK')
