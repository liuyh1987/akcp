#ifndef AKCP_CLIENT_H_
#define AKCP_CLIENT_H_

#include "typedef.h"
#include "kcp.h"


/*statu machine
              +----------------+
               |   invalied     +-----------+
               |                |           |
               +------+---------+           |
                      |                  recv_connect_req
                  call_connect           send_connect_resp
                  send_connect_req          |
                      |                     |
                      |                     |
               +------v---------+           |
    +----------+ connecting     |           |
    |          |                |           |
    |          +------+---------+           |
call_close            |                     |
send_close_req    recv_cnnecte_resp         |
    |                 |                     |
    |                 |                     |
    |           +-----v----------+<---------+       +----------------+
    |           |    connceted   |  recv_close_req  |  closed        |
    |           |                +--send_close_resp->                |
    |           +-----+----------+                  +------+---------+
    |                 |                                    ^
    |             call close                               |
    |             send_close_req                           |
    |           +-----v----------+                         |
    +---------->|                |                  recv_close_resp
                |  closing       +-------------------------+
                +----------------+
*/


namespace Akcp{

class Client: noncopyable
{
public:
    enum Status
    {
        Invalied,
        Connecting,
        Connected,
        Closing,
        Closed
    };

    typedef std::function<void (const std::string& message)> MessageCallback;
    typedef std::function<void ()> CloseCallback;
    typedef std::function<void (bool, const std::string& msg)> ConnectedCallback;

    Client(const std::string& address, int udp_port, TransMode mode = TransModeNomal);
    ~Client();
    void setMessageCallback(MessageCallback cb) { messageCb_ = cb; }
    void setCloseCallback(CloseCallback cb) { closeCb_ = cb; }
    //bool asyncConnect(const std::string& address, int udp_port, ConnectedCallback cb);
    bool syncConnect(const std::string& address, int udp_port/*outtime*/, std::chrono::milliseconds outtime);
    bool sendMsg(const char* buf, size_t len);
    bool close();

private:
    void onMessage(const std::string& message);
    bool waitConnectResult(ConnectedCallback cb, std::chrono::milliseconds outtime);
    void doConnect(const std::string& address, int udp_port);
    void initSession(const std::string& address, int udp_port);

    void setStatus(Status s) { state_ = s; } 
    Status status() const { return state_; }
    void runIoService();

private:
    SpKcp akcp_;
    SpSession remote_;
    asio::io_service io_;
    std::shared_ptr<CountDownLatch> spConnectNotify_;
    std::shared_ptr<CountDownLatch> spCloseNotify_;
    Status state_;
    std::thread io_runer_;

    MessageCallback messageCb_;
    CloseCallback closeCb_;
};

}

#endif //AKCP_CLIENT_H_