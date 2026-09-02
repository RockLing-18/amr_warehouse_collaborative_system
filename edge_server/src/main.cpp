#include "edge_server_app.h"
#include <signal.h>
#include <iostream>
#include <chrono>
#include <thread>
#include "utils/CommonFunc.h"


static bool running = true;

void signalHandler(int)
{
    running = false;
}

int main(int argc,char** argv)
{
    signal(SIGINT, signalHandler);

    std::string config;
    if(argc > 2 && std::string(argv[1]) == "--config")
    {
        config = argv[2];
    }
    else
    {
        auto bin_dir = get_exe_dir();
        config = bin_dir + "/config/edge_server.yaml";
    }

    edge_server::EdgeServerApp app;
    if(!app.init(config))
    {
        return -1;
    }

    while(running)
    {
        std::this_thread::sleep_for( std::chrono::seconds(1));
    }

    return 0;
}
