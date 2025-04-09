#include "groupmodel.hpp"
#include "db.h"

// 创建群组
bool GroupModel::createGroup(Group &group){
    // 1.组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "insert into into AllGroup(groupname, groupdesc) values('%s', '%s')",
        group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
           group.setID(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
bool addGroup(int userid, int groupid, string role){
    // 1.组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "insert into into GroupUser values(%d, %d, '%s')",
        groupid, userid, role.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

// 查询用户所在的群组信息
vector<Group> queryGroups(int userid){
    /*
    1. 先根据userid在froupuser表中查询出该用户所属的群组信息
    2. 再根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    */
   char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b on a.id = b.groupid where b.userid=%d", userid);

    vector<Group> groupVec;

     MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES * res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            // 查出userid所有群组消息
            while((row = mysql_fetch_row(res)) != nullptr){
                Group group;
                group.setID(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询群组的用户信息
    for(Group &group : groupVec){
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from User a inner join GroupUser b on a.id = b.userid where b.groupid=%d", group.getID());
        MYSQL_RES * res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                GroupUser user;
                user.setID(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
}

// 根据指定的groupid查询群组用户ID列表，除userid自己，主要用户群聊业务给群组其他成员群发消息
vector<int> queryGroupUsers(int userid, int groupid){
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid!=%d", groupid, userid);
    vector<int> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES * res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return vec;
}