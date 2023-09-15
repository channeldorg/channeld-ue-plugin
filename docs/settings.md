# ChanneldUE Settings
## Project Settings
This is the runtime settings. Can be found in `Edit > Project Settings > Plugins > ChanneldUE`.

### Channel Data View
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Channel Data View Class` | SingleChannelDataView | The channel data view class used by the client. |

### Transport
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Enable Networking` | false | Whether to enable ChanneldUE to replace the default UE networking framework. |
| `Channeld Ip for Client` | 127.0.0.1 | The IP address of the channeld server that the client connects to. |
| `Channeld Port for Client` | 12108 | The port of the channeld server that the client connects to. Does not affect the port that channeld listens on. |
| `Channeld Ip for Server` | 127.0.0.1 | The IP address of the channeld server that the server connects to. |
| `Channeld Port for Server` | 11288 | The port of the channeld server that the server connects to. Does not affect the port that channeld listens on. |
| `Use Receive Thread` | true | Whether to use a separate thread to receive data from channeld. |
| `Disable Handshaking` | true | Whether to skip the default UE handshake process. The client must connect to and be verified by channeld before entering the UE server. **In UE5, setting it to false (i.e. enabling the default handshake process) will cause the client to fail to enter the server.** |
| `Set Internal Ack` | true | Whether to disable the UE built-in heartbeat mechanism. It is recommended to turn it on when using reliable connections (such as TCP) to reduce bandwidth consumption. |
| `Rpc Redirection Max Retries` | true | The maximum number of retries for RPC redirection. When a server fails to process an RPC, it will try to forward the RPC to a server that can process it. When this value is set to 0, no redirection will occur, which will cause slight jitter in cross-server movement; when this value is set too high, the RPC may be sent back and forth between servers, causing network congestion. |

### Spatial
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Player Start Locator Class` | PlayerStartLocator_ModByConnId | The player start locator. |
| `Enable Spatial Visualizer` | false | Whether to enable the spatial channel visualizer. |

#### Client Interest
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Use Net Relevancy For Uninterested Actors` | false | Whether to call the IsNetRelevantFor method to determine whether an actor is relevant to the player when it leaves the player's interest area. |
| `Client Interest Presets` | - | Client interest area presets. |

#### Client Interest Presets
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Area Type` | None | <div style="white-space: nowrap">The geometry type of the interest area<br>- `Static Locations` Static locations<br>- `Box` Box<br>- `Sphere` Sphere<br>- `Cone` Cone</div> |
| `Preset` | None | Preset name |
| `Activate by Default` | true | Whether to enable by default |
| `Min Distance To Trigger Update` | 100.0 | The minimum distance to trigger an update |
| `Spots and Dists` | Empty | Spots and distances of the interest area. Only valid when `Area Type` is set to `Static Locations` |
| `Extent` | Vector( 15000.0, 15000.0, 15000.0 ) | The half length (world x-axis direction), width (world y-axis direction), and height (world z-axis direction) of the box. Only valid when `Area Type` is set to `Box` |
| `Radius` | 15000.0 | The radius of the sphere. Only valid when `Area Type` is set to `Sphere` or `Cone` |
| `Angle` | 15000.0 | The angle of the cone's longitudinal section. Only valid when `Area Type` is set to `Cone` |

## Editor Settings
This is the editor settings. Can be found in `Edit > Editor Preferences > Plugins > ChanneldUE Editor`.

### Channeld
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Launch Channeld Entry` | examples/channeld-ue-tps | The entry point of the channeld gateway, usually the folder containing main.go. Relative path to the environment variable `CHANNELD_PATH`. |
| `Launch Channeld Parameters` | -dev -loglevel=-1 -ct=0 -mcb=13 -cfsm="config/client_authoratative_fsm.json" -scc="config/spatial_static_2x2.json" -chs="config/channel_settings_ue.json" | The command line parameters for launching the channeld gateway. |

#### Channeld Gateway Command Line Parameters
* `-dev` Run in development mode
* `-sa=` The port that the listens server connection (default: 11288)
* `-ca=` The port that the listens client connection (default: 12108)
* `-ct=` Compression type. None=0, Snappy=1 (default: 0)
* `-loglevel=` Log output level. Debug=-1, Info=0, Warn=1, Error=2, Panic=3 (default: 0)
* `-logfile=` Log file path (default: none)
* `-mcb=` The maximum number of bits that can be used by the connection ID assigned to each connection (default: 32)
* `-sfsm=` The path of the server FSM configuration file (default: "config/server_authoratative_fsm.json")
* `-cfsm=` The path of the client FSM configuration file (default: "config/client_non_authoratative_fsm.json")
* `-scc=` The path of the spatial controller configuration file. In addition to the default 2x2 spatial controller configuration, channeld also has built-in 4x1 and 6x6 spatial controller configurations
* `-chs=` The path of the channel configuration file (default: "config/channel_settings_hifi.json")

### Server
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Server Groups` | Empty | Local test server groups. When `Launch Servers` is clicked, the groups are started sequencially. |

#### Server Groups
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Enable` | true | Whether to enable this server group |
| `Server Num` | 1 | The number of servers to start |
| `Delay Time` | 0 | How long to wait before starting this server group |
| `Server Map` | None | The map that this server group runs by default. If not configured, the map currently opened by the editor is used |
| `Server View Class` | None | The channel data view used by this server group. If not configured, the same channel data view as the client is used |
| `Additional Args` |  | Additional startup parameters for this server group |

### Tools
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Default Replication Component` | ChanneldReplicationComponent | The default replication component used when `Add Replication Components To Blueprint Actor` is executed. |

### Replication Generator
| Setting | Default Value | Description |
| ------ | ------ | ------ |
| `Automatically Repcompile After Generating Replication Code` | true | Whether to recompile automatically after generating replication code. |
| `Generated Go Replication Code Storage Folder` | examples/channeld-ue-tps | The path where the generated go-related replication code is stored. Relative path to the environment variable `CHANNELD_PATH`. |
| `Go Package Import Path Prefix` | github.com/metaworking/channeld/examples/channeld-ue-tps | The go package name used when generating go code and proto. |
