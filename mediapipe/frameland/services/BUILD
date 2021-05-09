package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "hand_detection_server",
    srcs = ["landpack/hand_detection_service.cc"],
    deps = [
        "//mediapipe/frameland/packages/landpack:frameland_landpack",
        "//mediapipe/frameland/calculators/util:counting_vector_size_calculator",

        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "//mediapipe/graphs/pose_tracking:pose_tracking_cpu_deps",
        "//mediapipe/graphs/pose_tracking:upper_body_pose_tracking_cpu_deps",
        "//mediapipe/graphs/holistic_tracking:holistic_tracking_cpu_graph_deps",
    ]
)