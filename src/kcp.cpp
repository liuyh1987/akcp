#include "kcp.h"
#include <random>

using namespace std::placeholders;

namespace {

IINT64 iclock64(void)
{
    using namespace std::chrono;
    milliseconds msecs =  
        duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	return msecs.count();
}

uint32_t createConv()
{
    static std::random_device rd;
    return rd();
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

}

namespace Akcp{

int Kcp::LOOP_TIMVER_INTERVAL_MSECONDS = 5;

Kcp::Kcp(asio::io_service& io_service, const std::string& address, int udp_port, MessageCallback cb, TransMode mode):
	io_service_(io_service),
	udp_socket_(io_service, asio::ip::udp::endpoint(asio::ip::address::from_string(address), udp_port)),
	loop_timer_(io_service),
    message_cb_(cb),
    trans_mode_(mode),
    notify_message_consumer_(std::bind(&Kcp::notifyMessageConsumer, this, std::placeholders::_1)),
    send_message_consumer_(std::bind(&Kcp::sendUdpConsumer, this, std::placeholders::_1)),
    runing_(true),
    dostop_(false),
    stop_success_notify_(1)
{
	setTimer(LOOP_TIMVER_INTERVAL_MSECONDS);
	doUdpAasyncRead();
}

std::shared_ptr<Kcp> Kcp::createKcp(asio::io_service& io_service, const std::string& address, int udp_port, 
                                    MessageCallback cb, TransMode mode)
{
    return std::shared_ptr<Kcp>(new Kcp(io_service, address, udp_port, cb, mode));
}

void Kcp::sendMessage(SpSession entity, const char* buf, size_t buf_len)
{
    send_queue_.put(Message(entity, buf, buf_len));
}

SpSession Kcp::createSession(const asio::ip::udp::endpoint& remote_point)
{
   return getKcpEntity(remote_point, createConv());
}


SpSession Kcp::createSession(const std::string& address, int udp_port)
{
    return createSession(asio::ip::udp::endpoint(
        asio::ip::address::from_string(address), udp_port));
}

Kcp::~Kcp() 
{
    stop();
}

void Kcp::handleTtimerLoop()
{
	setTimer(LOOP_TIMVER_INTERVAL_MSECONDS);
	handleKcpLoop();
}

void Kcp::doUdpAasyncRead(void)
{
    udp_socket_.async_receive_from(
          asio::buffer(recv_buf_, sizeof(recv_buf_)), udp_sender_endpoint_,
          std::bind(&Kcp::handleUdpRead, this, std::placeholders::_1, std::placeholders::_2));
}

void Kcp::handleUdpRead(const asio::error_code& code, std::size_t bytes_recvd)
{
    IUINT32 conv;
    int ret = ikcp_get_conv(recv_buf_, bytes_recvd, &conv);
	if (ret == 0)
	{
		doUdpAasyncRead();
		return;
	}
	
	handleKcpRead(bytes_recvd, conv);
    doUdpAasyncRead();
}

SpSession Kcp::getKcpEntity(const asio::ip::udp::endpoint& ep, uint32_t conv)
{
	SpSession entity;
    SessionKey key(ep, conv);
	MapSession::iterator iter = sessions_.find(key);
	if (iter == sessions_.end())
	{
	//Session(std::shared_ptr<Kcp> spakcp, IUINT32 id, const asio::ip::udp::endpoint& ep, 
     //       const SendPacketCallBack& output, TransMode mode);
        entity.reset(new Session(shared_from_this(), conv, ep, 
                                 std::bind(&Kcp::sendKcpPacket, this, std::placeholders::_1, std::placeholders::_2,
                                 std::placeholders::_3, std::placeholders::_4), trans_mode_));
		sessions_[key] = entity; 
	}
	else
		entity = iter->second;
    return entity;
}

void Kcp::handleKcpRead(size_t bytes_recvd, uint32_t conv)
{
	SpSession entity = getKcpEntity(udp_sender_endpoint_, conv);
	entity->input(recv_buf_, bytes_recvd);
}

void Kcp::setTimer(int milliseconds)
{
    loop_timer_.expires_from_now(std::chrono::milliseconds(milliseconds));
    loop_timer_.async_wait(std::bind(&Kcp::handleTtimerLoop, this));
}

void Kcp::doStop()
{
    if (dostop_)
        return;
    udp_socket_.cancel();
    loop_timer_.cancel();
    notify_message_consumer_.stop();
    send_message_consumer_.stop();
    assert(stop_success_notify_.getCount() == 1);
    stop_success_notify_.countDown();
    assert(stop_success_notify_.getCount() == 0);
    dostop_ = true;
}

bool Kcp::checkStop()
{
    if (!runing_)
        doStop();

    return !runing_;
}

void Kcp::handleKcpLoop()
{
    if (checkStop())
        return;

    //process send msg
    ThreadSafeQueue<Message>::QueueType queue; 
    if (send_queue_.tryTakeAll(&queue))
    {
        for (ThreadSafeQueue<Message>::QueueType::iterator iter = queue.begin();
                iter!=queue.end(); ++iter)
        {
            SpSession spkcp = iter->spsession_;
            assert(spkcp);
            spkcp->send(iter->msg.c_str(), iter->msg.size());
        }
    }

    //recv msg
    for (MapSession::iterator iter = sessions_.begin(); iter!=sessions_.end(); ++iter)
    {
        std::string recvstring(1024 * 16, '\0');
        int recvlen = 0;
        while ((recvlen = (*iter).second->recv(&recvstring[0], recvstring.size())) > 0)
        {
            recvstring.resize(recvlen);
            notify_message_consumer_.put(Message((*iter).second, recvstring));
        }
    }

    //update kcp node
	IUINT32 current = iclock();
    for (MapSession::iterator iter = sessions_.begin(); iter!=sessions_.end(); ++iter)
        (*iter).second->update(current);

    std::vector<SessionKey> delete_sessions;
    {
        std::lock_guard<std::mutex> lock(delete_sessions_mutex_);
        delete_sessions = delete_sessions_;
    }
    for (size_t i = 0; i < delete_sessions.size(); ++i)
        sessions_.erase(delete_sessions[i]);
}

int Kcp::sendKcpPacket(const char *buf, int len, const asio::ip::udp::endpoint& ent, uint32_t convid)
{
    SpSession entity = getKcpEntity(ent, convid);
    assert(entity);
	sendUdpPacket(buf, len, entity);
    return 1;
}

void Kcp::sendUdpPacket(const char *buf, int len, SpSession kcp)
{
    send_message_consumer_.put(Message(kcp, std::string(buf, len)));
}

void Kcp::sendUdpConsumer(const Message& msg)
{
    udp_socket_.send_to(asio::buffer(msg.msg.c_str(), msg.msg.size()), msg.spsession_->end_point());
}

void Kcp::notifyMessageConsumer(const Message& msg)
{
    if (msg.spsession_->msgCallback())
        msg.spsession_->msgCallback()(msg.msg);
    else if (message_cb_)
        message_cb_(msg.spsession_, msg.msg);
}

void Kcp::stop()
{
    if (!runing_)
        return;

    assert(dostop_ == false);
    assert(stop_success_notify_.getCount() == 1);

    runing_ = false;
    stop_success_notify_.wait();
}

void Kcp::closeSession(const asio::ip::udp::endpoint& ent, uint32_t convid)
{
    std::lock_guard<std::mutex> lock(delete_sessions_mutex_);
    SessionKey key(ent, convid);
    delete_sessions_.push_back(key);
}

}