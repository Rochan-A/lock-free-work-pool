cc_library(
    name = "concurrentqueue",
    srcs = ["concurrentqueue.h"],
    visibility = ["//visibility:public"]
)

cc_library(
    name = "blockingconcurrentqueue",
    srcs = ["blockingconcurrentqueue.h"],
    deps = [
        ":concurrentqueue",
        ":lightweightsemaphore"
    ],
    visibility = ["//visibility:public"]
)

cc_library(
    name = "lightweightsemaphore",
    srcs = ["lightweightsemaphore.h"],
    visibility = ["//visibility:public"]
)
