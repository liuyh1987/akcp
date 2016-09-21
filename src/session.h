#ifndef KCP_ENTITY_H_
#define KCP_ENTITY_H_

#include <memory>

#include "kcp/ikcp.h"
#include "concurrency_components.h"
#include <asio.hpp>

#include "typedef.h"

namespace Akcp{

class Kcp;
class Session: noncopyable, public std::enable_shared_from_this<Session>
{
public:
    typedef std::function<void (const std::string& message)> MessageCallback;
    typedef std::function<void ()> WriteCompleteCallback;

    void setMsgCallback(MessageCallback cb){msg_callback_ = cb;}
    MessageCallback& msgCallback() {return msg_callback_;}
    bool sendMsg(const char *buffer, int len);

	~Session();
    const asio::ip::udp::endpoint& end_point()const;
    void close();
    IUINT32 conv();

private:
    friend class Kcp;
    typedef std::function<int (
        const char *buf, int len, const asio::ip::udp::endpoint& ent, uint32_t convid)> SendPacketCallBack;

	Session(std::shared_ptr<Kcp> spakcp, IUINT32 id, const asio::ip::udp::endpoint& ep, 
            const SendPacketCallBack& output, TransMode mode);


	int recv(char *buffer, int len);
	int send(const char *buffer, int len);
    void update(IUINT32 current);
	int input(const char *data, long size);

	static int kcpOutPut(const char *buf, int len, struct IKCPCB *kcp, void *user);

private:
	asio::ip::udp::endpoint end_point_;
	ikcpcb* kcp_content_;
	SendPacketCallBack output_;
    MessageCallback msg_callback_;
    std::weak_ptr<Kcp> wpKcp_;
};

typedef std::shared_ptr<Session> SpSession;

}

#endif//KCP_ENTITY_H_