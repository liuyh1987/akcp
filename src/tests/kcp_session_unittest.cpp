#include "../kcp.h"
#include <gtest/gtest.h>
#include <string>

using namespace Akcp;
using namespace std;
using namespace std::placeholders;

void SessionMessageCallBack(CountDownLatch& clt, SpSession session, int& num, string msg)
{
    if (num < 10)
    {
        session->sendMsg(msg.c_str(), msg.size());
        num++;
    }
    else if (num == 10)
    {
        clt.countDown();
    }
}

void ServerMessageCallback(SpSession session, const string& msg)
{
    session->sendMsg(msg.c_str(), msg.size());
}

void StartRunServiceIo(asio::io_service& io)
{
    io.run();
}


TEST(CommunicateTest, Normal) {
	asio::io_service io;
    int session_num = 10;
    CountDownLatch cdl(session_num);

    string server_addr("127.0.0.1");
    int server_port(4001);
    SpKcp skcp = Kcp::createKcp(io, server_addr, server_port, ServerMessageCallback);

    string client_addr("127.0.0.1");
    int client_port(4000);
    SpKcp ckcp = Kcp::createKcp(io, client_addr, client_port, Kcp::MessageCallback());

    SpSession sessions[session_num];
    int send_num[session_num] = {0};
    for (int i=0; i<session_num; ++i) //test sesssion callback
    {
        sessions[i] = ckcp->createSession(server_addr, server_port);
        sessions[i]->setMsgCallback(std::bind(SessionMessageCallBack, std::ref(cdl), 
            std::ref(sessions[i]), std::ref(send_num[i]), std::placeholders::_1));
        string packet(1000, 't');
        sessions[i]->sendMsg(packet.c_str(), packet.size());
    }

    std::thread iorun(std::bind(StartRunServiceIo, std::ref(io)));

    cdl.wait();
    skcp->stop();
    ckcp->stop();
    io.stop();
    sleep(1);//wait for ioservice queue
    EXPECT_TRUE(true);
    iorun.join();
}

TEST(CommunicateTest, SessionNoRegisterCallback) {
	asio::io_service io;
    int session_num = 1;
    CountDownLatch cdl(1);

    string server_addr("127.0.0.1");
    int server_port(5001);
    SpKcp skcp1 = Kcp::createKcp(io, server_addr, server_port, ServerMessageCallback);

    string client_addr("127.0.0.1");
    int client_port(5000);
    int num = 0;

    SpKcp ckcp1 = Kcp::createKcp(io, client_addr, client_port, std::bind(SessionMessageCallBack, 
        std::ref(cdl), std::placeholders::_1, std::ref(num), std::placeholders::_2));

    SpSession  sessions = ckcp1->createSession(server_addr, server_port);
    string packet(1000, 't');
    sessions->sendMsg(packet.c_str(), packet.size());

    std::thread iorun(std::bind(StartRunServiceIo, std::ref(io)));
    cdl.wait();

    skcp1->stop();
    ckcp1->stop();
    io.stop();
    sleep(1); //wait for ioservice queue
    EXPECT_TRUE(true);
    iorun.join();
}

