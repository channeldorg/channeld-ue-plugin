# 运行环境要求
ChanneldUE插件目前仅在Windows 10/11操作系统+UE 4.27下进行过测试。在其它环境下可能会出现未知的问题。

# 1.安装ChanneldUE插件
## 1.1.克隆代码仓库
使用git命令行工具克隆代码仓库到本地：
```bash
git clone https://github.com/metaworking/channeld-ue-plugin.git
```

## 1.2.初始化插件
运行插件根路径下的`Setup.bat`脚本，该脚本会自动下载并安装插件所需的依赖项，包括：
```
golang - 用于运行channeld
channeld - 网关服务
protoc-gen-go - Protobuf的go代码生成工具
```

# 2.创建第三人称模板项目
## 2.1.创建项目
#### 2.1.1.新建一个基于第三人称模板的UE项目
使用第三人称模板的创建UE项目。选择项目类型为`C++`：
![](../images/create_project.png)

```
注意：ChannelUE插件只支持C++项目。如果您使用的是纯蓝图项目，需要先转换为C++项目。
```

##### 2.1.2.复制插件
将1.1.步骤中克隆的插件代码仓库目录复制到项目的`Plugins`目录下。

##### 2.1.3.修改项目Build.cs文件
为项目添加插件的模块`ChanneldUE`和`ProtobufUE`：

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ChanneldUE", "ProtobufUE" });
```

##### 2.1.4.修改项目的配置文件
打开项目的配置文件`Config/DefaultEngine.ini`，并添加插件相关的日志输出等级：

```
[Core.log]
LogChanneld=Verbose
LogChanneldGen=Verbose
LogChanneldEditor=Verbose
LogChanneldRepGenerator=Verbose
```

```
提示：该配置使用较低的日志输出级别。建议在开发阶段开启Verbose日志，以方便排查和报告问题。在发布阶段，建议将日志等级改为Log或Warn以减少性能和文件存储开销。
```

## 2.2.重新编译运行项目并开启插件
在文件浏览器中右键点击项目的`*.uproject`文件，选择"Generate Visual Studio project files"，重新生成项目的解决方案文件。
在Visual Studio中重新加载解决方案，并编译运行项目。当UE编辑器再次打开时，工具栏会出现插件图标：

![](../images/toolbar_channeld.png)

如果插件图标未显示，在编辑器的顶部菜单栏中选择`编辑 -> 插件 -> Other -> ChanneldUE -> 已启用`，启用插件。

## 下一步
在下面的章节里，将介绍如何基于ChanneldUE的网络框架运行第三人称模板项目。[点击这里](third-person-template.md)继续。