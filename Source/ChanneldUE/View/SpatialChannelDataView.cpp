#include "View/SpatialChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "ChanneldMetrics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/DataChannel.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Interest/ClientInterestManager.h"
#include "Kismet/GameplayStatics.h"
#include "Replication/ChanneldReplication.h"

USpatialChannelDataView::USpatialChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UChanneldNetConnection* USpatialChannelDataView::CreateClientConnection(Channeld::ConnectionId ConnId, Channeld::ChannelId ChId)
{
	ensureMsgf(!ClientInChannels.Contains(ConnId), TEXT("Client conn %d had already been added to this server"), ConnId);

	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())//NetDriver.IsValid())
	{
		if (NetDriver->GetClientConnectionMap().Contains(ConnId))
		{
			return NetDriver->GetClientConnectionMap()[ConnId];
		}
		UChanneldNetConnection* ClientConn = NetDriver->AddChanneldClientConnection(ConnId, ChId);
		// Create the ControlChannel and set OpenAcked = 1
		UChannel* ControlChannel = ClientConn->CreateChannelByName(NAME_Control, EChannelCreateFlags::OpenedLocally);
		ControlChannel->OpenAcked = 1;
		ControlChannel->OpenPacketId.First = 0;
		ControlChannel->OpenPacketId.Last = 0;
		
		return ClientConn;
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Spatial server failed to create client connection %d: NetDriver is not valid."), ConnId);
	}
	
	return nullptr;
}

void USpatialChannelDataView::InitPlayerController(UChanneldNetConnection* ClientConn, APlayerController* const NewPlayerController)
{
	if (AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode())
	{
		// ~ Begin copy of AGameModeBase::InitNewPlayer
		// Register the player with the session
		if (NewPlayerController->PlayerState)
		{
			GameMode->GameSession->RegisterPlayer(NewPlayerController, ClientConn->PlayerId.GetUniqueNetId(), false);
		}
		else
		{
			UE_LOG(LogChanneld, Error, TEXT("Unable to register player for the game session: PlayerState is NULL. ConnId: %d"), ClientConn->GetConnId());
		}
		//GameMode->ChangeName(NewPlayerController, FString::Printf(TEXT("Player_%d"), ClientConn->GetConnId()), false);
		// ~ End copy of AGameModeBase::InitNewPlayer
		
		// ~ End copy of AGameModeBase::Login
		
		// Possess the newly-spawned player.
		NewPlayerController->NetPlayerIndex = 0;

		// HACK: We need to set the private property bIsLocalPlayerController to false before calling SetPlayer(), so it won't create the spectator pawn for the PC.
		static const FBoolProperty* Prop = CastFieldChecked<const FBoolProperty>(APlayerController::StaticClass()->FindPropertyByName("bIsLocalPlayerController"));
		/* Won't work - causes error
		Prop->SetPropertyValue(NewPlayerController, false);
		*/
		bool* Ptr = Prop->ContainerPtrToValuePtr<bool>(NewPlayerController);
		*Ptr = false;
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Set bIsLocalPlayerController to %s"), NewPlayerController->IsLocalController() ? TEXT("true") : TEXT("false"));

		// Triggers ClientSetViewTarget RPC...
		NewPlayerController->SetPlayer(ClientConn);

		// IMPORTANT: Set to ROLE_Authority must be done AFTER SetPlayer(), otherwise UnPossess() will be called!
		NewPlayerController->SetRole(ROLE_Authority);
		NewPlayerController->SetReplicates(true);

		ClientConn->SetClientLoginState(EClientLoginState::ReceivedJoin);
	}
}

void USpatialChannelDataView::CreatePlayerController(UChanneldNetConnection* ClientConn)
{
	FURL InURL( NULL, *ClientConn->RequestURL, TRAVEL_Absolute );
	FString ErrorMsg;
	/* SpawnPlayActor will create the PC AND the Pawn. We need to skip some process that involves player login.
	ClientConn->PlayerController = GetWorld()->SpawnPlayActor( ClientConn, ROLE_AutonomousProxy, InURL, ClientConn->PlayerId, ErrorMsg );
	*/

	//~ Begin copy of UWorld::SpawnPlayActor
	if (AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode())
	{
		// Make the option string.
		FString Options;
		for (int32 i = 0; i < InURL.Op.Num(); i++)
		{
			Options += TEXT('?');
			Options += InURL.Op[i];
		}

		// ~ Begin copy of AGameModeBase::Login
		FString ErrorMessage = GameMode->GameSession->ApproveLogin(Options);
		if (!ErrorMessage.IsEmpty())
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to create player controller for conn %d, error: %s"), ClientConn->GetConnId(), *ErrorMessage);
			return;
		}
		
		APlayerController* const NewPlayerController = GameMode->SpawnPlayerController(ENetRole::ROLE_AutonomousProxy, Options);
		if (NewPlayerController == nullptr)
		{
			UE_LOG(LogGameMode, Log, TEXT("Login: Couldn't spawn player controller of class %s"), GameMode->PlayerControllerClass ? *GameMode->PlayerControllerClass->GetName() : TEXT("NULL"));
			return;
		}

		InitPlayerController(ClientConn, NewPlayerController);
	}
	//~ End copy of UWorld::SpawnPlayActor
}

TArray<UObject*> USpatialChannelDataView::GetHandoverObjects_Implementation(UObject* Obj, int32 SrcChId, int32 DstChId)
{
	TArray<UObject*> Result;
	Result.Add(Obj);
	
	// Group Pawn, PlayerController, and PlayerState together to make them handover at the same time.
	if (const APawn* Pawn = Cast<APawn>(Obj))
	{
		Result.Add(Pawn->GetController());
		Result.Add(Pawn->GetPlayerState());
	}

	return Result;
}

void USpatialChannelDataView::ServerHandleGetHandoverContext(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto GetContextMsg = static_cast<const unrealpb::GetHandoverContextMessage*>(Msg);
	const auto NetId = FNetworkGUID(GetContextMsg->netid());
	UObject* Obj = GetObjectFromNetGUID(NetId);

	unrealpb::GetHandoverContextResultMessage ResultMsg;
	ResultMsg.set_netid(NetId.Value);
	ResultMsg.set_srcchannelid(GetContextMsg->srcchannelid());
	ResultMsg.set_dstchannelid(GetContextMsg->dstchannelid());

	if (Obj == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to get handover context, netId: %d"), NetId.Value);
		// Always send back the result message.
		Connection->Send(ChId, unrealpb::HANDOVER_CONTEXT, ResultMsg);
		return;
	}

	TArray<UObject*> HandoverObjs = GetHandoverObjects(Obj, GetContextMsg->srcchannelid(), GetContextMsg->dstchannelid());

	for (UObject* HandoverObj : HandoverObjs)
	{
		if (HandoverObj == nullptr)
		{
			continue;
		}
		auto HandoverContext = ResultMsg.add_context();
		UChanneldNetConnection* NetConn = nullptr;
		if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
		{
			NetConn = Cast<UChanneldNetConnection>(HandoverActor->GetNetConnection());
		}
		HandoverContext->mutable_obj()->CopyFrom(*ChanneldUtils::GetRefOfObject(HandoverObj, NetConn));
		if (NetConn)
		{
			HandoverContext->set_clientconnid(NetConn->GetConnId());
		}
	}

	UE_LOG(LogChanneld, Verbose, TEXT("[Server] GetHandoverContext for channeld: %s"), UTF8_TO_TCHAR(ResultMsg.DebugString().c_str()));
	
	Connection->Send(ChId, unrealpb::HANDOVER_CONTEXT, ResultMsg);

	GEngine->GetEngineSubsystem<UChanneldMetrics>()->GetHandoverContexts->Add({{"objNum", std::to_string(ResultMsg.context_size())}}).Increment();
}

void USpatialChannelDataView::ServerHandleHandover(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	// unrealpb::HandoverData HandoverData;
	unrealpb::SpatialChannelData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);

	// Does current server has interest over the handover objects?
	const bool bHasInterest = Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid());
	// Does current server has authority over the handover objects?
	const bool bHasAuthority = Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid());
	
	FString NetIds;
	/*
	for (auto& HandoverContext : HandoverData.context())
	{
		NetIds.Appendf(TEXT("%d[%d], "), HandoverContext.obj().netguid(), HandoverContext.clientconnid());
	}
	*/
	for (auto& Pair : HandoverData.entities())
	{
		NetIds.Appendf(TEXT("%d[%d], "), Pair.second.objref().netguid(), Pair.second.objref().owningconnid());
	}
	UE_LOG(LogChanneld, Log, TEXT("ChannelDataHandover from channel %d to %d(%s), object netIds: %s"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(),
		bHasAuthority ? TEXT("A") : (bHasInterest ? TEXT("I") : TEXT("N")), *NetIds);
	
	// ===== Pass 1: Handle the logic of the source channel =====
	bool bHasAuthorityOverSourceChannel = Connection->OwnedChannels.Contains(HandoverMsg->srcchannelid());
	bool bUpdateSourceChannel = false;
	// for (auto& HandoverContext : HandoverData.context())
	for (auto& Pair : HandoverData.entities())
	{
		const unrealpb::UnrealObjectRef& HandoverObjRef = Pair.second.objref();//HandoverContext.obj();
		FNetworkGUID NetId(HandoverObjRef.netguid());
		
		// Source spatial server - the channel data is handed over from
		if (Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
		{
			UObject* HandoverObj = GetObjectFromNetGUID(NetId);
			// Check if object is already destroyed
			if (IsValid(HandoverObj))
			{
				RemoveObjectProvider(HandoverMsg->srcchannelid(), HandoverObj, bHasAuthorityOverSourceChannel);
				bUpdateSourceChannel = true;
				
				// If the handover actor is no longer in the interest area of current server, delete it.
				if (!bHasInterest)
				{
					UE_LOG(LogChanneld, Log, TEXT("[Server] Deleting object %s as it leaves the interest area"), *HandoverObj->GetName());
					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
						{
							HandoverPC->Player = nullptr;
							
							// Don't remove the client connection for now - we don't want to send CloseBunch to the client.
							// Keep the client connection until the client leaves the game, so we can reuse it for handover.
							HandoverPC->NetConnection = nullptr;
							
							// Make sure the Pawn and PlayerState are not destroyed together - we will handle it in the following handover context loop
							if (HandoverPC->GetPawn())
							{
								// HandoverPC->Pawn = nullptr;
								static const auto PawnProperty = CastFieldChecked<const FObjectProperty>(AController::StaticClass()->FindPropertyByName("Pawn"));
								*PawnProperty->ContainerPtrToValuePtr<APawn*>(HandoverPC) = nullptr;
							}
							HandoverPC->PlayerState = nullptr;
						}
						else if (auto HandoverPawn = Cast<APawn>(HandoverActor))
						{
							// Reset the pawn's controller (don't call UnPossess!) so it won't send ClientSetViewTarget RPC in APawn::DetachFromControllerPendingDestroy.
							HandoverPawn->Controller = nullptr;
						}

						/* OnDestroyedActor() handles this
						// Don't send "removed: true" update to the channel, because the state still exists.
						RemoveActorProvider(HandoverActor, false);
						*/

						// HACK: turn off SendDestroyToClients() temporarily - we don't want the actor to be destroyed in the clients.
						bSuppressSendOnServerDestroy = true;
						GetWorld()->DestroyActor(HandoverActor, true);
						bSuppressSendOnServerDestroy = false;
					}
					else
					{
						HandoverObj->ConditionalBeginDestroy();
					}

					/* Don't remove the mapping, as it may be used for getting the target channel of unresolvable RPC
					NetIdOwningChannels.Remove(NetId);
					*/

					// Remove from the GuidCache so that the object can be re-created in the future cross-server handover.
					auto GuidCache = GetWorld()->NetDriver->GuidCache;
					GuidCache->ObjectLookup.Remove(NetId);
					GuidCache->NetGUIDLookup.Remove(HandoverObj);
				}
				// If the handover actor is no longer in the authority area of current server,
				// make sure it no longer sends ChannelDataUpdate message.
				else if (!bHasAuthority)
				{
					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						HandoverActor->SetRole(ENetRole::ROLE_SimulatedProxy);
					}
				}
			}
		}
	}

	/* With entity channel, there's no need to update the source spatial channel.
	if (bHasAuthorityOverSourceChannel && bUpdateSourceChannel)
	{
		// Send removal update to the source channel
		SendChannelUpdate(HandoverMsg->srcchannelid());
	}
	*/

	// ===== Pass 2: Handle the logic of the destination channel =====
	
	// All the objects that moved across the channels.
	TArray<UObject*> HandoverObjs;
	// The actors that moved across the servers and are authorized by current (destination) server.
	TArray<AActor*> CrossServerActors;

	// for (auto& HandoverContext : HandoverData.context())
	for (auto& Pair : HandoverData.entities())
	{
		const unrealpb::UnrealObjectRef& HandoverObjRef = Pair.second.objref();//HandoverContext.obj();
		FNetworkGUID NetId(HandoverObjRef.netguid());
		
		// Set the NetId-ChannelId mapping before spawn the object, so AddProviderToDefaultChannel won't have to query the spatial channel.
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());
		
		// Destination spatial server - the channel data is handed over to
		if (bHasInterest)
		{
			UObject* HandoverObj = GetObjectFromNetGUID(NetId);
			// We need to know which client connection causes the handover, in order to spawn the object. 
			UChanneldNetConnection* ClientConn = nullptr;

			// Player enters the server - only Pawn, PlayerController or PlayerState has clientConnId > 0
			if (HandoverObjRef.owningconnid() > 0)
			{
				const Channeld::ConnectionId ClientConnId = HandoverObjRef.owningconnid();
				ClientConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnection(ClientConnId);
			
				// Try to spawn the object
				if (HandoverObj == nullptr)
				{
					if (ClientInChannels.Contains(ClientConnId))
					{
						// Update the channelId for LowLevelSend()
						ClientInChannels.Emplace(ClientConnId, HandoverMsg->dstchannelid());
						UE_LOG(LogChanneld, Log, TEXT("[Server] Updated mapping of connId: %d -> channelId: %d"), ClientConnId, HandoverMsg->dstchannelid());
					}
					else
					{
						// Create the client connection if it doesn't exist yet. Don't create the PlayerController for now.
						ClientConn = CreateClientConnection(ClientConnId, HandoverMsg->dstchannelid());
						UE_LOG(LogChanneld, Log, TEXT("[Server] Create client connection %d during handover, context obj: %d"), ClientConnId, NetId.Value);
					}
				
					// Set the NetId of the handover object as exported, so the NetConn won't send the Spawn message to the client.
					// FIXME: Won't work as the spawned object will have a different NetId!
					ClientConn->SetSentSpawned(NetId);

					// HACK: turn off AddProviderToDefaultChannel() temporarily, as the NetId of the handover object may mismatch when just created. (ChanneldUtils@L656)
					bSuppressAddProviderAndSendOnServerSpawn = true;
					HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true, ClientConn);
					bSuppressAddProviderAndSendOnServerSpawn = false;
					UE_LOG(LogChanneld, Log, TEXT("[Server] Spawned handover obj: %s, clientConnId: %d"), *GetNameSafe(HandoverObj), ClientConnId);
				}
			}
			// Non-player object enters the server. Simply create it.
			else if (HandoverObj == nullptr)
			{
				bSuppressAddProviderAndSendOnServerSpawn = true;
				// ClientConn should be assigned in order to spawn properly.
				HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true, NetConnForSpawn);
				bSuppressAddProviderAndSendOnServerSpawn = false;
				UE_LOG(LogChanneld, Log, TEXT("[Server] Spawned handover obj: %s"), *GetNameSafe(HandoverObj));

				/* Moved to ChanneldUtils::GetObjectByRef
				// Always remember to reset the NetConnForSpawn after using it.
				ResetNetConnForSpawn();
				*/
			}
		
			if (IsValid(HandoverObj))
			{
				HandoverObjs.Add(HandoverObj);
				
				// Now the NetId is properly set, call AddProviderToDefaultChannel().
				AddObjectProviderToDefaultChannel(HandoverObj);
				// MoveObjectProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), HandoverObj, true);
				// bUpdateSourceChannel = true;
				
				if (bHasAuthority)
				{
					// In-server handover - the srcChannel and dstChannel are in the same server
					if (Connection->OwnedChannels.Contains(HandoverMsg->srcchannelid()))
					{
					}
					// Cross-server handover
					else
					{
						if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
						{
							CrossServerActors.Add(HandoverActor);

							// Associate the NetConn with the PlayerController
							if (APlayerController* HandoverPC = Cast<APlayerController>(HandoverObj))
							{
								HandoverPC->NetConnection = ClientConn;
							}
						}
					}
				}
				else
				{
					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						// Set the role to SimulatedProxy
						HandoverActor->SetRole(ROLE_SimulatedProxy);
					}
				}
			}
			else if (HandoverObj == nullptr)
			{
				UE_LOG(LogChanneld, Error, TEXT("[Server] Failed to spawn object of netId %d from handover obj ref: %s"), NetId.Value, UTF8_TO_TCHAR(HandoverObjRef.DebugString().c_str()));
			}
			else
			{
				UE_LOG(LogChanneld, Error, TEXT("[Server] Handover object '%s' is not valid, pending kill: %d"), *GetNameSafe(HandoverObj), HandoverObj->IsPendingKill());
			}
		}
	}

	// ===== Pass 3: Applies the channel data update to the newly spawned objects =====
	// The references between the Pawn, PlayerController, and PlayerState should be set properly in this step.
	for (auto HandoverObj : HandoverObjs)
	{
		auto NetId = GetNetId(HandoverObj);
		auto Pair = HandoverData.entities().find(NetId.Value);
		// for (auto& Pair : HandoverData.entities())
		if (Pair != HandoverData.entities().end() && Pair->second.has_entitydata())
		{
			if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
			{
				// Set the role to SimulatedProxy so the actor can be updated by the handover channel data later.
				HandoverActor->SetRole(ROLE_SimulatedProxy);
				UE_LOG(LogChanneld, Verbose, TEXT("Set %s to ROLE_SimulatedProxy for ChannelDataUpdate"), *HandoverActor->GetName());
				
				channeldpb::ChannelDataUpdateMessage UpdateMsg;
				UpdateMsg.mutable_data()->CopyFrom(Pair->second.entitydata());
				UE_LOG(LogChanneld, Verbose, TEXT("Applying handover channel data to entity %d"), Pair->first);
				HandleChannelDataUpdate(_, Pair->first, &UpdateMsg);
			}
		}
	}

	// Post Handover - set the actors' properties as same as they were in the source server.
	
	// Pass 1: set up the cross-server PlayerControllers
	for (auto HandoverActor : CrossServerActors)
	{
		HandoverActor->SetRole(ENetRole::ROLE_Authority);
		UE_LOG(LogChanneld, Verbose, TEXT("Set %s to back to ROLE_Authority after ChannelDataUpdate"), *HandoverActor->GetName());
		
		if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
		{
			// ensureAlwaysMsgf(HandoverPC->GetPawn() != nullptr, TEXT("PlayerController doesn't have Pawn set properly"));
			// ensureAlwaysMsgf(HandoverPC->PlayerState != nullptr, TEXT("PlayerController doesn't have PlayerState set properly"));
			
			if (auto ClientConn = Cast<UChanneldNetConnection>(HandoverPC->NetConnection))
			{
				// Set the role to SimulatedProxy so it won't unpossess the pawn in BeginSpectatingState()
				HandoverPC->SetRole(ROLE_SimulatedProxy);
				// Call this after HandleChannelDataUpdate(), as RegisterPlayer() requires the PC has PlayerState set.
				InitPlayerController(ClientConn, HandoverPC);
				UE_LOG(LogChanneld, Verbose, TEXT("[Server] Initialized PlayerController with client conn %d"), ClientConn->GetConnId());
			}
			else
			{
				UE_LOG(LogChanneld, Error, TEXT("[Server] Unable to associate PlayerController with any NetConnection."));
			}			
			
			HandoverPC->ServerAcknowledgePossession(HandoverPC->GetPawn());

			ensureAlwaysMsgf(HandoverPC->GetPawn() && HandoverPC->GetPawn()->GetNetConnection(),
				TEXT("Handover Pawn '%s' should have NetConn set!"), *GetNameSafe(HandoverPC->GetPawn()));
		}
	}

	// Pass 2
	for (auto HandoverObj : HandoverObjs)
	{
		if (auto Actor = Cast<AActor>(HandoverObj))
		{
			Actor->SetRole(ChanneldUtils::ServerGetActorNetRole(Actor));
			UE_LOG(LogChanneld, Verbose, TEXT("[Server] Set %s back to %s after the handover"), *Actor->GetName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), Actor->GetLocalRole()));

			if (bHasAuthority)
			{
				if (auto PC = Cast<APlayerController>(HandoverObj))
				{
					if (auto ClientConn = Cast<UChanneldNetConnection>(PC->NetConnection))
					{
						// Authority server updates the client's interest area no matter if it's cross-server handover.
						ClientConn->PlayerEnterSpatialChannelEvent.Broadcast(ClientConn, HandoverMsg->dstchannelid());
					}
					else
					{
						UE_LOG(LogChanneld, Warning, TEXT("Failed to update spatial interest for %s"), *PC->GetName());
					}
				}
			}
		}
	}

	// Pass 3
	for (auto HandoverActor : CrossServerActors)
	{
		/*
		// Make sure the handover actors that fall into the authority area of current server start sending ChannelDataUpdate message. It has to be done AFTER calling HandleChannelDataUpdate.
		// NOTE: Setting to ROLE_Authority should be performed at last, as many UE code assume it's in the client.
		HandoverActor->SetRole(ENetRole::ROLE_Authority);
		UE_LOG(LogChanneld, Verbose, TEXT("Set %s to back to ROLE_Authority after ChannelDataUpdate"), *HandoverActor->GetName());
		*/
		
		if (auto RepComp = Cast<UChanneldReplicationComponent>(HandoverActor->GetComponentByClass(UChanneldReplicationComponent::StaticClass())))
		{
			RepComp->OnCrossServerHandover.Broadcast();
		}
	}

	GEngine->GetEngineSubsystem<UChanneldMetrics>()->Handovers->Add({{"handoverObjs", std::to_string(HandoverData.entities_size())}, {"crossServerObjs", std::to_string(CrossServerActors.Num())}}).Increment();
}

void USpatialChannelDataView::ServerHandleSubToChannel(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	const auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Sub %s conn %d to %s channel %d, data access: %s"),
		SubResultMsg->conntype() == channeldpb::CLIENT ? TEXT("client") : TEXT("server"),
		SubResultMsg->connid(),
		UTF8_TO_TCHAR(channeldpb::ChannelType_Name(SubResultMsg->channeltype()).c_str()),
		ChId,
		UTF8_TO_TCHAR(channeldpb::ChannelDataAccess_Name(SubResultMsg->suboptions().dataaccess()).c_str()));
	
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL)
	{
		// A client is subscribed to a spatial channel the server owns
		if (SubResultMsg->conntype() == channeldpb::CLIENT)
		{
			/* No need to do anything here - just wait for the client to send the handshake or Hello message.
			 * Then the server will add the client connection and call OnAddClientConnection()
			ClientInChannels.Emplace(SubResultMsg->connid(), ChId);
			if (auto ClientConn = CreateClientConnection(SubResultMsg->connid(), ChId))
			{
				GetWorld()->GetAuthGameMode()->RestartPlayer(ClientConn->PlayerController);
			}
			*/
		}
		// This server has created a spatial channel and has been subscribed to it.
		else
		{
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ChId);
		}
	}
}

void USpatialChannelDataView::ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId)
{
	// A client leaves the spatial channel
	if (ChannelType == channeldpb::SPATIAL)
	{
		ClientInChannels.Remove(ClientConnId);
	}
	// A client leaves the game - close and remove the client connection.
	else if (ChannelType == channeldpb::GLOBAL)
	{
		Super::ServerHandleClientUnsub(ClientConnId, ChannelType, ChId);
	}
}

void USpatialChannelDataView::OnRemovedProvidersFromChannel(Channeld::ChannelId ChId, channeldpb::ChannelType ChannelType, const TSet<FProviderInternal>& RemovedProviders)
{
	if (Connection->IsServer())
	{
		return;
	}

	if (ChannelType != channeldpb::SPATIAL)
	{
		return;
	}

	for (const auto& Provider : RemovedProviders)
	{
		if (!Provider.IsValid())
		{
			continue;
		}

		if (UObject* Obj = Provider->GetTargetObject())
		{
			if (!ClientDeleteObject(Obj))
			{
				UE_LOG(LogChanneld, Log, TEXT("Skipped deleting the net relevant actor. Now add the provider back to the channel."));
				AddProvider(ChId, Provider.Get());
			}
		}
	}
}

bool USpatialChannelDataView::ClientDeleteObject(UObject* Obj)
{
	UE_LOG(LogChanneld, Log, TEXT("Deleting object that is no longer in the client's interest area: %s"), *Obj->GetName());
	if (auto Actor = Cast<AActor>(Obj))
	{
		// Skip well-known actors
		if (Actor->bAlwaysRelevant)
		{
			return false;
		}
		
		// AutonomousProxy actors (PlayerControllers, Pawn, PlayerStates, etc.) are always relevant to the client.
		if (Actor->GetLocalRole() == ROLE_AutonomousProxy)
		{
			return false;
		}
		
		// Call the actor's IsNetRelevantFor() to determine if the actor should be deleted or not.
		if (GetMutableDefault<UChanneldSettings>()->bUseNetRelevancyForUninterestedActors)
		{
			if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
			{
				if (auto PC = NetDriver->GetServerConnection()->PlayerController)
				{
					FVector ViewLocation;
					FRotator ViewRotation;
					PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
					if (Actor->IsNetRelevantFor(PC, PC->GetViewTarget(), ViewLocation))
					{
						return false;
					}
				}
			}
		}
				
		GetWorld()->DestroyActor(Actor, true);
	}
	else
	{
		Obj->ConditionalBeginDestroy();
	}
			
	// Remove the object from the GuidCache so it can be re-created in CheckUnspawnedObject() when the client regain the interest.
	auto GuidCache = GetWorld()->NetDriver->GuidCache;
	FNetworkGUID NetId;
	if (GuidCache->NetGUIDLookup.RemoveAndCopyValue(Obj, NetId))
	{
		GuidCache->ObjectLookup.Remove(NetId);
	}
	
	return true;
}

bool USpatialChannelDataView::ConsumeChannelUpdateData(Channeld::ChannelId ChId, google::protobuf::Message* UpdateData)
{
	// We can't consume the spatial channel update data in the normal merge process.
	if (auto ChannelInfo = Connection->SubscribedChannels.Find(ChId))
	{
		if (ChannelInfo->ChannelType == EChanneldChannelType::ECT_Spatial)
		{
			return false;
		}
	}
	
	return Super::ConsumeChannelUpdateData(ChId, UpdateData);
}

bool USpatialChannelDataView::TryToResolveObjects(Channeld::ChannelId ChId, TArray<uint32> NetGUIDs)
{
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		return false;
	}
	
	TArray<uint32> UnresolvedNetGUIDs;
	for (uint32 NetGUID : NetGUIDs)
	{
		// The object has just been deleted, don't re-spawn it here.
		if (SuppressedNetIdsToResolve.Contains(NetGUID))
		{
			UE_LOG(LogChanneld, Verbose, TEXT("The object has just been deleted during recent handover, ignore resolving it: %d"), NetGUID);
			continue;
		}
		
		if (!ResolvingNetGUIDs.Contains(NetGUID))
		{
			// Don't use IsGUIDRegistered - the object may still exist in GuidCache but has been deleted.
			if (auto CacheObj = NetDriver->GuidCache->ObjectLookup.Find(FNetworkGUID(NetGUID)))
			{
				if (CacheObj->Object.IsValid())
				{
					continue;
				}
			}
			
			UnresolvedNetGUIDs.Add(NetGUID);
		}
	}

	if (UnresolvedNetGUIDs.Num() == 0)
	{
		// Stop the channel data from being consumed in UChannelDataView::HandleChannelDataUpdate if still under resolving.
		return ResolvingNetGUIDs.Num() > 0;
	}

	unrealpb::GetUnrealObjectRefMessage Msg;
	FString LogStr;
	for (uint32 NetGUID : UnresolvedNetGUIDs)
	{
		Msg.add_netguid(NetGUID);
		ResolvingNetGUIDs.Add(NetGUID);
		LogStr.Appendf(TEXT("%d, "), NetGUID);
	}
	
	Connection->Send(ChId, unrealpb::GET_UNREAL_OBJECT_REF, Msg);
	UE_LOG(LogChanneld, Verbose, TEXT("Sent GetUnrealObjectRefMessage to channel %d, NetIds: %s"), ChId, *LogStr);

	return true;
}

bool USpatialChannelDataView::CheckUnspawnedObject(Channeld::ChannelId ChId, const google::protobuf::Message* ChannelData)
{
	// Only client needs to spawn the objects.
	if (Connection->IsServer())
	{
		return false;
	}

	/* With the Entity channel, the Spatial channel data schema is fixed and contains the UnrealObjectRef to resolve
	 * the objects that aren't spawned yet.
	 * 
	const FName MessageName = UTF8_TO_TCHAR(ChannelData->GetTypeName().c_str());
	auto Processor = ChanneldReplication::FindChannelDataProcessor(MessageName);
	ensureMsgf(Processor, TEXT("Unable to find channel data processor for message: %s"), UTF8_TO_TCHAR(ChannelData->GetTypeName().c_str()));
	if (!Processor)
	{
		return false;
	}
	
	TArray<uint32> NetGUIDs = Processor->GetRelevantNetGUIDsFromChannelData(ChannelData);
	if (NetGUIDs.Num() == 0)
	{
		return false;
	}

	return TryToResolveObjects(ChId, NetGUIDs);
	*/

	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		return false;
	}

	EChanneldChannelType ChannelType = GetChanneldSubsystem()->GetChannelTypeByChId(ChId);
	if (ChannelType == EChanneldChannelType::ECT_Entity)
	{
		FNetworkGUID NetGUID(ChId);
		// Don't use IsGUIDRegistered - the object may still exist in GuidCache but has been deleted.
		if (auto CacheObj = NetDriver->GuidCache->ObjectLookup.Find(NetGUID))
		{
			if (CacheObj->Object.IsValid())
			{
				return false;
			}
		}

		auto ObjRefField = ChannelData->GetDescriptor()->field(0);
		if (ObjRefField == nullptr)
		{
			return true;
		}
		if (ObjRefField->name() == "objRef" && ChannelData->GetReflection()->HasField(*ChannelData, ObjRefField))
		{
			auto& ObjRef = static_cast<const unrealpb::UnrealObjectRef&>(ChannelData->GetReflection()->GetMessage(*ChannelData, ObjRefField));
			TCHAR* ClassPath = UTF8_TO_TCHAR(ObjRef.classpath().c_str());
			if (UClass* EntityClass = LoadObject<UClass>(nullptr, ClassPath))
			{
				// Do not resolve other PlayerController on the client.
				if (EntityClass->IsChildOf(APlayerController::StaticClass()))
				{
					return true;
				}
			}

			UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from unresolved EntityChannelData, NetId: %d"), ObjRef.netguid());
			UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
			if (NewObj)
			{
				AddObjectProvider(ChId, NewObj);
				/* We don't know the spatial channel id of the entity yet.
				OnNetSpawnedObject(NewObj, ChId);
				*/
			}
		}
	}

	if (ChannelType != EChanneldChannelType::ECT_Spatial)
	{
		return false;
	}
	
	auto SpatialChannelData = static_cast<const unrealpb::SpatialChannelData*>(ChannelData);
	for (auto& Pair : SpatialChannelData->entities())
	{
		FNetworkGUID NetGUID(Pair.first);
		// Don't use IsGUIDRegistered - the object may still exist in GuidCache but has been deleted.
		if (auto CacheObj = NetDriver->GuidCache->ObjectLookup.Find(NetGUID))
		{
			if (CacheObj->Object.IsValid())
			{
				continue;
			}
		}

		auto& ObjRef = Pair.second.objref();
		TCHAR* ClassPath = UTF8_TO_TCHAR(ObjRef.classpath().c_str());
		if (UClass* EntityClass = LoadObject<UClass>(nullptr, ClassPath))
		{
			// Do not resolve other PlayerController on the client.
			if (EntityClass->IsChildOf(APlayerController::StaticClass()))
			{
				continue;
			}
		}

		// Set up the mapping before actually spawn it, so AddProvider() can find the mapping.
		SetOwningChannelId(ObjRef.netguid(), ChId);
			
		// Also add the mapping of all context NetGUIDs
		for (auto& ContextObj : ObjRef.context())
		{
			SetOwningChannelId(ContextObj.netguid(), ChId);
		}

		UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from unresolved SpatialEntityState, NetId: %d"), ObjRef.netguid());
		UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
		if (NewObj)
		{
			AddObjectProviderToDefaultChannel(NewObj);
			OnNetSpawnedObject(NewObj, ChId);
		}
	}
	
	return false;
}

void USpatialChannelDataView::ClientHandleGetUnrealObjectRef(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const unrealpb::GetUnrealObjectRefResultMessage*>(Msg);
	for (auto& ObjRef : ResultMsg->objref())
	{
		// Set up the mapping before actually spawn it, so AddProvider() can find the mapping.
		SetOwningChannelId(ObjRef.netguid(), ChId);
			
		// Also add the mapping of all context NetGUIDs
		for (auto& ContextObj : ObjRef.context())
		{
			SetOwningChannelId(ContextObj.netguid(), ChId);
		}

		UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from GetUnrealObjectRefResultMessage, NetId: %d"), ObjRef.netguid());
		UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
		if (NewObj)
		{
			AddObjectProviderToDefaultChannel(NewObj);
			OnNetSpawnedObject(NewObj, ChId);
		}
		
		ResolvingNetGUIDs.Remove(ObjRef.netguid());
	}

	// Now there should be no unresolved object in the channel, so we can consume the accumulated data.
	if (auto UpdateData = ReceivedUpdateDataInChannels.FindRef(ChId))
	{
		ConsumeChannelUpdateData(ChId, UpdateData);
	}
}

void USpatialChannelDataView::InitServer()
{
	// Spatial server has no authority over the GameState.
	GetWorld()->GetAuthGameMode()->GameState->SetRole(ENetRole::ROLE_SimulatedProxy);
	Super::InitServer();

	NetConnForSpawn = NewObject<UChanneldNetConnection>(GetTransientPackage(), UChanneldNetConnection::StaticClass());
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	NetConnForSpawn->InitBase(NetDriver, NetDriver->GetSocket(), FURL(), USOCK_Open);
	ChanneldUtils::InitNetConnForSpawn(NetConnForSpawn);
	
	if (UClass* PlayerStartLocatorClass = GetMutableDefault<UChanneldSettings>()->PlayerStartLocatorClass)
	{
		PlayerStartLocator = NewObject<UPlayerStartLocatorBase>(this, PlayerStartLocatorClass);
	}
	ensureAlwaysMsgf(PlayerStartLocator, TEXT("[Server] SpatialChannelDataView has not specified the PlayerStartLocatorClass."));

	// Set the player's start position selected by the locator.
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		auto NetConn = Cast<UChanneldNetConnection>(NewPlayer->GetNetConnection());
		AActor* StartSpot = nullptr;
		FVector StartPos = PlayerStartLocator->GetPlayerStartPosition(NetConn->GetConnId(), StartSpot);
		// Spawn a PlayerStart actor if it doesn't exist.
		if (!StartSpot)
		{
			StartSpot = GetWorld()->SpawnActor<APlayerStart>();
			StartSpot->SetActorLocation(StartPos);
		}
		NewPlayer->StartSpot = StartSpot;
		NewPlayer->SetInitialLocationAndRotation(StartPos, NewPlayer->GetControlRotation());
		UE_LOG(LogChanneld, Log, TEXT("%s selected %s for client %d"), *PlayerStartLocator->GetName(), *StartPos.ToCompactString(), NetConn->GetConnId());
	});
	
	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialChannelDataView::ServerHandleSubToChannel);
	
	Connection->RegisterMessageHandler(unrealpb::HANDOVER_CONTEXT, new unrealpb::GetHandoverContextMessage, this, &USpatialChannelDataView::ServerHandleGetHandoverContext);
	
	Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_HANDOVER, this, &USpatialChannelDataView::ServerHandleHandover);

	Connection->RegisterMessageHandler(unrealpb::SERVER_PLAYER_LEAVE, new channeldpb::ServerForwardMessage, [&](UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
	{
		channeldpb::UnsubscribedFromChannelResultMessage UnsubMsg;
		if (UnsubMsg.ParseFromString(static_cast<const channeldpb::ServerForwardMessage*>(Msg)->payload()))
		{
			ServerHandleClientUnsub(UnsubMsg.connid(), UnsubMsg.channeltype(), ChId);
		}
		else
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse the payload of the SERVER_PLAYER_LEAVE message to UnsubscribedFromChannelResultMessage. ChannelId: %d"), ChId);
		}
	});

	// Only the master server has the write access to the GLOBAL channel.
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	Connection->SubToChannel(Channeld::GlobalChannelId, &SubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* _)
	{
		channeldpb::ChannelSubscriptionOptions SpatialSubOptions;
		SpatialSubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
		
		Connection->CreateSpatialChannel(TEXT(""), &SpatialSubOptions, ChannelInitData ? ChannelInitData->GetMessage() : nullptr, nullptr,
			[](const channeldpb::CreateSpatialChannelsResultMessage* ResultMsg)
		{
			FString StrChIds;
			for (int i = 0; i < ResultMsg->spatialchannelid_size(); i++)
			{
				StrChIds.AppendInt(ResultMsg->spatialchannelid(i));
				if (i < ResultMsg->spatialchannelid_size() - 1)
					StrChIds.AppendChar(',');
			}
			UE_LOG(LogChanneld, Log, TEXT("Created spatial channels: %s"), *StrChIds);
		});
	});

	/*
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		APawn* NewPawn = NewPlayer->GetPawn();

		unrealpb::ServerSpawnedPlayerMessage Msg;
		Msg.set_clientconnid(CastChecked<UChanneldNetConnection>(NewPlayer->GetNetConnection())->GetConnId());
		Msg.mutable_startpos()->CopyFrom(ChanneldUtils::GetVectorPB(NewPawn->GetActorLocation()));
		// Send to Master server
		Connection->Send(Channeld::GlobalChannelId, unrealpb::SERVER_PLAYER_SPAWNED, Msg);
	});
	*/
}

void USpatialChannelDataView::ClientHandleSubToChannel(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	UE_LOG(LogChanneld, Log, TEXT("[Client] Sub to %s channel %d"), UTF8_TO_TCHAR(channeldpb::ChannelType_Name(SubResultMsg->channeltype()).c_str()), ChId);
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL /*&& SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS*/)
	{
		if (bClientInMasterServer)
		{
			bClientInMasterServer = false;
			UE_LOG(LogChanneld, Log, TEXT("==================== Client no longer in Master server ===================="));
			
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ChId);

			// Join the spatial server which Master server chooses for the player.
			const FName LevelName("127.0.0.1");
			GetChanneldSubsystem()->OpenLevel(LevelName);

			// Clear the actors (PlayerController, Pawn, PlayerState, etc.) spawned when the client was in the Master server.

			if (GetMutableDefault<UChanneldSettings>()->bEnableSpatialVisualizer)
			{
				Visualizer = NewObject<USpatialVisualizer>(this, USpatialVisualizer::StaticClass());
				Visualizer->Initialize(Connection);
			}
		}
	}
}

void USpatialChannelDataView::ClientHandleHandover(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	// unrealpb::HandoverData HandoverData;
	unrealpb::SpatialChannelData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);

	TArray<FString> NetIds;
	// for (auto& HandoverContext : HandoverData.context())
	for (auto& Pair : HandoverData.entities())
	{
		NetIds.Add(FString::FromInt(Pair.second.objref().netguid()));
	}
	UE_LOG(LogChanneld, Log, TEXT("ChannelDataHandover from channel %d to %d, object netIds: %s"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), *FString::Join(NetIds, TEXT(",")));

	SuppressedNetIdsToResolve.Empty();

	// Does the client has interest over the handover objects?
	const bool bHasInterest = Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid());

	if (bHasInterest)
	{
		// Spawn the unresolved objects from the spatial entities
		CheckUnspawnedObject(HandoverMsg->dstchannelid(), &HandoverData);
	}
	
	// for (auto& HandoverContext : HandoverData.context())
	for (auto& Pair : HandoverData.entities())
	{
		FNetworkGUID NetId(Pair.second.objref().netguid());

		// Update NetId-ChannelId mapping
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());

		UObject* Obj = GetObjectFromNetGUID(NetId);
		if (!Obj)
		{
			continue;
		}

		if (bHasInterest)
		{
			// Move data provider to the new channel
			if (Obj->Implements<UChannelDataProvider>())
			{
				MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(Obj), false);
			}
			else if (Obj->IsA<AActor>())
			{
				for (auto& Comp : Cast<AActor>(Obj)->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
				{
					MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(Comp), false);
				}
			}

			if (Obj->IsA<APawn>())
			{
				APawn* Pawn = Cast<APawn>(Obj);
				// Only the client's owning Pawn has the PlayerController
				if (Pawn->GetController())
				{
					// Update the channelId for LowLevelSend() if it's a player's pawn that been handed over.
					GetChanneldSubsystem()->SetLowLevelSendToChannelId(HandoverMsg->dstchannelid());

					// Also update the NetId-ChannelId mapping of the player controller (otherwise RPCs won't be processed properly)
					SetOwningChannelId(GetNetId(Pawn->GetController()), HandoverMsg->dstchannelid());
				}
			}
		}
		else
		{
			if (ClientDeleteObject(Obj))
			{
				RemoveObjectProviderAll(Obj, false);
				SuppressedNetIdsToResolve.Add(NetId.Value);
		
				// Unsub from the entity channel which channelId equals to the NetId
				if (Connection->SubscribedChannels.Contains(NetId.Value))
				{
					Connection->UnsubFromChannel(NetId.Value);
				}
			}
			else
			{
				UE_LOG(LogChanneld, Log, TEXT("Skipped deleting the net relevant actor."));
			}
		}
	}

	// Applies the channel data update to spawn the objects that just entered the client's interest areas.
	/*
	if (HandoverData.has_channeldata() && bHasInterest)
	{
		channeldpb::ChannelDataUpdateMessage UpdateMsg;
		UpdateMsg.mutable_data()->CopyFrom(HandoverData.channeldata());
		UE_LOG(LogChanneld, Verbose, TEXT("Applying handover channel data:"));
		HandleChannelDataUpdate(_, ChId, &UpdateMsg);
	}
	*/
	for (auto& Pair : HandoverData.entities())
	{
		if (Pair.second.has_entitydata())
		{
			channeldpb::ChannelDataUpdateMessage UpdateMsg;
			UpdateMsg.mutable_data()->CopyFrom(Pair.second.entitydata());
			UE_LOG(LogChanneld, Verbose, TEXT("Applying handover channel data to entity %d"), Pair.first);
			HandleChannelDataUpdate(_, Pair.first, &UpdateMsg);
		}
	}
}

void USpatialChannelDataView::InitClient()
{
	Super::InitClient();

	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialChannelDataView::ClientHandleSubToChannel);
	Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_HANDOVER, this, &USpatialChannelDataView::ClientHandleHandover);
	Connection->RegisterMessageHandler(unrealpb::GET_UNREAL_OBJECT_REF, new unrealpb::GetUnrealObjectRefResultMessage, this, &USpatialChannelDataView::ClientHandleGetUnrealObjectRef);
	
	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(GlobalChannelFanOutIntervalMs);
	GlobalSubOptions.set_fanoutdelayms(GlobalChannelFanOutDelayMs);
	Connection->SubToChannel(Channeld::GlobalChannelId, &GlobalSubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* Msg)
	{
		// GetChanneldSubsystem()->SetLowLevelSendToChannelId(Channeld::GlobalChannelId);
		// // Join Master server as soon as the client subscribed.
		// GetChanneldSubsystem()->OpenLevel(FName("127.0.0.1"));
		bClientInMasterServer = true;
		UE_LOG(LogChanneld, Log, TEXT("==================== Client starts in Master server ===================="));
	});
}

Channeld::ChannelId USpatialChannelDataView::GetOwningChannelId(AActor* Actor) const
{
	// GameState is owned by the Master server.
	if (Actor->IsA<AGameStateBase>())
	{
		return Channeld::GlobalChannelId;
	}
	return Super::GetOwningChannelId(Actor);
}

void USpatialChannelDataView::SetOwningChannelId(const FNetworkGUID NetId, Channeld::ChannelId ChId)
{
	Super::SetOwningChannelId(NetId, ChId);

	if (Visualizer)
	{
		Visualizer->OnUpdateOwningChannel(GetObjectFromNetGUID(NetId), ChId);
	}
}

bool USpatialChannelDataView::GetSendToChannelId(UChanneldNetConnection* NetConn, uint32& OutChId) const
{
	const Channeld::ChannelId* ChId = ClientInChannels.Find(NetConn->GetConnId());
	if (ChId)
	{
		OutChId = *ChId;
		return true;
	}
	return false;
}

void USpatialChannelDataView::AddProviderToDefaultChannel(IChannelDataProvider* Provider)
{
	if (bSuppressAddProviderAndSendOnServerSpawn)
	{
		return;
	}

	FNetworkGUID NetId = GetNetId(Provider);
	
	// Also add the provider to the entity channel
	if (auto ChannelInfo = Connection->SubscribedChannels.Find(NetId.Value))
	{
		if (ChannelInfo->ChannelType == EChanneldChannelType::ECT_Entity)
		{
			AddProvider(NetId.Value, Provider);
		}
	}
	
	if (Connection->IsServer())
	{
		if (NetIdOwningChannels.Contains(NetId))
		{
			Super::AddProviderToDefaultChannel(Provider);
		}
		else if (const AActor* Actor = Cast<AActor>(Provider->GetTargetObject()))
		{
			if (Actor->IsA<AInfo>())
			{
				UE_LOG(LogChanneld, Verbose, TEXT("Ignore querying AInfo %s's spatial channel"), *Actor->GetName());
				return;
			}
			TArray<FVector> Positions;
			Positions.Add(Actor->GetActorLocation());
			Connection->QuerySpatialChannel(Positions, [&, NetId, Provider, Actor](const channeldpb::QuerySpatialChannelResultMessage* ResultMsg)
			{
				const Channeld::ChannelId SpatialChId = ResultMsg->channelid(0);
				UE_LOG(LogChanneld, Log, TEXT("Queried spatial channelId %d for provider: %s"), SpatialChId, *IChannelDataProvider::GetName(Provider));
				SetOwningChannelId(NetId, SpatialChId);
				AddProvider(SpatialChId, Provider);
				// Add the PlayerController and the PlayerState to the same spatial channel as the Pawn.
				if (const APawn* Pawn = Cast<APawn>(Actor))
				{
					// Set up the handover group for the pawn
					TArray<Channeld::EntityId> HandoverGroup;
					HandoverGroup.Add(NetId.Value);
					
					if (auto Controller = Pawn->GetController())
					{
						const auto ControllerNetId = GetNetId(Controller);
						auto OwningChId = UChannelDataView::GetOwningChannelId(ControllerNetId);
						if (OwningChId != Channeld::InvalidChannelId)
						{
							// Should make sure the PlayerController is not added to other spatial channels.
							RemoveObjectProvider(OwningChId, Controller, false);
						}
						SetOwningChannelId(ControllerNetId, SpatialChId);
						AddObjectProvider(SpatialChId, Controller);

						HandoverGroup.Add(ControllerNetId.Value);
					}

					if (auto PlayerState = Pawn->GetPlayerState())
					{
						auto PlayerStateNetId = GetNetId(PlayerState);
						auto OwningChId = UChannelDataView::GetOwningChannelId(PlayerStateNetId);
						if (OwningChId != Channeld::InvalidChannelId)
						{
							// Should make sure the PlayerState is not added to other spatial channels.
							RemoveObjectProvider(OwningChId, PlayerState, false);
						}
						SetOwningChannelId(PlayerStateNetId, SpatialChId);
						AddObjectProvider(SpatialChId, PlayerState);

						HandoverGroup.Add(PlayerStateNetId.Value);
					}

					Connection->AddToEntityGroup(NetId.Value, channeldpb::HANDOVER, HandoverGroup);
				}
				
				//SendSpawnToAdjacentChannels(Provider->GetTargetObject(), SpatialChId);
			});
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to add provider: can't retrieve location from provider: %s"), *IChannelDataProvider::GetName(Provider));
		}
	}
	else
	{
		Super::AddProviderToDefaultChannel(Provider);
	}
}

void USpatialChannelDataView::OnAddClientConnection(UChanneldNetConnection* ClientConnection, Channeld::ChannelId ChId)
{
	ClientInChannels.Emplace(ClientConnection->GetConnId(), ChId);
}

void USpatialChannelDataView::OnRemoveClientConnection(UChanneldNetConnection* ClientConn)
{
	ClientInChannels.Remove(ClientConn->GetConnId());
}

void USpatialChannelDataView::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn)
{
	Super::OnClientPostLogin(GameMode, NewPlayer, NewPlayerConn);

	Channeld::ChannelId SpatialChId;
	if (GetSendToChannelId(NewPlayerConn, SpatialChId))
	{
		// The PC's spawn location should have been set correctly by SendSpawnToConn().
		NewPlayerConn->PlayerEnterSpatialChannelEvent.Broadcast(NewPlayerConn, SpatialChId);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to update spatial interest for %s"), *NewPlayer->GetName());
	}
}

void USpatialChannelDataView::OnNetSpawnedObject(UObject* Obj, const Channeld::ChannelId ChId)
{
	if (Visualizer)
	{
		Visualizer->OnSpawnedObject(Obj, ChId);
	}

	if (Obj->IsA<AGameStateBase>())
	{
		return;
	}
		
	uint32 NetId = GetNetId(Obj).Value;
	if (!Connection->SubscribedChannels.Contains(NetId))
	{
		// Sub to the entity channel which channelId equals to the NetId
		// TODO: subOptions should reflect the RepConditions of the properties
		Connection->SubToChannel(NetId, nullptr, [this, Obj, NetId](const channeldpb::SubscribedToChannelResultMessage* ResultMsg)
		{
			AddObjectProvider(NetId, Obj);
		});
	}
}

bool USpatialChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	// Spatial channels don't support Gameplay Debugger yet.
	if (Obj->GetClass()->GetFName() == GameplayerDebuggerClassName)
	{
		return false;
	}

	// GameState is in GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		SetOwningChannelId(NetId, Channeld::GlobalChannelId);
	}

	if (bSuppressAddProviderAndSendOnServerSpawn)
	{
		return false;
	}
	
	// Don't set the NetId-ChannelId mapping, as we don't have the spatial channelId of the object yet.
	// The spatial channelId will be queried in AddProviderToDefaultChannel()
	AddObjectProviderToDefaultChannel(Obj);
	
	return true;
}

void USpatialChannelDataView::OnDestroyedActor(AActor* Actor, const FNetworkGUID NetId)
{
	// Server keeps the NetId-ChannelId mapping for cross-server RPC.
	if (Connection->IsServer())
	{
		RemoveObjectProviderAll(Actor, false);
		
		// Unsub from the entity channel which channelId equals to the NetId
		if (Connection->SubscribedChannels.Contains(NetId.Value))
		{
			Connection->UnsubFromChannel(NetId.Value);
		}
		return;
	}
	
	Super::OnDestroyedActor(Actor, NetId);
}

void USpatialChannelDataView::SendSpawnToConn_EntityChannelReady(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		// Location must be set for channeld to update the spatial channelId.
		FVector Location = Actor->GetActorLocation();
		// Set the location of PlayerController or PlayerState to the start position, to make sure them are in the same spatial channel as the pawn.
		if (Actor->IsA<APlayerController>() || Actor->IsA<APlayerState>())
		{
			AActor* StartSpot;
			Location = PlayerStartLocator->GetPlayerStartPosition(OwningConnId, StartSpot);
			if (auto PC = Cast<APlayerController>(Actor))
			{
				PC->SetInitialLocationAndRotation(Location, PC->GetControlRotation());
			}
			else
			{
				Actor->SetActorLocation(Location);
			}
		}
		Channeld::ChannelId SendToChId;
		if (GetSendToChannelId(NetConn, SendToChId))
		{
			NetConn->SendSpawnMessage(Actor, Actor->IsA<APlayerController>() ? ROLE_AutonomousProxy : Actor->GetRemoteRole(), SendToChId, OwningConnId, &Location);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to send spawn message to client: can't find the channelId for connId: %d, actor: %s"), NetConn->GetConnId(), *Actor->GetName());
		}
	}
	else
	{
		Super::SendSpawnToConn(Obj, NetConn, OwningConnId);
	}
}

void USpatialChannelDataView::SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	// GameState is spawned via GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		NetConn->SendSpawnMessage(Obj, ROLE_SimulatedProxy, Channeld::GlobalChannelId);
		return;
	}

	const uint32 NetId = GetNetId(Obj, true).Value;
	// The entity channel already exists, send directly
	if (Connection->SubscribedChannels.Contains(NetId))
	{
		SendSpawnToConn_EntityChannelReady(Obj, NetConn, OwningConnId);
		return;
	}

	// Create the entity channel before sending spawn message
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
	Connection->CreateEntityChannel(Channeld::GlobalChannelId, Obj, NetId, TEXT(""), &SubOptions, GetEntityData(Obj)/*nullptr*/, nullptr,
		[this, Obj, NetConn, OwningConnId](const channeldpb::CreateChannelResultMessage* ResultMsg)
		{
			AddObjectProvider(ResultMsg->channelid(), Obj);

			SendSpawnToConn_EntityChannelReady(Obj, NetConn, OwningConnId);
		});

	UE_LOG(LogChanneld, Verbose, TEXT("Creating entity channel for obj: %s, netId: %d"), *Obj->GetName(), NetId);
}

// Broadcast the spawning to all clients subscribed in the nearby spatial channels
void USpatialChannelDataView::SendSpawnToClients(UObject* Obj, uint32 OwningConnId)
{
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("USpatialChannelDataView::SendSpawnToClients: Unable to get ChanneldNetDriver"));
		return;
	}
	
	const FNetworkGUID NetId = NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
	// It doesn't matter if the spatial channelId is not right, as channeld will adjust it based on the location.
 	Channeld::ChannelId SpatialChId = Super::GetOwningChannelId(NetId);
	if (SpatialChId == Channeld::InvalidChannelId)
	{
		SpatialChId = GetChanneldSubsystem()->LowLevelSendToChannelId.Get();
	}
	
	bool bWellKnown = false;
	if (const AActor* Actor = Cast<AActor>(Obj))
	{
		bWellKnown = Actor->bAlwaysRelevant;
	}
	
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);

	Connection->CreateEntityChannel(SpatialChId, Obj, NetId.Value, bWellKnown ? TEXT("well-known") : TEXT(""), &SubOptions, GetEntityData(Obj)/*nullptr*/, nullptr,
		[this, NetId, Obj, bWellKnown, OwningConnId, SpatialChId, NetDriver](const channeldpb::CreateChannelResultMessage* _)
		{
			AddObjectProvider(NetId.Value, Obj);
	
			unrealpb::SpawnObjectMessage SpawnMsg;
	
			// As we don't have any specific NetConnection to export the NetId, use a virtual one.
			SpawnMsg.mutable_obj()->CopyFrom(*ChanneldUtils::GetRefOfObject(Obj, NetConnForSpawn, true));
			/* Moved to ChanneldUtils::GetRefOfObject
			// Clear the export map and ack state so everytime we can get a full export.
			ResetNetConnForSpawn();
			*/
	
			if (const AActor* Actor = Cast<AActor>(Obj))
			{
				// Broadcast the EntityChannelCreated event to the actor so it can do some initialization.
				if (auto RepComp = Cast<UChanneldReplicationComponent>(Actor->GetComponentByClass(UChanneldReplicationComponent::StaticClass())))
				{
					RepComp->OnEntityChannelCreated.Broadcast(NetId.Value);
				}
				
				SpawnMsg.set_localrole(Actor->GetRemoteRole());
				if (OwningConnId > 0)
				{
					SpawnMsg.mutable_obj()->set_owningconnid(OwningConnId);
				}
				// The spatial info must be set for channeld to adjust the spatial channelId
				SpawnMsg.mutable_location()->MergeFrom(ChanneldUtils::GetVectorPB(Actor->GetActorLocation()));
			}
	
			SpawnMsg.set_channelid(SpatialChId);

			// Broadcast the spawn message to all client connections if it's a Well-Known Object.
			if (bWellKnown)
			{
				Connection->Broadcast(Channeld::GlobalChannelId, unrealpb::SPAWN, SpawnMsg, channeldpb::ALL_BUT_SERVER);
				UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Spawn message of well-known obj: %s, netId: %d"), *Obj->GetName(), NetId.Value);
			}
			else
			{
				Connection->Broadcast(SpatialChId,unrealpb::SPAWN, SpawnMsg, /*channeldpb::ADJACENT_CHANNELS |*/ channeldpb::ALL_BUT_SERVER);
				UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Spawn message to spatial channel %d, obj: %s, netId: %d"), SpatialChId, *Obj->GetName(), NetId.Value);
			}
			
			NetDriver->SetAllSentSpawn(NetId);
		});

	UE_LOG(LogChanneld, Verbose, TEXT("Creating entity channel for obj: %s, netId: %d, spatial channelId: %d"), *Obj->GetName(), NetId.Value, SpatialChId);
}

void USpatialChannelDataView::SendDestroyToClients(UObject* Obj, const FNetworkGUID NetId)
{
	if (bSuppressSendOnServerDestroy)
	{
		return;
	}

	int BroadcastType = channeldpb::ALL_BUT_SENDER;
	// Don't broadcast the destroy of objects that are only spawned in the owning client.
	// Spatial channels don't support Gameplayer Debugger yet.
	if (/*Obj->IsA<APlayerState>() ||*/ Obj->IsA<APlayerController>() || Obj->GetClass()->GetFName() == GameplayerDebuggerClassName)
	{
		/* Still need to send the Destroy message to interested servers, and channeld to clean up the entity channel and SpatialEntityState.
		// return;
		*/
		BroadcastType |= channeldpb::ALL_BUT_CLIENT;
	}
	
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("USpatialChannelDataView::SendDestroyToClients: Unable to get ChanneldNetDriver"));
		return;
	}
	
 	Channeld::ChannelId SpatialChId = Super::GetOwningChannelId(NetId);
	if (SpatialChId == Channeld::InvalidChannelId)
	{
		SpatialChId = GetChanneldSubsystem()->LowLevelSendToChannelId.Get();
	}
	
	unrealpb::DestroyObjectMessage DestroyMsg;
	DestroyMsg.set_netid(NetId.Value);
	DestroyMsg.set_reason(static_cast<uint8>(EChannelCloseReason::Destroyed));
	Connection->Broadcast(SpatialChId, unrealpb::DESTROY, DestroyMsg, /*channeldpb::ADJACENT_CHANNELS |*/ channeldpb::ALL_BUT_SENDER);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Destroy message to spatial channel %d, obj: %s, netId: %d"), SpatialChId, *GetNameSafe(Obj), NetId.Value);
}

