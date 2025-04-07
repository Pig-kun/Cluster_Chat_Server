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