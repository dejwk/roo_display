load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "ili9341",
    srcs = ["main.cpp"],
    target_compatible_with = select({
        "@roo_testing//roo_testing/platforms:is_idf": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
    deps = [
        "//:roo_display",
        "@roo_testing//:esp_idf_main",
    ],
)
