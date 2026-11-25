# Wokwi simulator build: after the app is built, build the LittleFS image (clock faces)
# and merge bootloader + partition table + app + LittleFS into one flash image
# ($BUILD_DIR/wokwi_flash.bin), which wokwi.toml loads as "firmware".
# Unlike scripts/script_build_unified_binary.py nothing is copied into firmware/.

import csv
import os
import subprocess
import sys

Import("env")

PYTHON = env.subst("$PYTHONEXE")
PROJECT_DIR = env.subst("$PROJECT_DIR")


def _run(cmd):
    cmd = [str(c) for c in cmd]
    print("[wokwi]", " ".join(cmd))
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(proc.stdout)
    if proc.returncode != 0:
        sys.exit(proc.returncode)


def _partition_offsets():
    parts = env.BoardConfig().get("build.partitions")
    path = parts if os.path.isabs(parts) else os.path.join(PROJECT_DIR, parts)
    app_offset = fs_offset = None
    with open(path, newline="", encoding="utf-8-sig") as f:
        for row in csv.reader(f):
            if not row or row[0].strip().startswith("#") or len(row) < 4:
                continue
            ptype, subtype, offset = (c.strip().lower() for c in row[1:4])
            if ptype == "app" and subtype in ("factory", "ota_0") and app_offset is None:
                app_offset = offset
            if ptype == "data" and subtype in ("spiffs", "littlefs"):
                fs_offset = offset
    return app_offset, fs_offset


def _extra_images():
    pairs = []
    for item in env.get("FLASH_EXTRA_IMAGES", []):
        if isinstance(item, (list, tuple)) and len(item) == 2:
            pairs.append((env.subst(str(item[0])), env.subst(str(item[1]))))
    return pairs


def _esptool():
    """esptool from the PlatformIO tool package (v4 syntax: merge_bin, --flash_mode, ...)."""
    pkg = env.PioPlatform().get_package_dir("tool-esptoolpy")
    if pkg and os.path.exists(os.path.join(pkg, "esptool.py")):
        return [PYTHON, os.path.join(pkg, "esptool.py")]
    return [PYTHON, "-m", "esptool"]


def build_wokwi_flash_image(target, source, env):
    build_dir = env.subst("$BUILD_DIR")
    config = env.subst("$PROJECT_CONFIG") or os.path.join(PROJECT_DIR, "wokwi", "platformio_wokwi.ini")

    # LittleFS image with the clock faces from data/
    _run([PYTHON, "-m", "platformio", "run", "-c", config, "-e", env.subst("$PIOENV"), "--target", "buildfs"])
    fs_image = os.path.join(build_dir, "littlefs.bin")
    if not os.path.exists(fs_image):
        print("[wokwi][ERROR] LittleFS image not found:", fs_image)
        sys.exit(1)

    app_offset, fs_offset = _partition_offsets()
    sections = _extra_images()
    sections.append((app_offset, env.subst("$BUILD_DIR/${PROGNAME}.bin")))
    sections.append((fs_offset, fs_image))

    output = os.path.join(build_dir, "wokwi_flash.bin")
    args = _esptool() + ["--chip", env.get("BOARD_MCU"), "merge_bin", "-o", output,
            "--flash_mode", env["__get_board_flash_mode"](env),
            "--flash_freq", env["__get_board_f_flash"](env),
            "--flash_size", env.BoardConfig().get("upload.flash_size", "4MB")]
    for offset, path in sections:
        args += [offset, path]
    _run(args)
    print("[wokwi] Flash image written:", output)


env.AlwaysBuild("buildprog")
env.AddPostAction("buildprog", build_wokwi_flash_image)
