#!/usr/bin/env python3
"""Regenerates logo.png, the 336x336 mod icon Geode shows in the mod list.

Kept as a script rather than hand-drawn art so the icon stays reproducible and
so the repository does not depend on an image editor. Run from the repo root:

    python3 tools/make_logo.py

Writes the PNG directly with zlib, so there are no third-party dependencies.
"""

import math
import struct
import zlib

W = H = 336


def write_png(path, width, height, pixels):
    raw = b''.join(
        b'\x00' + bytes(pixels[y * width * 4:(y + 1) * width * 4])
        for y in range(height)
    )

    def chunk(tag, data):
        return (
            struct.pack('>I', len(data))
            + tag
            + data
            + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
        )

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(
            b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b'')
        )


def clamp(v):
    return 0.0 if v < 0 else (1.0 if v > 1 else v)


def smoothstep(a, b, x):
    if a == b:
        return 0.0
    t = clamp((x - a) / (b - a))
    return t * t * (3 - 2 * t)


def aces(c):
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14))


def main():
    buf = bytearray(W * H * 4)
    cx, cy = W / 2.0, H / 2.0 - 8

    for y in range(H):
        for x in range(W):
            nx = (x - cx) / (W * 0.5)
            ny = (y - cy) / (H * 0.5)
            d = math.hypot(nx, ny)

            # Background: deep indigo, darker towards the bottom.
            t = y / H
            r = 0.043 + 0.02 * (1 - t)
            g = 0.055 + 0.03 * (1 - t)
            b = 0.105 + 0.06 * (1 - t)

            # Wide warm bloom halo, the thing the mod is actually about.
            halo = math.exp(-d * d * 3.2)
            r += halo * 0.95
            g += halo * 0.52
            b += halo * 0.18
            halo2 = math.exp(-d * d * 12.0)
            r += halo2 * 0.75
            g += halo2 * 0.55
            b += halo2 * 0.30

            # A GD block, rotated, sitting inside the glow.
            dm = (abs(nx) + abs(ny)) * 1.28
            body = 1.0 - smoothstep(0.60, 0.63, dm)
            edge = smoothstep(0.50, 0.60, dm) * (1.0 - smoothstep(0.60, 0.66, dm))

            r *= 1 - body * 0.88
            g *= 1 - body * 0.88
            b *= 1 - body * 0.88

            # Emissive rim with a slight chromatic split.
            r += edge * 1.25
            g += edge * 0.95
            b += edge * 0.55
            rim = math.exp(-((dm - 0.615) ** 2) / 0.0012)
            r += rim * 0.55
            g += rim * 0.75
            b += rim * 1.00

            v = 1.0 - 0.55 * smoothstep(
                0.55, 1.35, math.hypot((x - W / 2) / (W / 2), (y - H / 2) / (H / 2))
            )
            r *= v
            g *= v
            b *= v

            r, g, b = aces(r), aces(g), aces(b)

            i = (y * W + x) * 4
            buf[i] = int(r * 255 + 0.5)
            buf[i + 1] = int(g * 255 + 0.5)
            buf[i + 2] = int(b * 255 + 0.5)
            buf[i + 3] = 255

    write_png('logo.png', W, H, buf)
    print('wrote logo.png')


if __name__ == '__main__':
    main()
