#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <opencv2/opencv.hpp>

#include "src/frameland.h"

using namespace std;
using namespace cv;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using frameland::Image;
using frameland::ImageProcessing;

// Logic and data behind the server's behavior.
class VideoStreamServiceImpl final : public ImageProcessing::Service
{

    ::grpc::Status Identity(ServerContext* context, const Image* request, Image* reply) override
    {
        reply->set_width(request->width());
        reply->set_height(request->height());
        reply->set_channels(request->channels());
        reply->set_data(request->data());

        return Status::OK;
    }

    ::grpc::Status RgbToGrayFilter(ServerContext* context, const Image* request, Image* reply) override
    {
        reply->set_width(request->width());
        reply->set_height(request->height());

        {
            const int w = request->width();
            const int h = request->height(); 
            const int c = request->channels();
            const int sz = w*h*c;

            // Copy image data from request do opencv Map
            cv::Mat frame = cv::Mat(cv::Size(w,h), CV_8UC(c));
            std::memcpy(frame.data, request->data().c_str(), sz*sizeof(uchar));

            rgbToGray(frame);
            reply->set_channels(frame.channels());


            // Update reply image data with the processed frame.
            const std::string data((const char *)frame.data, sz);
            reply->set_data(data);

            frame.release();
        }

        return Status::OK;
    }

    void rgbToGray(cv::Mat &frame)
    {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
    }
};

void RunServer()
{
    std::string address = "0.0.0.0";
    std::string port = "50051";
    std::string server_address = address + ":" + port;
    VideoStreamServiceImpl service;

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

int main(int argc, char **argv)
{
    RunServer();

    return 0;
}