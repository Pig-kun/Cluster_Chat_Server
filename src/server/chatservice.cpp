#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>

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
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
}

// 服务器异常，重置用户的状态信息
void ChatService::reset(){
    // 把online状态的用户，设置成offline
    _usermodel.resetState();
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
            response["errmsg"] = "this user has already logged in";
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

            // 查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 查询用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string>vec2;
                for(User &user : userVec){
                    json js;
                    js["id"] = user.getID();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询群组消息并返回
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty()){
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for(Group &group : groupuserVec){
                    json grpjson;
                    grpjson["id"] = group.getID();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user : group.getUsers()){
                        json js;
                        js["id"] = user.getID();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }else{
        
        // 该用户不存在，用户存在但是密码错误，登录失败
        if(user.getID() == -1){
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 1;
            response["errmsg"] = "the user is not exist";
            conn->send(response.dump());
        }else{
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["erron"] = 1;
            response["errmsg"] = "the password is wrong";
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
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组消息
    Group group(-1, name, desc);
    if(_groupModel.createGroup(group)){
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getID(), "creator");
    }
}   
//  加入群组
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

 // 群组聊天
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec){
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()){
            // 转发群消息
            it->second->send(js.dump());
        }else{
            // 存储离线消息
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}