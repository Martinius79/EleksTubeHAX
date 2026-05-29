"""
PlatformIO pre-build script: Select and stage a subset of clockface sets.

Per-environment configuration in platformio.ini:

    custom_clockface_sets = 1-6          ; sets 1 through 6 (range syntax)
    custom_clockface_sets = 1,2,4,6      ; specific sets (comma list)
    custom_clockface_sets = 1-3,5,7      ; mix of range and list

If the option is absent or empty, all sets found in the source data directory
are included unchanged and no generated directory is created.

Set numbering: set N contains the image files N*10 .. N*10+9 (e.g. set 1 =
files 10.bmp/10.clk .. 19.bmp/19.clk, set 2 = 20.bmp .. 29.bmp, etc.).
The anchor file for detection is always N*10.bmp (or .clk).

Because the firmware (CountNumberOfClockFaces) scans for 10.bmp, 20.bmp, ...
sequentially and stops at the first gap, selected sets are always renumbered
into a contiguous sequence starting at 1 in the generated directory.

The clockfaces.txt file is also filtered and rewritten to match the selected
sets in their new order.
"""

import os
import re
import shutil

Import("env")   # noqa: F821  (SConstruct global)

_IMAGE_RE = re.compile(r"^(\d+)\.(bmp|clk)$", re.IGNORECASE)


def _parse_sets(spec):
    """Parse '1-4', '1,2,3', or '1-3,5,7' into a sorted list of ints."""
    result = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        m = re.match(r"^(\d+)\s*-\s*(\d+)$", part)
        if m:
            result.update(range(int(m.group(1)), int(m.group(2)) + 1))
        else:
            result.add(int(part))
    return sorted(result)


def _select_clockfaces():
    sets_spec = env.GetProjectOption("custom_clockface_sets", default="").strip()  # noqa: F821

    if not sets_spec:
        # No selection configured — keep the default data directory as-is.
        return

    selected = _parse_sets(sets_spec)
    if not selected:
        print("[select-clockfaces] WARNING: custom_clockface_sets is set but parsed to empty — skipping.")
        return

    project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
    source_data_dir = env.subst("$PROJECT_DATA_DIR")  # noqa: F821
    env_name = env.subst("$PIOENV")  # noqa: F821
    generated_dir = os.path.join(project_dir, ".pio", "generated_data", env_name + "_sets")

    # Always rebuild the generated directory for a clean state.
    if os.path.isdir(generated_dir):
        shutil.rmtree(generated_dir)
    os.makedirs(generated_dir)

    # Build a map: source set number -> list of (offset, ext, source_filename)
    src_files = {}
    for entry in os.listdir(source_data_dir):
        m = _IMAGE_RE.match(entry)
        if not m:
            continue
        file_num = int(m.group(1))
        src_set = file_num // 10
        offset = file_num % 10
        if src_set not in src_files:
            src_files[src_set] = []
        src_files[src_set].append((offset, m.group(2).lower(), entry))

    # Copy selected sets, renumbered to a contiguous sequence starting at 1.
    total_copied = 0
    missing_sets = []
    for dest_idx, src_set_num in enumerate(selected, start=1):
        if src_set_num not in src_files:
            missing_sets.append(src_set_num)
            continue
        dst_base = dest_idx * 10
        for offset, ext, entry in src_files[src_set_num]:
            src_path = os.path.join(source_data_dir, entry)
            dst_path = os.path.join(generated_dir, f"{dst_base + offset}.{ext}")
            shutil.copy2(src_path, dst_path)
            total_copied += 1

    if missing_sets:
        print(f"[select-clockfaces] WARNING: sets not found in source: {missing_sets}")

    # Read the original clockfaces.txt (line N = set N, 1-indexed).
    src_txt = os.path.join(source_data_dir, "clockfaces.txt")
    clockface_names = []
    if os.path.isfile(src_txt):
        with open(src_txt, "r", encoding="utf-8") as f:
            clockface_names = [line.rstrip("\r\n") for line in f if line.strip()]

    # Write filtered clockfaces.txt with names for the selected (and present) sets only.
    dst_txt = os.path.join(generated_dir, "clockfaces.txt")
    selected_names = []
    for src_set_num in selected:
        if src_set_num in src_files:
            if 1 <= src_set_num <= len(clockface_names):
                selected_names.append(clockface_names[src_set_num - 1])
            else:
                selected_names.append(f"Set {src_set_num}")

    with open(dst_txt, "w", encoding="utf-8", newline="\n") as f:
        for name in selected_names:
            f.write(name + "\n")

    env.Replace(PROJECT_DATA_DIR=generated_dir)  # noqa: F821

    print(
        f"[select-clockfaces] Sets [{sets_spec}] → {total_copied} files staged in {generated_dir}"
    )
    print(f"[select-clockfaces] Clockfaces: {', '.join(selected_names)}")


_select_clockfaces()
