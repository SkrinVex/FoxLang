"""Sound without a sound card: a stand-in player records what FoxLang asks it to play.

On Linux FOXLANG_SOUND_PLAYER replaces pw-play/paplay/aplay, so the generated WAV of a
tone and the path of a file can be checked. On Windows the calls must only return a
bool: a CI machine may have no audio device.
"""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())

with tempfile.TemporaryDirectory(prefix='fox-sound-') as directory:
    root = Path(directory)
    (root / 'click.wav').write_bytes(b'RIFF' + b'\0' * 40)
    program = '''using sound;
print("tone " + play_tone(440, 250, 0.5));
print("file " + play_sound("click.wav"));
print("missing " + play_sound("nothing.wav"));
bool range = false;
try { play_tone(5, 100, 0.5); } catch (e) { range = str_contains(e, "frequency"); }
print("range " + range);
wait(300);
stop_sounds();
'''
    (root / 'main.fox').write_text(program, encoding='utf-8')
    env = dict(os.environ)
    if os.name != 'nt':
        player = root / 'player.sh'
        player.write_text('#!/bin/sh\ncp "$1" "$(dirname "$0")/played-$(basename "$1")"\necho "$1" >> "$(dirname "$0")/log.txt"\n')
        player.chmod(0o755)
        env['FOXLANG_SOUND_PLAYER'] = str(player)
    result = subprocess.run([binary, 'main.fox'], cwd=root, env=env, capture_output=True, text=True, encoding='utf-8')
    assert result.returncode == 0, result.stderr
    lines = result.stdout.split()
    assert 'missing' in result.stdout and 'missing false' in result.stdout and 'range true' in result.stdout, result.stdout
    if os.name != 'nt':
        assert 'tone true' in result.stdout and 'file true' in result.stdout, result.stdout
        played = sorted(root.glob('played-*'))
        tone = [p for p in played if p.name.startswith('played-foxlang-tone-')]
        assert len(tone) == 1 and any(p.name == 'played-click.wav' for p in played), played
        data = tone[0].read_bytes()
        riff, size, wave, fmt, fmt_size, pcm, channels, rate, byte_rate, align, bits, data_id, data_size = \
            struct.unpack('<4sI4s4sIHHIIHH4sI', data[:44])
        assert (riff, wave, fmt, data_id) == (b'RIFF', b'WAVE', b'fmt ', b'data'), data[:44]
        assert (pcm, channels, rate, bits) == (1, 1, 44100, 16) and data_size == 44100 * 250 // 1000 * 2, (rate, bits, data_size)
        samples = struct.unpack('<%dh' % (data_size // 2), data[44:44 + data_size])
        peak = max(abs(s) for s in samples)
        assert 15000 < peak <= 16384 and samples[0] == 0, peak
        # The generated file is gone once its player finished (stop_sounds collects them).
        assert not Path(tone[0].name[len('played-'):]).exists(), 'temporary tone left behind'
print('SOUND_OK')
