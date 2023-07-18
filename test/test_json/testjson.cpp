#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

// json 序列化实例1
string func1() {
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing?";

    string sendBuf = js.dump();     // 将json对象转换成序列化json字符串，从而可以在网络中发送

    cout << sendBuf.c_str() << endl;    // c_str()把string对象转换成c风格的字符串
    
    return sendBuf;
}

// json序列化实例2
void func2() {
    json js;
    // 添加数组
    js["id"] = {1,2,3,4,5};
    // 添加key-value
    js["name"] = "zhang san";
    // 添加对象(这个对象指的应该是json对象 )
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello china"}};
    cout << js << endl;
}

// json序列化代码示例3
string func3() {
    json js;
    // 直接序列化一个vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(5);
    js["list"] = vec;
    // 直接序列化一个map容器
    map<int, string> m;
    //m.insert({1, "黄山"});
    //m.insert({2, "华山"});
    //m.insert({3, "泰山"});
    m[3] = "黄山";
    m[5] = "华山";
    m[9] = "泰山";
    js["path"] = m;
    cout<<js<<endl;

    return js.dump();

}



int main() {
    string recvBuf = func3();

    // 数据的反序列化 json字符串 =》 反序列化数据对象（看作容器，方便访问）
    json jsbuf = json::parse(recvBuf);
    auto v = jsbuf["list"];
    cout << v[0] << endl;


    return 0;
}