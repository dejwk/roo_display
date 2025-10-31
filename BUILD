cc_library(
    name = "roo_display",
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.c",
            "src/**/*.h",
            "src/**/*.inl",
        ],
        exclude = ["test/**"],
    ),
    linkstatic = 1,
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@roo_backport",
        "@roo_collections",
        "@roo_io",
        "@roo_testing//:arduino",
        "@roo_testing//roo_testing/frameworks/arduino-esp32-2.0.4/libraries/FS",
        "@roo_testing//roo_testing/frameworks/arduino-esp32-2.0.4/libraries/Wire"
    ],
)

cc_library(
    name = "testing",
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.c",
            "src/**/*.h",
            "src/**/*.inl",
        ],
        exclude = ["test/**"],
    ),
    defines = ["ROO_DISPLAY_TESTING"],
    linkstatic = 1,
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@roo_collections",
        "@roo_io",
        "@roo_testing//:arduino_gtest_main",
    ],
)

cc_test(
    name = "background_filter_test",
    srcs = [
        "test/background_filter_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "color_test",
    srcs = [
        "test/color_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "color_blending_test",
    srcs = [
        "test/color_blending_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
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
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "color_subpixel_test",
    srcs = [
        "test/color_subpixel_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "drawing_context_test",
    srcs = [
        "test/drawing_context_test.cpp",
        "test/testing.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "front_to_back_writer_test",
    srcs = [
        "test/front_to_back_writer_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "foreground_filter_test",
    srcs = [
        "test/foreground_filter_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "nibble_rect_test",
    srcs = [
        "test/nibble_rect_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "orientation_test",
    srcs = [
        "test/orientation_test.cpp",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "smooth_shapes_test",
    srcs = [
        "test/smooth_shapes_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "streamable_test",
    srcs = [
        "test/streamable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "streamable_stack_test",
    srcs = [
        "test/streamable_stack_test.cpp",
        "test/testing.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "rasterizable_test",
    srcs = [
        "test/rasterizable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "rasterizable_stack_test",
    srcs = [
        "test/rasterizable_stack_test.cpp",
        "test/testing.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "raster_test",
    srcs = [
        "test/raster_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "raw_streamable_test",
    srcs = [
        "test/raw_streamable_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
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
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "basic_shapes_test",
    srcs = [
        "test/basic_shapes_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "image_test",
    srcs = [
        "test/image_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
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
    deps = [
        ":testing",
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
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "smooth_font_test",
    srcs = [
        "test/smooth_font_test.cpp",
        "test/testing.h",
        "test/testing_drawable.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
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
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "compactor_test",
    srcs = [
        "test/compactor_test.cpp",
        "test/testing.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)

cc_test(
    name = "addr_window_device_test",
    srcs = [
        "test/addr_window_device_test.cpp",
        "test/testing.h",
        "test/testing_display_device.h",
    ],
    linkstatic = 1,
    deps = [
        ":testing",
    ],
)
