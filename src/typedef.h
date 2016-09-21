#ifndef TYPE_DEF_H_
#define TYPE_DEF_H_

enum TransMode
{
    TransModeNomal = 0, //fast retrans
    TransModeNoDelay, //fast retrans + nodelay
    TransModeNoCC // fast  restrans + nodelay + noCC
};



#define ConnectReq "AkcpConnectReq"
#define ConnectResp "AkcpConnectResp"
#define CloseReq "AkcpCloseReq"
#define CloseResp "AkcpCloseResp"

//#define CAN_NOT_ASSIGN_AND_COPY_DECLARE(TypeName) \
            TypeName ( const TypeName& ) = delete; \
            const TypeName& operator= ( const TypeName& ) = delete;

#endif//TYPE_DEF_H_