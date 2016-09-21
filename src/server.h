#ifndef AKCP_SERVER_H_
#define AKCP_SERVER_H_

#include <memory>
#include "typedef.h"
#include "kcp.h"

namespace Akcp{

enum T_CALLBACK
{
    C_ACCEPTED,
    C_HEADREADED, // 接到此回调时，返回之后所需要的body的大小
    C_BODYREADED,
    C_CLOSED
};
typedef std::function<int (T_CALLBACK, std::weak_ptr<void>, const char *, unsigned int) > fun_callback;

/// 发送消息
/// pData传NULL表示断开连接
bool SendTcpMsg(std::weak_ptr<void> ptrConn, const char *pData, unsigned int nLen);
/// 将weak_ptr转换为整型，返回0表示失败
int GetConnID(std::weak_ptr<void> ptrConn);
/// 
std::string GetRemoteAddr(std::weak_ptr<void> ptrConn);
/// 关闭连接
void Close(std::weak_ptr<void> ptrConn);

class Connection: noncopyable, public std::enable_shared_from_this<Connection>
{
public:
    enum Status { Invalied, Conneted, Closing, Closed };

    Connection(int nUniqueID, const int nHeadSize, SpSession  session, fun_callback cb);
    ~Connection();

    int getUniqueID() { return uniqueID_; }
    std::string getRemoteAddr() { session_->end_point().address().to_string(); }

    void send(const char *pData, unsigned int nLen);
    void onMessage(const std::string& message);
    Status status()const { return state_; }
    void setStatus(Status s) {state_ = s;}
    bool close();

private:
    void handleMsg(const std::string& msg);

private:
    SpSession session_;
    Status state_;
    fun_callback cb_;
    std::weak_ptr<void> wkPtr_;
    int uniqueID_; 
    int headSize_;
};
typedef std::shared_ptr<Connection> SpConnection;

class Server: noncopyable, public std::enable_shared_from_this<Server>
{
public:
    Server(const std::string& address, const std::string& port, 
           fun_callback cb_function, const int nHeadSize, TransMode mode);
    ~Server();
    void run();
	void stop();

private:
    void onMessage(SpSession entity, const std::string& message);
    void runIoService();
    void setTimer(int milliseconds);
    void handleTimter();

private:
    std::map<void*, SpConnection> connections_;
    int uniqueID_; 
    int headSize_;
    fun_callback cb_;
    std::thread io_runer_;
    asio::io_service io_;
    SpKcp spakcp_;

    asio::basic_waitable_timer<std::chrono::system_clock> loop_timer_;
};

}

#endif//AKCP_SERVER_H_