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
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
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
constexpr char kHandCountOutputStream[] = "hand_count";
constexpr char kLandmarksOutputStream[] = "landmarks";

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
    std::unique_ptr<mediapipe::OutputStreamPoller> outputVideoPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> outputLandmarksPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> outputHandCountPoller;
    int prev_face_count;

public:
    MediaPipeHands(std::string calculator_graph_config_file = 
                    "mediapipe/frameland/graphs/hand_tracking_desktop_live.pbtxt")
                    // "mediapipe/graphs/holistic_tracking/holistic_tracking_cpu.pbtxt")
                    // "mediapipe/graphs/pose_tracking/upper_body_pose_tracking_cpu.pbtxt")
                    // "mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt")
    {
        LOG(INFO) << Setup(calculator_graph_config_file);
        prev_face_count = -1;
    }

    virtual ~MediaPipeHands()
    {
        Shutdown();
    }

    absl::Status RunMPPGraph(cv::Mat &camera_frame_raw)
    {
        // LOG(INFO) << "Start grabbing and processing frames.";

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

        
        ::mediapipe::Packet handCountPacket;
        if (!outputHandCountPoller->Next(&handCountPacket))
        {
            absl::string_view msg("Error when outputHandCountPoller try to get the result pack from graph!");
        }
        auto &hand_count = handCountPacket.Get<int>();

        if (hand_count != 0)
        {
            LOG(INFO) << "Found hand count : " << hand_count;

            ::mediapipe::Packet landmarksPacket;
            if (!outputLandmarksPoller->Next(&landmarksPacket))
            {
                absl::string_view msg("Error when outputLandmarksPoller try to get the result pack from graph!");
            }
            auto &multi_hand_landmarks = landmarksPacket.Get<std::vector<::mediapipe::NormalizedLandmarkList>>();

            // Tip: https://gist.github.com/eknight7/d4a57504c8f866fc80c0eb2c61ff6b4f
            LOG(INFO) << "#Multi Hand landmarks: " << multi_hand_landmarks.size();
            int hand_id = 0;
            for (const auto &single_hand_landmarks : multi_hand_landmarks)
            {
                ++hand_id;
                LOG(INFO) << "Hand [" << hand_id << "]:";
                for (int i = 0; i < single_hand_landmarks.landmark_size(); ++i)
                {
                    const auto &landmark = single_hand_landmarks.landmark(i);
                    LOG(INFO) << "\tLandmark [" << i << "]: ("
                              << landmark.x() << ", "
                              << landmark.y() << ", "
                              << landmark.z() << ")";
                }
            }
        }

        // Get the graph result packet, or stop if that fails.
        ::mediapipe::Packet packet;
        if (!outputVideoPoller->Next(&packet))
        {
            absl::string_view msg("Error when outputVideoPoller try to get the result pack from graph!");
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

        ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                        graph.AddOutputStreamPoller(kLandmarksOutputStream));
        // Tip: https://github.com/google/mediapipe/issues/1537
        outputLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   graph.AddOutputStreamPoller(kHandCountOutputStream));
        outputHandCountPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   graph.AddOutputStreamPoller(kOutputStream));
        outputVideoPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        LOG(INFO) << "Start running the calculator graph.";
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
    public:
        HandDetectionMediaPipeServiceImpl()
            : _mediapipeHands( new MediaPipeHands)
        {}

    private:

        std::auto_ptr<MediaPipeHands> _mediapipeHands;

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
            absl::Status status =  _mediapipeHands->RunMPPGraph(frame);
            // LOG(INFO) << status;

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