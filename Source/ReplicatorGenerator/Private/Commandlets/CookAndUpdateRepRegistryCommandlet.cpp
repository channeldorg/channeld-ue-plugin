// Fill out your copyright notice in the Description page of Project Settings.


#include "Commandlets/CookAndUpdateRepRegistryCommandlet.h"

#include "ReplicatorGeneratorUtils.h"
#include "Components/TimelineComponent.h"

void FLoadedObjectListener::StartListen()
{
	GUObjectArray.AddUObjectCreateListener(this);
}

void FLoadedObjectListener::StopListen()
{
	GUObjectArray.RemoveUObjectCreateListener(this);
}

void FLoadedObjectListener::NotifyUObjectCreated(const UObjectBase* Object, int32 Index)
{
	const UClass* LoadedClass = Object->GetClass();
	while (LoadedClass != nullptr)
	{
		const FString ClassPath = LoadedClass->GetPathName();
		if (CheckedClasses.Contains(ClassPath))
		{
			break;
		}
		CheckedClasses.Add(ClassPath);
		if (LoadedClass == AActor::StaticClass() || LoadedClass == UActorComponent::StaticClass())
		{
			FilteredClasses.Add(LoadedClass);
			break;
		}
		if (ChanneldReplicatorGeneratorUtils::TargetToGenerateChannelDataField(LoadedClass))
		{
			FilteredClasses.Add(LoadedClass);
		}
		LoadedClass = LoadedClass->GetSuperClass();
	}
}

void FLoadedObjectListener::OnUObjectArrayShutdown()
{
	GUObjectArray.RemoveUObjectCreateListener(this);
}

UCookAndUpdateRepRegistryCommandlet::UCookAndUpdateRepRegistryCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCookAndUpdateRepRegistryCommandlet::Main(const FString& CmdLineParams)
{
	FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();

	TSet<FSoftClassPath> LoadedRepClasses;

	FLoadedObjectListener ObjLoadedListener;
	ObjLoadedListener.StartListen();

	const FString AdditionalParam(TEXT(" -SkipShaderCompile"));
	FString NewCmdLine = CmdLineParams;
	NewCmdLine.Append(AdditionalParam);
	int32 Result = Super::Main(NewCmdLine);
	ObjLoadedListener.StopListen();
	if (Result != 0)
	{
		return Result;
	}

	LoadedRepClasses.Append(ObjLoadedListener.FilteredClasses);
	TArray<const UClass*> TargetClasses;
	bool bHasTimelineComponent = false;
	for (const FSoftClassPath& ObjSoftPath : LoadedRepClasses)
	{
		if (const UClass* LoadedClass = ObjSoftPath.TryLoadClass<UObject>())
		{
			TargetClasses.Add(LoadedClass);
			if (LoadedClass == UTimelineComponent::StaticClass())
			{
				bHasTimelineComponent = true;
			}
			else if (!bHasTimelineComponent)
			{
				if (ChanneldReplicatorGeneratorUtils::HasTimelineComponent(LoadedClass))
				{
					TargetClasses.Add(UTimelineComponent::StaticClass());
					bHasTimelineComponent = true;
				}
			}
		}
	}

	if (!GeneratorManager.UpdateReplicationRegistryTable(TargetClasses))
	{
		Result = 1;
	}

	return Result;
}
