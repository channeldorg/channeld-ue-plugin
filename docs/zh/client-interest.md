# 客户端兴趣
## 简介
客户端兴趣是指客户端玩家能够看到，并被服务器同步的游戏内信息，例如：
- 附近玩家的虚拟形象、位置、动作等
- NPC的位置、血量等
- 重要的世界事件，如击杀世界Boss

在不同的游戏类型、游戏模式，或玩家状态下，客户端的兴趣可能会有很大差别，例如：
- 在俯视角中，可以看到大面积的城市或战场，也可以拉近摄像机看到地面上的细节
- 在第三人称摄像机中，可以看到玩家角色前方的信息，也可以旋转摄像机，看到玩家角色身后的信息
- 在射击游戏中，开启枪械的瞄准镜，可以看到很远处的细节，但是此时看不到身后的信息

虚幻引擎中有多种方式来实现单服务器中的兴趣管理，而ChanneldUE在支持部份虚幻引擎原生特性（如：网络相关性，网络更新频率。具体见“[ChanneldUE和原生UE的差异](zh/native-ue-comparison.md)”）的基础上，实现了一套全新的框架，来支持大世界多服架构。

## ChanneldUE的兴趣管理
ChanneldUE的大世界多服架构的底层核心是[空间频道](zh/use-spatial-channel.md#71空间频道简介)。整个世界被划分为大小相同的单元格，每个单元格即是一个空间频道，也是ChanneldUE中的最小兴趣单元。换句话说，一个客户端要么订阅到了某个空间频道（单元格），能够接收频道数据更新；要么没有订阅，不接收更新。所以空间频道的单元格大小是开发者需要认真考虑的参数。

以UE默认的150米同步范围为参考，空间频道单元格大小的建议范围是50-100米（边长）。更小的单元格可以让迁移（玩家每次穿过一个空间频道时都会订阅和退订一些空间频道）更平滑，但是迁移频率越高，channeld网关服务的负载越高。

客户端兴趣的更新默认只在迁移时发生。开发者也可以开启兴趣跟随，即达到条件后（如移动了多少距离）立刻更新客户端兴趣。这样做也会增加系统负载，所以建议只在兴趣范围很敏感的场景，如狙击镜瞄准时，才开启兴趣跟随。下面截图演示了如何通过蓝图开启兴趣跟随：

![](../images/interest_follow_actor.png)
> 注意：兴趣管理的逻辑由UE服务端控制，而channeld网关服务最终进行计算并生效。所以上图的逻辑应该发生在UE服务端，而不是客户端。

ChanneldUE内置如下类型的兴趣范围（AOI，Area Of Interest）：
- 立方体区域。以玩家为中心的立方体区域内的所有空间频道。跟世界坐标轴平行，无法旋转。适用于俯视角游戏。
- 球形区域。以玩家为中心的球形区域内的所有空间频道。适用于第三人称游戏。
- 锥形区域。以玩家为中心的锥形区域内的所有空间频道。适用于望远镜或瞄准镜的场景。
- 固定位置点。位置点会被映射到空间频道，所以相当于订阅到固定的空间频道。该兴趣范围可用于玩家关注地图中某个特定的区域（如夺旗模式中的旗帜），或由程序动态设置区域。

下表列出了兴趣范围的参数以及其适用的兴趣范围类型：

| 参数 | 默认值 | 说明 | 适用类型 |
| ------ | ------ | ------ | ------ |
| `Preset Name` | 空 | 预设名称。用于调用`ClientInterestManager`的API时进行查找 | 所有类型 |
| `Activete By Default` | true | 是否默认激活 | 所有类型 |
| `Min Distance To Trigger Update` | 100.0 | 开启兴趣跟随后，触发更新的最小距离 | 立方体，球形，锥形 |
| `Spots And Dists` | 空 | 位置点以及对应的空间距离表。距离越近，更新的频道越高。 | 固定位置点 |
| `Extent` | X=15000.0, Y=15000.0, Z=15000.0 | 立方体边长的一半 | 立方体 |
| `Radius` | 15000.0 | 半径 | 球形，锥形 |
| `Angle` | 120.0 | 锥形的顶角度数(0-360)。应该比摄像机视角略大，以保证玩家角色在旋转时，观察到的对象能够及时出现 | 锥形 |

客户端的实际兴趣范围是上述的兴趣范围的**组合**。开发者也可以自行扩展兴趣类型。要添加、删除或者修改兴趣范围，需要打开主菜单`编辑 -> 项目设置 -> 插件 -> Channeld`，在`Spatial -> Client Interest -> Client Interest Presets`中添加一个预设。下图示例添加了一个1000立方米范围的兴趣，并设置为默认关闭：

![](../images/add_box_interest.png)

## 兴趣范围的同步频率
所有兴趣范围的同步频率，即对应的空间频道同步数据更新到客户端的频率，由channeld中的一组参数统一控制：
```go
var spatialDampingSettings []*SpatialDampingSettings = []*SpatialDampingSettings{
	{
		MaxDistance:      0,
		FanOutIntervalMs: 20,
	},
	{
		MaxDistance:      1,
		FanOutIntervalMs: 50,
	},
	{
		MaxDistance:      2,
		FanOutIntervalMs: 100,
	},
}
```

这段代码表示空间频道的同步频率以频道距离衰减，默认为50Hz（距离为0，即同一空间频道），20Hz（距离为1个空间频道），10Hz（距离为2个及以上空间频道）。目前这段参数只能通过修改代码[message_spatial.go](/../../../channeld/blob/master/pkg/channeld/message_spatial.go)来调整。

## 扩展兴趣类型
您的兴趣范围类需要继承自`UAreaOfInterestBase`（不可跟随）或`UPlayerFollowingAOI`（可跟随），并实现虚方法：
```cpp
virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;
```
`Query`参数是最终发往Channeld的空间兴趣查询对象；`PawnLocation`和`PawnRotation`参数是玩家角色的位置和旋转。您需要在`SetSpatialQuery`中设置查询的参数，如中心点和半径等。下面的示例代码是一个球形兴趣范围的实现：
```cpp
ChanneldUtils::SetSpatialInfoPB(Query->mutable_sphereaoi()->mutable_center(), PawnLocation);
Query->mutable_sphereaoi()->set_radius(Radius);
```
然后，在运行时，需要调用`UClientInterestManager::AddAOI`将其加入兴趣范围管理。

对于可跟随的兴趣范围，通过重写`TickQuery`方法，可以定义空间兴趣查询对象被在什么情况下被更新。

## 测试兴趣范围
观察客户端兴趣范围的主要方式，是使用ChanneldUE内置的空间频道可视化工具。在开启后，该工具能以网格的形式显示所有空间频道，位于不同服务器的空间以不同颜色表示；同时，在当前客户端兴趣范围内（即：已订阅）的空间频道会高亮显示。

下面的动图展示了开启了跟随的锥形区域的兴趣范围。当玩家角色移动和旋转时，兴趣范围会跟随变化：

![](../images/cone_interest.gif)
