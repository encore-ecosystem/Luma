#!/usr/bin/env python3
"""Rebuild the checked-in Vulkan shader arrays using glslc."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / "workspace/ui_native"
output = ["/* Generated from shaders/ui.vert and shaders/ui.frag with glslc. */"]
with tempfile.TemporaryDirectory(prefix="luma-shaders-") as directory:
    for stage, name in (("vert", "vertex"), ("frag", "fragment")):
        binary = Path(directory) / f"ui.{stage}.spv"
        subprocess.run(["glslc", str(root / f"shaders/ui.{stage}"), "-o", str(binary)], check=True)
        data = binary.read_bytes()
        words = struct.unpack(f"<{len(data) // 4}I", data)
        output.append(f"static const uint32_t luma_ui_{name}_spirv[] = {{")
        for offset in range(0, len(words), 10):
            output.append(",".join(f"{value}u" for value in words[offset:offset + 10]) + ",")
        output.append("};")
(root / "ui_shaders.inc").write_text("\n".join(output) + "\n")
