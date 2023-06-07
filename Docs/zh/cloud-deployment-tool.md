# 云部署工具
ChanneldUE提供了一套针对云部署的工具，在进行配置后，可以实现一键部署到云，也可以分别用于打包，上传，或者启动云服务。
云部署主要基于Docker和Kubernetes实现，建议先了解[Docker](https://docs.docker.com/get-started/overview)和[Kubernetes](https://kubernetes.io/docs/concepts/overview/what-is-kubernetes)的基本概念。

## 运行环境要求
- Windows 10/11操作系统
- 源代码版本的虚幻引擎4.27.2（用于构建Linux目标平台的服务器包）
- [Linux交叉编译工具链](https://docs.unrealengine.com/4.27/zh-CN/SharingAndReleasing/Linux/GettingStarted/)
- [Docker Desktop for Windows](https://docs.docker.com/desktop/windows/install)
- Kubernetes命令行工具[kubectl](https://kubernetes.io/docs/reference/kubectl)
- 开通基于Kubernetes的云容器服务，例如：
  - 阿里云容器服务[ACK](https://www.aliyun.com/product/containerservice)
  - 腾讯云容器服务[TKE](https://cloud.tencent.com/product/tke)
- 开通容器镜像仓库，例如：
  - 阿里云容器镜像服务[ACR](https://www.aliyun.com/product/acr)
  - 腾讯云容器镜像服务[TCR](https://cloud.tencent.com/product/tcr)
  - [Docker Hub](https://hub.docker.com)

## 步骤1：打包
打包的过程又分为两个步骤：首先是调用虚幻引擎的[打包项目](https://docs.unrealengine.com/4.27/en-US/Basics/Projects/Packaging/)功能，构建Linux平台的服务器目标；然后使用`docker build`分别构建网关服务和游戏服务器的镜像。

打包时，会使用项目配置中的打包设置。点击`Open Packaging Settings`，会跳转到设置面板。

>注意：打包前，建议关闭插件下拉菜单中的`Enable Compatible Recompilation`选项，并**重新生成同步代码**。这样可以保证每次打包使用同样的同步代码类，避免因为同步代码类的不同导致的兼容性问题。

`Channeld Remote Image Tag`和`Server Remote Image Tag`分别是要使用的网关服务和游戏服务器的镜像标签，会在打包、上传和部署中都被用到。镜像标签的格式为：*容器镜像仓库的基础地址*/*仓库名*/*镜像名*:*版本号*。以下是合法的标签示例：
```
// 阿里云
registry.cn-shanghai.aliyuncs.com/channeld/channeld:1.2.3

// 腾讯云
ccr.ccs.tencentyun.com/channeld/tps-server:0.6.0

// Docker Hub（可以省略基础地址）
channeld/tps-channeld:latest
```

>提示：如果没有指定版本号，会默认使用`latest`。

## 步骤2：上传
上传步骤会将步骤1中构建的镜像上传到容器镜像仓库。

如果容器镜像仓库是私有的，需要填写`Registry Username`和`Registry Password`来进行登录。如果您不希望明文保存这些信息，也可以使用`docker login`命令手动登录到仓库，再点击`Upload`进行上传。

>提示：`Registry Username`和`Registry Password`也用于步骤3：部署中`Image Pull Secret`的生成。
## 步骤3：部署
部署步骤中各个设置项的含义如下：
- `Target Cluster Context`：要使用的Kubernetes集群的上下文。点击`Detect`会获取并填入当前的上下。可以使用`kubectl config get-contexts`命令查看所有可用的上下文。
- `Namespace`：要部署到的Kubernetes命名空间。点击`Detect`会获取并填入当前的命名空间。可以使用`kubectl get namespaces`命令查看所有可用的命名空间。
- `YAML Template`：部署时使用的YAML模板文件，包含了`channeld`、`grafana`、`prometheus`的Deployment和Service。
- `Image Pull Secret`：用于从私有仓库拉取镜像的Secret。如果使用的是公有仓库，可以留空。填写完`Image Pull Secret`后点击右侧的`Generate`，则会根据已经填写的`Channeld Remote Image Tag`、`Target Cluster Context`、`Namespace`、`Registry Username`和`Registry Password`在集群中创建Secret。
- `channeld Launch Args`：网关服务的启动参数。默认跟随编辑器配置中的启动参数。
- `Main Server Group`：主服务器（组）的配置。默认跟随编辑器配置中的第一个服务器组配置。
  - `Enabled`：是否启用。关闭时不会部署主服务器。
  - `Server Num`：服务器数量。主服务器一般只需要一个。
  - `Server Map`：地图名称。默认使用当前地图。
  - `Server View Class`：频道数据视图类。默认使用项目配置中的视图类。
  - `YAML Template`：UE服务器部署时使用的YAML模板文件。
  - `Additional Args`：额外的启动参数。
- `Spatial Server Group`：空间服务器（组）的配置。默认跟随编辑器配置中的第二个服务器组配置。
  - `Enabled`：是否启用。关闭时不会部署空间服务器。如果运行单游戏服务器，应该关闭空间服务器。
  - 其它字段同上。

>注意：使用腾讯云容器镜像服务时请使用ChanneldUE插件目录下的`Template/DeploymentTencentCloud.yaml`作为 `YAML Template`。

点击`Deploy`按钮后，会根据YAML模板文件和设置项生成最终用于`kubectl apply`的YAML文件。

部署成功后，会显示Grafana的URL，用于查看服务器的运行状态。默认的用户名和密码都是`admin`。

>提示：Grafana会从github下载仪表盘文件。如果集群位于大陆地区，可能会因为网络问题导致下载较慢甚至超时，请耐心等待。超时后，Grafana服务仍能够正常启动，但是仪表盘为空。此时可以在Grafana中手动导入仪表盘文件（[下载地址](https://raw.githubusercontent.com/metaworking/channeld/master/grafana/dashboard.json)）。

### 关于Image Pull Secret
`Image Pull Secret`右侧的`Generate`功能只支持创建基于镜像仓库用户名和密码的Secret。您仍可以使用[kubectl](https://kubernetes.io/zh-cn/docs/concepts/configuration/secret/#creating-a-secret)，或云服务商的控制台创建Secret，然后将其填入`Image Pull Secret`文本框中。

## 一键部署
在上述步骤都顺利完成后，点击`One-Click Deploy`按钮，自动执行打包、上传和部署三个步骤，来加速云部署工作流。

点击`Shut Down`按钮，关闭当前正在运行的网关服务和游戏服务器，并销毁相应的云资源。

## 故障排查
### 打包
- 控制台输出`ERROR: error during connect: this error may indicate that the docker daemon is not running`的报错时，请确保`Docker Desktop`正在运行。

### 上传
- 控制台输出`Please login to the RepoUrl first.`表示镜像上传至仓库时需要登录，请在控制台中按照提示输入镜像仓库的用户名和密码。

### 部署
- `Output Log`窗口输出`Something wrong with xxx pod`时，请根据下方给出的pod状态文件进行进一步排查。
- pod状态中出现`ImagePullBackOff`或`ErrImagePull`时，请检查`Image Pull Secret`字段中填入的Secret名称是否正确。
- Grafana无法访问：请耐心等待仪表盘文件下载完毕或超时。