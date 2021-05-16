#pragma once

#include <grpcpp/grpcpp.h>

#include "src/frameland.h"
#include "mediapipe_landmarks.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using frameland::Hand;
using frameland::HandDetection;
using frameland::HandOptions;

constexpr char kHandCountOutputStream[] = "hand_count";
constexpr char kLandmarksOutputStream[] = "landmarks";

class MediaPipeHands : public MediaPipeLandmarks
{
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputLandmarksPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputHandCountPoller;

public:
    MediaPipeHands(std::string calculator_graph_config_file = 
                    "mediapipe/frameland/graphs/hand_tracking_desktop_live.pbtxt")
    {
        LOG(INFO) << Setup(calculator_graph_config_file);
    }

    absl::Status RunMPPGraph(cv::Mat &camera_frame_raw)
    {
        MP_RETURN_IF_ERROR(MediaPipeLandmarks::RunMPPGraph(camera_frame_raw));

        ::mediapipe::Packet handCountPacket;
        if (!_outputHandCountPoller->Next(&handCountPacket))
        {
            absl::string_view msg("Error when _outputHandCountPoller try to get the result pack from _graph!");
        }
        auto &hand_count = handCountPacket.Get<int>();

        if (hand_count != 0)
        {
            LOG(INFO) << "Found hand count : " << hand_count;

            ::mediapipe::Packet landmarksPacket;
            if (!_outputLandmarksPoller->Next(&landmarksPacket))
            {
                absl::string_view msg("Error when _outputLandmarksPoller try to get the result pack from _graph!");
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

        return absl::Status();
    }

protected:
    absl::Status Setup(std::string calculator_graph_config_file)
    {
        MediaPipeLandmarks::Setup(calculator_graph_config_file, false);

        ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                        _graph.AddOutputStreamPoller(kLandmarksOutputStream));
        // Tip: https://github.com/google/mediapipe/issues/1537
        _outputLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   _graph.AddOutputStreamPoller(kHandCountOutputStream));
        _outputHandCountPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        LOG(INFO) << "Start running the calculator _graph.";
        MP_RETURN_IF_ERROR(_graph.StartRun({}));

        return absl::Status();
    }

};

namespace frameland::mediapipe::landpack
{

    class HandDetectionMediaPipeServiceImpl final : public HandDetection::Service
    {
    public:
        HandDetectionMediaPipeServiceImpl()
            : _mediapipeHands(new MediaPipeHands)
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