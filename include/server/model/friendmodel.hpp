#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H
#include <vector>
#include "user.hpp"
using namespace std;

// 维护好友信息的操作接口方法
class FriendModel 
{
public:
    // 添加好友信息
    void insert(int userid, int friendid);

    // 查找一个userid的所有friendid（返回一个用户的好友列表）
    vector<User> query(int userid);
private:

};

#endif