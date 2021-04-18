#pragma once

#include <iostream>
#include <string>
#include <bits/stdc++.h>

#include <grpcpp/grpcpp.h>

#include "src/frameland.h"

#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/port/logging.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using frameland::Text;
using frameland::TextProcessing;

namespace mediapipe
{

    absl::Status PrintHelloWorld(std::string &output)
    {
        // Configures a simple graph, which concatenates 2 PassThroughCalculators.
        CalculatorGraphConfig config =
            ParseTextProtoOrDie<CalculatorGraphConfig>(R"pb(
        input_stream: "in"
        output_stream: "out"
        node {
          calculator: "PassThroughCalculator"
          input_stream: "in"
          output_stream: "out1"
        }
        node {
          calculator: "PassThroughCalculator"
          input_stream: "out1"
          output_stream: "out"
        }
      )pb");

        CalculatorGraph graph;
        MP_RETURN_IF_ERROR(graph.Initialize(config));
        ASSIGN_OR_RETURN(OutputStreamPoller poller,
                         graph.AddOutputStreamPoller("out"));
        MP_RETURN_IF_ERROR(graph.StartRun({}));

        MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
            "in", MakePacket<std::string>("Hello World!").At(Timestamp(0))));

        // Close the input stream "in".
        MP_RETURN_IF_ERROR(graph.CloseInputStream("in"));
        mediapipe::Packet packet;

        // Get the output packets std::string.
        while (poller.Next(&packet))
        {
            output += packet.Get<std::string>();
            LOG(INFO) << output;
        }

        return graph.WaitUntilDone();
    }

}

namespace frameland::mediapipe::component::service
{

    // Logic and data behind the server's behavior.
    class TextProcessingMediaPipeServiceImpl final : public TextProcessing::Service
    {

        ::grpc::Status Identity(ServerContext *context, const Text *request, Text *reply) override
        {
            std::string prefix("DUCK ");

            CHECK(::mediapipe::PrintHelloWorld(prefix).ok());

            reply->set_text(prefix + request->text());

            return Status::OK;
        }

        ::grpc::Status Invert(ServerContext *context, const Text *request, Text *reply) override
        {

            std::string text = request->text();
            std::reverse(text.begin(), text.end());

            reply->set_text(text);

            return Status::OK;
        }
    };

    void RunTextProcessingMediaPipeServer(
        std::string address = "0.0.0.0",
        std::string port = "50052")
    {
        std::string server_address = address + ":" + port;
        TextProcessingMediaPipeServiceImpl service;

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
