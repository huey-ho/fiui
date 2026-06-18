#!/usr/bin/env python3
import argparse
import math
import os
import struct
import zlib


def clamp(value, lo=0, hi=255):
    return max(lo, min(hi, int(value)))


def rgba(r, g, b, a=255):
    return (clamp(r), clamp(g), clamp(b), clamp(a))


def blend(dst, src):
    sr, sg, sb, sa = src
    if sa <= 0:
        return dst
    if sa >= 255:
        return src
    dr, dg, db, da = dst
    alpha = sa / 255.0
    inv = 1.0 - alpha
    return (
        clamp(sr * alpha + dr * inv),
        clamp(sg * alpha + dg * inv),
        clamp(sb * alpha + db * inv),
        clamp(255 * (alpha + da / 255.0 * inv)),
    )


class Canvas:
    def __init__(self, width, height, color=(0, 0, 0, 0)):
        self.width = width
        self.height = height
        self.pixels = [color for _ in range(width * height)]

    def set(self, x, y, color):
        if 0 <= x < self.width and 0 <= y < self.height:
            index = y * self.width + x
            self.pixels[index] = blend(self.pixels[index], color)

    def rect(self, x, y, w, h, color):
        for py in range(max(0, y), min(self.height, y + h)):
            for px in range(max(0, x), min(self.width, x + w)):
                self.set(px, py, color)

    def rounded_rect(self, x, y, w, h, radius, color):
        r = max(0, radius)
        for py in range(max(0, y), min(self.height, y + h)):
            for px in range(max(0, x), min(self.width, x + w)):
                dx = max(x + r - px, 0, px - (x + w - r - 1))
                dy = max(y + r - py, 0, py - (y + h - r - 1))
                if dx * dx + dy * dy <= r * r:
                    self.set(px, py, color)

    def circle(self, cx, cy, radius, color):
        r2 = radius * radius
        for py in range(cy - radius, cy + radius + 1):
            for px in range(cx - radius, cx + radius + 1):
                dx = px - cx
                dy = py - cy
                if dx * dx + dy * dy <= r2:
                    self.set(px, py, color)

    def line(self, x1, y1, x2, y2, width, color):
        steps = max(abs(x2 - x1), abs(y2 - y1), 1)
        radius = max(1, width // 2)
        for i in range(steps + 1):
            t = i / steps
            x = round(x1 + (x2 - x1) * t)
            y = round(y1 + (y2 - y1) * t)
            self.circle(x, y, radius, color)

    def gradient(self, top, bottom):
        for y in range(self.height):
            t = y / max(1, self.height - 1)
            color = tuple(clamp(top[i] * (1 - t) + bottom[i] * t) for i in range(4))
            self.rect(0, y, self.width, 1, color)

    def save_png(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        raw = bytearray()
        for y in range(self.height):
            raw.append(0)
            for x in range(self.width):
                raw.extend(self.pixels[y * self.width + x])

        def chunk(kind, data):
            payload = kind + data
            return (
                struct.pack(">I", len(data))
                + payload
                + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)
            )

        png = b"\x89PNG\r\n\x1a\n"
        png += chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        png += chunk(b"IEND", b"")
        with open(path, "wb") as file:
            file.write(png)


def button_bg(path, top, bottom, accent):
    c = Canvas(240, 80)
    c.gradient(top, bottom)
    c.rounded_rect(2, 2, 236, 76, 18, rgba(255, 255, 255, 28))
    c.line(22, 56, 58, 24, 4, accent)
    c.line(58, 24, 102, 42, 4, rgba(255, 255, 255, 90))
    c.circle(190, 24, 18, rgba(255, 255, 255, 26))
    c.circle(210, 54, 10, rgba(255, 255, 255, 34))
    c.save_png(path)


def icon(path, kind, fg, bg):
    c = Canvas(64, 64)
    c.rounded_rect(4, 4, 56, 56, 14, bg)
    if kind == "check":
        c.line(18, 34, 28, 44, 7, fg)
        c.line(28, 44, 48, 22, 7, fg)
    elif kind == "warning":
        c.line(32, 15, 14, 48, 5, fg)
        c.line(32, 15, 50, 48, 5, fg)
        c.line(14, 48, 50, 48, 5, fg)
        c.line(32, 27, 32, 38, 4, fg)
        c.circle(32, 45, 3, fg)
    elif kind == "layout":
        c.rounded_rect(16, 16, 14, 14, 3, fg)
        c.rounded_rect(34, 16, 14, 14, 3, fg)
        c.rounded_rect(16, 34, 32, 14, 3, fg)
    elif kind == "resource":
        c.circle(32, 20, 8, fg)
        c.circle(22, 42, 8, fg)
        c.circle(44, 42, 8, fg)
        c.line(29, 27, 24, 35, 3, fg)
        c.line(35, 27, 42, 35, 3, fg)
    elif kind == "spark":
        for angle in range(0, 360, 45):
            rad = math.radians(angle)
            x1 = round(32 + math.cos(rad) * 8)
            y1 = round(32 + math.sin(rad) * 8)
            x2 = round(32 + math.cos(rad) * 22)
            y2 = round(32 + math.sin(rad) * 22)
            c.line(x1, y1, x2, y2, 4, fg)
        c.circle(32, 32, 7, fg)
    c.save_png(path)


def preview(path, dark):
    c = Canvas(720, 360)
    if dark:
        c.gradient(rgba(19, 24, 31, 255), rgba(10, 14, 20, 255))
        surface = rgba(34, 41, 52, 255)
        card = rgba(46, 56, 70, 255)
        text = rgba(220, 228, 238, 255)
        muted = rgba(130, 145, 165, 255)
        accent = rgba(82, 145, 255, 255)
    else:
        c.gradient(rgba(244, 248, 252, 255), rgba(225, 232, 240, 255))
        surface = rgba(255, 255, 255, 255)
        card = rgba(236, 242, 249, 255)
        text = rgba(30, 38, 48, 255)
        muted = rgba(118, 130, 146, 255)
        accent = rgba(34, 108, 220, 255)

    c.rounded_rect(40, 34, 640, 292, 24, surface)
    c.rounded_rect(68, 66, 178, 224, 16, card)
    c.rounded_rect(274, 66, 378, 80, 16, card)
    c.rounded_rect(274, 168, 178, 122, 16, card)
    c.rounded_rect(474, 168, 178, 122, 16, card)
    c.rect(92, 94, 96, 10, text)
    c.rect(92, 118, 126, 8, muted)
    c.rounded_rect(92, 154, 104, 36, 10, accent)
    c.rounded_rect(92, 210, 126, 12, 6, muted)
    c.rounded_rect(92, 236, 92, 12, 6, muted)
    c.rect(306, 96, 180, 12, text)
    c.rect(306, 120, 260, 8, muted)
    c.circle(612, 106, 20, accent)
    for i, h in enumerate([44, 72, 54, 92, 68]):
        x = 306 + i * 24
        c.rounded_rect(x, 262 - h, 14, h, 5, accent if i % 2 else muted)
    c.line(512, 246, 542, 206, 5, accent)
    c.line(542, 206, 574, 232, 5, accent)
    c.line(574, 232, 622, 184, 5, accent)
    c.save_png(path)


def resource_preview(path):
    c = Canvas(720, 360)
    c.gradient(rgba(18, 22, 30, 255), rgba(12, 15, 22, 255))
    c.rounded_rect(42, 36, 636, 288, 24, rgba(33, 39, 50, 255))
    for row in range(3):
        y = 78 + row * 72
        c.rounded_rect(78, y, 564, 48, 14, rgba(47, 56, 70, 255))
        c.circle(112, y + 24, 14, rgba(138, 92, 246, 255))
        c.rect(142, y + 15, 180, 8, rgba(226, 232, 240, 255))
        c.rect(142, y + 31, 300 - row * 42, 6, rgba(139, 151, 170, 255))
        c.rounded_rect(538, y + 14, 76, 20, 10, rgba(34, 160, 112, 255))
    c.line(106, 92, 106, 246, 3, rgba(138, 92, 246, 120))
    c.save_png(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    out = args.out
    os.makedirs(out, exist_ok=True)

    preview(os.path.join(out, "control-center-preview.png"), True)
    preview(os.path.join(out, "control-center-preview-light.png"), False)
    resource_preview(os.path.join(out, "control-center-resource-preview.png"))
    button_bg(os.path.join(out, "button-normal.png"), rgba(32, 88, 168, 255), rgba(21, 54, 120, 255), rgba(115, 178, 255, 210))
    button_bg(os.path.join(out, "button-hover.png"), rgba(47, 128, 235, 255), rgba(28, 86, 184, 255), rgba(170, 215, 255, 230))
    button_bg(os.path.join(out, "button-pressed.png"), rgba(17, 56, 130, 255), rgba(8, 30, 84, 255), rgba(90, 150, 235, 230))
    icon(os.path.join(out, "button-icon.png"), "spark", rgba(255, 255, 255, 255), rgba(62, 132, 245, 255))
    icon(os.path.join(out, "icon-check.png"), "check", rgba(255, 255, 255, 255), rgba(34, 160, 112, 255))
    icon(os.path.join(out, "icon-warning.png"), "warning", rgba(255, 255, 255, 255), rgba(220, 74, 86, 255))
    icon(os.path.join(out, "icon-layout.png"), "layout", rgba(255, 255, 255, 255), rgba(82, 112, 255, 255))
    icon(os.path.join(out, "icon-resource.png"), "resource", rgba(255, 255, 255, 255), rgba(138, 92, 246, 255))


if __name__ == "__main__":
    main()
