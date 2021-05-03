#include <iostream>

#include "src/frameland.h"

using frameland::Image;
using frameland::ImageProcessing;
using frameland::component::client::DesktopImageFilter;
using frameland::component::io::DesktopImageIoCv;

int main()
{

    std::string address = "localhost";
    std::string port = "50051";
    std::string server_address = address + ":" + port;
    std::cout << "Client querying server address: " << server_address << std::endl;

    DesktopImageIoCv ioCv;
    DesktopImageFilter proc(grpc::CreateChannel(
        server_address, grpc::InsecureChannelCredentials()));

    while(!ioCv.done())
    {
        Image requestImage = ioCv.Acquire();
        ioCv.Show(requestImage, "Frame");

        proc.SetFilter(DesktopImageFilter::Filter::RgbToGray);
        Image grayImage = proc.Process(requestImage);
        ioCv.Show(grayImage, "Gray frame");

        proc.SetFilter(DesktopImageFilter::Filter::Identity);
        Image identityImage = proc.Process(requestImage);
        ioCv.Show(identityImage, "Identity frame");
    }

    return 0;
}
