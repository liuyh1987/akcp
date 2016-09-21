#ifndef AKCP_H_
#define AKCP_H_

#include <chrono>

#include <asio.hpp>

#include "concurrency_components.h"
#include "typedef.h"
#include "session.h"

namespace Akcp{

class Kcp: noncopyable, public std::enable_shared_from_this<Kcp>
{
public:

    typedef std::function<void (SpSession, const std::string& message)> MessageCallback;

    static std::shared_ptr<Kcp> createKcp(asio::io_service& io_service, const std::string& address, 
                                          int udp_port, MessageCallback cb, TransMode mode = TransModeNomal);

    SpSession createSession(const std::string& address, int udp_port);
    void closeSession(const asio::ip::udp::endpoint& ent, uint32_t convid);

	~Kcp(); 

    void stop();
	const asio::ip::udp::endpoint localEndPoint()
	{
		return udp_socket_.local_endpoint();
	}

private:
    struct SessionKey
    {
        asio::ip::udp::endpoint edt;
        uint32_t id;
        SessionKey(const asio::ip::udp::endpoint& aedt, uint32_t aid) :edt(aedt), id(aid) { }
        bool operator< (const SessionKey& entity) const
        {
            if (edt < entity.edt)
                return true;
            else if (edt == entity.edt)
                return id < entity.id;

            return false;
        }
    };
	typedef std::map<SessionKey, SpSession> MapSession;

    struct Message
    {
        Message(SpSession entity, const char* cmsg, size_t len): spsession_(entity), msg(cmsg, len) { }
        Message(SpSession entity, const std::string& message): spsession_(entity), msg(message) { }
        Message(){}
        std::string msg;
        SpSession spsession_;
    };

private:
    friend class Session;
	Kcp(asio::io_service& io_service, const std::string& address, int udp_port, 
        MessageCallback cb, TransMode mode = TransModeNomal);
    void handleTtimerLoop();
	void doUdpAasyncRead(void);
    void handleUdpRead(const asio::error_code& code, std::size_t bytes_recvd);
	void handleKcpRead(size_t bytes_recvd, uint32_t conv);
	void setTimer(int milliseconds);
	void handleKcpLoop();

    void sendMessage(SpSession entity, const char* buf, size_t buf_len);
	int sendKcpPacket(const char *buf, int len, const asio::ip::udp::endpoint& ent, uint32_t convid);
	void sendUdpPacket(const char *buf, int len, SpSession kcp);
    void sendUdpConsumer(const Message& msg);

    SpSession getKcpEntity(const asio::ip::udp::endpoint& ep, uint32_t conv);
    SpSession createSession(const asio::ip::udp::endpoint& remote_point);

    void notifyMessageConsumer(const Message& msg);
    bool checkStop();
    void doStop();

private:
    asio::basic_waitable_timer<std::chrono::system_clock> loop_timer_;
	asio::io_service& io_service_;

	asio::ip::udp::socket udp_socket_;
	asio::ip::udp::endpoint udp_sender_endpoint_;
    char recv_buf_[1024 * 32];

	MapSession sessions_;
    MessageCallback message_cb_;

    ThreadSafeQueue<Message> send_queue_;
    QueueConsumer<Message> notify_message_consumer_;
    QueueConsumer<Message> send_message_consumer_;

    TransMode trans_mode_;
    static int LOOP_TIMVER_INTERVAL_MSECONDS;

    bool runing_;
    bool dostop_; 
    CountDownLatch  stop_success_notify_;

    std::mutex delete_sessions_mutex_;
    std::vector<SessionKey> delete_sessions_;
};

typedef std::shared_ptr<Kcp> SpKcp;
}

#endif//AKCP_H_