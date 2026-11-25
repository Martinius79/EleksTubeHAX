# Wokwi simulator build: use a fixed program name, so wokwi.toml does not
# depend on BUILDVER. Runs after scripts/script_rename_firmware.py.
Import("env")

env.Replace(PROGNAME="wokwi_firmware")
print("script_wokwi_progname.py: Firmware program name is:", env["PROGNAME"])
