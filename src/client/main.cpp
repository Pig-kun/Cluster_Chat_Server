#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登陆成功用户的基本信息
void showCurrentUserData();

// 接收线程
void readTasHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 住聊天页面程序
void mainMenu();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv){
    if ( argc <3 ){
        cerr << "command error, please input: ./chatclient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( -1 == clientfd ){
        cerr << "create socket error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息的ip和port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client 和 server 进行连接
    if ( -1 == connect(clientfd, (struct sockaddr *)&server, sizeof(sockaddr_in))){
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入，负责发送数据
    for(;;){
        // 显示首页面菜单 ： 登录 注册 退出
        cout << "=========================" <<endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=========================" <<endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice){
            case 1: // login业务
            {
                int id = 0;
                char password[50] = {0};
                cout << "user_id:";
                cin >> id;
                cin.get(); // 读掉缓冲区残留的回车
                cout << "user_password:";
                cin.getline(password, 50); // 读入一行

                // 组装json数据
                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = password;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

                if (len == -1){
                    cerr << "send login msg error:" << request << endl;
                }else{
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if( -1 == len){
                        cerr << "recv login response error" << endl;
                    }else{
                        json responsejs = json::parse(buffer);
                        if (0 != responsejs["erron"].get<int>()){  // 登录失败
                            cerr << responsejs["errmsg"] << endl;
                        }else{ // 登陆成功
                            // 记录当前用户的id和name
                            g_currentUser.setID(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);

                            // 记录当前用户的好友列表信息
                            if (responsejs.contains("friends")){
                                vector<string> vec = responsejs["friends"];
                                for (string &str : vec){
                                    json js = json::parse(str);
                                    User user;
                                    user.setID(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            // 记录当前用户的群组列表信息
                            if (responsejs.contains("groups")){
                                vector<string> vec1 = responsejs["groups"];
                                for(string &groupstr : vec1){
                                    json grpjs = json::parse(groupstr);
                                    Group group;
                                    group.setID(grpjs["id"].get<int>());
                                    group.setName(grpjs["groupname"]);
                                    group.setDesc(grpjs["groupdesc"]);

                                    vector<string> vec2 = grpjs["users"];
                                    for (string &userstr : vec2){
                                        GroupUser user;
                                        json js = json::parse(userstr);
                                        user.setID(js["id"].get<int>());
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }
                                    g_currentUserGroupList.push_back(group);
                                }
                            }

                            // 显示登录用户的基本信息
                            showCurrentUserData();

                            // 显示当前用户的离线信息 个人聊天信息或者群组消息
                            if(responsejs.contains("offlinemsg")){
                                vector<string> vec = responsejs["offlinemsg"];
                                for(string & str : vec){
                                    json js = json::parse(str);
                                    // time + [id] + name + "said:" + xxx
                                    cout << js["time"] << "[" << js["id"] << "]" << js["name"] 
                                         << " said: " << js["msg"] << endl;
                                }
                            }

                            //登陆成功，启动接收线程负责接收数据
                            std::thread readTask(readTasHandler, clientfd);
                            readTask.detach();
                            // 进入聊天主菜单页面
                            mainMenu();
                        }
                    }
                }
            }
            break;
            case 2: // register业务
            {
                char name[50] = {0};
                char password[50] = {0};
                cout << "user_name:";
                cin.getline(name, 50); // 读入一行
                cout << "user_password:";
                cin.getline(password, 50); // 读入一行

                // 组装json数据
                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = password;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), request.size(), 0);
                if (len == -1){
                    cerr << "send reg msg error:" << request << endl;
                }else{
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, sizeof(buffer)-1, 0);
                    if (-1 == len){
                        cerr << "recv reg response error" << endl;
                    }else{
                        json responsjs = json::parse(buffer);
                        if (0 != responsjs["errno"].get<int>()){ // 注册失败
                            cerr << name << "is already exist, register error!"<< endl;
                        }else{ // 注册成功
                            cout << name << "register success, userid is" << responsjs["id"] << ", do not forget it!" << endl;
                        }
                    }
                }
            }
            break;
            case 3: // quit业务
            {
                close(clientfd);
                exit(0);
            }
            
            default:
                cerr << "invalid input" << endl;
                break;
        }
    }
}

// 显示当前登陆成功用户的基本信息
void showCurrentUserData(){
    cout << "========================login user========================" << endl;
    cout <<"current login user => id:" << g_currentUser.getID() << " name:" << g_currentUser.getName() << " state:" << g_currentUser.getState() << endl;
    cout << "------------------------friend list-----------------------" << endl;
    if ( !g_currentUserFriendList.empty()){
        for (User &user : g_currentUserFriendList){
            cout << "friend id:" << user.getID() << " name:" << user.getName() << " state:" << user.getState() << endl;
        }
    }
    cout << "------------------------group list------------------------" << endl;
    if ( !g_currentUserGroupList.empty()){
        for (Group &group : g_currentUserGroupList){
            cout << "group id:" << group.getID() << " name:" << group.getName() << " desc:" << group.getDesc() << endl;
            cout << "group user list:" << endl;
            for (GroupUser &user : group.getUsers()){
                cout << "user id:" << user.getID() << " name:" << user.getName() << " role:" << user.getRole() << endl;
            }
        }
    }
    cout << "==========================================================" << endl;
}

// 接收线程
void readTasHandler(int clientfd){

}

void mainMenu(){

}