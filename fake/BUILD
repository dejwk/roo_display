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
        "@roo_testing//roo_testing/transducers/ui/viewport",
    ],
)
