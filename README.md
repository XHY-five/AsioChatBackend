# AsioChat

AsioChat 是一个基于 C++17 与 Boost.Asio 的命令行聊天室项目，仓库中同时包含客户端与服务端。项目支持多房间聊天、私聊、离线消息、在线状态维护，以及按房间配置的 AI 助手能力。

## 项目简介

<<<<<<< Updated upstream
- 基于 `Boost.Asio` 实现异步 TCP 服务端和命令行客户端
- 使用 `4` 字节长度头 + `JSON` 消息体协议处理 TCP 粘包/拆包
- 支持房间聊天、私聊、在线用户列表、房间列表、心跳保活
- 支持 `MySQL` 离线私聊消息存储与登录补发
- 支持 `Redis` 在线状态管理，也支持 `none` 模式关闭
- 支持为所有房间统一配置一个 AI，也支持按房间单独覆盖
- AI 回复通过业务线程池异步执行，不阻塞网络线程
- Windows 控制台已切换到 `UTF-8`，中文消息显示正常
=======
服务端通过 TCP 接收带长度头的 JSON 消息帧，负责用户登录、房间切换、消息分发、离线消息存储和在线状态维护。当前实现已经接入 MySQL 用户与离线消息存储、Redis 在线状态存储，并支持通过兼容 OpenAI 风格接口的模型服务生成房间 AI 回复。
>>>>>>> Stashed changes

客户端是一个轻量级的终端程序，主要面向命令行交互场景。它支持注册、登录、切换房间、私聊、心跳保活，以及连接断开和空闲超时后的提示处理。

## 目录结构

项目按头文件与实现文件分层组织：

- `include/asiochat/client/`：客户端对外头文件
- `include/asiochat/common/`：客户端与服务端共用的协议定义
- `include/asiochat/server/`：服务端核心模块头文件
- `src/client/`：客户端实现与入口
- `src/common/`：公共协议序列化与解析实现
- `src/server/`：服务端实现与入口
- `cmake/`：按平台拆分的依赖发现与三方库配置脚本
- `app.cfg`：本地运行时使用的配置文件
- `app.example.cfg`：演示用途的配置样例文件

## 核心能力

- 基于 TCP 的多房间聊天
- 用户注册、登录与房间切换
- 公聊与私聊消息分发
- 离线消息持久化
- Redis 在线状态维护
- 可配置的空闲超时检测
- 房间级 AI 助手欢迎语与自动回复

## 构建结构

工程使用 CMake 管理构建，当前拆分为公共模块、客户端核心模块和服务端核心模块，再分别生成 `asiochat_client` 与 `asiochat_server` 可执行文件。依赖发现已按 Windows 与 Linux 分离，便于在不同平台下接入本地三方库目录。

## 配置说明

<<<<<<< Updated upstream
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

## 注意事项

- `config.json` 建议本地保留，不要提交到仓库
- 如果把 `api_key` 直接写进配置文件，注意不要泄露或提交到 Git
- 如果 key 已经暴露，建议立刻到 provider 后台旋转并替换
- Windows 下如果 `build\Debug\asiochat_server.exe` 正在运行，重新编译该目标会因 `LNK1168` 失败，先关掉旧进程再编译
=======
仓库中的 `app.cfg` 用于实际运行时读取，`app.example.cfg` 用于展示配置结构和字段含义。演示文件中的数据库口令、Redis 口令和 AI 接口密钥均为占位内容，不对应真实环境。
>>>>>>> Stashed changes
