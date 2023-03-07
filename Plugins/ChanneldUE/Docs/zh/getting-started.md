# 运行环境要求
ChanneldUE插件目前仅在Windows 10/11操作系统+UE 4.27下进行过测试。在其它环境下可能会出现未知的问题。

# 1.安装ChanneldUE插件
## 1.1.克隆代码仓库
使用git命令行工具克隆代码仓库到本地：
```bash
git clone https://github.com/channeld/channeld-ue-plugin.git
```

## 1.2.初始化插件
运行插件根路径下的`Setup.bat`脚本，该脚本会自动下载并安装插件所需的依赖项，包括：
```
golang - 用于运行channeld
channeld - 网关服务
protoc-gen-go - Protobuf的go代码生成工具
```

# 2.创建第三人称模板项目
## 2.1.创建项目并复制插件
新建一个基于第三人称模板的UE蓝图项目：在Unreal Project Browser中选择"Game" -> "Third Person" -> "Blueprint"。

项目创建后，关闭UE编辑器。将1.1.步骤中克隆的插件代码仓库目录复制到项目的`Plugins`目录下。

## 2.2.编译运行项目并开启插件
双击项目的`*.uproject`文件，再次打开项目。UE编辑器会提示编译插件中的模块。编译完成后，编辑器工具栏会出现插件图标：

![](../images/toolbar_channeld.png)

如果插件图标未显示，在编辑器的顶部菜单栏中选择"Plugins" -> "ChanneldUE" -> "Enable"，启用插件。

## 2.3.为项目创建基础同步类和Game Mode
接下来，因为项目默认用到Gameplay框架并不支持基于channeld的网络同步，所以需要创建一批添加了同步组件的蓝图类。

首先，打开第三人称角色的蓝图`ThirdPersonCharacter`，并为其添加同步组件`ChanneldReplicationComponent`：

![](../images/character_rep_component.png)

然后，新建第三人称控制器的蓝图`ThirdPersonPlayerController`，并为其添加同步组件。

因为`GameStateBase`和`PlayerState`分别负责同步游戏和玩家中的状态，还需要创建分别为它们创建添加了同步组件的蓝图类：

![](../images/game_state_base.png)

![](../images/player_state.png)


```
提示：记得在添加同步组件后，编译和保存上述蓝图！
```

最后，添加一个新的GameMode蓝图`ThirdPersonGameMode`（如果已存在，则打开），并为其设置Game State Class, Player Controller Class, Player State Class和Default Pawn Class：

![](../images/game_mode.png)

在项目设置中,将`ThirdPersonGameMode`设置为默认Game Mode：

![](../images/project_settings_game_mode.png)

# 3.配置插件
## 3.1.配置频道数据视图
频道数据视图是ChanneldUE插件的核心概念之一。它主要用于关联同步对象（角色，控制器，Game State等）与频道数据。UE客户端和服务端都会存在一个视图对象。
接下来，打开主菜单`编辑 -> 项目设置 -> 插件 -> Channeld`，我们需要为项目设置一个默认视图：

![](../images/settings_view_class.png)

`BP_SingleChannelView`是插件中内置的视图蓝图类，它会在服务端创建**全局频道**，并在客户端连接成功后订阅到该频道。订阅成功后，客户端发送的网络数据会通过channeld转发到全局频道的所有者，即创建该频道的服务端。

## 3.2.配置服务器组
作为UE的分布式架构扩展，ChanneldUE插件支持同时启动多个UE服务器进程，每个进程可以配置自己的视图和启动参数。
要添加一个服务器组，打开主菜单`编辑 -> 编辑器偏好设置 -> 插件 -> Channeld Editor`。点击`Server Groups`一栏的加号按钮，并展开设置项：

![](../images/settings_server_group.png)

确保Enabled为勾选，Server Num为1，并设置Server View Class同样为`BP_SingleChannelView`。Server Map留空则表示启动服务器时，会使用编辑器当前打开的地图。

# 4.启动channeld服务和游戏服务器
点击工具栏中插件图标的下拉按钮，确保`Enable Channeld Networking`为选中状态：

![](../images/toolbar_menu.png)

然后，点击`Launch Channeld`选项，启动channeld服务（如上图中标记2所示）。如果弹出Windows防火墙提示，请允许channeld通过防火墙。

最后，点击`Launch Servers`选择，启动游戏服务器（如上图中标记3所示）。此时每一个命令行窗口，都对应一个UE服务器进程。正常启动的UE服务器进程，会在控制台中打印以下类似信息：

![](../images/server_view_initialized.png)

```
注意：如果UE服务器连接channeld失败，则会退出。
```

# 5.运行游戏并测试
在Play Standalone模式下运行游戏。此时客户端尚未连接到channeld，需要手动连接。在客户端的控制台中输入`connect 127.0.0.1`并回车。观察到客户端重新加载地图并创建角色，说明连接成功。

在地图中移动角色，并观察服务器的控制台中打印的日志，会出现对应的同步数据输出：

![](../images/server_replication_output.png)


# 6.基本的开发工作流
## 6.1.创建有同步变量的Actor
新建一个Actor蓝图`BP_TestActor`，并为其添加同步组件`ChanneldReplicationComponent`。Actor默认不开启同步，需要在组件中手动勾选`Replicate`选项：

![](../images/actor_replicates.png)

接下来，为该Actor添加一个`Cube`组件，使其可见。由于接下来要通过蓝图实现该Cube的位移和旋转，所以需要开启Cube组件的网络同步：

![](../images/cube_component_replicates.png)

然后，为该Actor添加一个同步变量`Size`，类型为Float，设置Replication为`RepNotify`，开启同步及回调：

![](../images/test_actor_size.png)

在Size的同步回调函数`On Rep Size`中，打印出同步的值：

![](../images/test_actor_size_on_rep.png)

## 6.2.在蓝图中实现移动逻辑
在`BP_TestActor`的事件图表中，为Tick事件添加如下节点，使`Cube`组件可以随着时间上下移动和旋转：

![](../images/test_actor_tick.png)

```
注意：添加IsServer节点，确保只有服务端才会执行该逻辑。
```

## 6.3.在玩家控制器中实现创建Actor的逻辑
在`BP_ThirdPersonPlayerController`中，添加如下节点，使玩家按下`F`键时，在角色前方创建一个`BP_TestActor`，并为其设置一个随机的Size：

![](../images/pc_spawn_cube.png)

```
注意：ServerSpawnCube函数需要设置为“在服务器上运行”。这样创建出来的Actor才会出现在其它客户端。

另外，调用SpawnActor函数的参数中，需要设置Owner为当前控制器。这样的话，之后的跨服示例才能正常工作。
```

## 6.4.生成同步代码
因为我们在上面的步骤里添加了新的同步类和变量（`BP_TestActor`），以及为已有的同步类添加了RPC（`BP_ThirdPersonPlayerController`），所以需要添加同步相关代码。

在ChanneldUE中，已经内置了Actor，Character，PlayerController等常用类的同步代码，所以我们只需要为新添加的`BP_TestActor`和`BP_ThirdPersonPlayerController`生成同步代码。

首先，点击插件工具栏中的`Stop Serveres`关闭之前开启的游戏服务器。然后，点击`Stop Channeld`关闭之前开启的channeld服务：

// TODO 截图

然后，点击`Generate Replication Code`选项，生成同步代码。首次生成时间要遍历项目中所有的代码和蓝图，所以可能较长，请耐心等待。

代码生成后，会自动编译。编译成功，则整个生成步骤完毕。下面可以进入游戏看看效果了。

```
注意：每次进行同步相关的修改后（包括：增删改名同步类，同步变量，或RPC），都需要重新生成同步代码。
```

## 6.5.启动服务器并测试
重复步骤4，启动channeld服务和游戏服务器。然后，重复步骤5，运行游戏并连接到服务器。

连接成功后，按下`F`键，可以看到在角色前方创建了一个`BP_TestActor`，并在屏幕左上方打印出`Size`的值：

![](../images/test_actor_spawn.png)

# 7.使用空间频道运行地图
## 7.1.空间频道简介
前面在配置频道数据视图时提到了频道的概念——频道是订阅和状态数据的集合。订阅是指哪些连接（包括客户端和服务端）关心该频道的状态数据更新；状态数据则是游戏中的需要同步的对象（比如Actor，Character，PlayerController等）的同步属性的集合。

开发者可以定义游戏中包含哪些频道，以及每种频道的状态数据的结构。常见的频道类型有：
- 全局频道：所有连接都订阅该频道。状态数据一般为所有玩家共享，如世界时间。
- 子世界频道：在空间上隔离的游戏世界，如MMO中的副本，或一局游戏的房间。每个子世界视频对应一个UE服务器进程。
- 空间频道：在空间上联通的游戏世界，适用于将较大的地图分割为多个区域。每个区域对应一个UE服务器进程。

空间频道相较于子世界频道，因为它在空间上是联通的，所以玩家在频道之间切换，不需要重新加载地图，也不需要重新连接到服务器。这样，可以大大减少玩家在切换频道时的等待时间，提升游戏体验。

空间频道有很多实现方式，从最简单的静态网格切分，到复杂的基于负载的动态切分。channeld内置提供了一种简单的静态网格切分的实现，开发者可以根据自己的需求，自行实现更复杂的切分方式。  

下面将介绍如何在UE中通过配置的方式使用空间频道。

## 7.2.在项目设置中配置空间频道


## 7.3.在编辑器设置中配置空间频道服务器

## 7.4 运行游戏并测试

# 故障排查
## 无法启动channeld服务

## 游戏服务器启动后自动退出

## 无法保存蓝图

## Setup脚本没有下载channeld
如果您在运行Setup前，已经配置了环境变量%CHANNELD_PATH%，Setup脚本会认为您已经安装了channeld，并跳过下载安装步骤。插件会根据环境变量%CHANNELD_PATH%来运行channeld。

如果您需要下载安装channeld到插件目录中，请先删除环境变量%CHANNELD_PATH%。
