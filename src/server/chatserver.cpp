#include "chatserver.hpp"

#include <functional>
using namespace std;
using namespace placeholders;

ChatServer::ChatServer(EventLoop *loop,
         const InetAddress &listenAddr,
         const string &nameArg)
    :_server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    //注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    //设置线程数量
    _server.setThreadNum(4);

}

// 启动聊天服务器
void ChatServer::start(){
    _server.start();
}

// 上报连接相关的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn){

}

// 上报读写事件相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time)
{
    
}
