#ifndef GROUP_HPP
#define GROUP_HPP

#include "groupuser.hpp"
#include <string>
#include <vector>
using namespace std;

// Group表的 ORM 类
class Group {
public:
    Group(int id = -1, string name = "", string desc = ""){
        this->id = id;
        this->name = name;
        this->desc = desc;
    }

    void setID(int id) { this->id = id; }
    int getID() { return this->id; }

    void setName(string name) { this->name = name; }
    string getName() { return this->name; }

    void setDesc(string desc) { this->desc = desc; }
    string getDesc() { return this->desc; }

    vector<GroupUser> &getUsers() { return this->users; }

private:
    int id;
    string name;
    string desc;
    vector<GroupUser> users; 
};

#endif