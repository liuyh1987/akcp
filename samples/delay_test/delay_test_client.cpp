#include <stdlib.h>
#include "../../src/kcp.h"

using namespace Akcp;
using namespace std::placeholders;
using namespace std::chrono;

struct Msg
{
    unsigned long long send_tv;
    int index;
};

class DelayTestClient
{
public:
    DelayTestClient(asio::io_service& io_service, const char* addr, int port, int session_num, int send_num, int block_size,
        const std::string& remote_addr, int remote_porte, TransMode mode)
        :spkcp_(Kcp::createKcp(io_service, addr, port, Kcp::MessageCallback(), mode)),
        block_size_(block_size),
        remote_addr_(remote_addr),
        remote_port_(remote_porte),
        session_num_(session_num),
        send_num_(send_num),
        io_(io_service),
		loop_timer_(io_service)
    {
        using namespace std::chrono;
        begin_ =  
            (duration_cast<milliseconds>(system_clock::now().time_since_epoch())).count();

        setTimer(50);
        remote_ = spkcp_->createSession(remote_addr_, remote_porte);
        remote_ ->setMsgCallback(std::bind(&DelayTestClient::onMessage, this, _1));
    }

    void setTimer(int milliseconds)
	{
        loop_timer_.expires_from_now(std::chrono::milliseconds(milliseconds));
	    loop_timer_.async_wait(std::bind(&DelayTestClient::handleTimter, this));
	}

    void handleTimter()
    {
        setTimer(50);
        sendMsg();
    }

    void onMessage(const std::string& message) 
    {
        Msg msg;
        memcpy(&msg, &message[0], sizeof(msg));
        unsigned long long current = 
            (duration_cast<milliseconds>(system_clock::now().time_since_epoch())).count();
        printf("%0.3f %d\n", (current - begin_) / 1000.0, 
            (current - msg.send_tv));
    }

    void sendMsg()
    {
        std::string packet(block_size_, 't');
        Msg msg;
        msg.send_tv = (duration_cast<milliseconds>(system_clock::now().time_since_epoch())).count();
        memcpy(&packet[0], &msg, sizeof(msg));
        remote_->sendMsg(packet.c_str(), packet.size());
    }

    void start()
    {
    	io_.run();
    }

private:
    SpKcp spkcp_;
    int block_size_;
    std::string remote_addr_;
    int remote_port_;
    int session_num_;
    int send_num_;
    asio::io_service& io_;
    asio::basic_waitable_timer<std::chrono::system_clock> loop_timer_;

    unsigned long long begin_;
    SpSession remote_;
};

void printUsage()
{
    printf("usage:  delay_test_client <host_ip> <port> "
            "<esession_num> <send_num> <remote_addr> <block_size> <remote_port> <transmode>{normal | nodelay | nocc}\n");
}

int main(int argc, char* argv[])
{
    if (argc != 9)
    {
        printUsage();
        exit(-1);
    }

    TransMode mode;
    if (strcmp(argv[8], "normal") == 0)
        mode = TransModeNomal;
    else if (strcmp(argv[8], "nodelay") == 0)
        mode = TransModeNoDelay;
    else if (strcmp(argv[8], "nocc") == 0)
        mode = TransModeNoCC;
    else
    {
        printUsage();
        exit(-1);
    }   

    try
    {
        asio::io_service io;
        DelayTestClient client(io, argv[1], atoi(argv[2]), atoi(argv[3]), 
            atoi(argv[4]), atoi(argv[5]), argv[6], atoi(argv[7]), mode);
        client.start();
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