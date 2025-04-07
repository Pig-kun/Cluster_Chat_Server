#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "json.hpp"
using json = nlohmann::json;

using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

// 聊天服务器业务类
class ChatService{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
private:
    ChatService();

    unordered_map<int, MsgHandler> _msgHandlerMap;
};

#endif