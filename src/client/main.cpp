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
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 显示当前登陆成功用户的基本信息
void showCurrentUserData();
// 接收线程
void readTasHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 住聊天页面程序
void mainMenu(int clientfd);

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

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTasHandler, clientfd); // pthread_create
    readTask.detach();                              // pthread_detach

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

                g_isLoginSuccess = false;

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

                if (len == -1){
                    cerr << "send login msg error:" << request << endl;
                }

                sem_wait(&rwsem);   // 等待信号量，由子线程处理完登录的响应消息后，通知这里

                if(g_isLoginSuccess){
                    // 进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
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
                }
                sem_wait(&rwsem);   // 等待信号量
            }
            break;
            case 3: // quit业务
            {
                close(clientfd);
                sem_destroy(&rwsem);
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
    cout <<"current login user => id:" << g_currentUser.getID() << " name:" << g_currentUser.getName() << endl;
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
                cout << "user id:" << user.getID() << " name:" << user.getName() << " role:" << user.getRole() 
                     << "user state:" << user.getState() << endl;
            }
        }
    }
    cout << "==========================================================" << endl;
}

// 处理注册的相应逻辑
void doRegResponse(json &responsjs){

    if (0 != responsjs["errno"].get<int>()){ // 注册失败
        cerr << "name is already exist, register error!"<< endl;
    }else{ // 注册成功
        cout << "name register success, userid is" << responsjs["id"] << ", do not forget it!" << endl;
    }
}
// 处理登录的响应逻辑
void doLoginResponse(json &responsejs){

    if (0 != responsejs["erron"].get<int>()){  // 登录失败
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }else{ // 登陆成功
        // 记录当前用户的id和name
        g_currentUser.setID(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表信息
        if (responsejs.contains("friends")){
            // 初始化
            g_currentUserFriendList.clear();

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
            // 初始化
            g_currentUserGroupList.clear();
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
                int msgtype = js["msgid"].get<int>();
                if (ONE_CHAT_MSG == msgtype)
                {
                    cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                            << "said:" << js["msg"].get<string>() << endl;
                }
                else if (GROUP_CHAT_MSG == msgtype)
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                            << " ->said:" << js["msg"].get<string>() << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 子线程 - 接收线程
void readTasHandler(int clientfd){
    for(;;){
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (len == -1 || len == 0){
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化成json对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype){
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said:" << js["msg"].get<string>() <<endl;
            continue;
        }else if(GROUP_CHAT_MSG == msgtype){
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said:" << js["msg"].get<string>() <<endl;
            continue;
        }

        if(LOGIN_MSG_ACK == msgtype){
            doLoginResponse(js);    // 处理登录响应的业务逻辑
            sem_post(&rwsem);       // 通知主线程处理结果完成
            continue;
        }
        if (REG_MSG_ACK == msgtype){
            doRegResponse(js);      // 处理注册响应的业务逻辑
            sem_post(&rwsem);       // 通知主线程处理结果完成
            continue;
        }
    }
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string,string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friend:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int,string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if( -1 == idx ){
            command = commandbuf;
        }else{
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()){
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的时间处理回调，，mainMenu对修改封闭，添加新功能不需要修改此函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); 
    }
}

// "help" command handler
void help(int fd, string str){
    cout << "show command list >>>" << endl;
    for(auto &p : commandMap){
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}
// "chat" command handler
void chat(int clientfd, string str){
    int idx = str.find(":");
    if( idx == -1){
        cerr << "chat command invalid" << endl;
        return;
    }
    int friendid = atoi(str.substr(0 ,idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["name"] = g_currentUser.getName();
    js["to"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len){
        cerr << "send chat msg error -> " << buffer << endl;
    }
}
// "addfriend" command handler
void addfriend(int clientfd, string str){
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getID();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);

    if ( -1 == len){
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}
// "creategroup" command handler
void creategroup(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "create command invalid" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx+1, str.size()-idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getID();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len){
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
    
}
// "addgroup" command handler
void addgroup(int clientfd, string str){
     int groupid = atoi(str.c_str());
     
     json js;
     js["msgid"] = ADD_GROUP_MSG;
     js["id"] = g_currentUser.getID();
     js["groupid"] = groupid;
     string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len){
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}
// "groupchat" command handler
void groupchat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "groupchat command invalid" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx+1, str.size()-idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["groupid"] = groupid;
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len){
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}
// "loginout" command handler
void loginout(int clientfd, string){
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getID();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len){
    cerr << "send loginout msg error -> " << buffer << endl;
    }else{
        isMainMenuRunning = false;
    }
}

string getCurrentTime() {
    // 获取当前时间
    time_t now = time(nullptr);
    // 将时间转换为本地时间结构
    tm* localTime = localtime(&now);

    // 检查是否获取到有效时间
    if (localTime == nullptr) {
        return "Error: Unable to get local time.";
    }

    // 创建一个字符数组用于存储格式化后的时间
    char buffer[64];
    // 使用 strftime 格式化时间
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);

    // 将字符数组转换为字符串并返回
    return string(buffer);
}