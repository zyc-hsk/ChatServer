#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <string>
#include <map>
#include <vector>
using namespace muduo;
using namespace std;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{

    static ChatService service; // 一个单例对象
    return &service;
}

// 注册消息以及对应的handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginOut, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
    
}

void ChatService::reset()
{
    // 把online状态的用户设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调函数
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time) { // 这些参数是为了匹配返回值的MsgHandler类型
            LOG_ERROR << "msgid: " << msgid << " can not find handler!";
        };
        // 这样设计即使没有对应的处理器，程序也不会挂掉，还是能正常进行
        // 这应该是比较正常的思路，相当于给不存在的msgid也提供了一个回调函数
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "do login service !";
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id); // 通过id值查找得到对应的User对象
    if (user.getId() != -1)
    {
        // 用户存在
        if (user.getPwd() == pwd)
        {
            // 用户名、密码都正确
            if (user.getState() == "online")
            {
                // 用户已登录
                json response;
                response["msgid"] = LOGIN_MSG_ACK;
                response["errno"] = 3;
                response["errmsg"] = "用户已登录，请勿重复登录";
                conn->send(response.dump());
            }
            else
            {
                // 登录成功，记录用户连接信息
                {
                    lock_guard<mutex> lock(_connMutex);
                    _userConnMap.insert({id, conn});
                } // 出这个作用域之后锁就释放了

                // id用户登录成功后，向redis订阅channel（id）
                _redis.subscribe(id);

                // 登录成功，更新用户状态信息
                user.setState("online");
                _userModel.updateState(user);

                json response;
                response["msgid"] = LOGIN_MSG_ACK;
                response["errno"] = 0;
                response["id"] = user.getId();
                response["name"] = user.getName();

                // 用户登录之后，查询该用户是否有离线消息
                vector<string> vec = _offlineMsgModel.query(id);
                if (!vec.empty())
                {
                    response["offlinemsg"] = vec;
                    // 读取后删除
                    _offlineMsgModel.remove(id);
                }

                // 查询该用户的好友信息并返回
                vector<User> userVec = _friendModel.query(id);
                if (!userVec.empty())
                {
                    vector<string> vec2;
                    for (User &user : userVec)
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        vec2.push_back(js.dump());
                    }
                    response["friends"] = vec2;
                }

                // 查询该用户的群组信息并返回
                vector<Group> groupVec = _groupModel.queryGroups(id);
                if (!groupVec.empty())
                {
                    vector<string> vec3;
                    for (Group &group : groupVec)
                    {
                        json js;
                        js["id"] = group.getId();
                        js["groupname"] = group.getName();
                        js["groupdesc"] = group.getDesc();
                        vector<GroupUser> vec4 = group.getUsers();
                        vector<string> vec5;
                        for (GroupUser &groupuser : vec4)
                        {
                            json gujs;
                            gujs["id"] = groupuser.getId();
                            gujs["name"] = groupuser.getName();
                            gujs["state"] = groupuser.getState();
                            gujs["role"] = groupuser.getRole();
                            vec5.push_back(gujs.dump());
                        }
                        js["users"] = vec5;
                        vec3.push_back(js.dump());
                    }
                    response["groups"] = vec3;
                }

                conn->send(response.dump());
            }
        }
        else
        {
            // 密码错误
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "密码错误";
            conn->send(response.dump());
        }
    }
    else
    {
        // 用户不存在
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "该用户不存在";
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>(); // 只需要一个id

    {
        lock_guard<mutex> lock(_connMutex); // 因为要对_userConnMap进行操作，所以要注意线程安全
        auto it = _userConnMap.begin();
        while (it != _userConnMap.end())
        {
            if (it->first == userid)
            {
                _userConnMap.erase(it); // 删除对应的连接
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user); // 状态改为offline
}

// 处理注册业务     name  password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 表示响应成功，若为1则需要加errmsg说明错误消息
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // 表示响应失败
        response["id"] = user.getId();
        conn->send(response.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex); // 因为要对_userConnMap进行操作，所以要注意线程安全
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            // 查表，因为没有id，所以不能直接访问，只能遍历哈希表
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    bool userState = false;
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())   // 说明目标用户在同样的服务器上登录了，那就可以直接转发消息
        {
            // toid 在线，转发消息  服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 若目标用户未在该服务器上登录，则有两种情况
    // 1.目标用户登录在其他服务器上
    // 2.目标用户不在线

    // 查询toid是否在线
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        // toid用户在其他服务器上登录
        _redis.publish(toid, js.dump());
        return;
    }

    // toid 不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务  msgid  id  friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 判断friendid是否存在
    User user = _userModel.query(friendid);
    if (user.getId() == -1)
    {
        json response;
        response["msgid"] = ADD_FRIEND_ACK;
        response["errno"] = 1; // 表示friendid不存在
        response["errmsg"] = "friendid 不存在";
        conn->send(response.dump());
        return;
    }
    // 判断是否已经是好友
    vector<User> vec = _friendModel.query(userid);
    auto it = vec.begin();
    while (it != vec.end())
    {
        if (it->getId() == friendid)
            break;
        else
            it++;
    }
    if (it != vec.end())
    {
        // 好友已存在
        json response;
        response["msgid"] = ADD_FRIEND_ACK;
        response["errno"] = 2; // 已经是好友了
        response["errmsg"] = "你们已经是好友";
        conn->send(response.dump());
        return;
    }
    _friendModel.insert(userid, friendid);
    json response;
    response["msgid"] = ADD_FRIEND_ACK;
    response["errno"] = 0; // 成功添加好友
    conn->send(response.dump());
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "do groupchat service !";
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        LOG_INFO << "transfer message";
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }


    // 存储该用户的离线信息
    _offlineMsgModel.insert(userid, msg);

}