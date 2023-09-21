# ChanneldUE and Native UE Comparison
This chapter describes the differences between ChanneldUE and the native UE networking system. Some of the differences can be solved by alternative solutions, but some of them do not have alternative solutions yet. If your game needs to use these features, please raise an issue in [Issues](/../../issues).

## Conditional Property Replication
ChanneldUE does not support [Conditional Property Replication](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Networking/Actors/Properties/Conditions/) yet.

This means that all changed properties will be sent from the UE server to the channeld gateway service and broadcast to all subscribers. The client will receive the property changes of all players in the channel, including the properties of other players' PlayerController and PlayerState, but will not process them because the client will not create PlayerController or PlayerState instances of other players. This does cause some bandwidth waste and potential security issues. The feature of implementing conditional property replication based on the channeld gateway service is planned.

## Network Update Frequency
ChanneldUE supports [Network Update Frequency](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Networking/Actors/Properties/#data-driven-type-network-update-frequency).

On the client, the frequency of receiving property update depends on the `FanOutIntervalMs` parameter set when subscribing to the channel, which is 20ms by default, equivalent to 50Hz. The update frequency of the Global channel can be modified by modifying `GlobalChannelFanOutIntervalMs` in the channel data view; the update frequency of the spatial channel decays with the distance of the channel, which is 50Hz (distance is 0, that is, the same spatial channel), 20Hz (distance is 1 spatial channel), 10Hz (distance is 2 or more spatial channels). To change the update frequency of the spatial channel, you need to modify the `spatialDampingSettings` in the code [message_spatial.go](/../../../channeld/blob/master/pkg/channeld/message_spatial.go) of the channeld gateway service.

## Net Cull Distance
ChanneldUE does not support the `Net Cull Distance Squared` property of Actor, but it can be used as an alternative solution.

In the native UE, the spatial range that a client can be updated to is controlled by the `Net Cull Distance Squared` property of each replicated Actor, which is 150 meters squared by default. In ChanneldUE, the client's interest range (Area of Interest) is the set of spatial channels subscribed by the client. The minimum spatial range that the client can receive is a spatial channel, which is 10x10 meters in the third-person example used in the [Getting Started](getting-started.md).
For specific configuration methods, please refer to the [Client Interest Management](client-interest.md) document.

To achieve a spherical interest area centered on the player with a radius of 150 meters, open the `Project Settings -> Plugins -> Channeld -> Spatial -> Client Interest`, add a `Client Interest Preset`, and set `Area Type` to **Sphere** and `Radius` to **15000**.

## Replication Graph
ChanneldUE does not support [Replication Graph](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Networking/ReplicationGraph/), but it can be used as an alternative solution.

ChanneldUE improves the performance of UE servers in network replication in the following ways:

1. Generate C++ replication code for all C++ and Blueprint classes that need to be replicated, which greatly improves performance compared to the native UE reflection mechanism
2. Move the property replication and RPC message broadcast to the channeld gateway service, reducing the CPU overhead of UE server caused by traversal
3. Move the logic of interest management to the channeld gateway service, further reducing the CPU overhead of the UE server

The Replication Graph groups Actors or creates special lists, which is similar to the channel function in channeld. By replicating different Actors to different channels with specific state sets, the developer can achieve the similar effect as of Replication Graph.

## Network Relevancy and Priority
ChanneldUE partially supports [Network Relevancy](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Networking/Actors/Relevancy/), but does not support priority settings.

When a replicated Actor opens `bAlwaysRelevant`, all clients will subscribe to the entity channel corresponding to the Actor;

When a replicated Actor leaves the player's interest area, the client will call the Actor's IsNetRelevantFor method to determine whether it is relevant to the player. If it is relevant, the Actor will not be destroyed. To enable the network relevancy call check, you need to check `Use Net Relavancy For Uninterested Actors` in the `Project Settings -> Plugins -> Channeld -> Spatial -> Client Interest`.

## Cross-server Support for Frameworks and Subsystems
Unreal Engine assumes that all simulations take place on a single server, so there is no concept of cross-server. The Gameplay framework, Physics system, AI system, Gameplay Ability system, etc. in UE are all implemented based on this logic. However, in channeld, if the spatial channel is used, the simulated objects may migrate between multiple servers. ChanneldUE currently only implements cross-server migration of the Gameplay framework (PlayerController, PlayerState, etc.), and other frameworks and subsystems need to be integrated to support cross-server migration, otherwise unexpected results may occur due to loss of state.

Therefore, before implementing the integration, your game needs to limit the cross-server migration related to these frameworks and subsystems in the design. For example, adding invisible barriers to prevent rigid bodies from being pushed across the boundaries between servers, or restrict the AI to patrol outside the spatial area within the server.
