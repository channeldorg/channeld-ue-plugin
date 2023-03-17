# ChanneldUE Plugin

The open source plugin that enables distributed simulation with Unreal Engine's dedicated servers. 

ChanneldUE是为虚幻引擎专用服务器提供分布式模拟能力的开源插件。

## Features
- Easily increase the maximum capacity of a single UE dedicated server to 100-200 players.
- Can combine multiple dedicated servers into one large world, supporting thousands of players online concurrently.
- Support a variety of application scenarios, including seamless large worlds, as well as traditional multi-room architecture and relay server architecture.
- Out-of-the-box synchronization solution that seamlessly integrates with the native UE's networking framework.
- Agile and extensible client interest management mechanism.
- Support cross-server interaction (currently only support cross-server RPC; cross-server for Physics, AI, GAS and other systems requires additional integration).
- Cloud-based dynamic load balancing can greatly save server costs (under development).

## 特性
- 轻松将单个UE专用服务器的最大承载人数提升到100-200人
- 可以将多个UE专用服务器组合成一个大世界，支持上千玩家同时在线
- 支持多种应用场景，包括无缝大世界，以及传统的多房间架构和中转服务器架构
- 开箱即用的同步方案，与原生UE的开发方式无缝集成
- 灵活且可扩展的客户端兴趣管理机制
- 支持跨服交互（目前仅支持跨服RPC；物理、AI、GAS等系统的跨服需要额外集成）
- 基于云计算的动态负载均衡能够极大节省服务器成本（开发中）
## 链接
[快速开始](Docs/zh/installation.md)

[channeld代码仓库](https://github.com/metaworking/channeld)