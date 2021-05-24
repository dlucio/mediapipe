package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "text_processing_server",
    srcs = ["text_processing/text_processing_server.cc"],
    deps = [
        "//mediapipe/frameland/packages/textpack:frameland_textpack",
    ]
)

cc_binary(
    name = "videostream_client",
    srcs = [
        "videostream_client.cc"
    ],
    deps = [
        "//mediapipe/calculators/core:pass_through_calculator",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/port:logging",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",

        "@frameland//src:frameland",
        "@frameland//src/protos:frameland_image_processing_grpc",
        "@linux_opencv//:opencv",
        "@com_github_grpc_grpc//:grpc++",
    ]
)

cc_binary(
    name = "videostream_server",
    srcs = ["videostream_server.cc"],
    deps = [
        "//mediapipe/calculators/core:pass_through_calculator",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/port:logging",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",

        "@frameland//src:frameland",
        "@frameland//src/protos:frameland_image_processing_grpc",
        "@linux_opencv//:opencv",
        "@com_github_grpc_grpc//:grpc++",
    ]
)