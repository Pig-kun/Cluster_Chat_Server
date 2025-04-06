/*
moduo网络库给用户提供了两个主要的类
TcpServer：用于编写服务器程序
TcpClient：用于编写客户端程序

epoll + 线程池
好处：能够把网络I/O代码和业务代码区分开
                       用户的连接和断开     用户的可读写事件
*/
#include <iostream>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <functional>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

/*基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明月TcpServer构造函数需要什么参数，输出chatserver构造函数
4.在当前服务器类构造函数中，注册处理用户连接和断开的回调函数和处理用户读写事件的回调函数
5.设置合适的服务器线程数量
*/
class ChatServer{
public:
    ChatServer(EventLoop* loop, //事件循环
               const InetAddress& listenAddr,   //IP+Port 
               const string& nameArg)   // 服务器的名字
        : server_(loop, listenAddr, "ChatServer")
    {
        //  给服务器注册用户链接和断开的回调函数
        server_.setConnectionCallback(
            bind(&ChatServer::onConnection, this, _1));
        //  给服务器注册用户读写事件的回调函数
        server_.setMessageCallback(
            bind(&ChatServer::onMessage, this, _1, _2, _3));

        //  设置服务器的线程数量
        server_.setThreadNum(4); // 4个线程
    }

    void start()
    {
        server_.start();
    }
private:
    //  用户连接和断开事件的回调函数
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            cout << conn->peerAddress().toIpPort() << " -> "
                 << conn->localAddress().toIpPort()<<"online" << endl;
        }
        else
        {
            cout << conn->peerAddress().toIpPort() << " -> "
                 << conn->localAddress().toIpPort()<<"offline" << endl;
        }
    }

    void onMessage(const TcpConnectionPtr& conn,    //连接
                   Buffer* buf,   //读到的数据
                   Timestamp time)   //接收时间
    {
        string msg(buf->retrieveAllAsString());
        cout << "onMessage: " << msg <<"time:"<< time.toString()<< endl;
        conn->send(msg); //回发给客户端
    }

    TcpServer server_;  // #1
    EventLoop* loop_; // #2 epoll
};

int main(){
    EventLoop loop; 
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start(); 
    loop.loop();    // epoll_wait以阻塞方式等待连接和读写

    return 0;
}