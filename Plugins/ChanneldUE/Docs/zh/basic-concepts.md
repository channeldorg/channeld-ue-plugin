# 基本概念
## channeld网关服务
channeld网关服务是一个独立于UE客户端和服务端运行的进程。理论上，所有网络流量都会经由channeld进行转发或广播。

强烈建议将channeld和游戏服务器就近部署。如果是在同一集群，不同的物理服务器上，channeld和游戏服务器之间的网络延迟通常只有1-2毫秒；如果是在同一物理服务器上，其延迟会低至几十微秒。真正要考虑的是channeld本身的扇出机制带来的同步延迟。如果订阅时设置的`FanOutIntervalMs`太高，理论上，服务器发给channeld同步消息后，最差的情况下，`FanOutIntervalMs`时间后channeld才会给客户端发送这些同步。在FPS这种对延迟高敏感的游戏项目中，建议将`FanOutIntervalMs`设置为10或更小。

```
注意：FanOutIntervalMs越小，频道的CPU负载越大。
```

channeld网关服务使用Go语言编写。ChanneldUE插件的`Setup.bat`初始化脚本会自动克隆[channeld的代码仓库](https://github.com/channeld/channeld)到插件的`Source/ThirdParty/channeld`目录。如果在运行`Setup.bat`之间已经克隆了channeld的代码仓库，并配置了`%CHANNELD_PATH%`环境变量，则会跳过自动克隆。

## 连接
对于channeld而言，无论是客户端还是服务端，都是一种连接。channeld支持TCP、KCP、WebSocket等协议；目前ChanneldUE的客户端和服务端都通过TCP连接channeld。

每个连接都有一个uint32类型的ID，作为唯一标识。

连接层在ChanneldUE中被封装到了`UChanneldConnection`类。

## 消息
channeld接收和发送的消息基于[Google Protocol Buffers](https://protobuf.dev/)（简称Protobuf）。每个数据包可以包含一到多个消息。每个消息都必须指定要发送到的频道。

channeld的内部消息比较简单，主要包括：
- 频道的创建，删除，查询
- 频道的订阅和退订
- 频道数据更新
- 空间频道相关

channeld支持扩展消息类型，默认100及以上为**用户消息**。ChanneldUE增加了以下用户消息类型：
- 创建和销毁对象
- RPC
- 跨服相关

## 频道
频道包含一个频道所有者连接，一份频道数据，和多个订阅连接。

频道所有者往往是频道的创建者。它是发往频道的用户消息的接收者，也就是说，频道接收到消息后，如果不是内部消息，都会转发到创建该频道的服务器（或客户端）。

频道数据是一个[Any](https://protobuf.dev/programming-guides/proto3/#any)类型的Protobuf消息，它内部的数据结构由项目的开发者定义。在ChanneldUE中，该数据结构默认由代码生成工具定义。一个典型的频道数据消息定义是这样的：
```protobuf
message DefaultChannelData {
    unrealpb.GameStateBase gameState = 1;
    map<uint32, unrealpb.ActorState> actorStates = 2;
    map<uint32, unrealpb.PawnState> pawnStates = 3;
    map<uint32, unrealpb.CharacterState> characterStates = 4;
    map<uint32, unrealpb.PlayerState> playerStates = 5;
    map<uint32, unrealpb.ControllerState> controllerStates = 6;
    map<uint32, unrealpb.PlayerControllerState> playerControllerStates = 7;
    map<uint32, unrealpb.ActorComponentState> actorComponentStates = 8;
    map<uint32, unrealpb.SceneComponentState> sceneComponentStates = 9;
    // ...更多状态定义
}
```

频道维护一份订阅列表，包括每个订阅的连接以及连接的订阅参数。
频道数据发生变化后，会根据订阅参数中的频率，依次将累积的更新发送到所有订阅的连接。这个过程叫做扇出（fan-out）。

## 频道数据中的状态
状态是频道数据中更细粒度的概念，一个状态与一个同步Actor对应。事实上，一个同步Actor的实例往往包含多个状态，比如一个Character会包含一个CharacterState，一个PawnState，以及一个ActorState。你会发现这三个状态恰好对应从ACharacter往上的三个基类。每个状态只关心对应基类的同步属性。通过这种方式，可以将状态自由地组合来实现任何Actor的同步，而不用担心同步代码的重复或同步属性的冗余。

## 同步器
每个同步器负责一个状态的同步。Channeld同步组件（`ChanneldReplicationComponent`）在`BeginPlay`时会根据所属Actor的类型，以及所有开启了同步的组件的类型，创建出对应的同步器。在每个同步帧中，每个同步器会检查同步属性是否发生了变化，如果有变化，就会将变化的属性写入到状态中。同步组件负责将差异性的状态（Delta State）写入到频道数据中。

## NetId
在上面的频道数据消息定义中可以看到，大部分状态都被放进了Map中，以一个uint32进行索引。这个uint32就是NetId，它是一个全局唯一的ID，用于标识一个同步Actor。在ChanneldUE中，NetId等同于[FNetworkGUID](https://docs.unrealengine.com/4.27/en-US/API/Runtime/Core/Misc/FNetworkGUID/)，但是在原生的UE上做了修改：它的高13位是连接ID，低19位才是自增的ID。这样做的目的是避免不同服务器上生成的NetworkGUID冲突。

没有被放进Map的状态会有一个固定NetId，例如GameStateBase的NetId就是1。
