# ChanneldUE和原生UE的差异
本章介绍了ChanneldUE和虚幻引擎自带的网络同步机制的差异。部分差异可以通过替代方案解决，但是部分差异目前还没有替代方案。如果你的游戏需要使用这些功能，请在[Issues](/issues)中提出。

## 条件属性复制
ChanneldUE尚不支持[条件属性复制](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/Actors/Properties/Conditions/)。

这意味着，所有发生变化的属性都会从UE服务器发送到channeld网关服务，并广播到所有订阅者。客户端会收到频道内所有玩家的属性变化，包括其它玩家的PlayerController和PlayerState的属性，但是不会进行处理，因为客户端不会创建其它玩家的PlayerController或PlayerState实例。这确实会造成一定的带宽浪费，以及潜在的安全性问题。基于channeld网关服务实现条件属性复制的功能正在计划中。

## 网络相关性和优先级
ChanneldUE部分支持[网络相关性和优先级](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/Actors/Relevancy/)。

## 网络更新频率
ChanneldUE尚不支持[网络更新频率](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/Actors/Properties/#%E6%95%B0%E6%8D%AE%E9%A9%B1%E5%8A%A8%E5%9E%8B%E7%BD%91%E7%BB%9C%E6%9B%B4%E6%96%B0%E9%A2%91%E7%8E%87)。

在服务器端，属性变化在每帧都会被同步到channeld网关服务。在客户端，收到的属性同步频率取决于订阅频道时设置的`FanOutIntervalMs`参数，默认为20ms，相当于`NetUpdateFrequency=50`。全局频道的同步频率可以在频道数据视图中修改`GlobalChannelFanOutIntervalMs`来调整；空间频道的同步频率以频道距离衰减，默认为50Hz（距离为0），20Hz（距离为1），10Hz（距离为2及以上）。若要修改空间频道的同步频率，需要修改channeld网关服务的代码[message_spatial.go](https://github.com/channeld/channeld/pkg/channeld/message_spatial.go)中的`spatialDampingSettings`。

## 网络裁剪距离
ChanneldUE不支持`Net Cull Distance Squared`，但是可以作为替代方案使用。

在原生UE中，一个客户端能够被同步到的空间范围由每个网络Actor的`Net Cull Distance Squared`属性来控制，默认为150米的平方。在ChanneldUE中，客户端能够接收到的最小同步范围是一个空间频道，在[入门指南](getting-started.md)使用的第三人称示例里，为20x20米。
可以修改channeld网关服务的配置[spatial_static_2x2.json](https://github.com/channeld/channeld/config/spatial_static_2x2.json)来调整该数值。具体配置方法见[配置channeld](config-channeld.md)文档。

客户端的兴趣范围（即客户端订阅的所有频道）可以包括多个空间频道。若要实现以玩家为中心的，半径为150米的球型兴趣范围，打开主菜单`编辑 -> 项目设置 -> 插件 -> Channeld -> Spatial -> Client Interest`，添加一个`Client Interest Preset`，设置`Area Type`为**Sphere**，`Radius`为**15000**。

## Replication Graph
ChanneldUE不支持[Replication Graph](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/ReplicationGraph/)，但是可以作为替代方案使用。

ChanneldUE本身通过以下方式提升了UE服务器在网络同步方面的性能：

1. 为所有需要同步的C++和蓝图类生成C++同步代码，相比于原生UE的反射机制，大幅提升了性能
2. 将属性同步和RPC消息的广播放到了channeld网关服务中，减少了UE服务器因遍历而产生的CPU开销
3. 将玩家能够接收到哪些对象同步的逻辑放到了channeld网关服务中，进一步减少了UE服务器的CPU开销

Replication Graph中将Actor分组或是建立特殊的列表，跟channeld中的频道的功能类似。通过将不同的Actor以特定的状态集和频道同步到不同的频道，可以实现类似Replication Graph的功能。


## 框架和系统的跨服支持
虚幻引擎假设所有的模拟都在一个服务器上发生，所以没有跨服的概念。UE中的Gameplay框架、物理系统、AI系统、Gameplay技能系统等，都是基于这个逻辑实现的。然而在channeld中，如果使用了空间频道，模拟的对象可能在多个服务器之间发生迁移。ChanneldUE目前仅实现了Gameplay框架的跨服迁移（PlayerController，PlayerState等），其它框架和系统需要额外的集成才能支持跨服迁移，否则会因为丢失状态而发生难以预料的结果。

这就意味着，在实现集成之前，你的游戏需要在设计上限制这些框架和系统发生跨服迁移，例如增加空气墙以防止将刚体推过服务器之间的边界，或者只允许AI在服务器内的地图区域进行巡逻。
