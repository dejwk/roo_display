load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "roo_display_png",
    srcs = glob(["*.c"]),
    hdrs = glob(["*.h"]),
    textual_hdrs = ["png.inl"],
    copts = [
        # Vendored zlib sources intentionally use K&R-style declarations.
        "-Wno-old-style-definition",
    ],
    includes = ["."],
    linkstatic = 1,
    visibility = ["//:__pkg__"],
)
