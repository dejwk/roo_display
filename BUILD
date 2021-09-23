cc_library(
    name = "roo_display",
    srcs = glob(
        [
            "**/*.cpp",
            "**/*.h",
        ],
        exclude = ["test/**"],
    ),
    includes = [
        ".",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//roo_testing:arduino",
    ],
)

cc_test(
    name = "byte_order_test",
    srcs = [
        "test/byte_order_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "background_fill_optimizer_test",
    srcs = [
        "test/background_fill_optimizer_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "color_test",
    srcs = [
        "test/color_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "clip_mask_test",
    srcs = [
        "test/clip_mask_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "color_subpixel_test",
    srcs = [
        "test/color_subpixel_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "orientation_test",
    srcs = [
        "test/orientation_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "io_memory_test",
    srcs = [
        "test/io_memory_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "memfill_test",
    srcs = [
        "test/memfill_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "streamable_test",
    srcs = [
        "test/streamable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "rasterizable_test",
    srcs = [
        "test/rasterizable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "raster_test",
    srcs = [
        "test/raster_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "raw_streamable_test",
    srcs = [
        "test/raw_streamable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "offscreen_test",
    srcs = [
        "test/offscreen_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
        #        "test/testing_drawable.h"
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "basic_shapes_test",
    srcs = [
        "test/basic_shapes_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "image_test",
    srcs = [
        "test/image_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "transformed_test",
    srcs = [
        "test/testing.h",
        "test/testing_drawable.h",
        "test/transformed_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "tile_test",
    srcs = [
        "test/testing.h",
        "test/testing_drawable.h",
        "test/tile_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "smooth_font_test",
    srcs = [
        "test/testing.h",
        "test/testing_drawable.h",
        "test/smooth_font_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "text_label_test",
    srcs = [
        "test/testing.h",
        "test/testing_drawable.h",
        "test/text_label_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "compactor_test",
    srcs = [
        "test/testing.h",
        "test/compactor_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "addr_window_device_test",
    srcs = [
        "test/testing.h",
        "test/testing_display_device.h",
        "test/addr_window_device_test.cpp",
    ],
    linkstatic = 1,
    defines = [ "ROO_DISPLAY_TEST" ],
    deps = [
        "//lib/roo_display",
        "@gtest//:gtest_main",
    ],
)
