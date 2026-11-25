"""Compile the Wokwi custom chips (wokwi/chips/*.chip.c) to WebAssembly.

Uses the Zig C compiler from the PyPI package "ziglang" (no LLVM/wasi-sdk install needed).
One-time setup of a local, git-ignored toolchain:

    python -m venv .pio/wokwi-toolchain
    .pio/wokwi-toolchain/Scripts/python -m pip install ziglang      (Windows)
    .pio/wokwi-toolchain/bin/python -m pip install ziglang          (Linux/macOS)

Then, from the project root:

    .pio/wokwi-toolchain/Scripts/python wokwi/build_chip.py
"""

import pathlib
import subprocess
import sys

CHIP_DIR = pathlib.Path(__file__).resolve().parent / "chips"


def main():
    # --debug: link wasi-libc and enable the chip's printf() instrumentation (CHIP_DEBUG)
    debug = "--debug" in sys.argv[1:]
    sources = sorted(CHIP_DIR.glob("*.chip.c"))
    if not sources:
        sys.exit("No *.chip.c files found in " + str(CHIP_DIR))
    for src in sources:
        out = src.with_suffix(".wasm")  # foo.chip.c -> foo.chip.wasm
        cmd = [
            sys.executable, "-m", "ziglang", "cc",
            "--target=wasm32-wasi", "-O2", "-Wall", "-Werror", "-Wno-unused-function",
            *(["-DCHIP_DEBUG", "-mexec-model=reactor"] if debug else ["-nostdlib"]),
            "-Wl,--no-entry", "-Wl,--import-memory", "-Wl,--export-table",
            "-Wl,-z,stack-size=65536",
            "-I", str(CHIP_DIR),
            "-o", str(out), str(src),
        ]
        print(" ".join(cmd))
        subprocess.run(cmd, check=True)
        print(f"-> {out.relative_to(CHIP_DIR.parent.parent)} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
