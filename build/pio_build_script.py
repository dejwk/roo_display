Import("env")

# Xtensa targets (ESP32, ESP32-S2, ESP32-S3) need -mtext-section-literals to
# co-locate literal pools with IRAM code.  Without it, IRAM_ATTR functions that
# reference peripheral-register addresses trigger "dangerous relocation: l32r:
# literal placed after use" linker errors.  ESP-IDF's own CMake build sets this
# flag globally, but PlatformIO does not propagate it to library compilations.
#
# The flag is Xtensa-specific and must not be passed to RISC-V targets
# (ESP32-C3, ESP32-C6, ESP32-H2).
cc = env.get("CC", "")
if "xtensa" in cc:
    env.Append(CCFLAGS=["-mtext-section-literals"])
