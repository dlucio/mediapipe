// #include "hand_dectection_service.h"

// using namespace frameland::mediapipe::landpack;

// grpc::Status HandDetectionMediaPipeServiceImpl::DetectAndDraw(ServerContext *context, const HandOptions *request, frameland::Image *reply)
// {

//     // Create the graph
//     {
//         calculator_graph_config_contents = 
//             "mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt";
        
//         // MP_RETURN_IF_ERROR(::mediapipe::file::GetContents(
//         //     absl::GetFlag(FLAGS_calculator_graph_config_file),
//         //     &calculator_graph_config_contents));

//         LOG(INFO) << "Get calculator graph config contents: "
//                 << calculator_graph_config_contents;
//         ::mediapipe::CalculatorGraphConfig config =
//             ::mediapipe::ParseTextProtoOrDie<::mediapipe::CalculatorGraphConfig>(
//                 calculator_graph_config_contents);

//         LOG(INFO) << "Initialize the calculator graph.";
//         MP_RETURN_IF_ERROR(graph.Initialize(config));

//         LOG(INFO) << "Start running the calculator graph.";
//         ASSIGN_OR_RETURN(poller,
//                         graph.AddOutputStreamPoller(kOutputStream));
//         MP_RETURN_IF_ERROR(graph.StartRun({}));
//     }

//     // // Run the graph
//     // {

//     // }


//     reply->set_width(request->image().width());
//     reply->set_height(request->image().height());
//     reply->set_channels(request->image().channels());
//     reply->set_data(request->image().data());

//     return Status::OK;
// }

