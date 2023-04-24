## 
## 本地测试

# 推荐工作流

## 本地开发迭代

### 使用UE原生网络同步开发
开发过程推荐优先使用[UE原生网络同步](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/Actors/)进行GamePlay的开发和调试，在实现GamePlay功能后再使用ChanneldUE进行同步。
>使用UE原生网络同步时请确保已关闭ChanneldUE的同步功能，如下图所示：
>
>![](../images/disable_channeld_networking.png)

### 开启Channeld同步功能
但您已经实现了GamePlay功能并测试完成后，可以使用ChanneldUE的同步功能来进行基于channeld的分布式专用服务器开发。
>首先需要开启ChanneldUE的同步功能，如下图所示：
>
>![](../images/enable_channeld_networking.png)

#### 为Actor开启同步功能
如果希望使用ChannelUE同步Actor的属性则需要在UE原生网络同步的配置上为Actor添加同步组件。ChannelUE支持为C++和蓝图Actor添加同步组件。具体操作如下：
* C++ Actor的Class中添加同步组件
    1. Actor 类中声明`ChanneldReplicationComponent`组件
    ```
    #include "Replication/ChanneldReplicationComponent.h"
    ...
    class XXX_API ACppActor : public AActor
    {
        GENERATED_BODY()
    protected:
        UPROPERTY()
        UChanneldReplicationComponent* ChanneldReplicationComponent;
        ...
    };
    ```

    2. Actor 的构造函数中实例化`ChanneldReplicationComponent`组件
    ```
    ACppActor::ACppActor()
    {
        ChanneldReplicationComponent = CreateDefaultSubobject<UChanneldReplicationComponent>(TEXT("ChanneldReplication"));
    }
    ```

* 蓝图Actor中添加同步组件，如下图所示：

    ![](../images/character_rep_component.png)

#### 为已有项目的同步Actor添加同步组件
除手动为同步Actor添加同步组件外，ChanneldUE提供了工具为已有项目中的Actor添加同步组件。具体操作如下：

![](../images/add_rep_comp_to_bpactor.png)

>注意：该工具会加载项目中所有的Actor，如果项目中有大量的Actor，可能会导致加载时间过长，建议在仅在项目中途才使用ChanneldUE插件时使用一次该功能或者较小项目中使用该功能。

### 配置频道数据模型
为同步Actor添加同步组件后还不足以使他们通过ChanneldUE同步，还需指定同步Actor的状态在哪个频道中同步。ChanneldUE通过频道数据模型来指定同步Actor的状态在哪个频道中同步。

频道数据模型需要在频道数据模型编辑器中进行配置。点击ChannelUE插件的`Editor Channel Data Schema` 按钮。

![](../images/open_channel_data_schema_editor.png)

将会打开如下编辑器窗口：

<img src="../images/default_channel_data_schema_editor.png" height = "600" alt="" />

>频道数据模型详细说明和是使用可参考[频道数据模型](./channel-data-schema.md)

#### 新增Actor
当新增了一个同步Actor时，需要先更新同步Actor缓存。在频道数据编辑器上方点击`Refresh...`按钮：

![](../images/refresh_channel_data_state.png)

等待同步Actor缓存更新完成后，就可以将新增的同步Actor状态添加到某一个频道数据下：

![](../images/add_channel_data_state.png)

#### GameState
通常GameState在服务器中只有唯一的实例，因此可以将GameState的状态设置为单例：

![](../images/singleton_channel_data_state.png)

>在多服情况下建议将GameState的状态放到Global频道中并设置为单例，以保证GameState的状态在所有的服务器中保持一致。
>除了GameState外，如WorldSettings等Actor都是单例，同样建议将其状态设置为单例。

### 生成同步代码
配置完成频道数据模型后，需要生成C++的同步代码。同步代码主要是用来处理频道数据模型的状态同步的。

#### 生成
在频道数据编辑器上方点击`Generate...`按钮即可生成同步代码：

![](../images/generate_rep_code.png)

#### 热编译兼容模式
ChannelUE提供了热编译兼容模式，该模式下每次生成同步代码后自动热编译项目源码，如果关闭热编译兼容模式则在生成同步代码后需要先关闭UE编辑器再通过源码编译。

![](../images/enable_compatible_recompilation.png)

>热编译兼容模式下每次生成的同步代码都会存在差异，所以在发布前建议关闭热编译兼容模式再生成一次同步代码。

### 本地测试
由于ChanneldUE采用了分布式专用服务器的架构，所以在本地测试时需要启动多个专用服务器和网关。ChannelUE提供了专用服务器和网关的启动工具。

#### 启动专用服务器和网关
通过插件的二级菜单开启专用服务器和网关，步骤如下：

![](../images/toolbar_menu.png)
<!-- ，但是请先确保`Launch Channeld`成功开启网关后再通过`Launch Servers`开启专用服务器。 -->
1. 确保开启了ChanneldUE的同步功能
2. 点击`Launch Channeld`开启channel网关

    channeld网关开启成功会有如下输出：
    ![](../images/launch_channeld_success.png)

3. 点击`Launch Servers`开启专用服务器

#### 测试多个客户端
如果要同时开启多个客户端，需要对默认的编辑器设置做一些修改。打开主菜单`编辑 -> 编辑器偏好设置 -> 关卡编辑器 -> 播放`，在`Multiplayer options`中，**取消**`单进程下的运行`的勾选：

![](../images/settings_run_under_one_process.png)

#### 关闭专用服务器和网关
测试完成后，可以通过插件的二级菜单关闭专用服务器和网关，依次点击`Stop Servers`和`Stop Channeld`即可关闭专用服务器和网关。

![](../images/stop_servers_and_channeld.png)

## 协作和版本控制

### 频道数据模型
#### 定义文件
频道数据模型定义文件需要通过版本控制工具进行版本控制，以保证多人协作时频道数据模型的一致性。频道数据模型的定义文件为项目路径的`Config/ChanneldChannelDataSchema.json`。

#### 配置频道数据模型
当开发者从版本控制中拉取项目最新代码后，在配置频道数据模型前建议先[更新一次同步Actor缓存](#新增Actor)以确保频道数据状态能正确的显示在频道数据编辑器中。

#### 重新生成一次同步代码
当开发者从版本控制中拉取项目最新代码后，建议重新[生成一次同步代码](#生成同步代码)以保证同步代码的一致性。

### 不需要上传至版本控制
当使用版本控制时，可以将ChanneldUE生成的同步代码忽略，以免版本控制工具将其纳入版本控制中。ChanneldUE生成的同步代码在项目路径下的`Source/<游戏模块>/ChanneldGenerated`目录下。
>如果使用git作为版本控制工具，可以在项目根目录下的`.gitignore`文件中添加如下内容：
>```
># ChanneldUE生成的同步代码
>/Source/**/ChanneldGenerated
>```