#include <stdlib.h>
#include <fstream>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include "../../src/kcp.h"
#include "msg_def.h"

using namespace Akcp;
using namespace std::placeholders;

class RecvTask
{
public:
    enum Status { WaitBegin, Reciving, End };

    RecvTask() :state_(WaitBegin) 
    {
        start_ = std::chrono::system_clock::now();
    }

    ~RecvTask()
    {
        std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
        std::chrono::milliseconds total_mis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        float rate = file_size_ * 1000.0 / total_mis.count();
        std::cout << "recv file: " << file_name_ << " rate = " << rate << "BS" << std::endl;
    }

    bool finish() {return state_ == End;}

    void onMsg(const std::string& msg)
    {
        MsgSendFileEndNotify* commmsg = (MsgSendFileEndNotify*)(&msg[0]);
        switch (commmsg->msgType)
        {
        case    SendFileBegin:
            {
                state_ = Reciving;
                MsgSendFileBeginNotify* beginMsg = (MsgSendFileBeginNotify*)(&msg[0]);
                file_name_ = std::string(beginMsg->file_name, beginMsg->file_name_size);
                outfile_.open(file_name_, std::ofstream::binary);
                file_size_ = beginMsg->file_size;
                break;
            }

        case    SendFileBlock:
            {
                MsgSendFileBlockNotify* blockMsg = (MsgSendFileBlockNotify*)(&msg[0]);
                outfile_.write(blockMsg->content, blockMsg->block_size);
                break;
            }

        case    SendFileEnd:
            {
                outfile_.close();
                state_ = End;
                break;

            }

        default:
            break;
        }
    }

private:
    Status state_;
    std::ofstream outfile_; 
    long file_size_;
    std::chrono::time_point<std::chrono::system_clock> start_;
    std::string file_name_;
};

typedef std::shared_ptr<RecvTask> SpRecvTask;

class FileTransServer
{
public:
    FileTransServer(asio::io_service& io_service, const char* addr, int port, TransMode mode)
        :spakcp_(Kcp::createKcp(io_service, addr, port, std::bind(
                        &FileTransServer::onMessage, this, _1, _2), mode)),
        io_(io_service)
    { }

    void onMessage(SpSession entity, const std::string& message) 
    {
        std::map<Session*, SpRecvTask>::iterator iter = tasks_.find(entity.get());
        SpRecvTask task;
        if (iter == tasks_.end())
        {
            task = std::make_shared<RecvTask>();
            tasks_[entity.get()] = task;
        }
        else
            task = iter->second;
        task->onMsg(message);
        if (task->finish())
        {
            std::cout << "recv file finish !" << std::endl;
            tasks_.erase(entity.get());

            MsgSendFileEndAckNotify endAck;
            entity->sendMsg((const char*)&endAck, sizeof(endAck));
        }
    }

    void start()
    {
    	io_.run();
    }

private:
    SpKcp spakcp_;
    asio::io_service& io_;
    std::map<Session*, SpRecvTask> tasks_;
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
        FileTransServer server(io, argv[1], atoi(argv[2]), mode);
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