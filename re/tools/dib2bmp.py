"""Convert DelphiX Picture.Data blobs (Pascal class name + DIB) into .bmp files.

Usage: python3 dib2bmp.py <blobdir> <dfm.txt> <outdir>
Item names are taken from the decoded form text so the output files are
named like the original image-list entries.
"""
import os
import re
import struct
import sys


def main():
    blobdir, dfm, outdir = sys.argv[1:4]
    os.makedirs(outdir, exist_ok=True)
    text = open(dfm).read()
    names = {}
    for m in re.finditer(r'object (DXImageList\w+): TDXImageList.*?Items = <(.*?)>\n', text, re.S):
        for i, nm in enumerate(re.findall(r"item Name = '([^']*)'", m.group(2))):
            names[(m.group(1), i)] = nm
    for fn in sorted(os.listdir(blobdir)):
        m = re.match(r'TFORM1_Form1_(DXImageList\w+)_Items_(\d+)_Picture_Data\.bin', fn)
        if not m:
            continue
        b = open(os.path.join(blobdir, fn), 'rb').read()
        cls = b[1:1 + b[0]].decode()
        dib = b[1 + b[0]:]
        hsz, w, h, planes, bpp, comp = struct.unpack_from('<IiiHHI', dib)
        ncol = struct.unpack_from('<I', dib, 32)[0] or (1 << bpp if bpp <= 8 else 0)
        pal = ncol * 4
        bmp = b'BM' + struct.pack('<IHHI', 14 + len(dib), 0, 0, 14 + hsz + pal) + dib
        lst, idx = m.group(1), int(m.group(2))
        nm = names.get((lst, idx), str(idx)).replace('/', '_')
        out = os.path.join(outdir, '%s_%02d_%s.bmp' % (lst.replace('DXImageList', ''), idx, nm))
        open(out, 'wb').write(bmp)
        print('%s: %s %dx%d %dbpp comp=%d' % (os.path.basename(out), cls, w, h, bpp, comp))


if __name__ == '__main__':
    main()
