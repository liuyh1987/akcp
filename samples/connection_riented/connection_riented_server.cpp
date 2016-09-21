#include <stdlib.h>
#include "../../src/server.h"
#include "../../src/kcp.h"

using namespace std::placeholders;
using namespace Akcp;

class ConnectionServer
{
public:
    ConnectionServer(const std::string& address, const std::string& port, TransMode mode)
        :server_(address, port, std::bind(&ConnectionServer::callbacks, this, _1, _2, _3, _4), 0, mode)
    {
        server_.run();
    }

    ~ConnectionServer()
    { }

    int callbacks(T_CALLBACK event, std::weak_ptr<void> conn, const char *msg, unsigned int len)
    { 
        std::shared_ptr<Akcp::Connection> cn = std::static_pointer_cast<Akcp::Connection>(conn.lock());
        if (event == C_ACCEPTED)
        {
            std::cout << "C_ACCEPTED" << std::endl;
        }
        else if (event == C_HEADREADED)
        {
            std::cout << "C_HEADREADED" << std::endl;
        }
        else if (event == C_BODYREADED)
        {
            std::cout << "C_BODYREADED" << std::endl;
            std::cout << msg << std::endl;
            std::cout << len << std::endl;
            std::string x(msg, len);
            cn->send(x.c_str(), x.size());
        }
        else if (event == C_CLOSED)
        {
            std::cout << "C_CLOSED" << std::endl;
        }
    }

private:
    Akcp::Server server_;
};

void printUsage()
{
    printf("usage: 0_connection_server <1_host_ip> <2_port> <3_transmode>{normal | nodelay | nocc}\n");
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
    ConnectionServer server(argv[1], argv[2], mode);

    int x;
    std::cin >> x;

    return 0;
}