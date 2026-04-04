# AsioChat

AsioChat 是一个基于 C++17 和 Boost.Asio 的命令行聊天室项目，包含服务端与客户端，支持房间聊天、私聊、离线消息、在线状态，以及按房间配置的 AI 助手。

## 功能

- 多房间聊天与私聊
- 用户注册、登录、切换房间
- 离线消息存储
- 在线状态维护
- 房间 AI 助手回复
- 基于 `4` 字节长度头 + `JSON` 消息体的通信协议

## 目录

- `include/asiochat/`：对外头文件
- `src/client/`：客户端实现
- `src/server/`：服务端实现
- `src/common/`：共享协议实现
- `cmake/`：构建与依赖配置
- `app.example.cfg`：配置示例
- `app.cfg`：运行时配置

## 构建

### Windows

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

### Linux

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

服务端启动时会读取当前目录下的 `app.cfg`。

1. 复制配置模板

```powershell
Copy-Item app.example.cfg app.cfg
```

2. 按本地环境修改 `app.cfg`

3. 启动服务端

```powershell
.\build\Debug\asiochat_server.exe
```

4. 启动客户端

```powershell
.\build\Debug\asiochat_client.exe 127.0.0.1 5555
```

客户端默认连接 `127.0.0.1:5555`。

## 配置

项目使用 `key=value` 形式的 `app.cfg` 配置文件，示例见 [app.example.cfg]。

常用配置项：

- `server.port`：服务端端口
- `executor.business_threads`：业务线程数
- `storage.offline_message_store`：离线消息存储，支持 `file`、`mysql`、`none`
- `storage.online_status_store`：在线状态存储，支持 `memory`、`redis`
- `storage.user_store`：用户存储，当前使用 `mysql`
- `mysql.*`：MySQL 连接配置
- `redis.*`：Redis 连接配置
- `ai_agents.default.*`：默认房间 AI 配置
- `ai_agents.rooms.<room>.*`：房间级 AI 覆盖配置

## 客户端命令

```text
/register alice secret
/login alice secret
/join cpp
/pm bob hello
/pm ai 介绍一下自己
/users
/rooms
/ping
/quit
```

普通文本会作为当前房间消息发送。

## 说明

- `app.cfg` 适合本地运行使用
- `app.example.cfg` 适合作为模板保留在仓库中
- 不要把真实数据库、Redis 或 AI 密钥提交到仓库
