#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <mutex>
#include "redis.hpp"
#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include <muduo/net/TcpConnection.h>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;
using json =  nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler =  std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>; // 用一个已经存在的类型定义新的类型名称

// 服务类（区分与server，server是服务器），这里是业务代码
// 业务类，采用单例模式
class ChatService {
public:
    // 获取单例对象的接口函数，其返回值是一个对象的指针，如果返回对象的话，每次调用都会创建一个新的对象，就不是单例了，所以要返回指针
    static ChatService* instance(); // 单例模式的设计，暴露这样一个方法
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 服务器异常，业务重置方法
    void reset();
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);
private:
    ChatService();  // 由于采用了单例模式，所以要把构造函数私有化（***）

    unordered_map<int, MsgHandler> _msgHandlerMap;  // 保存不同的消息id对应的回调函数 

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;  // 要注意线程安全问题，因为在系统运行时会发生变化

    // 定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;

    // 离线消息操作对象
    OfflineMsgModel _offlineMsgModel;

    // 好友操作对象
    FriendModel _friendModel;

    // 群组操作对象
    GroupModel _groupModel;

    // redis操作对象
    Redis _redis;

};


#endif