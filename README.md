# AsioChat

AsioChat 是一个基于 C++17 和 Boost.Asio 的命令行聊天室项目，包含服务端与客户端，支持房间聊天、私聊、离线消息、在线状态，以及按房间配置的 AI 助手。

## 功能

<<<<<<< HEAD
- 基于 `Boost.Asio` 实现异步 TCP 服务端和命令行客户端
- 使用 `4` 字节长度头 + `JSON` 消息体协议处理 TCP 粘包/拆包
- 支持房间聊天、私聊、在线用户列表、房间列表、心跳保活
- 支持 `MySQL` 离线私聊消息存储与登录补发
- 支持 `Redis` 在线状态维护与 TTL 刷新
- 服务端采用多线程 `io_context` worker 模型提升连接处理能力
- 引入 `BlockingQueue + ThreadPool` 处理阻塞型业务任务，避免数据库和 Redis 操作阻塞网络线程
- 使用 `config.json` 管理数据库、Redis、端口和运行时命令路径
- 在存储层中引入 `Strategy + Factory + Null Object + Singleton` 模式，方便按配置切换实现
=======
- 多房间聊天与私聊
- 用户注册、登录、切换房间
- 离线消息存储
- 在线状态维护
- 房间 AI 助手回复
- 基于 `4` 字节长度头 + `JSON` 消息体的通信协议
>>>>>>> d9fcb899107ed50a9c078673b6dd6b95af1175e6

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

<<<<<<< HEAD
## Redis 在线状态说明

- 用户登录后，服务端可异步写入 Redis 在线状态键
- 用户发送心跳或正常收发消息时，会按节流策略刷新在线状态 TTL
- 用户主动退出或连接断开后，服务端异步删除对应在线状态键
- 如果 `online_store.backend=none`，则关闭在线状态持久化

示例 Key：

```text
asiochat:online:alice
```

## 协议设计

项目底层使用“长度前缀 + JSON 消息体”的应用层协议：

- 前 `4` 个字节表示消息体长度
- 后续为完整 `JSON` 数据
- 接收端先读取固定长度头，再按长度读取消息体
- 可以正确处理 TCP 粘包和拆包问题

协议格式：

```text
[4字节消息长度][JSON消息体]
```

## 设计与并发

- `Strategy`：离线消息和在线状态都通过统一抽象接口接入
- `Factory`：根据配置创建 `MySQL`、`File`、`Redis` 或空实现
- `Null Object`：数据库或 Redis 关闭时仍保持业务逻辑稳定
- `Singleton`：`BusinessExecutor` 统一管理全局业务线程池
- `BlockingQueue`：业务任务排队，避免阻塞网络线程
- `ThreadPool`：消费阻塞型任务，如离线存储、Redis 刷新、AI 请求

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
=======
## 注意事项

- `config.json` 建议本地保留，不要提交到仓库
- 如果把 `api_key` 直接写进配置文件，注意不要泄露或提交到 Git
- 如果 key 已经暴露，建议立刻到 provider 后台旋转并替换
- Windows 下如果 `build\Debug\asiochat_server.exe` 正在运行，重新编译该目标会因 `LNK1168` 失败，先关掉旧进程再编译
=======
- `app.cfg` 适合本地运行使用
- `app.example.cfg` 适合作为模板保留在仓库中
- 不要把真实数据库、Redis 或 AI 密钥提交到仓库
>>>>>>> d9fcb899107ed50a9c078673b6dd6b95af1175e6
