# ChanneldUE插件设置

## ChanneldUE设置
ChannelUE插件在游戏运行时的相关设置。可以在`编辑 > 项目设置 > 插件 > ChanneldUE`中找到。

### 频道数据视图 `Channel Data View`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Channel Data View Class` | SingleChannelDataView | 客户端所使用的频道数据视图 |

### 网络传输 `Transport`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Enable Networking` | false | 是否启用ChanneldUE来代替UE默认的网络层 |
| `Channeld Ip for Client` | 127.0.0.1 | 客户端连接Channeld的IP地址 |
| `Channeld Port for Client` | 12108 | 客户端连接Channeld的端口。不会影响启动channeld时监听的端口 |
| `Channeld Ip for Server` | 127.0.0.1 | 服务器连接Channeld的IP地址 |
| `Channeld Port for Server` | 11288 | 服务器连接Channeld的端口。不会影响启动channeld时监听的端口 |
| `Use Receive Thread` | true | 是否使用独立线程接收来自channeld的数据 |
| `Disable Handshaking` | true | 是否跳过UE默认的握手过程。客户端在进入UE服务器之前，必须先经过channeld的连接和验证。**在UE5中，设置为false（即开启默认握手过程）会导致无法正常进入服务器。** |
| `Set Internal Ack` | true | 是否禁用UE内置的心跳机制。使用可靠连接（如TCP）时建议打开，以减小带宽消耗。 |
| `Rpc Redirection Max Retries` | true | RPC重定向的次数上限。当一个服务器无法处理RPC时，会尝试将RPC转发到可以处理的服务器。该值设为0时，不会发生重定向，会导致跨服移动会出现轻微的抖动；该值设得太高时，RPC可能会在服务器之间反复发送，导致网络阻塞 |

### 空间频道 `Spatial`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Player Start Locator Class` | PlayerStartLocator_ModByConnId | 玩家初始位置定位器 |
| `Enable Spatial Visualizer` | false | 是否启用空间频道可视化工具 |

#### 客户端兴趣 `Client Interest`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Use Net Relevancy For Uninterested Actors` | false | 当某一个Actor离开玩家兴趣范围后是否调用IsNetRelevantFor方法来判断是否跟玩家相关 |
| `Client Interest Presets` | - | 客户端兴趣范围预设 |

#### 客户端兴趣范围预设 `Client Interest Presets`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Area Type` | None | <div style="white-space: nowrap">兴趣范围的几何类型<br>- `Static Locations` 静态位置点<br>- `Box` 长方体<br>- `Sphere` 球体<br>- `Cone` 圆锥</div> |
| `Preset` | None | 预设名称 |
| `Activate by Default` | true | 默认启用 |
| `Min Distance To Trigger Update` | 100.0 | 触发更新的最小距离 |
| `Spots and Dists` | Empty | 位置点和空间网格距离。仅当`Area Type`设置为`Static Locations`时有效 |
| `Extent` | Vector( 15000.0, 15000.0, 15000.0 ) | 长方体的长（世界x轴方向）、宽（世界y轴方向）、高（世界z轴方向）的一半。仅当`Area Type`设置为`Box`时有效 |
| `Radius` | 15000.0 | 球体的半径。仅当`Area Type`设置为`Sphere`或`Cone`时有效 |
| `Angle` | 15000.0 | 圆锥纵截面的角度。仅当`Area Type`设置为`Cone`时有效 |

## ChanneldUE Editor设置
ChannelUE插件在编辑器运行时的相关设置。可以在`编辑 > 编辑器偏好设置 > 插件 > ChanneldUE Editor`中找到。

### channeld网关 `Channeld`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Launch Channeld Entry` | examples/channeld-ue-tps | 启动channeld网关的入口，通常来说是包含了main.go的文件夹。为环境变量`CHANNELD_PATH`的相对路径。 |
| `Launch Channeld Parameters` | -dev -loglevel=-1 -ct=0 -mcb=13 -cfsm="config/client_authoratative_fsm.json" -scc="config/spatial_static_2x2.json" -chs="config/channel_settings_ue.json" | 启动channeld网关的命令行参数 |

#### channeld网关命令行参数说明
* `-dev` 以开发模式运行
* `-sa=` 服务端监听端口（默认值：11288）
* `-ca=` 客户端监听端口（默认值：12108）
* `-ct=` 压缩类型。无=0，Snappy=1（默认值：0）
* `-loglevel=` 日志输出等级。Debug=-1，Info=0，Warn=1，Error=2，Panic=3（默认值：0）
* `-logfile=` 日志文件路径（默认值：无）
* `-mcb=` 为各个连接分配的连接ID所能占用的最大位数（默认值：32）
* `-sfsm=` 服务端FSM配置文件的路径（默认值："config/server_authoratative_fsm.json"）
* `-cfsm=` 客户端FSM配置文件的路径（默认值："config/client_non_authoratative_fsm.json"）
* `-scc=` 空间控制器配置文件的路径。除了默认的2x2空间控制器配置, channeld还内置了4x1和6x6空间控制器配置
* `-chs=` 频道配置文件的路径（默认值："config/channel_settings_hifi.json"）

### 本地测试服务器 `Server`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Server Groups` | Empty | 本地测试服务器组.当点击`Launch Servers`时会依次启动服务器组 |

#### 本地测试服务器组
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Enable` | true | 是否启用该组服务器 |
| `Server Num` | 1 | 启动数量 |
| `Delay Time` | 0 | 等待多久再启动该组服务器 |
| `Server Map` | None | 该组服务器默认运行的关卡。若未配置则使用当前编辑器打开的关卡 |
| `Server View Class` | None | 该组服务器使用的频道数据视图。若未配置则使用和客户端相同的频道数据视图 |
| `Additional Args` |  | 启动该组服务器时的额外启动参数 |

### 工具 `Tools`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Default Replication Component` | ChanneldReplicationComponent | 执行`Add Replication Components To Blueprint Actor`时所使用的默认同步组件 |

### 同步代码生成器 `Replication Generator`
| 配置项 | 默认值 | 说明 |
| ------ | ------ | ------ |
| `Automatically Repcompile After Generating Replication Code` | true | 是否在生成同步代码成功后进行重编译 |
| `Generated Go Replication Code Storage Folder` | examples/channeld-ue-tps | 生成的go相关的同步代码存放的路径。为环境变量`CHANNELD_PATH`的相对路径。 |
| `Go Package Import Path Prefix` | github.com/metaworking/channeld/examples/channeld-ue-tps | 生成go代码和proto时，使用的go的package名称 |
