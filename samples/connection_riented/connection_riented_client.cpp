#include <stdlib.h>
#include "../../src/client.h"
#include "../../src/kcp.h"

using namespace Akcp;

class ConnectionClient
{
public:
    ConnectionClient(const std::string& address, int udp_port, TransMode mode = TransModeNomal)
        :client_(address, udp_port, mode)
    {
        client_.setCloseCallback(std::bind(&ConnectionClient::closeCallback, this));
        client_.setMessageCallback(std::bind(&ConnectionClient::messageCallback, this, std::placeholders::_1));
    }
    
    void closeCallback()
    {
        std::cout << "closeCallback" << std::endl;
    }

    void messageCallback(const std::string& x)
    {
        std::cout << x << std::endl;
        client_.sendMsg(x.c_str(), x.size());
    }

    void connect(const std::string& address, int udp_port)
    {
        if (client_.syncConnect(address, udp_port, std::chrono::milliseconds(2*1000)))
            std::cout << "connect ok!" << std::endl;
        else
            std::cout << "connect failed!" << std::endl;
    }

    Akcp::Client& ackpClient() {return client_;}


private:
    Akcp::Client client_;
};

void printUsage()
{
    printf("usage: 0_connection_client <1_host_ip> <2_port> <3_remote_addr> <4_remote_port> <5_transmode>{normal | nodelay | nocc}\n");
}

int main(int argc, char* argv[])
{
    if (argc != 6)
    {
        printUsage();
        exit(-1);
    }

    TransMode mode;
    if (strcmp(argv[5], "normal") == 0)
        mode = TransModeNomal;
    else if (strcmp(argv[5], "nodelay") == 0)
        mode = TransModeNoDelay;
    else if (strcmp(argv[5], "nocc") == 0)
        mode = TransModeNoCC;
    else
    {
        printUsage();
        exit(-1);
    }

    ConnectionClient client(argv[1], atoi(argv[2]), mode);
    client.connect(argv[3], atoi(argv[4]));
    client.ackpClient().sendMsg("hello world!", strlen("hello world!"));

    sleep(100);

    return 0;
}
