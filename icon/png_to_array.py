"""
This script reads icon.png in the same directory
and generates a .h/.cpp file pair containing an array
of the raw RGBA data so that it can be embedded directly
into the executable.

The output is only used for non-Windows builds.
Windows builds use icon.rc and icon.ico instead.
"""

from PIL import Image
import pathlib

# Load png
current_folder = str(pathlib.Path(__file__).parent)
image = Image.open(current_folder + "/icon.png")
pixels = image.load()
w, h = image.size

# Write .cpp
source = '#include "icon.h"\nconst char ICON_DATA[] = {\n\t'
i = 0
for y in range(h):
    for x in range(w):
        p = pixels[x, y]
        source += f"(char){p[0]}, (char){p[1]}, (char){p[2]}, (char){p[3]}, "
        i += 1
        if i % 8 == 0:
            source += "\n\t"
source = source[:-1] + "};\n"
with open(current_folder + "/icon.cpp", "w") as f:
    f.write(source)


# Write.h
header = f"""#pragma once
constexpr int ICON_WIDTH = {w};
constexpr int ICON_HEIGHT = {h};
extern const char ICON_DATA[{w} * {h} * 4];
"""
with open(current_folder + "/icon.h", "w") as f:
    f.write(header)
