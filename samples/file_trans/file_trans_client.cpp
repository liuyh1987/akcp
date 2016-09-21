#include <future>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>

#include "../../src/kcp.h"
#include "msg_def.h"

using namespace Akcp;

class FileTransClient
{
public:
    FileTransClient(asio::io_service& io_service, const char* addr, 
                    int port, TransMode mode)
        :spkcp_(Kcp::createKcp(io_service, addr, port, Kcp::MessageCallback(), mode)),
        io_(io_service)
    { }

    void start()
    {
        start_io_service_ = std::async(std::launch::async, std::bind(&FileTransClient::startIoService, this));
    }

    bool stop()
    {
        spkcp_->stop();
        io_.stop();
        return start_io_service_.get();
    }

    bool sendFile(const std::string& filepath, const std::string& remoteaddr, int remoteport, int rate_KBS)
    {
        do_send_file_ = std::async(std::launch::async, 
                                   std::bind(&FileTransClient::doSendFile, this, filepath, remoteaddr, remoteport, rate_KBS));
        return do_send_file_.get();
    }

    void onMessage(const std::string& message, CountDownLatch& ctl) 
    {
        MsgSendFileEndNotify* commmsg = (MsgSendFileEndNotify*)(&message[0]);
        switch (commmsg->msgType)
        {
        case    SendFileBegin:
        case    SendFileBlock:
        case    SendFileEnd:
            break;

        case SendFileEndAck:
            std::cout << "SendFileEndAck" << std::endl;
            ctl.countDown();
            break;

        default:
            break;
        }

    }

private:
    bool doSendFile(const std::string& filepath, const std::string& remoteaddr, int remoteport, int rate_KBS)
    {
        CountDownLatch ctl(1);
        SpSession sp_session = spkcp_->createSession(remoteaddr, remoteport);
        sp_session->setMsgCallback(std::bind(&FileTransClient::onMessage, this, std::placeholders::_1, std::ref(ctl)));

        std::ifstream ifs(filepath.c_str(), std::ifstream::in | std::ifstream::binary);
        if (!ifs) 
            return false;

        size_t last_sep_char_pos = filepath.rfind("/");
        size_t begin_pos = last_sep_char_pos==std::string::npos ? 0 : last_sep_char_pos + 1;
        std::string basename = filepath.substr(begin_pos);

        MsgSendFileBeginNotify  beginNotify;
        memcpy(beginNotify.file_name, basename.c_str(), basename.size());
        beginNotify.file_name_size = basename.size();

        ifs.seekg(0, std::ios_base::end);
        beginNotify.file_size = ifs.tellg();
        ifs.seekg(0, std::ios_base::beg);

        sp_session->sendMsg((const char*)(&beginNotify), sizeof(beginNotify));

        std::chrono::milliseconds sleep_time(static_cast<int>(BLOCK_SIZE * 1.2 / rate_KBS));

        MsgSendFileBlockNotify cnt;
        while (!ifs.eof())
        {
            ifs.read(cnt.content, BLOCK_SIZE);
            cnt.block_size = ifs.gcount();
            sp_session->sendMsg((const char*)(&cnt), sizeof(cnt));
            std::this_thread::sleep_for(sleep_time);
        }

        MsgSendFileEndNotify endnotify;
        sp_session->sendMsg((const char*)(&endnotify), sizeof(endnotify));
        ifs.close();

        ctl.wait();
        return true;
    }

    bool startIoService()
    {
        io_.run();
    }

    SpKcp spkcp_;
    asio::io_service& io_;
    std::future<bool> start_io_service_;
    std::future<bool> do_send_file_;
};

void printUsage()
{
    printf("usage:  file_trans_client <1 host_ip> <2 port> "
            "<3 remote_addr> <4 remote_port> <5 transmode>{normal | nodelay | nocc} 6<file name > 7<rate_kbs>\n");
}

int main(int argc, char* argv[])
{
    if (argc != 8)
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

    try
    {
        //asio::io_service& io_service, 1 const char* addr, 
         //           2 int port, , TransMode mode
        asio::io_service io;
        FileTransClient client(io, argv[1], atoi(argv[2]), mode);
        client.start();
        std::string  filename(argv[6]);
        std::string remoteaddr(argv[3]);
        int remote_port = atoi(argv[4]);
        int rate_kBs = atoi(argv[7]);
        bool result = client.sendFile(filename, remoteaddr, remote_port, rate_kBs);
        if (result)
            std::cout << "send file sucessful!" << std::endl;
        else
            std::cout << "send file faild!" << std::endl;
        client.stop();
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