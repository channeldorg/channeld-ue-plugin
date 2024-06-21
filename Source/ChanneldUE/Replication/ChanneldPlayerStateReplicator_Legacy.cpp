#include "ChanneldPlayerStateReplicator_Legacy.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"

FChanneldPlayerStateReplicator_Legacy::FChanneldPlayerStateReplicator_Legacy(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	PlayerState = CastChecked<APlayerState>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), APlayerState::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::PlayerState;
	DeltaState = new unrealpb::PlayerState;

	{
		auto Property = CastFieldChecked<const FFloatProperty>(PlayerState->GetClass()->FindPropertyByName(FName("Score")));
		ScorePtr = Property->ContainerPtrToValuePtr<float>(PlayerState.Get());
		check(ScorePtr);
	}
	{
		auto Property = CastFieldChecked<const FIntProperty>(PlayerState->GetClass()->FindPropertyByName(FName("PlayerId")));
		PlayerIdPtr = Property->ContainerPtrToValuePtr<int32>(PlayerState.Get());
		check(PlayerIdPtr);
	}
	{
	//	UE4: there's no SetPing(); UE5: use SetCompressedPing()
#if ENGINE_MAJOR_VERSION < 5
		auto Property = CastFieldChecked<const FByteProperty>(PlayerState->GetClass()->FindPropertyByName(FName("Ping")));
		PingPtr = Property->ContainerPtrToValuePtr<uint8>(PlayerState.Get());
		check(PingPtr);
#endif
	}
}

FChanneldPlayerStateReplicator_Legacy::~FChanneldPlayerStateReplicator_Legacy()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldPlayerStateReplicator_Legacy::GetDeltaState()
{
	return DeltaState;
}

void FChanneldPlayerStateReplicator_Legacy::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldPlayerStateReplicator_Legacy::Tick(float DeltaTime)
{
	if (!PlayerState.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!PlayerState->HasAuthority())
	{
		return;
	}

	if (PlayerState->GetScore() != FullState->score())
	{
		DeltaState->set_score(PlayerState->GetScore());
		bStateChanged = true;
	}
	if (PlayerState->GetPlayerId() != FullState->playerid())
	{
		DeltaState->set_playerid(PlayerState->GetPlayerId());
		bStateChanged = true;
	}

	//	UE4 -> UE5: "Ping" -> "CompressedPing"
#if ENGINE_MAJOR_VERSION >= 5
	if (PlayerState->GetCompressedPing() != FullState->ping())
	{
		DeltaState->set_ping(PlayerState->GetCompressedPing());
		bStateChanged = true;
	}
#else
	if (PlayerState->GetPing() != FullState->ping())
	{
		DeltaState->set_ping(PlayerState->GetPing());
		bStateChanged = true;
	}
#endif
	
	if (PlayerState->GetPlayerName() != FString(UTF8_TO_TCHAR(FullState->mutable_playername()->c_str())))
	{
		DeltaState->set_playername(TCHAR_TO_UTF8(*PlayerState->GetPlayerName()));
		bStateChanged = true;
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldPlayerStateReplicator_Legacy::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!PlayerState.IsValid())
	{
		return;
	}

	// Only client needs to apply the new state
	if (PlayerState->HasAuthority())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::PlayerState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_score())
	{
		*ScorePtr = NewState->score();
	}
	if (NewState->has_playerid())
	{
		*PlayerIdPtr = NewState->playerid();
	}
	if (NewState->has_ping())
	{
  #if ENGINE_MAJOR_VERSION >= 5
		PlayerState->SetCompressedPing(NewState->ping());
#else
		*PingPtr = NewState->ping();
#endif
	}
	if (NewState->has_playername())
	{
		PlayerState->SetPlayerName(FString(UTF8_TO_TCHAR(NewState->playername().c_str())));
	}

	if (NewState->has_score())
	{
		PlayerState->OnRep_Score();
	}
	if (NewState->has_playerid())
	{
		PlayerState->OnRep_PlayerId();
	}
}

