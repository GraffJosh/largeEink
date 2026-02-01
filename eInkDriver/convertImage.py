#!/usr/bin/env python3

from PIL import Image, ImageOps
import sys
import os
import math

# ---------------- CONFIG ----------------
GAMMA = 0.4  # < 1 brightens, > 1 darkens
BYTES_PER_LINE = 16  # formatting only
# ---------------------------------------


GAMMA = 0.4


def image_to_4bpp_c(
    image_path, output_file, target_width=None, target_height=None, resample=Image.BICUBIC
):

    img = Image.open(image_path)
    # Handle transparency correctly
    if img.mode in ("RGBA", "LA"):
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img.convert("RGBA"))

    # Normalize to grayscale
    img = img.convert("L")

    # Resize if requested
    if target_width is not None and target_height is not None:
        img = img.resize(size=(int(target_width), int(target_height)), resample=resample)

    img = ImageOps.invert(img)
    w, h = img.size
    pixels = list(img.getdata())

    # Build gamma LUT (0..255 → 0..3)
    lut = []
    for i in range(256):
        norm = i / 255.0
        corrected = pow(norm, 1.0 / GAMMA)
        level = int(corrected * 4)
        lut.append(min(level, 3))

    data = []

    for y in range(h):
        row = pixels[y * w : (y + 1) * w]
        for x in range(0, w, 2):
            p0 = lut[row[x]]
            p1 = lut[row[x + 1]] if x + 1 < w else 0
            data.append((p0 << 4) | p1)
    name = os.path.splitext(os.path.basename(image_path))[0]
    write_c_array_to_file(output_file, name, data, w, h)


def write_c_array_to_file(filename, name, data, width, height, bytes_per_line=16):
    with open(filename, "a") as f:
        if "-" in name or " " in name:
            name = name.replace("-", "_").replace(" ", "_")
        f.write(f"\nconst unsigned int {name}_iconWidth = {width};\n")
        f.write(f"const unsigned int {name}_iconHeight = {height};\n")
        f.write(f"const unsigned char {name}[] PROGMEM = {{\n")

        for i in range(0, len(data), bytes_per_line):
            chunk = data[i : i + bytes_per_line]
            line = ", ".join(f"0x{b:02X}" for b in chunk)
            f.write(f"  {line},\n")

        f.write("};\n")
        f.write(f"\n// Size: {len(data)} bytes ({width}x{height}, 4bpp)\n")


def writeHeader(output_file):
    with open(output_file, "w") as f:
        f.write(f"#ifndef _IMAGE_H_\n#define _IMAGE_H_\n")
        f.write(f"#include <pgmspace.h>\n")


def writeFooter(output_file):
    with open(output_file, "a") as f:
        f.write(f"#endif _IMAGE_H_\n")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} inputDirectory OutputFilename width height")
        sys.exit(1)
    inputPath = sys.argv[1]
    if not os.path.isdir(inputPath):
        print(f"Input path {inputPath} is not a directory")
        sys.exit(1)
    outputFile = sys.argv[2]
    targetWidth = int(sys.argv[3])
    targetHeight = int(sys.argv[4])
    writeHeader(sys.argv[2])
    print("Converting ", len(os.listdir(inputPath)), "images in:", inputPath)
    for file in os.listdir(inputPath):
        if file.lower().endswith((".png", ".jpg", ".jpeg")):
            image_to_4bpp_c(
                image_path=os.path.join(inputPath, file),
                output_file=outputFile,
                target_width=targetWidth,
                target_height=targetHeight,
            )
    writeFooter(output_file=outputFile)
