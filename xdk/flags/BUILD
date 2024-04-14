cc_library(
    name = "flags",
    hdrs = ["flags.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "flags_test",
    srcs = ["flags_test.cc"],
    linkstatic = True,
    deps = [
        ":flags",
        "@googletest//:gtest_main",
    ],
)
