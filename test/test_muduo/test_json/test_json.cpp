#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

// json 序列化示例1
string fun1(){
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhu kun";
    js["to"] = "kun zhu";
    js["msg"] = "heello, what are you doing now?";
    
    string sendbuf = js.dump();
    return sendbuf;
}

int main(){
    string recvbuf = fun1();
    json jsbuf = json::parse(recvbuf);
    cout << jsbuf["msg_type"] << endl;
    cout << jsbuf["from"] << endl;
    cout << jsbuf["to"] << endl;
    cout << jsbuf["msg"] << endl;
    return 0;
}

