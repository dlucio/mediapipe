package(default_visibility = ["//visibility:public"])

cc_library(
    name = "frameland_landpack",
    srcs = glob(
        [
            "**/*.h",
            "**/*.cc",
        ]
    ),
    hdrs = glob(
        [
            "**/*.h",
        ]
        
    ), 
    deps = [
        "//mediapipe/framework/port:logging",
        "//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",

        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",

        "@frameland//src:frameland",
        "@com_github_grpc_grpc//:grpc++",
    ],
)