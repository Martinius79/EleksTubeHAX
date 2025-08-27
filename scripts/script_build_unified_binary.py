# Combines separate bin files with their respective offsets into a single file
# This single file must then be flashed to an ESP32 node with 0x0 offset.
from os.path import join
import os
import sys
import subprocess
Import("env")
platform = env.PioPlatform()


env.Execute("$PYTHONEXE -m pip install intelhex")
sys.path.append(join(platform.get_package_dir("tool-esptoolpy")))
import esptool

def pio_run_buildfs(source, target, env):
    print("script_build_unified_binary.py: Building SPIFFS filesystem image...")

    # Build the SPIFFS filesystem
    buildfs_cmd = [
        env.subst("$PYTHONEXE"),
        "-m",
        "platformio",
        "run",
        "--target",
        "buildfs"
    ]

    result = subprocess.run(
        buildfs_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if result.returncode != 0:
        print("script_build_unified_binary.py: Failed to build SPIFFS filesystem image.")
        print(result.stderr)
        sys.exit(1)
    else:
        print("script_build_unified_binary.py: Successfully built SPIFFS filesystem image.")
        print ("FLASH_EXTRA_IMAGES: ")
        print(env.subst(env.get("FLASH_EXTRA_IMAGES")))



def esp32_create_combined_bin(source, target, env):
    print("script_build_unified_binary.py: Generating combined binary for serial flashing...")

    # The offset from begin of the file where the app0 partition starts
    # This is defined in the partition .csv file
    app_offset = env.get("ESP32_APP_OFFSET")

    object_file_name = env.subst("$BUILD_DIR/FW_${PROGNAME}.bin")
    print ("FLASH_EXTRA_IMAGES: ")
    print(env.subst(env.get("FLASH_EXTRA_IMAGES")))
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")

    # Partition table file
    # Path to the partition table CSV
    partition_csv = env.GetProjectOption("board_build.partitions")
    if not partition_csv:
        print("[Error] 'board_build.partitions' not defined in platformio.ini.")
        env.Exit(1)

    # Resolve the full path to the partition CSV file
    partition_csv_path = partition_csv
    if not os.path.isabs(partition_csv_path):
        partition_csv_path = os.path.join(env.subst("$PROJECT_DIR"), partition_csv_path)

    # Check if the partition CSV file exists
    if not os.path.isfile(partition_csv_path):
        print(f"[Error] Partition CSV file '{partition_csv_path}' not found.")
        env.Exit(1)
    
    # Read all partitions from CSV, store offsets and names
    max_offset = 0
    partition_info = []  # List of tuples: (offset, name)
    with open(partition_csv_path, "r") as pf:
        for line in pf:
            line = line.strip()
            if line.startswith("#") or not line:
                continue
            parts = line.split(",")
            if len(parts) < 5:
                continue
            try:
                name = parts[0].strip()
                offset = int(parts[3], 16)
                size = int(parts[4], 16)
                end = offset + size
                if end > max_offset:
                    max_offset = end
                partition_info.append((offset, name))
            except Exception:
                continue

    # sections: list of "offset filename"
    # Build a set of offsets already present in sections
    section_offsets = set()
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        try:
            section_offsets.add(int(sect_adr, 0))
        except Exception:
            pass

    # Add missing partitions to sections
    for offset, name in partition_info:
        if offset not in section_offsets:
            # Assume file name is name + ".bin" in build dir
            part_file = os.path.join(env.subst("$BUILD_DIR"), name + ".bin")
            print(f"[Info] Adding missing partition: {hex(offset)} | {part_file}")
            sections.append(f"{hex(offset)} {part_file}")

    cmd = [
        "--chip",
        chip,
        "merge_bin",
        "-o",
        object_file_name,
    ]

    print("    Offset | File")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        print(f" -  {sect_adr} | {sect_file}")
        cmd += [sect_adr, sect_file]

    print(f" - {app_offset} | {firmware_name}")
    cmd += [app_offset, firmware_name]

    print('script_build_unified_binary.py: Using esptool.py arguments: %s' %
          ' '.join(cmd))

    esptool.main(cmd)


my_flags = env.ParseFlags(env['BUILD_FLAGS'])

# import pprint
# print("rename_firmware.py: Parsed BUILD_FLAGS:")
# pprint.pprint(my_flags)

defines = dict()
for b in my_flags.get("CPPDEFINES"):
    if isinstance(b, list):
        defines[b[0]] = b[1]
    else:
        defines[b] = b

if defines.get("CREATE_FIRMWAREFILE"):
    print("[Post-Build] CREATE_FIRMWAREFILE is defined. Registering post-build actions.")
    env.AddPostAction(
        "buildprog", [pio_run_buildfs, esp32_create_combined_bin])
else:
    print("[Post-Build] CREATE_FIRMWAREFILE is not defined. Skipping firmware merge.")
