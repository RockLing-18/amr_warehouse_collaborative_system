#include "edge_server_app.h"
#include <signal.h>
#include <iostream>

static bool running = true;

void signalHandler(int)
{
    running = false;
}

int main()
{
    signal(SIGINT, signalHandler);

    edge_server::EdgeServerApp app;
    if(!app.start())
    {
        return -1;
    }

    while(running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );
    }

    app.stop();

    return 0;
}
