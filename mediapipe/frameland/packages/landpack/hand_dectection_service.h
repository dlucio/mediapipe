#pragma once

#include <iostream>
#include <string>
#include <cstdlib>
#include <bits/stdc++.h>

#include <grpcpp/grpcpp.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/logging.h"
// #include "mediapipe/framework/port/opencv_highgui_inc.h"
// #include "mediapipe/framework/port/opencv_imgproc_inc.h"
// #include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

#include "src/frameland.h"

constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";
constexpr char kWindowName[] = "MediaPipe";

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using frameland::Hand;
using frameland::HandDetection;
using frameland::HandOptions;

using namespace cv;


class MediaPipeHands {
    mediapipe::CalculatorGraph graph;
    mediapipe::OutputStreamPoller *poller;

public:
    MediaPipeHands(std::string calculator_graph_config_file = 
                    "mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt")
    {
        Setup(calculator_graph_config_file);
    }

    virtual ~MediaPipeHands()
    {
        Shutdown();
    }

    absl::Status RunMPPGraph(cv::Mat &camera_frame_raw)
    {
        LOG(INFO) << "Start grabbing and processing frames.";

        cv::Mat camera_frame;
        cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
        // cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);

        // Wrap Mat into an ImageFrame.
        auto input_frame = absl::make_unique<::mediapipe::ImageFrame>(
            ::mediapipe::ImageFormat::SRGB, camera_frame.cols, camera_frame.rows,
            ::mediapipe::ImageFrame::kDefaultAlignmentBoundary);
        cv::Mat input_frame_mat = ::mediapipe::formats::MatView(input_frame.get());
        camera_frame.copyTo(input_frame_mat);

        // Send image packet into the graph.
        size_t frame_timestamp_us =
            (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
        MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
            kInputStream, ::mediapipe::Adopt(input_frame.release())
                              .At(::mediapipe::Timestamp(frame_timestamp_us))));

        // Get the graph result packet, or stop if that fails.
        ::mediapipe::Packet packet;
        if (!poller->Next(&packet))
        {
            absl::string_view msg("Error when poller try to get the result pack from graph!");
            return absl::Status(absl::StatusCode::kUnknown, msg);
        }
        auto &output_frame_mat_view = packet.Get<::mediapipe::ImageFrame>();

        // Convert back to opencv for display or saving.
        cv::Mat output_frame_mat = ::mediapipe::formats::MatView(&output_frame_mat_view);
        cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);

        output_frame_mat.copyTo(camera_frame_raw);


        return absl::Status();
    }

protected:
    absl::Status Setup(std::string calculator_graph_config_file)
    {
        std::string calculator_graph_config_contents;
        MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
            calculator_graph_config_file,
            &calculator_graph_config_contents));

        LOG(INFO) << "Get calculator graph config contents: "
                  << calculator_graph_config_contents;
        mediapipe::CalculatorGraphConfig config =
            mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
                calculator_graph_config_contents);

        LOG(INFO) << "Initialize the calculator graph.";
        MP_RETURN_IF_ERROR(graph.Initialize(config));

        LOG(INFO) << "Start running the calculator graph.";
        ASSIGN_OR_RETURN(*poller,
                        graph.AddOutputStreamPoller(kOutputStream));
        MP_RETURN_IF_ERROR(graph.StartRun({}));

        return absl::Status();
    }

    absl::Status Shutdown()
    {
        LOG(INFO) << "Shutting down.";
        MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
        graph.WaitUntilDone();
    }

};

namespace frameland::mediapipe::landpack
{

    class HandDetectionMediaPipeServiceImpl final : public HandDetection::Service
    {

        MediaPipeHands _mediapipeHands;

        ::grpc::Status DetectAndDraw(ServerContext *context, const HandOptions *request, frameland::Image *reply) override
        {

            const int w = request->image().width();
            const int h = request->image().height();
            const int c = request->image().channels();
            const int sz = w * h * c;

            // Copy image data from request do opencv Map
            cv::Mat frame = cv::Mat(cv::Size(w, h), CV_8UC(c));
            std::memcpy(frame.data, request->image().data().c_str(), sz * sizeof(uchar));

            // Detect and draw
            _mediapipeHands.RunMPPGraph(frame);

            // Update reply image data with the processed frame.
            const std::string data((const char *)frame.data, sz);
            reply->set_data(data);
            reply->set_channels(frame.channels());
            reply->set_width(request->image().width());
            reply->set_height(request->image().height());

            frame.release();

            return Status::OK;
        }

    };

    // }

    void RunHandDetectionMediaPipeServer(
        std::string address = "0.0.0.0",
        std::string port = "50055")
    {
        google::InitGoogleLogging("FrameLand.LandPack.HandDetection");

        std::string server_address = address + ":" + port;
        HandDetectionMediaPipeServiceImpl service;

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *synchronous* service.
        builder.RegisterService(&service);
        // Finally assemble the server.
        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;

        // Wait for the server to shutdown. Note that some other thread must be
        // responsible for shutting down the server for this call to ever return.
        server->Wait();
    }

}