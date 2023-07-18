#ifndef PUBLIC_H
#define PUBLIC_H

/*
server 和 client 的公共文件
*/
enum EnMsgType {
    LOGIN_MSG = 1,      // 登录消息1
    LOGIN_MSG_ACK,      // 登录响应消息2
    LOGINOUT_MSG,       // 注销消息3
    
    REG_MSG,            // 注册消息4
    REG_MSG_ACK,        // 注册响应消息5

    ONE_CHAT_MSG,       // 聊天消息6

    ADD_FRIEND_MSG,     // 添加好友消息7
    ADD_FRIEND_ACK,     // 添加好友响应消息8

    CREATE_GROUP_MSG,   // 创建群组9
    ADD_GROUP_MSG,      // 加入群组10
    GROUP_CHAT_MSG,     // 群聊天11
};

#endif