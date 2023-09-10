# Basic Concepts
## channeld Gateway Service
channeld is a standalone process that runs independently of UE clients and servers. In theory, all network traffic will be forwarded or broadcasted via channeld.

channeld Gateway Service is written in Go. The `Setup.bat` initialization script of ChanneldUE plugin will automatically clone the [channeld code repository](https://github.com/metaworking/channeld) to the `Source/ThirdParty/channeld` directory of the plugin. If the channeld code repository has been cloned and the `%CHANNELD_PATH%` environment variable has been configured before running `Setup.bat`, the automatic cloning will be skipped.

It is strongly recommended to deploy channeld and game servers nearby. If they are in the same cluster but on different physical servers, the network latency between channeld and game servers is usually only 1-2 milliseconds; if they are on the same physical server, the latency will be as low as tens of microseconds. What really needs to be considered is the update latency brought by the fan-out mechanism of channeld itself. If the `FanOutIntervalMs` in the subscription options is too high, after the server sends the synchronization message to channeld, theoretically the worst case is that channeld will send these update to the client after `FanOutIntervalMs` time. In FPS games that are highly sensitive to latency, it is recommended to set `FanOutIntervalMs` to 10 or less.

>Note: The smaller the `FanOutIntervalMs`, the higher the CPU load of the channel.

## Connection
For channeld, both the client and the server are a type of connection. channeld supports TCP, KCP and WebSocket protocols; currently, the client and server of ChanneldUE connect to channeld via TCP.

Each connection has a uint32 ID as a unique identifier.

The transport layer is implemented in the `UChanneldConnection` class in ChanneldUE.

## Message
channeld receives and sents messages encoded in [Google Protocol Buffers](https://protobuf.dev/) (short for Protobuf). Each packet can contain one or more messages. Each message must specify the channel to which it is sent.

The internal messages include:
- Channel creation, deletion, and query
- Channel subscription and unsubscription
- Channel data update
- Spatial channel related

channeld supports custom message types. By default, 100 and above are **user-space messages**. ChanneldUE adds the following user-space message types:
- Spawn and destroy objects
- RPC
- Handover related

## Channel
A channel contains a channel owner connection, the channel data, and multiple subscription connections.

The channel owner is often the creator of the channel. It is the receiver of user-space messages sent to the channel. That is to say, after the channel receives a message, if it is not an internal message, it will be forwarded to the server (or client) that created the channel.

The channel data is a Protobuf message of type [Any](https://protobuf.dev/programming-guides/proto3/#any). The actual data structure is defined by the game developer. In ChanneldUE, the data structure is defined by the code generation tool. A typical channel data message definition is like this:
```protobuf
message DefaultGlobalChannelData {
    unrealpb.GameStateBase gameState = 1;
    map<uint32, unrealpb.ActorState> actorStates = 2;
    map<uint32, unrealpb.PawnState> pawnStates = 3;
    map<uint32, unrealpb.CharacterState> characterStates = 4;
    map<uint32, unrealpb.PlayerState> playerStates = 5;
    map<uint32, unrealpb.ControllerState> controllerStates = 6;
    map<uint32, unrealpb.PlayerControllerState> playerControllerStates = 7;
    // ...more state definitions
}
```

The channel maintains a subscription list, including each subscription connection and the subscription options of the connection.
After the channel data changes, the accumulated updates will be sent to all subscribed connections one by one according to the frequency in the subscription options. This process is called fan-out.

## State in Channel Data
State is a finer-grained concept in channel data, and a state corresponds to a replicated Actor. In fact, an instance of a replicated Actor often contains multiple states. For example, a Character contains a CharacterState, a PawnState, and an ActorState. You will find that these three states correspond to the three super classes from ACharacter to AActor. Each state only cares about the replicated properties of the corresponding super class. In this way, states can be flexibly combined to achieve the replication of any Actor, without worrying about the repetition of replication code or the redundancy of replicated properties.

## Replicator
Each replicator is responsible for the replication of a state. The `ChanneldReplicationComponent` will create the corresponding replicator in `BeginPlay` according to the type of the Actor and its ActorComponents that have enabled replication. In each replication frame, each replicator will check whether the replicated properties have changed. If there are changes, the changed properties will be written to the state. The replication component is responsible for writing the delta state to the channel data.

## NetId
In the channel data message definition above, you can see that most of the states are stored into a Map, indexed by a uint32. This uint32 is the NetId, which is a globally unique ID used to identify a replicated Actor. In ChanneldUE, NetId is equivalent to [FNetworkGUID](https://docs.unrealengine.com/4.27/en-US/API/Runtime/Core/Misc/FNetworkGUID/), but it has been modified on the original UE: its high 13 bits are the connection ID, and the low 19 bits are the incremental ID. The purpose of doing this is to avoid NetworkGUID conflicts generated on different servers.

States that are not stored in the Map will have a fixed NetId, for example, the NetId of GameStateBase is 1.
