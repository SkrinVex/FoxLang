"""PNG and BMP decoding checked pixel by pixel against images written here.

The encoder below covers what the decoder must handle: every color type, bit depths
1..16, palettes with transparency, tRNS color keys, all five row filters, stored and
compressed zlib data and Adam7 interlacing. No window is needed.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

binary = str(Path(sys.argv[1]).resolve())
WIDTH, HEIGHT = 11, 7


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)


def pack_row(samples, depth):
    if depth == 8:
        return bytes(samples)
    if depth == 16:
        return b''.join(struct.pack('>H', s) for s in samples)
    out, acc, bits = bytearray(), 0, 0
    for s in samples:
        acc = (acc << depth) | s
        bits += depth
        if bits == 8:
            out.append(acc)
            acc, bits = 0, 0
    if bits:
        out.append(acc << (8 - bits))
    return bytes(out)


def filtered(rows, bpp):
    out, previous = bytearray(), bytes(len(rows[0]) if rows else 0)
    for index, row in enumerate(rows):
        kind = index % 5
        line = bytearray()
        for i, value in enumerate(row):
            left = row[i - bpp] if i >= bpp else 0
            up = previous[i]
            corner = previous[i - bpp] if i >= bpp else 0
            if kind == 0: predicted = 0
            elif kind == 1: predicted = left
            elif kind == 2: predicted = up
            elif kind == 3: predicted = (left + up) // 2
            else:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                predicted = left if pa <= pb and pa <= pc else up if pb <= pc else corner
            line.append((value - predicted) & 255)
        out.append(kind)
        out += line
        previous = row
    return bytes(out)


def png(samples_of, color_type, depth, channels, extra=b'', interlace=False, level=6):
    bpp = max(1, channels * depth // 8)
    data = b''
    if interlace:
        passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
        for x0, y0, dx, dy in passes:
            xs, ys = range(x0, WIDTH, dx), range(y0, HEIGHT, dy)
            if not len(xs) or not len(ys):
                continue
            rows = [pack_row([s for x in xs for s in samples_of(x, y)], depth) for y in ys]
            data += filtered(rows, bpp)
    else:
        rows = [pack_row([s for x in range(WIDTH) for s in samples_of(x, y)], depth) for y in range(HEIGHT)]
        data = filtered(rows, bpp)
    header = struct.pack('>IIBBBBB', WIDTH, HEIGHT, depth, color_type, 0, 0, 1 if interlace else 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + extra + chunk(b'IDAT', zlib.compress(data, level)) + chunk(b'IEND', b'')


def rgb(x, y):
    return ((x * 23) % 256, (y * 37) % 256, (x * y * 11) % 256)


cases = []  # (name, bytes, expected (rgb, alpha) per pixel)
for depth in (1, 2, 4, 8, 16):
    top = (1 << depth) - 1
    gray = lambda x, y, top=top: (x + y * 3) % (top + 1)
    scale = 255 // top if depth <= 8 else None
    def expect_gray(x, y, depth=depth, gray=gray, scale=scale):
        v = gray(x, y) * scale if depth <= 8 else gray(x, y) >> 8
        return ((v << 16) | (v << 8) | v, 255)
    cases.append((f'gray{depth}', png(lambda x, y, gray=gray: [gray(x, y)], 0, depth, 1), expect_gray))
for depth in (8, 16):
    samples = (lambda x, y: list(rgb(x, y))) if depth == 8 else (lambda x, y: [c * 257 for c in rgb(x, y)])
    cases.append((f'rgb{depth}', png(samples, 2, depth, 3), lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], 255)))
    rgba = (lambda x, y: list(rgb(x, y)) + [(x * 20) % 256]) if depth == 8 else (lambda x, y: [c * 257 for c in rgb(x, y)] + [((x * 20) % 256) * 257])
    cases.append((f'rgba{depth}', png(rgba, 6, depth, 4), lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], (x * 20) % 256)))
cases.append(('graya8', png(lambda x, y: [x * 10, y * 30], 4, 8, 2), lambda x, y: (((x * 10) << 16) | ((x * 10) << 8) | x * 10, y * 30)))
palette = [(i * 30 % 256, i * 70 % 256, i * 110 % 256) for i in range(16)]
plte = chunk(b'PLTE', bytes(c for entry in palette for c in entry))
trns = chunk(b'tRNS', bytes([0, 128]))
for depth in (1, 2, 4, 8):
    count = min(16, 1 << depth)
    index = lambda x, y, count=count: (x + y) % count
    def expect_palette(x, y, index=index):
        r, g, b = palette[index(x, y)]
        return ((r << 16) | (g << 8) | b, 0 if index(x, y) == 0 else 128 if index(x, y) == 1 else 255)
    cases.append((f'palette{depth}', png(lambda x, y, index=index: [index(x, y)], 3, depth, 1, plte + trns), expect_palette))
key = chunk(b'tRNS', struct.pack('>HHH', *rgb(3, 2)))
cases.append(('rgbkey', png(lambda x, y: list(rgb(x, y)), 2, 8, 3, key),
              lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], 0 if (x, y) == (3, 2) else 255)))
cases.append(('interlaced', png(lambda x, y: list(rgb(x, y)) + [200], 6, 8, 4, interlace=True),
              lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], 200)))
cases.append(('interlaced4', png(lambda x, y: [(x + y) % 16], 0, 4, 1, interlace=True),
              lambda x, y: (((x + y) % 16 * 17) * 0x10101, 255)))
cases.append(('stored', png(lambda x, y: list(rgb(x, y)), 2, 8, 3, level=0),
              lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], 255)))
# BMP, bottom-up 24-bit
rows = b''
for y in reversed(range(HEIGHT)):
    row = b''.join(bytes((rgb(x, y)[2], rgb(x, y)[1], rgb(x, y)[0])) for x in range(WIDTH))
    rows += row + b'\0' * ((4 - len(row) % 4) % 4)
bmp = b'BM' + struct.pack('<IHHI', 54 + len(rows), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, WIDTH, HEIGHT, 1, 24, 0, len(rows), 0, 0, 0, 0) + rows
cases.append(('bmp24', bmp, lambda x, y: ((rgb(x, y)[0] << 16) | (rgb(x, y)[1] << 8) | rgb(x, y)[2], 255)))

with tempfile.TemporaryDirectory(prefix='fox-images-') as directory:
    root = Path(directory)
    lines = ['using graphics;']
    for name, data, _ in cases:
        (root / (name + ('.bmp' if name.startswith('bmp') else '.png'))).write_bytes(data)
    for name, _, _ in cases:
        file = name + ('.bmp' if name.startswith('bmp') else '.png')
        lines.append(f'int {name} = load_image("{file}");')
        lines.append(f'string {name}_out = "{name} " + image_width({name}) + "x" + image_height({name});')
        lines.append(f'for (int y = 0; y < image_height({name}); y++) {{ for (int x = 0; x < image_width({name}); x++) {{ '
                     f'{name}_out += " " + image_pixel({name}, x, y) + "/" + image_alpha({name}, x, y); }} }}')
        lines.append(f'print({name}_out);')
    lines.append('bool damaged = false;')
    lines.append('write_file("bad.png", "\\x89PNG broken");')
    lines.append('try { int bad = load_image("bad.png"); } catch (e) { damaged = str_contains(e, "Graphics Error"); }')
    lines.append('print("damaged " + damaged);')
    (root / 'main.fox').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    result = subprocess.run([binary, 'main.fox'], cwd=root, capture_output=True, text=True, encoding='utf-8')
    assert result.returncode == 0, result.stderr
    printed = {line.split(' ', 1)[0]: line for line in result.stdout.splitlines()}
    for name, _, expect in cases:
        values = printed[name].split()
        assert values[1] == f'{WIDTH}x{HEIGHT}', (name, values[1])
        got = values[2:]
        want = [f'{expect(x, y)[0]}/{expect(x, y)[1]}' for y in range(HEIGHT) for x in range(WIDTH)]
        bad = [(i % WIDTH, i // WIDTH, g, w) for i, (g, w) in enumerate(zip(got, want)) if g != w]
        assert not bad, (name, bad[:5])
    assert printed['damaged'] == 'damaged true', printed['damaged']
print('IMAGES_OK', len(cases), 'files')
