package(default_visibility = ["//visibility:public"])

cc_library(
    name = "frameland_mediapipe_service",
    hdrs = glob(
        [
            "**/*.h",
        ]
        
    ), 
    deps = [
        "//mediapipe/calculators/core:pass_through_calculator",
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/port:logging",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        
        "@frameland//src:frameland",
    ],
)