#pragma once

#include <grpcpp/grpcpp.h>

#include "src/frameland.h"
#include "mediapipe_landmarks.h"
#include "mediapipe/framework/formats/classification.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using frameland::Hand;
using frameland::Hands;
using frameland::Landmark;
using frameland::HandDetection;
using frameland::HandsRequest;
using frameland::HandsReply;

constexpr char kHandCountOutputStream[] = "hand_count";
constexpr char kLandmarksOutputStream[] = "landmarks";
constexpr char kHandednessOutputStream[] = "handedness";
constexpr char kHandRectsOutputStream[] = "multi_hand_rects";
constexpr char kPalmRectsOutputStream[] = "multi_palm_rects";

class MediaPipeHands : public MediaPipeLandmarks
{
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputHandednessPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputLandmarksPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputHandCountPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputHandRectsPoller;
    std::unique_ptr<mediapipe::OutputStreamPoller> _outputPalmRectsPoller;

public:
    MediaPipeHands(std::string calculator_graph_config_file = 
                    "mediapipe/frameland/graphs/hand_tracking_desktop_live.pbtxt")
    {
        LOG(INFO) << Setup(calculator_graph_config_file);
    }

    absl::Status RunMPPGraph(cv::Mat &camera_frame_raw)
    {
        MP_RETURN_IF_ERROR(MediaPipeLandmarks::RunMPPGraph(camera_frame_raw));
        return absl::Status();
    }

    absl::Status RunMPPGraph(
        cv::Mat &camera_frame_raw, 
        std::vector<::mediapipe::ClassificationList> &multi_handedness,
        std::vector<::mediapipe::NormalizedLandmarkList> &multi_hand_landmarks,
        std::vector<::mediapipe::NormalizedRect> &multi_hand_rects,
        std::vector<::mediapipe::NormalizedRect> &multi_palm_rects)
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
            ::mediapipe::Packet handednessPacket;
            if (!_outputHandednessPoller->Next(&handednessPacket))
            {
                absl::string_view msg("Error when _outputHandednessPoller try to get the result pack from _graph!");
            }
            multi_handedness = handednessPacket.Get<std::vector<::mediapipe::ClassificationList>>();
            

            ::mediapipe::Packet landmarksPacket;
            if (!_outputLandmarksPoller->Next(&landmarksPacket))
            {
                absl::string_view msg("Error when _outputLandmarksPoller try to get the result pack from _graph!");
            }
            multi_hand_landmarks = landmarksPacket.Get<std::vector<::mediapipe::NormalizedLandmarkList>>();


            ::mediapipe::Packet handRectsPacket;
            if (!_outputHandRectsPoller->Next(&handRectsPacket))
            {
                absl::string_view msg("Error when _outputHandRectsPoller try to get the result pack from _graph!");
            }
            multi_hand_rects = handRectsPacket.Get<std::vector<::mediapipe::NormalizedRect>>();


            // NOTE: Disabled until to implement the calculator CountingNormalizedPalmRectVectorSizeCalculator
            // ::mediapipe::Packet palmRectsPacket;
            // if (!_outputPalmRectsPoller->Next(&palmRectsPacket))
            // {
            //     absl::string_view msg("Error when _outputPalmRectsPoller try to get the result pack from _graph!");
            // }
            // multi_palm_rects = palmRectsPacket.Get<std::vector<::mediapipe::NormalizedRect>>();
        }

        return absl::Status();
    }

protected:
    absl::Status Setup(std::string calculator_graph_config_file)
    {
        LOG(INFO) << MediaPipeLandmarks::Setup(calculator_graph_config_file, false);

        ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                        _graph.AddOutputStreamPoller(kHandednessOutputStream));
        // Tip: https://github.com/google/mediapipe/issues/1537
        _outputHandednessPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                        _graph.AddOutputStreamPoller(kLandmarksOutputStream));
        _outputLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   _graph.AddOutputStreamPoller(kHandCountOutputStream));
        _outputHandCountPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   _graph.AddOutputStreamPoller(kHandRectsOutputStream));
        _outputHandRectsPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

        ASSIGN_OR_RETURN(poller,
                   _graph.AddOutputStreamPoller(kPalmRectsOutputStream));
        _outputPalmRectsPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poller));

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

        ::grpc::Status DetectAndDraw(ServerContext *context, const HandsRequest *request, HandsReply *reply) override
        {
            Detect(request, reply);
            return Status::OK;
        }

        ::grpc::Status GetHands(ServerContext *context, const HandsRequest *request, HandsReply *reply)
        {
            Detect(request, reply, false);
            return Status::OK;
        }

        void Detect(const HandsRequest *request, HandsReply *reply, bool fillImage = true)
        {
            const int w = request->image().width();
            const int h = request->image().height();
            const int c = request->image().channels();
            const int sz = w * h * c;

            // Copy image data from request do opencv Map
            cv::Mat frame = cv::Mat(cv::Size(w, h), CV_8UC(c));
            std::memcpy(frame.data, request->image().data().c_str(), sz * sizeof(uchar));

            auto multi_handedness = std::vector<::mediapipe::ClassificationList>();
            auto multi_hand_landmarks = std::vector<::mediapipe::NormalizedLandmarkList>();
            auto multi_hand_rects = std::vector<::mediapipe::NormalizedRect>();
            auto multi_palm_rects = std::vector<::mediapipe::NormalizedRect>();
            
            absl::Status status =  _mediapipeHands->RunMPPGraph(
                                        frame, 
                                        multi_handedness, 
                                        multi_hand_landmarks,
                                        multi_hand_rects,
                                        multi_palm_rects);

            // Populating hands and handnesses
            {
                Hands hands;

                int hand_id = 0;
                for (const auto &single_hand_landmarks : multi_hand_landmarks)
                {

                    Hand *hand = hands.add_hand();

                    const auto &classification = multi_handedness[hand_id].classification(0);
                    hand->set_label(classification.label());
                    hand->set_score(classification.score());

                    const auto &hand_rect = multi_hand_rects[hand_id];
                    frameland::NormalizedRect *handRect = hand->mutable_hand_rect();
                    handRect->set_x_center(hand_rect.x_center());
                    handRect->set_y_center(hand_rect.y_center());
                    handRect->set_height(hand_rect.height());
                    handRect->set_width(hand_rect.width());
                    handRect->set_rect_id(hand_rect.rect_id());


                    // NOTE: Disabled until to implement the calculator CountingNormalizedPalmRectVectorSizeCalculator
                    // const auto &palm_rect = multi_palm_rects[hand_id];
                    // frameland::NormalizedRect *palmRect = hand->mutable_palm_rect();
                    // palmRect->set_x_center(palm_rect.x_center());
                    // palmRect->set_y_center(palm_rect.y_center());
                    // palmRect->set_height(palm_rect.height());
                    // palmRect->set_width(palm_rect.width());
                    // palmRect->set_rect_id(palm_rect.rect_id());


                    for (int i = 0; i < single_hand_landmarks.landmark_size(); ++i)
                    {
                        const auto &mediapipeLandmark = single_hand_landmarks.landmark(i);
                        
                        frameland::Landmark *landmark = hand->add_landmark();
                        landmark->set_x(mediapipeLandmark.x());
                        landmark->set_y(mediapipeLandmark.y());
                        landmark->set_z(mediapipeLandmark.z());
                    }
                    
                    hand_id++;

                }

                Hands *replyHands = reply->mutable_hands();
                replyHands->CopyFrom(hands);

            }


            if (fillImage)
            {
                // Update reply image data with the processed frame.
                const std::string data((const char *)frame.data, sz);

                frameland::Image image;
                image.set_data(data);
                image.set_channels(frame.channels());
                image.set_width(request->image().width());
                image.set_height(request->image().height());

                // NOTE: Using this lines to avoid SEGFAULT
                Image *replyImage = reply->mutable_image();
                replyImage->CopyFrom(image);
            }
            
            frame.release();
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