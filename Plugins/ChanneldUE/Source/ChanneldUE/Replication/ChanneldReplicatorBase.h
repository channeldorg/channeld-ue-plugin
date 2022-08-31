#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/SceneComponent.h"
#include "google/protobuf/message.h"
//#include "ChanneldReplicatorBase.generated.h"

//template<class T>
class CHANNELDUE_API FChanneldReplicatorBase// : UObject
{
    //static_assert(TIsDerivedFrom<T, UObject>::IsDerived, "T needs to be inherited from UObject");
    //GENERATED_BODY()
public:
    //template<class T>
    FChanneldReplicatorBase(UObject* InTargetObj);

    FORCEINLINE UObject* GetTargetObject() { return TargetObject.Get(); }
    FORCEINLINE uint32 GetNetGUID() { return NetGUID; }

    FORCEINLINE bool IsStateChanged() { return bStateChanged; }
    virtual google::protobuf::Message* GetState() = 0;
	virtual void ClearState() { bStateChanged = true; }
	// Collect State change for sending ChannelDataUpdate to channeld
    virtual void Tick(float DeltaTime) = 0;
	// Apply ChannelDataUpdate received from channeld
    virtual void OnStateChanged(const google::protobuf::Message* NewState) = 0;

protected:
    TWeakObjectPtr<UObject> TargetObject;

    uint32 NetGUID;

    bool bStateChanged;

};