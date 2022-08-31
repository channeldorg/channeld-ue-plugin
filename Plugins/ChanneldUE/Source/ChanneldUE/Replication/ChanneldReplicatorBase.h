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

    // [Server] Is the state changed since last send?
    FORCEINLINE bool IsStateChanged() { return bStateChanged; }
    // [Client] State is the accumulated channel data of the target object
    // [Server] State is the accumulated delta change before next send
    virtual google::protobuf::Message* GetState() = 0;
    // [Server] Reset the state change (after send)
	virtual void ClearState() { bStateChanged = true; }
	// [Server] Collect State change for sending ChannelDataUpdate to channeld
    virtual void Tick(float DeltaTime) = 0;
	// [Client] Apply ChannelDataUpdate received from channeld
    virtual void OnStateChanged(const google::protobuf::Message* NewState) = 0;

protected:
    TWeakObjectPtr<UObject> TargetObject;

    uint32 NetGUID;

    bool bStateChanged;

};