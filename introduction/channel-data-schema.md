# 频道数据模型

## 什么是频道数据模型

频道数据模型定义了[频道](basic-concepts.md#频道)对应的[频道数据状态](basic-concepts.md#频道数据中的状态)。channeld每次进行频道数据同步时，都会根据频道对应的频道数据模型进行同步。

> GameState、WorldSetting等同步Actor在一个虚拟世界中仅存在一个实例，其在频道数据模型中应该为单例。

## 频道数据模型编辑器

频道数据模型的定义是用JSON格式描述的，但是不推荐直接对json文件进行修改，而是使用ChanneldUE内置的编辑器进行编辑。

> 频道数据模型的定义文件储存在：项目根目录下的`Config/ChanneldChannelDataSchema.json`
>
> 频道数据模型的默认定义文件储存在：ChanneldUE插件根目录下的`Config/DefaultChannelDataSchema.json`

### 打开编辑器

如下图所示，点击ChannelUE插件的`Editor Channel Data Schema...` 按钮，打开频道数据模型编辑器。

![](../Docs/images/open\_channel\_data\_schema\_editor.png)

打开的编辑器如下图所示：

![](../Docs/images/default\_channel\_data\_schema\_editor.png)

> 当在新项目中第一次打开频道数据模型编辑器时，会自动创建默认的频道数据模型。

### 频道数据模型的保存

频道数据模型的编辑器会自动保存，当修改频道数据模型时，会自动保存频道数据模型到项目根目录下的`Config/ChanneldChannelDataSchema.json`文件中。

### 更新同步缓存

当项目中新增或删除了同步Actor、ActorComponent时，需要更新同步缓存，以便编辑器能够正确的显示频道数据状态的选项。

如同下图所示，在频道数据编辑器上方点击`Refresh...`按钮，更新同步缓存。

![](../Docs/images/refresh\_rep\_actor\_cache.png)

> 当从未更新过同步缓存，或项目中有任何资产发生了变化时，![](../Docs/images/refresh\_rep\_actor\_cache\_button\_alarm.png)按钮左侧会有一个警告图标，用于提示需要更新同步缓存。但是可以按照自己的需求来决定是否需要更新同步缓存。

等待一段时间，当Unreal Engine编辑器右下角提示`Successfully Updated Replication Actor Cache`时，表示更新成功。

![](../Docs/images/successfully\_updated\_rep\_actor\_cache.png)

更新完成后，就可以为频道数据模型新增频道数据状态了。

### 新增频道数据模型

如同下图所示，在频道数据编辑器上方点击`Add`按钮，选中子菜单中的需要新增的频道数据模型，即可新增频道数据模型。

![](../Docs/images/add\_channel\_data\_type.png)

### 删除频道数据模型

如同下图所示，在需要被删除的频道数据模型项点击`Delete`按钮，即可删除频道数据模型。

![](../Docs/images/delete\_channel\_data\_type.png)

### 新增频道数据状态

当在项目中新增了一个同步Actor、ActorComponent时，并希望它能够在某个频道中进行同步时，需要在频道数据模型中新增一个频道数据状态。

如同下图所示，在需要添加频道数据状态的频道数据模型项点击`Add State`按钮，选中子菜单中的需要新增的频道数据状态，即可新增频道数据状态。

![](../Docs/images/add\_channel\_data\_state.png)

> 在选择需要添加的频道数据状态时会高亮其（带有同步属性的）父类以及使用的（带有同步属性的）组件，当添加该频道数据状态时，会自动添加其（带有同步属性的）父类以及使用的（带有同步属性的）组件。

### 配置频道数据状态

如同下图所示，频道数据状态可以为频道数据状态配置三项属性： ![](../Docs/images/config\_channel\_data\_state.png)

* 启用（Enable）：是否在所属的频道数据中启用该频道数据状态。
* 单例（Singleton）：该频道数据状态是否在该频道只存在一个实例。实体频道的数据状态一般都是单例。
* 顺序（⏶|⏷）：频道数据状态的顺序。

### 删除频道数据状态

如同下图所示，在需要被删除的频道数据状态项点击`Delete`按钮，即可删除频道数据状态。

![](../Docs/images/delete\_channel\_data\_state.png)

> 在删除某个状态前，需要先删除依赖它的状态，如子类或组件的状态。
>
> * 下图所呈现的是想要删除GameStateBase的频道数据状态，但是由于GameStateBase的子类BP\_RepGameState是该频道数据模型的状态，所以需要先删除BP\_RepGameState才能将GameStateBase删除。
>
> <img src="../Docs/images/delete_channel_data_state_demo1.png" alt="" data-size="original">
>
> * 下图呈现的是想要删除SceneComponent的频道数据状态，但是由于BP\_RepGameState和Actor都使用了SceneComponent组件，所以需要先删除BP\_RepGameState和Actor才能将SceneComponent删除。
>
> <img src="../Docs/images/delete_channel_data_state_demo2.png" alt="" data-size="original">

### 导入、导出频道数据模型

* 导入频道数据模型 在频道数据编辑器上方点击`Import...`按钮，选择需要导入的频道数据模型json文件，即可导入频道数据模型。
* 导出频道数据模型 在频道数据编辑器上方点击`Export...`按钮，选择需要导出的频道数据模型json文件，即可导出频道数据模型。
