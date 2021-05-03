package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "hand_detection_server",
    srcs = ["landpack/hand_detection_service.cc"],
    deps = [
        "//mediapipe/frameland/packages/landpack:frameland_landpack",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
    ]
)