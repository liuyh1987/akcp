#include <stdlib.h>
#include "../../src/kcp.h"

using namespace Akcp;
using namespace std::placeholders;

class DelayTestServer
{
public:
    DelayTestServer(asio::io_service& io_service, const char* addr, int port, TransMode mode)
        :spakcp_(Kcp::createKcp(io_service, addr, port, std::bind(
                        &DelayTestServer::onMessage, this, _1, _2), mode)),
        io_(io_service)
    { }

    void onMessage(SpSession entity, const std::string& message) 
    {
        entity->sendMsg(message.c_str(), message.size());
    }

    void start()
    {
    	io_.run();
    }

private:
    SpKcp spakcp_;
    asio::io_service& io_;
};

void printUsage()
{
    printf("usage: throughput_test_server <host_ip> <port> <transmode>{normal | nodelay | nocc}\n");
}

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        printUsage();
        exit(-1);
    }
    TransMode mode;

    if (strcmp(argv[3], "normal") == 0)
        mode = TransModeNomal;
    else if (strcmp(argv[3], "nodelay") == 0)
        mode = TransModeNoDelay;
    else if (strcmp(argv[3], "nocc") == 0)
        mode = TransModeNoCC;
    else
    {
        printUsage();
        exit(-1);
    }

    try
    {
    	asio::io_service io;
        DelayTestServer server(io, argv[1], atoi(argv[2]), mode);
        server.start();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "unknown error!" << std::endl;
    }

	return 0;
}