#include "ChanneldPlayerStateReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"

FChanneldPlayerStateReplicator::FChanneldPlayerStateReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	PlayerState = CastChecked<APlayerState>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), APlayerState::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::PlayerState;
	DeltaState = new unrealpb::PlayerState;
}

FChanneldPlayerStateReplicator::~FChanneldPlayerStateReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldPlayerStateReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldPlayerStateReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldPlayerStateReplicator::Tick(float DeltaTime)
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

	if (PlayerState->Score != FullState->score())
	{
		DeltaState->set_score(PlayerState->Score);
		bStateChanged = true;
	}
	if (PlayerState->PlayerId != FullState->playerid())
	{
		DeltaState->set_playerid(PlayerState->PlayerId);
		bStateChanged = true;
	}
	if (PlayerState->Ping != FullState->ping())
	{
#if UE_BUILD_SHIPPING
		DeltaState->set_ping(PlayerState->Ping);
		bStateChanged = true;
#endif
	}
	if (PlayerState->GetPlayerName() != FString(UTF8_TO_TCHAR(FullState->mutable_playername()->c_str())))
	{
		DeltaState->set_playername(TCHAR_TO_UTF8(*PlayerState->GetPlayerName()));
		bStateChanged = true;
	}

	FullState->MergeFrom(*DeltaState);
}

void FChanneldPlayerStateReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!PlayerState.IsValid())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::PlayerState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_score())
	{
		PlayerState->Score = NewState->score();
		PlayerState->OnRep_Score();
	}
	if (NewState->has_playerid())
	{
		PlayerState->PlayerId = NewState->playerid();
		PlayerState->OnRep_PlayerId();
	}
	if (NewState->has_ping())
	{
		PlayerState->Ping = NewState->ping();
	}
	if (NewState->has_playername())
	{
		PlayerState->SetPlayerName(FString(UTF8_TO_TCHAR(NewState->playername().c_str())));
	}
}

