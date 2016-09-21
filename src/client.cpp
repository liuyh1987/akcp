#include "client.h"

namespace Akcp{

Client::Client(const std::string& address, int udp_port, TransMode mode)
    :state_(Invalied)
{
    akcp_ = Kcp::createKcp(io_, address, udp_port, Kcp::MessageCallback(), mode);
    io_runer_ = std::thread(std::bind(&Client::runIoService, this));
}

Client::~Client()
{
    if (status() == Client::Connected)
        close();

    akcp_->stop();
    io_.stop();
    io_runer_.join();
    akcp_.reset();
}

/*
bool Client::asyncConnect(const std::string& address, int udp_port, ConnectedCallback cb)
{
    doConnect(address, udp_port);
    std::thread tmp(std::bind(&Client::waitConnectResult, this, cb));
    tmp.detach();
    return true;
}
*/

bool Client::syncConnect(const std::string& address, int udp_port/*outtime*/, std::chrono::milliseconds outtime)
{
    doConnect(address, udp_port);
    return waitConnectResult(ConnectedCallback(), outtime);
}

bool Client::sendMsg(const char* buf, size_t len)
{
    if (status() != Connected)
    {
        //todo:
        return false;
    }

    return remote_->sendMsg(buf, len);
}

bool Client::close()
{
    if (status()!=Connected && status()!=Connecting)
    {
        //todo:
        return false;
    }
    remote_ ->sendMsg(CloseReq, strlen(CloseReq));
    setStatus(Closing);

    spCloseNotify_.reset(new CountDownLatch(1));
    bool ret = spCloseNotify_->waitFor(std::chrono::milliseconds(2 * 1000));
    const char* msg = (ret ? "success!" : "timeout");
    std::cout << "close: " << msg << std::endl;
	return true;
}

void Client::onMessage(const std::string& message) 
{
    if (state_ == Invalied)
    {
        //todo:
    }
    else if(state_ == Connecting)
    {
        if (strcmp(message.c_str(), ConnectResp) == 0)
        {
            setStatus(Connected);
            assert(spConnectNotify_);
            spConnectNotify_->countDown();
        }
        else
        {
            //todo:
        }
    }
    else if (state_ == Connected)
    {
        if (strcmp(message.c_str(), CloseReq) == 0)
        {
            remote_->sendMsg(CloseResp, strlen(CloseResp));
            setStatus(Closed);
            if (closeCb_)
                closeCb_();
        }
        else if (messageCb_)
            messageCb_(message);
    }
    else if (state_ == Closing)
    {
        if (strcmp(message.c_str(), CloseResp) == 0)
        {
            setStatus(Closed);
            assert(spCloseNotify_);
            spCloseNotify_->countDown();
        }
        else
        {
            //todo:
        } 
    }
    else if (state_ == Closed)
    { 
        //todo:
    }
}

bool Client::waitConnectResult(ConnectedCallback cb, std::chrono::milliseconds outtime)
{
    bool ret = spConnectNotify_->waitFor(outtime);
    if (cb)
        cb(ret, ret ? "" : "timeout");
    return ret;
}

void Client::doConnect(const std::string& address, int udp_port)
{
    initSession(address, udp_port);
    spConnectNotify_.reset(new CountDownLatch(1));
    remote_ ->sendMsg(ConnectReq, strlen(ConnectReq));
    setStatus(Connecting);
}

void Client::initSession(const std::string& address, int udp_port)
{
    remote_ = akcp_->createSession(address, udp_port);
    remote_ ->setMsgCallback(std::bind(&Client::onMessage, this,std::placeholders::_1));
}

void Client::runIoService()
{
    io_.run();
}

}