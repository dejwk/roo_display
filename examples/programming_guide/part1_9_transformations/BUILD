load("@roo_testing//roo_testing/emulation:arduino.bzl", "roo_arduino_example")

roo_arduino_example(
    name = "part1_9_transformations",
    sketch = "part1_9_transformations.ino",
    visibility = ["//visibility:public"],
    deps = [
        "//:roo_display",
        "@roo_testing//roo_testing/devices/display/ili9341:spi",
        "@roo_testing//roo_testing/devices/display/st77xx:spi",
        "@roo_testing//roo_testing/devices/touch/xpt2046:spi",
        "@roo_testing//roo_testing/microcontrollers/esp32",
        "@roo_testing//roo_testing/transducers/ui/viewport:flex",
        "@roo_testing//roo_testing/transducers/ui/viewport/fltk",
    ],
)
