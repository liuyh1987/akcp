#ifndef AKCP_SAMPLES_FILE_TRANS_MSG_DEF_H__
#define AKCP_SAMPLES_FILE_TRANS_MSG_DEF_H__

#define BLOCK_SIZE 1300

enum MsgType 
{
    SendFileBegin,
    SendFileBlock,
    SendFileEnd,
    SendFileEndAck
};
struct MsgSendFileBeginNotify 
{ 
    MsgType msgType{SendFileBegin}; 
    int file_name_size; 
    char file_name[250]; 
    long file_size; 
};
struct MsgSendFileBlockNotify 
{ 
    MsgType msgType{SendFileBlock}; 
    int block_size; 
    char content[BLOCK_SIZE]; 
};
struct MsgSendFileEndNotify 
{ 
    MsgType msgType{SendFileEnd}; 
    const char* end = "end"; 
};
struct MsgSendFileEndAckNotify 
{ 
    MsgType msgType{SendFileEndAck}; 
    const char* end = "endAck"; 
};

#endif // AKCP_SAMPLES_FILE_TRANS_MSG_DEF_H__
