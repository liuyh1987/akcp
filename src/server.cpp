#include "server.h"

namespace Akcp{

Connection::Connection(int nUniqueID, const int nHeadSize, SpSession  session, fun_callback cb)
    : uniqueID_(nUniqueID),
      headSize_(nHeadSize),
      session_(session),
      state_(Invalied),
      cb_(cb)
{
    assert(nHeadSize >= 0);
}


void Connection::send(const char *pData, unsigned int nLen)
{
    if (status() != Conneted)
    {
        //todo: log
        return;
    }
    session_->sendMsg(pData, nLen);
}


void Connection::onMessage(const std::string& message) 
{
    if (status() == Invalied)
    {
        if (strcmp(message.c_str(), ConnectReq) == 0)
        {
            wkPtr_ = shared_from_this();
            session_->sendMsg(ConnectResp, strlen(ConnectResp));
            setStatus(Conneted);
            cb_(C_ACCEPTED, wkPtr_ , NULL, 0);
        }
        else
        {
            //todo:
            //log
        }
    }
    else if (status() == Conneted)
    {
        if (strcmp(message.c_str(), CloseReq) == 0)
        {
            session_->sendMsg(CloseResp, strlen(CloseResp));
            setStatus(Closed);
            cb_(C_CLOSED, wkPtr_ , NULL, 0);
        }
        else
        {
            handleMsg(message);
        }
    }
    else if (status() == Closing)
    {
        if (strcmp(message.c_str(), CloseResp) == 0)
        {
            setStatus(Closed);
            cb_(C_CLOSED, wkPtr_ , NULL, 0);
        }
        else
        {
            //todo:
        }
    }
    else if (status() == Closed)
    {
        //todo: log
    }
}

Connection::~Connection()
{
	close();
}

bool Connection::close()
{
    if (status() == Conneted)
    {
		//LOG_FORMAT(info, "[kcp] KcpConnection close, remote addr %1%, ptr(%2%), remoteaddr(%3%)", getRemoteAddr()% this % getRemoteAddr());
		session_ ->sendMsg(CloseReq, strlen(CloseReq));
		setStatus(Closing);
    }
	return true;
}

void Connection::handleMsg(const std::string& msg)
{
    try
    {
        int nBodySize = msg.size();
        if (headSize_>0 && (nBodySize = cb_(C_HEADREADED, wkPtr_, &msg[0], headSize_))<0)
        {
            cb_(C_CLOSED, wkPtr_, "bad body size", 0);
            close();
            return;
        }

        if (nBodySize == msg.size()-headSize_)
        {
            cb_(C_BODYREADED, wkPtr_, &msg[0]+headSize_, nBodySize);
        }
        else
        {
            //todo:
            //log
        }
            
    }
    catch(std::exception &e)
    {
        //m_funCall(C_CLOSED, m_pWeakPtr, e.message().c_str(), 0);
        //LOG_FORMAT(warning, "server connection %1% got exception %2% connection key %3%",
    	//	remote_addr_ % e.what() % m_nUniqueID);
    }
    catch (...)
    {
        //m_funCall(C_CLOSED, m_pWeakPtr, e.message().c_str(), 0);
        //LOG_FORMAT(warning, "server connection %1% got unkownexception connection key %2%", 
    	//	remote_addr_  % m_nUniqueID);
    }

}

Server::Server(const std::string& address, 
       const std::string& port, 
       fun_callback cb_function, 
       const int nHeadSize,
       TransMode mode):
    cb_(cb_function),
    headSize_(nHeadSize),
    uniqueID_(0),
    loop_timer_(io_)
{
    spakcp_ = Kcp::createKcp(io_, address, atoi(port.c_str()), 
        std::bind(&Server::onMessage, this, std::placeholders::_1, std::placeholders::_2), mode);
}

Server::~Server()
{
    //fix me: make once
    spakcp_->stop();
    io_.stop();
    io_runer_.joinable();
}

void Server::run()
{
    //fix me: make once
    io_runer_ = std::thread(std::bind(&Server::runIoService, this));
    setTimer(60 * 1000);
}

void Server::onMessage(SpSession entity, const std::string& message)
{
    SpConnection cn;
    std::map<void*, SpConnection>::iterator iter = connections_.find(entity.get());
    if (iter == connections_.end())
    {
        cn.reset(new Connection(uniqueID_++, headSize_, entity, cb_));
        connections_[entity.get()] = cn;
    }
    else
        cn = iter->second;

    cn->onMessage(message);
}

void Server::runIoService()
{
    io_.run();
}

void Server::setTimer(int milliseconds)
{
    loop_timer_.expires_from_now(std::chrono::milliseconds(milliseconds));
    loop_timer_.async_wait(std::bind(&Server::handleTimter, this));
}

void Server::handleTimter()
{
    for (std::map<void*, SpConnection>::iterator iter = connections_.begin(); 
            iter != connections_.end(); )
    {
		if (iter->second->status() == Connection::Closed ||
			iter->second->status() == Connection::Closing)
		{
			//LOG_FORMAT(info, "[kcp] KcpConnection delete, remote addr %1%, ptr(%2%)", iter->second->getRemoteAddr() % iter->second.get());
			iter = connections_.erase(iter);
		}
        else 
            ++iter;
    }
    setTimer(60 * 1000);
}

void Server::stop()
{
	spakcp_->stop();
	io_.stop();
	io_runer_.join();
}
void Close(std::weak_ptr<void> ptrConn)
{
  std::shared_ptr<Akcp::Connection> ptrShared = std::static_pointer_cast<Akcp::Connection>(ptrConn.lock());
  if (ptrShared)
  {
	ptrShared->close();
  }
}


bool SendTcpMsg(std::weak_ptr<void> ptrConn, const char *pData, unsigned int nLen)
{
  std::shared_ptr<Akcp::Connection> ptrShared = std::static_pointer_cast<Akcp::Connection>(ptrConn.lock());
  if (ptrShared)
  {
	  ptrShared->send(pData, nLen);
    return true;
  }

  return false;
  /*
  tcp::server::connection_ptr ptrShared = std::static_pointer_cast<tcp::server::connection>(ptrConn.lock());
  if (ptrShared)
  {
    ptrShared->Send(pData, nLen);
    return true;
  }
  return false;*/
}


int GetConnID(std::weak_ptr<void> ptrConn)
{
  std::shared_ptr<Akcp::Connection> ptrShared = std::static_pointer_cast<Akcp::Connection>(ptrConn.lock());
  if (ptrShared)
  {
      return ptrShared->getUniqueID();
  }
  //LOG(warning, "GetConnID weak_ptr lock return null");
  return 0;
}


std::string GetRemoteAddr(std::weak_ptr<void> ptrConn)
{
  std::shared_ptr<Akcp::Connection> ptrShared = std::static_pointer_cast<Akcp::Connection>(ptrConn.lock());
  if (ptrShared)
  {
      return ptrShared->getRemoteAddr();
  }
  //LOG(warning, "GetRemoteAddr weak_ptr lock return null");
  return "";
}

}