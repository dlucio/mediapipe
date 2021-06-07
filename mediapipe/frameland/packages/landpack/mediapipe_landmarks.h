#pragma once

#include <iostream>
#include <string>
#include <cstdlib>
#include <bits/stdc++.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/logging.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";

class MediaPipeLandmarks 
{
protected:
    mediapipe::CalculatorGraph _graph;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputVideoPoller;

public:
    virtual ~MediaPipeLandmarks()
    {
        Shutdown();
    }

    virtual absl::Status RunMPPGraph(cv::Mat &camera_frame_raw)
    {
        // LOG(INFO) << "Start grabbing and processing frames.";

        cv::Mat camera_frame;
        // cv::flip(camera_frame_raw, camera_frame_raw, /*flipcode=HORIZONTAL*/ 1);
        camera_frame_raw.copyTo(camera_frame);

        ::mediapipe::ImageFormat_Format imageFormat;
        switch (camera_frame_raw.channels())
        {
        case 3:
            imageFormat = ::mediapipe::ImageFormat::SRGB;
            break;
        case 4:
            imageFormat = ::mediapipe::ImageFormat::SRGBA;
            break;
        
        default:
            break;
        } 
        
        // Wrap Mat into an ImageFrame.
        auto input_frame = absl::make_unique<::mediapipe::ImageFrame>(
            imageFormat, camera_frame.cols, camera_frame.rows,
            ::mediapipe::ImageFrame::kDefaultAlignmentBoundary);
        cv::Mat input_frame_mat = ::mediapipe::formats::MatView(input_frame.get());
        camera_frame.copyTo(input_frame_mat);

        // Send image packet into the _graph.
        size_t frame_timestamp_us =
            (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
        MP_RETURN_IF_ERROR(_graph.AddPacketToInputStream(
            kInputStream, ::mediapipe::Adopt(input_frame.release())
                              .At(::mediapipe::Timestamp(frame_timestamp_us))));

        // Get the _graph result packet, or stop if that fails.
        ::mediapipe::Packet packet;
        if (!_outputVideoPoller->Next(&packet))
        {
            absl::string_view msg("Error when _outputVideoPoller try to get the result pack from _graph!");
            return absl::Status(absl::StatusCode::kUnknown, msg);
        }
        auto &output_frame_mat_view = packet.Get<::mediapipe::ImageFrame>();

        // Convert back to opencv for display or saving.
        cv::Mat output_frame_mat = ::mediapipe::formats::MatView(&output_frame_mat_view);
        output_frame_mat.copyTo(camera_frame_raw);

        return absl::Status();
    }

    absl::Status Setup(std::string calculator_graph_config_file, bool startRun = true)
    {
        std::string calculator_graph_config_contents;
        MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
            calculator_graph_config_file,
            &calculator_graph_config_contents));

        LOG(INFO) << "Get calculator _graph config contents: "
                  << calculator_graph_config_contents;
        mediapipe::CalculatorGraphConfig config =
            mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
                calculator_graph_config_contents);

        LOG(INFO) << "Initialize the calculator _graph.";
        MP_RETURN_IF_ERROR(_graph.Initialize(config));

        ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                   _graph.AddOutputStreamPoller(kOutputStream));
        _outputVideoPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        if (startRun)
        {
            LOG(INFO) << "Start running the calculator _graph.";
            MP_RETURN_IF_ERROR(_graph.StartRun({}));
        }

        return absl::Status();
    }

    absl::Status Shutdown()
    {
        LOG(INFO) << "Shutting down.";
        MP_RETURN_IF_ERROR(_graph.CloseInputStream(kInputStream));
        _graph.WaitUntilDone();
    }

};