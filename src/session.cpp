#include "session.h"
#include "kcp.h"

namespace {

void writelog(const char *log, struct IKCPCB *kcp, void *user)
{
    //std::cout << log << std::endl;
}
    
}

namespace Akcp{

Session::Session(std::shared_ptr<Kcp> spakcp, IUINT32 id, const asio::ip::udp::endpoint& ep, 
                 const SendPacketCallBack& output, TransMode mode)
	:wpKcp_(spakcp),
     end_point_(ep),
	 output_(output)
     
{
	kcp_content_ = ikcp_create(id, this);
	kcp_content_->output = kcpOutPut;

    ikcp_wndsize(kcp_content_, 128, 128);

    // 启动快速模式
    // 第二个参数 nodelay-启用以后若干常规加速将启动
    // 第三个参数 interval为内部处理时钟，默认设置为 10ms
    // 第四个参数 resend为快速重传指标，设置为2
    // 第五个参数 为是否禁用常规流控，这里禁止

    //一般: ikcp_nodelay(p_kcp_, 0, 5, 2, 0);
    //快速: ikcp_nodelay(p_kcp_, 1, 5, 2, 0);
    //超快: ikcp_nodelay(p_kcp_, 1, 5, 2, 1);

    assert(mode>=TransModeNomal && mode<=TransModeNoCC);
    if (mode == TransModeNomal)
        ikcp_nodelay(kcp_content_, 0, 5, 2, 0);
    else if(mode == TransModeNoDelay)
        ikcp_nodelay(kcp_content_, 1, 5, 2, 0);
    else if (mode == TransModeNoCC)
        ikcp_nodelay(kcp_content_, 1, 5, 2, 1);

    //kcp_content_->writelog = writelog;
    //kcp_content_->logmask = 0xff;
}

IUINT32 Session::conv()
{
	return kcp_content_->conv;
}


Session::~Session()
{
	if (kcp_content_ != NULL)
	{
		ikcp_release(kcp_content_);
		kcp_content_ = NULL;
		close();
	}
}

int Session::recv(char *buffer, int len)
{
	return ikcp_recv(kcp_content_, buffer, len);
}

int Session::send(const char *buffer, int len)
{
	return ikcp_send(kcp_content_, buffer, len);
}

void Session::update(IUINT32 current)
{
    ikcp_update(kcp_content_, current);
}

int Session::input(const char *data, long size)
{
	return ikcp_input(kcp_content_, data, size);
}

int Session::kcpOutPut(const char *buf, int len, struct IKCPCB *kcp, void *user)
{
	assert(user != NULL);
	Session* kcpentity = (Session*)(user);
	assert(kcp == kcpentity->kcp_content_);

	return kcpentity->output_(buf, len, kcpentity->end_point(), kcp->conv);
}

const asio::ip::udp::endpoint& Session::end_point()const
{
    return end_point_;
}

bool Session::sendMsg(const char *buffer, int len)
{
    std::shared_ptr<Kcp> SpAkcp = wpKcp_.lock();
    if (SpAkcp)
	{
        SpAkcp->sendMessage(shared_from_this(), buffer, len);
		return true;
	}

	return false;
}

void Session::close()
{
    std::shared_ptr<Kcp> SpAkcp = wpKcp_.lock();
    if (SpAkcp)
        SpAkcp->closeSession(end_point_, conv());
}

}