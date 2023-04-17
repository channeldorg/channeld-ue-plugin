# 推荐工作流

## 开发迭代
开发过程推荐优先使用[UE原生网络同步](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Networking/Actors/)进行GamePlay的开发和调试，在实现GamePlay功能后再使用ChanneldUE进行同步。

## 开启Channeld同步功能
### 为Actor开启同步功能
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

* 蓝图Actor中添加同步组件
    
    如下图所示：<br>
    ![](../images/character_rep_component.png)

### 为已有项目的Actor添加同步组件
如果已有项目中的Actor没有添加同步组件，可以使用ChanneldUE提供的工具为已有项目中的Actor添加同步组件。具体操作如下：
    ![](../images/add_repcomp_to_bpactor.png)

## 配置频道数据模型
频道数据模型的配置决定了每种频道
### 第一次配置
### 迭代过程中配置

## 生成同步代码
### 热编译兼容模式
### 生成
## 测试
### 使用ChanneldUE替代UE原生同步
### 单服调试
### 多服调试

## 协作和版本控制
### Ignore
### 频道数据模型