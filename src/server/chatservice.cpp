#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>

using namespace muduo;
using namespace std;

// 获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler的回调操作
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    // 记录错误日志，msgid没有对应的时间处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        // 返回一个默认的处理器， 空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time){
            LOG_ERROR << "msgid:" << msgid <<"can not find handler!";
        };
    }
    else{
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int id = js["id"];
    string pwd = js["password"];
    User user = _usermodel.query(id);
    if(user.getID() == id && user.getPassword() == pwd){
        if(user.getState() == "online"){
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 2;
            response["errmsg"] = "该账号已经登录，请重新输入新账号";
            conn->send(response.dump());
        }else{
            // 登陆成功，记录用户连接信息
            {
                // {}减少锁的作用域
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            // 登录成功，更新用户状态信息，state -> online
            user.setState("online");
            _usermodel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 0;
            response["id"] = user.getID();
            response["name"] = user.getName();
            conn->send(response.dump());
        }
    }else{
        
        // 该用户不存在，用户存在但是密码错误，登录失败
        if(user.getID() == -1){
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 1;
            response["errmsg"] = "用户不存在";
            conn->send(response.dump());
        }else{
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 1;
            response["errmsg"] = "密码错误";
            conn->send(response.dump());
        }
    }
}
    
// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time){
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _usermodel.insert(user);
    if(state){
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getID();
        conn -> send(response.dump());
    }else{  
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn -> send(response.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if(it->second == conn){
                user.setID(it->first);
                // 从map表删除用户的连接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 更新用户的状态信息
    if (user.getID() != -1){
        user.setState("offline");
        _usermodel.updateState(user);
    }   
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int toid = js["to"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            // todi用户在线，转发消息  
            it->second->send(js.dump());
            return;
        }
    }
    // toid用户不在线，存储离线消息
}