# AsioChatBackend

`AsioChatBackend` 是一个基于 `Boost.Asio` 的跨平台 C++ 聊天后端，支持 `Windows` 和 `Linux`。当前版本支持房间聊天、私聊、MySQL 离线消息、Redis 在线状态，以及可接 `local`、`OpenAI`、`DeepSeek`、`Ollama` 的房间 AI agent。

## 项目亮点

- 基于 `Boost.Asio` 实现异步 TCP 服务端和命令行客户端
- 使用 `4` 字节长度头 + `JSON` 消息体协议处理 TCP 粘包/拆包
- 支持房间聊天、私聊、在线用户列表、房间列表、心跳保活
- 支持 `MySQL` 离线私聊消息存储与登录补发
- 支持 `Redis` 在线状态维护与 TTL 刷新
- 服务端采用多线程 `io_context` worker 模型提升连接处理能力
- 引入 `BlockingQueue + ThreadPool` 处理阻塞型业务任务，避免数据库和 Redis 操作阻塞网络线程
- 使用 `config.json` 管理数据库、Redis、端口和运行时命令路径
- 在存储层中引入 `Strategy + Factory + Null Object + Singleton` 模式，方便按配置切换实现

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
        |-- room_ai_agent_service.cpp
        |-- room_ai_agent_service.hpp
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
- `Boost`，至少包含 `system` 和 `json`
- 如果启用 `mysql` 离线存储，需要可访问的 MySQL 服务和 `mysql` 命令行客户端
- 如果启用 `redis` 在线状态，需要可访问的 Redis 服务和 `redis-cli`
- 如果启用真实 AI provider，需要本机可用的 `curl`

如果 `mysql`、`redis-cli`、`curl` 不在系统 `PATH` 中，也可以在配置文件里写绝对路径。

## 编译方式

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Debug
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

2. 按本机环境修改 `config.json`

3. 启动服务端：

```powershell
cd /d E:\QtProject\project\project_backup_before_rebuild
.\build\Debug\asiochat_server.exe config.json
```

4. 再开一个终端启动客户端：

```powershell
cd /d E:\QtProject\project\project_backup_before_rebuild
.\build\Debug\asiochat_client.exe 127.0.0.1 5555
```

## 配置文件说明

推荐先参考 [config.example.json](/e:/QtProject/project/project_backup_before_rebuild/config.example.json)。

示例：

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
  "online_store": {
    "backend": "none"
  },
  "ai_agents": {
    "default": {
      "enabled": true,
      "provider": "deepseek",
      "bot_name": "room-bot",
      "persona": "你是当前房间里的 AI 助手，负责欢迎用户、简要回答问题，并引导大家把需求说清楚。",
      "welcome_message": "room-bot 已加入当前房间。你们在这个房间里用 /pm ai 或 /pm 小d 提问时，我会公开回复。",
      "model": "deepseek-chat",
      "api_key_env": "DEEPSEEK_API_KEY",
      "base_url": "https://api.deepseek.com",
      "endpoint": "/chat/completions",
      "curl_executable": "curl",
      "history_messages": 6,
      "timeout_seconds": 30,
      "max_tokens": 300,
      "temperature": 0.7
    },
    "rooms": {
      "interview": {
        "enabled": true,
        "bot_name": "interview-bot",
        "persona": "你是 interview 房间里的面试辅导助手，回答更偏结构化。"
      }
    }
  }
}
```

主要字段：

- `server.port`：监听端口
- `server.worker_threads`：网络 IO 线程数，填 `0` 时自动按硬件并发选择
- `server.business_threads`：业务线程池大小，负责数据库、Redis、AI 等阻塞型任务
- `offline_store.backend`：可选 `mysql`、`file`、`none`
- `offline_store.file_path`：当 `backend=file` 时使用的本地文件路径
- `mysql.*`：MySQL 连接信息与客户端路径
- `online_store.backend`：可选 `redis`、`none`
- `redis.*`：Redis 连接信息与 `redis-cli` 路径
- `ai_agents.default`：所有房间通用的 AI 配置
- `ai_agents.rooms.<room>`：某个房间的覆盖配置；如果没配，就继承 `ai_agents.default`
- `enabled`：是否启用 AI
- `provider`：可选 `local`、`openai`、`deepseek`、`ollama`
- `bot_name`：bot 显示名称
- `persona`：system prompt / 角色设定
- `welcome_message`：用户进入房间时看到的 bot 欢迎语
- `reply_template`：本地 bot 使用的模板，支持 `{room}`、`{user}`、`{message}`、`{persona}`
- `api_key`：直接把 API key 写进配置文件，最方便但安全性较差
- `api_key_env`：从环境变量读取 API key，更推荐
- `model`：模型名
- `base_url`：provider 基础地址
- `endpoint`：接口路径
- `curl_executable`：`curl` 可执行文件路径，默认可写 `curl`
- `history_messages`：传给 AI 的最近上下文条数
- `timeout_seconds`：请求超时秒数
- `max_tokens`：最大输出 token
- `temperature`：采样温度

## AI Agent 使用说明

### 当前触发规则

房间 bot 不是“每条群消息都自动回复”。

只有当用户在当前房间输入：

```text
/pm ai 你的问题
/pm 小d 你的问题
```

bot 才会响应。

行为是：

- `ai` 不区分大小写
- `小d` 作为中文别名也可用
- 这条消息不会走真正私聊
- 服务端会把它当成“在当前房间 @机器人”的公开消息处理
- 房间里所有人都能看到你的提问和 bot 的回复

例如：

```text
/login alice
/join cpp
/pm ai 你的模型用的是？
/pm 小d 介绍一下自己
```

普通群消息例如 `/msg hello` 不会触发 bot。

### 所有房间共用一个 DeepSeek AI

如果你希望“只要是个房间都能用 DeepSeek 的 AI”，最简单的方式就是只配 `ai_agents.default`：

```json
"ai_agents": {
  "default": {
    "enabled": true,
    "provider": "deepseek",
    "bot_name": "room-bot",
    "persona": "你是当前房间里的 AI 助手，负责欢迎用户、简要回答问题，并引导大家把需求说清楚。",
    "welcome_message": "room-bot 已加入当前房间。你们在这个房间里用 /pm ai 或 /pm 小d 提问时，我会公开回复。",
    "model": "deepseek-chat",
    "api_key_env": "DEEPSEEK_API_KEY",
    "base_url": "https://api.deepseek.com",
    "endpoint": "/chat/completions",
    "curl_executable": "curl"
  },
  "rooms": {}
}
```

这样 `lobby`、`cpp`、`interview` 等任意房间都会继承这套 AI 配置。

如果某个房间想单独换 bot 名、欢迎语或 persona，再给它额外写一个 `ai_agents.rooms.<room>` 覆盖即可。

### Provider 选择

- `local`：适合本地演示，不依赖外网
- `openai` / `deepseek` / `ollama`：走统一的 OpenAI-compatible chat completions 接口
- 当前实现通过 `curl` 发起 HTTP 请求，所以需要本机能执行 `curl`

### OpenAI 示例

```json
"provider": "openai",
"model": "your-openai-model",
"api_key_env": "OPENAI_API_KEY",
"base_url": "https://api.openai.com",
"endpoint": "/v1/chat/completions"
```

### Ollama 示例

```json
"provider": "ollama",
"model": "your-ollama-model",
"base_url": "http://127.0.0.1:11434",
"endpoint": "/v1/chat/completions"
```

## 支持的客户端命令

```text
/login alice
/join cpp
/msg hello everyone
/pm bob hi, this is a private message
/pm ai 介绍一下自己
/pm 小d 这个房间适合聊什么？
/users
/rooms
/ping
/quit
```

## MySQL 离线消息说明

- 当私聊目标用户在线时，消息实时投递
- 当私聊目标用户不在线时，消息进入业务线程池并写入离线存储
- 目标用户下次使用相同用户名登录后，服务端会异步拉取并补发离线消息
- 如果 `offline_store.backend=none`，则不会持久化离线私聊

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
