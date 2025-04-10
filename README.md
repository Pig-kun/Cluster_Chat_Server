# Cluster_Chat_Server
基于muduo网络库开发的集群聊天服务器，涉及Json序列化与反序列化，nginx，Redis，MySQL等

# 创建数据库
```sql
create database chat;
use chat;

CREATE TABLE User (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    state ENUM('online', 'offline') DEFAULT 'offline'
);

CREATE TABLE Friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY (userid, friendid)
);

CREATE TABLE AllGroup(
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL,
    groupdesc VARCHAR(200) DEFAULT ' '
);

CREATE TABLE GroupUser(
    groupid INT PRIMARY KEY,
    userid INT NOT NULL,
    grouprole ENUM('creator','normal')  DEFAULT'normal'
);

CREATE TABLE OfflineMessage(
    userid INT PRIMARY KEY,
    message VARCHAR(50) NOT NULL
);
```
# 配置Nginx，实现负载均衡功能
首先下载nginx安装包：格式：nginx-1.12.2.tar.gz 
## 解压
```bash
tar -axvf nginx-1.12.2.tar.gz 
```
解压后进入文件夹nginx-1.12.2/  找到可执行脚本编译文件configure
## 执行
```bash
./configure --with-stream
```
编译后需要进入root用户，需要往/user/local/写入
## 执行
```bash
make && make install
```
执行完后进入/user/local/nginx/conf/ 目录，修改nginx.conf文件
```
vim nginx.conf
```
## 添加
```conf
# nginx tcp loadbalace config
stream {
        upstream MyServer {
                server 127.0.0.0:6001 weight=1 max_fails=3 fail_timeout=30s;
                server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
        }

        server {
                proxy_connect_timeout 1s;
                #proxy_timeout 3s;
                listen 8000;
                proxy_pass MyServer;
                tcp_nodelay on;
        }
}
```
配置完成
## 启动
进入/user/local/sbin/ 目录
```
# 启动
./nginx
# 重新加载配置文件（增加服务器数量）
./nginx -s reload
# 关闭
./nginx -s stop