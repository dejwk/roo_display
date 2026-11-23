load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "reference_device",
    srcs = [
        "roo_display/fake/reference_device.cpp",
        "roo_display/fake/reference_device.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        "//:roo_display",
        ":viewport_pixels",
        "@roo_testing//roo_testing/transducers/ui/viewport",
    ],
)

cc_library(
    name = "viewport_pixels",
    hdrs = ["roo_display/fake/viewport_pixels.h"],
    visibility = ["//:__pkg__"],
    deps = ["@roo_testing//roo_testing/transducers/ui/viewport"],
)
