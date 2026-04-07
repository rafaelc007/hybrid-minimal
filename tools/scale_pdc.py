#!/usr/bin/env python3
"""
Scale a Pebble PDC image file by integer divisor.

Usage:
    python3 scale_pdc.py <input.pdc> <output.pdc> <divisor>

PDC Image binary layout (all little-endian):
  [0-3]  magic     : 'PDCI'
  [4-7]  total_len : uint32  (bytes after this field, unchanged)
  [8]    version   : uint8
  [9]    reserved  : uint8
  [10-11] vbox_w   : uint16  <- SCALED
  [12-13] vbox_h   : uint16  <- SCALED
  [14-15] num_cmds : uint16
  Then for each command:
    [+0] type        : uint8  (1=Path, 2=Circle, 3=PrecisePath)
    [+1] hidden      : uint8
    [+2] stroke_col  : uint8
    [+3] stroke_w    : uint8  <- SCALED (min 1)
    [+4] fill_col    : uint8
    Path (type 1):
      [+5] open      : uint8
      [+6] reserved  : uint8
      [+7-8] npts    : uint16
      [+9..] GPoint[npts]: (int16 x, int16 y) per point  <- SCALED
    Circle (type 2):
      [+5-6] radius  : uint16  <- SCALED
      [+7-8] cx      : int16   <- SCALED
      [+9-10] cy     : int16   <- SCALED
    PrecisePath (type 3): same header as Path, but each point is
      GPointPrecise: (int16 xi, uint8 xf, int16 yi, uint8 yf) = 6 bytes
      Only integer parts are scaled; fractions are zeroed.
"""

import struct
import sys
from pathlib import Path

PDCI_MAGIC = b'PDCI'
TYPE_PATH         = 1
TYPE_CIRCLE       = 2
TYPE_PRECISE_PATH = 3


def scale_coord(v: int, div: int) -> int:
    """Integer-division scale, preserving sign."""
    return int(v) // div


def scale_pdc(data: bytes, divisor: int) -> bytes:
    if data[:4] != PDCI_MAGIC:
        raise ValueError(f"Not a PDCI file (got {data[:4]!r})")
    if divisor < 1:
        raise ValueError("Divisor must be >= 1")

    result = bytearray(data)

    # Scale view box
    vw = struct.unpack_from('<H', result, 10)[0]
    vh = struct.unpack_from('<H', result, 12)[0]
    struct.pack_into('<H', result, 10, max(1, vw // divisor))
    struct.pack_into('<H', result, 12, max(1, vh // divisor))

    num_cmds = struct.unpack_from('<H', result, 14)[0]
    offset = 16  # first command

    for _ in range(num_cmds):
        cmd_type = result[offset]

        # Scale stroke width (min 1)
        sw = result[offset + 3]
        result[offset + 3] = max(1, sw // divisor)

        if cmd_type in (TYPE_PATH, TYPE_PRECISE_PATH):
            npts = struct.unpack_from('<H', result, offset + 7)[0]
            pts_off = offset + 9

            if cmd_type == TYPE_PATH:
                # Each point: int16 x, int16 y (4 bytes)
                for i in range(npts):
                    x = struct.unpack_from('<h', result, pts_off + i * 4)[0]
                    y = struct.unpack_from('<h', result, pts_off + i * 4 + 2)[0]
                    struct.pack_into('<h', result, pts_off + i * 4,     scale_coord(x, divisor))
                    struct.pack_into('<h', result, pts_off + i * 4 + 2, scale_coord(y, divisor))
                cmd_size = 9 + npts * 4
            else:  # TYPE_PRECISE_PATH — GPointPrecise: xi(2)+xf(1)+yi(2)+yf(1) = 6 bytes each
                for i in range(npts):
                    xi = struct.unpack_from('<h', result, pts_off + i * 6)[0]
                    yi = struct.unpack_from('<h', result, pts_off + i * 6 + 3)[0]
                    struct.pack_into('<h', result, pts_off + i * 6,     scale_coord(xi, divisor))
                    result[pts_off + i * 6 + 2] = 0  # zero fraction x
                    struct.pack_into('<h', result, pts_off + i * 6 + 3, scale_coord(yi, divisor))
                    result[pts_off + i * 6 + 5] = 0  # zero fraction y
                cmd_size = 9 + npts * 6

        elif cmd_type == TYPE_CIRCLE:
            r  = struct.unpack_from('<H', result, offset + 5)[0]
            cx = struct.unpack_from('<h', result, offset + 7)[0]
            cy = struct.unpack_from('<h', result, offset + 9)[0]
            struct.pack_into('<H', result, offset + 5, max(0, r // divisor))
            struct.pack_into('<h', result, offset + 7, scale_coord(cx, divisor))
            struct.pack_into('<h', result, offset + 9, scale_coord(cy, divisor))
            cmd_size = 11

        else:
            raise ValueError(f"Unknown GDrawCommandType {cmd_type:#04x} at offset {offset:#x}")

        offset += cmd_size

    return bytes(result)


def main():
    if len(sys.argv) != 4:
        print("Usage: scale_pdc.py <input.pdc> <output.pdc> <divisor>")
        sys.exit(1)

    src  = Path(sys.argv[1])
    dst  = Path(sys.argv[2])
    div  = int(sys.argv[3])

    data   = src.read_bytes()
    scaled = scale_pdc(data, div)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(scaled)

    orig_vw = struct.unpack_from('<H', data,   10)[0]
    orig_vh = struct.unpack_from('<H', data,   12)[0]
    new_vw  = struct.unpack_from('<H', scaled, 10)[0]
    new_vh  = struct.unpack_from('<H', scaled, 12)[0]
    print(f"{src.name}  {orig_vw}x{orig_vh}  →  {new_vw}x{new_vh}  (÷{div})  →  {dst}")


if __name__ == '__main__':
    main()
