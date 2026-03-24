# AsioChatBackend

`AsioChatBackend` 是一个基于 `Boost.Asio` 的跨平台 C++ 聊天后端项目，支持在 `Windows` 和 `Linux` 上运行。当前版本支持 `MySQL` 离线消息存储和 `Redis` 在线状态管理，并且已经改成配置文件驱动，方便他人克隆仓库后按自己的环境修改配置直接运行。

## 项目亮点

- 基于 `Boost.Asio` 实现跨平台 TCP 服务端和命令行客户端
- 使用 `async_accept`、`async_read`、`async_write` 实现异步非阻塞通信
- 使用 `4 字节长度字段 + JSON 消息体` 协议解决 TCP 粘包/拆包问题
- 支持 `MySQL` 离线私聊消息存储与登录补发
- 支持 `Redis` 在线状态维护与 TTL 刷新
- 服务端采用多线程 `io_context` worker 模型提升连接处理能力
- 引入 `BlockingQueue + ThreadPool` 处理阻塞型业务任务，避免数据库和 Redis 操作阻塞网络线程
- 使用 `config.json` 管理数据库、Redis、端口和运行时命令路径
- 在存储层中引入 `Strategy + Factory + Null Object + Singleton` 模式，方便按配置切换实现
- 适合写进 `C++ 后端开发实习` 简历

## 项目结构

```text
.
|-- CMakeLists.txt
|-- README.md
|-- config.example.json
`-- src
    |-- client
    |   |-- chat_client.cpp
    |   |-- chat_client.hpp
    |   `-- main.cpp
    |-- common
    |   |-- protocol.cpp
    |   `-- protocol.hpp
    `-- server
        |-- app_config.cpp
        |-- app_config.hpp
        |-- blocking_queue.hpp
        |-- business_executor.cpp
        |-- business_executor.hpp
        |-- chat_room.cpp
        |-- chat_room.hpp
        |-- chat_server.cpp
        |-- chat_server.hpp
        |-- chat_session.cpp
        |-- chat_session.hpp
        |-- offline_message_store.cpp
        |-- offline_message_store.hpp
        |-- online_status_store.cpp
        |-- online_status_store.hpp
        |-- store_factory.cpp
        |-- store_factory.hpp
        |-- thread_pool.cpp
        |-- thread_pool.hpp
        `-- main.cpp
```

## 环境要求

你需要准备：

- `CMake 3.16+`
- 支持 `C++17` 的编译器
- `Boost`，至少包含 `system` 和 `json` 组件
- 已安装并可访问的 `mysql` 命令行客户端
- 已安装并可访问的 `redis-cli` 命令行客户端
- 可连接的 MySQL / Redis 服务

如果 `mysql` 或 `redis-cli` 不在系统 `PATH` 中，也可以在配置文件里写绝对路径。

## 编译方式

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### Linux

```bash
cmake -S . -B build
cmake --build build -j
```

## 第一次运行

1. 复制配置模板：

```powershell
Copy-Item config.example.json config.json
```

`config.json` 是本地运行配置，建议不要提交到仓库，仓库里只保留 `config.example.json`。

2. 按你自己的环境修改 `config.json`

3. 启动服务端：

```powershell
.\build\Release\asiochat_server.exe config.json
```

4. 启动客户端：

```powershell
.\build\Release\asiochat_client.exe 127.0.0.1 5555
```

## 配置文件示例

```json
{
  "server": {
    "port": 5555,
    "worker_threads": 4,
    "business_threads": 2
  },
  "offline_store": {
    "backend": "mysql",
    "file_path": "data/offline_messages.json"
  },
  "mysql": {
    "host": "127.0.0.1",
    "port": 3306,
    "user": "root",
    "password": "your_mysql_password",
    "database": "asiochat",
    "mysql_executable": "mysql"
  },
  "online_store": {
    "backend": "redis"
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "password": "",
    "ttl_seconds": 120,
    "key_prefix": "asiochat:online:",
    "redis_cli_executable": "redis-cli"
  }
}
```

字段说明：

- `server.port`：服务端监听端口
- `server.worker_threads`：网络 IO 线程数，填 `0` 时会自动按硬件并发数选择
- `server.business_threads`：业务线程池大小，用于执行离线消息存储、在线状态刷新等阻塞型任务
- `offline_store.backend`：离线消息存储后端，可选 `mysql`、`file`、`none`
- `offline_store.file_path`：选择 `file` 后端时的本地持久化文件路径
- `mysql.*`：MySQL 连接信息与 `mysql` 客户端路径
- `online_store.backend`：在线状态后端，可选 `redis`、`none`
- `redis.*`：Redis 连接信息与 `redis-cli` 路径

## 支持的客户端命令

```text
/login alice
/join cpp
/msg hello everyone
/pm bob hi, this is a private message
/users
/rooms
/ping
/quit
```

## MySQL 离线消息说明

- 当私聊目标用户在线时，消息直接实时投递
- 当私聊目标用户不在线时，消息会先进入业务线程池，再写入 MySQL 的 `offline_messages` 表
- 目标用户下次使用相同用户名登录后，服务端会异步拉取并补发离线消息
- 补发完成后，对应离线记录会从 MySQL 中删除
- 服务端启动时会自动创建配置中指定的数据库和 `offline_messages` 表

## Redis 在线状态说明

- 用户登录后，服务端会异步写入 Redis 在线状态键
- 用户发送心跳或正常收发消息时，会按节流策略刷新在线状态 TTL
- 用户主动退出或连接断开后，服务端会异步删除对应在线状态键

示例 Key：

```text
asiochat:online:alice
```

## 协议设计

项目底层使用“长度前缀 + JSON 消息体”的应用层协议进行消息传输：

- 前 `4` 个字节表示消息体长度
- 后续为完整的 `JSON` 数据
- 接收端先读取固定长度消息头，再按长度读取消息体
- 可以正确处理 TCP 粘包和拆包问题

协议格式：

```text
[4字节消息长度][JSON消息体]
```

## 设计模式与并发设计

- `Strategy`：`OfflineMessageStore` 和 `OnlineStatusStore` 抽象出统一接口，业务层只依赖抽象，不依赖具体存储实现。
- `Factory`：通过 `store_factory` 按配置创建 `MySQL`、`File`、`Redis` 或空实现，启动入口不再直接耦合具体类。
- `Null Object`：提供 `NullOfflineMessageStore` 和 `NullOnlineStatusStore`，在不接数据库或 Redis 时仍能保持服务端逻辑稳定。
- `Singleton`：提供可复用的 `Singleton<T>` 模板基类，具体类通过 `friend class Singleton<T>` 开放实例化权限；`BusinessExecutor` 基于这套模板统一管理全局业务线程池。
- `Message Queue`：`BlockingQueue<std::function<void()>>` 作为线程池任务队列，把离线消息落库、Redis 状态刷新等任务排队处理。
- `Thread Pool`：`ThreadPool` 持续消费任务队列，将阻塞型业务逻辑与 `io_context` 网络线程分离。

## 可分发性说明

当前版本已经去掉了仓库里的数据库地址、密码、Redis 端口、命令行工具绝对路径等硬编码内容。其他人从 Git 克隆仓库后，只需要：

1. 安装 Boost、MySQL、Redis
2. 配好自己的 `config.json`
3. 根据环境选择后端，比如本地演示可用 `file + none`，完整版可用 `mysql + redis`
4. 保证 `mysql` / `redis-cli` 在 PATH 中，或在配置里写路径
5. 执行 `cmake` 和运行服务端

就可以按自己的环境启动项目。


## 后续可扩展方向

- MySQL 用户表、好友关系表、聊天记录表
- Redis 发布订阅做跨进程消息转发
- Token 登录鉴权
- 异步日志系统
- 压测脚本和性能指标采集

