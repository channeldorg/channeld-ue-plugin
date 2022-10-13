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
    virtual UClass* GetTargetClass() = 0;
    virtual uint32 GetNetGUID();

    // [Server] Is the state changed since last send?
    FORCEINLINE bool IsStateChanged() { return bStateChanged; }
    // [Server] The accumulated delta change before next send
    virtual google::protobuf::Message* GetDeltaState() = 0;
    // [Server] Reset the state change (after send)
	virtual void ClearState() { bStateChanged = true; }
	// [Server] Collect State change for sending ChannelDataUpdate to channeld
    virtual void Tick(float DeltaTime) = 0;
	// [Client] Apply ChannelDataUpdate received from channeld
    virtual void OnStateChanged(const google::protobuf::Message* NewState) = 0;

    virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess) { bSuccess = false; return nullptr; }
    virtual void* DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC) { bSuccess = false; return nullptr; }

protected:
    TWeakObjectPtr<UObject> TargetObject;

    FNetworkGUID NetGUID;

    bool bStateChanged;

};