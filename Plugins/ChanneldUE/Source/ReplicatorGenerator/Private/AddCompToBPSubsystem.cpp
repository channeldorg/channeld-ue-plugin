// Fill out your copyright notice in the Description page of Project Settings.


#include "AddCompToBPSubsystem.h"
#include "ReplicatorGeneratorUtils.h"
#include "Async/Async.h"
#include "Commandlets/CommandletHelpers.h"
#include "Commandlets/CookAndFilterRepActorCommandlet.h"
#include "Engine/Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogAddRepCompToBP, Log, All);

#define COMMANDLET_LOG_PROXY(Type, Else) \
Else if ((MsgIndex = OutMsg.Find(TEXT(#Type ":"))) != INDEX_NONE) \
{ \
	UE_LOG(LogAddRepCompToBP, Type, TEXT("[Proxy]%s"), *OutMsg + MsgIndex + FString(TEXT(#Type)).Len() + 1); \
}

#define LOCTEXT_NAMESPACE "ChanneldAddCompToBPSubsystem"

namespace AddCompToBPSubsystem
{
	struct FRestoreSelectedInstanceComponent
	{
		TWeakObjectPtr<UClass> ActorClass;
		FName ActorName;
		TWeakObjectPtr<UObject> ActorOuter;

		struct FComponentKey
		{
			FName Name;
			TWeakObjectPtr<UClass> Class;

			FComponentKey(FName InName, UClass* InClass) : Name(InName), Class(InClass)
			{
			}
		};

		TArray<FComponentKey> ComponentKeys;

		FRestoreSelectedInstanceComponent()
			: ActorClass(nullptr)
			  , ActorOuter(nullptr)
		{
		}

		void Save(AActor* InActor)
		{
			check(InActor);
			ActorClass = InActor->GetClass();
			ActorName = InActor->GetFName();
			ActorOuter = InActor->GetOuter();

			check(GEditor);
			TArray<UActorComponent*> ComponentsToSaveAndDelesect;
			for (FSelectionIterator Iter = GEditor->GetSelectedComponentIterator(); Iter; ++Iter)
			{
				UActorComponent* Component = CastChecked<UActorComponent>(*Iter, ECastCheckedType::NullAllowed);
				if (Component && InActor->GetInstanceComponents().Contains(Component))
				{
					ComponentsToSaveAndDelesect.Add(Component);
				}
			}

			for (UActorComponent* Component : ComponentsToSaveAndDelesect)
			{
				USelection* SelectedComponents = GEditor->GetSelectedComponents();
				if (ensure(SelectedComponents))
				{
					ComponentKeys.Add(FComponentKey(Component->GetFName(), Component->GetClass()));
					SelectedComponents->Deselect(Component);
				}
			}
		}

		void Restore()
		{
			AActor* Actor = (ActorClass.IsValid() && ActorOuter.IsValid())
				                ? Cast<AActor>((UObject*)FindObjectWithOuter(ActorOuter.Get(), ActorClass.Get(), ActorName))
				                : nullptr;
			if (Actor)
			{
				for (const FComponentKey& IterKey : ComponentKeys)
				{
					UActorComponent* const* ComponentPtr = Algo::FindByPredicate(Actor->GetComponents(), [&](UActorComponent* InComp)
					{
						return InComp && (InComp->GetFName() == IterKey.Name) && (InComp->GetClass() == IterKey.Class.Get());
					});
					if (ComponentPtr && *ComponentPtr)
					{
						check(GEditor);
						GEditor->SelectComponent(*ComponentPtr, true, false);
					}
				}
			}
		}
	};
}

void UAddCompToBPSubsystem::ApplyChangesToBP(AActor* ActorContex)
{
	int32 NumChangedProperties = 0;

	AActor* Actor = ActorContex;
	UBlueprint* Blueprint = (Actor != nullptr) ? Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy) : nullptr;

	if (Actor != NULL && Blueprint != NULL && Actor->GetClass()->ClassGeneratedBy == Blueprint)
	{
		// Cache the actor label as by the time we need it, it may be invalid
		const FString ActorLabel = Actor->GetActorLabel();
		AddCompToBPSubsystem::FRestoreSelectedInstanceComponent RestoreSelectedInstanceComponent;
		{
			const FScopedTransaction Transaction(LOCTEXT("PushToBlueprintDefaults_Transaction", "Apply Changes to Blueprint"));

			// The component selection state should be maintained
			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedComponents()->Modify();

			Actor->Modify();

			// Mark components that are either native or from the SCS as modified so they will be restored
			for (UActorComponent* ActorComponent : Actor->GetComponents())
			{
				if (ActorComponent && (ActorComponent->CreationMethod == EComponentCreationMethod::SimpleConstructionScript || ActorComponent->CreationMethod == EComponentCreationMethod::Native))
				{
					ActorComponent->Modify();
				}
			}

			// Perform the actual copy
			{
				AActor* BlueprintCDO = Actor->GetClass()->GetDefaultObject<AActor>();
				if (BlueprintCDO != NULL)
				{
					const EditorUtilities::ECopyOptions::Type CopyOptions = (EditorUtilities::ECopyOptions::Type)(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties | EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances | EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties);
					NumChangedProperties = EditorUtilities::CopyActorProperties(Actor, BlueprintCDO, CopyOptions);
					const TArray<UActorComponent*>& InstanceComponents = Actor->GetInstanceComponents();
					if (InstanceComponents.Num() > 0)
					{
						RestoreSelectedInstanceComponent.Save(Actor);
						FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, InstanceComponents);
						NumChangedProperties += InstanceComponents.Num();
						Actor->ClearInstanceComponents(true);
					}
					if (NumChangedProperties > 0)
					{
						Actor = nullptr; // It is unsafe to use Actor after this point as it may have been reinstanced, so set it to null to make this obvious
					}
				}
			}

			if (NumChangedProperties > 0)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				// FKismetEditorUtilities::CompileBlueprint(Blueprint);
				RestoreSelectedInstanceComponent.Restore();
			}
		}

		// Set up a notification record to indicate success/failure
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.FadeInDuration = 1.0f;
		NotificationInfo.FadeOutDuration = 2.0f;
		NotificationInfo.bUseLargeFont = false;
		SNotificationItem::ECompletionState CompletionState;
		if (NumChangedProperties > 0)
		{
			if (NumChangedProperties > 1)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("BlueprintName"), FText::FromName(Blueprint->GetFName()));
				Args.Add(TEXT("NumChangedProperties"), NumChangedProperties);
				Args.Add(TEXT("ActorName"), FText::FromString(ActorLabel));
				NotificationInfo.Text = FText::Format(LOCTEXT("PushToBlueprintDefaults_ApplySuccess", "Updated Blueprint {BlueprintName} ({NumChangedProperties} property changes applied from actor {ActorName})."), Args);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("BlueprintName"), FText::FromName(Blueprint->GetFName()));
				Args.Add(TEXT("ActorName"), FText::FromString(ActorLabel));
				NotificationInfo.Text = FText::Format(LOCTEXT("PushOneToBlueprintDefaults_ApplySuccess", "Updated Blueprint {BlueprintName} (1 property change applied from actor {ActorName})."), Args);
			}
			CompletionState = SNotificationItem::CS_Success;
		}
		else
		{
			NotificationInfo.Text = LOCTEXT("PushToBlueprintDefaults_ApplyFailed", "No properties were copied");
			CompletionState = SNotificationItem::CS_Fail;
		}

		// Add the notification to the queue
		const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		Notification->SetCompletionState(CompletionState);
	}
}

void UAddCompToBPSubsystem::Tick(float DeltaTime)
{
	if (FilterRepActorProcessStatus == Busy && FilterRepActorProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(FilterRepActorProcessID))
	{
		FString NewLine = FPlatformProcess::ReadPipe(FilterRepActorProcReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			FilterRepActorProcOutLine += NewLine;
			TArray<FString> StringArray;
			int32 count = FilterRepActorProcOutLine.ParseIntoArray(StringArray, TEXT("\n"), true);
			if (count > 1)
			{
				for (int32 Index = 0; Index < count - 1; ++Index)
				{
					StringArray[Index].TrimEndInline();

					FString OutMsg = StringArray[Index];
					int32 MsgIndex;
					COMMANDLET_LOG_PROXY(Display,)
					COMMANDLET_LOG_PROXY(Log, else)
					COMMANDLET_LOG_PROXY(Verbose, else)
					COMMANDLET_LOG_PROXY(VeryVerbose, else)
					COMMANDLET_LOG_PROXY(Warning, else)
					COMMANDLET_LOG_PROXY(Error, else)
					COMMANDLET_LOG_PROXY(Fatal, else)
				}
				FilterRepActorProcOutLine = StringArray[count - 1];
				if (NewLine.EndsWith(TEXT("\n")))
				{
					FilterRepActorProcOutLine += TEXT("\n");
				}
			}
		}
	}
	else if (FilterRepActorProcessStatus == Busy)
	{
		int32 ProcReturnCode;
		if (FPlatformProcess::GetProcReturnCode(FilterRepActorProcessHandle, &ProcReturnCode))
		{
			if (ProcReturnCode == 0)
			{
				FilterRepActorProcessStatus = AddingComp;
				AddCompToActorBPInternal(true);
				FilterRepActorProcessStatus = Completed;
				SpawnFilterRepActorSucceedNotification();
			}
			else
			{
				FilterRepActorProcessStatus = Failed;
				SpawnFilterRepActorFailedNotification();
			}
		}
		else
		{
			FilterRepActorProcessStatus = Failed;
			SpawnFilterRepActorFailedNotification();
		}
	}
}

ETickableTickType UAddCompToBPSubsystem::GetTickableTickType() const
{
	return IsTemplate() && !GIsEditor ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType();
}

bool UAddCompToBPSubsystem::IsAllowedToTick() const
{
	return !IsTemplate() && GIsEditor;
}

bool UAddCompToBPSubsystem::IsTickableInEditor() const
{
	return true;
}

void UAddCompToBPSubsystem::AddComponentToBlueprints(UClass* InCompClass, FName InCompName)
{
	if (FilterRepActorProcessStatus == Busy || FilterRepActorProcessStatus == Canceling)
	{
		return;
	}
	FilterRepActorProcessStatus = Busy;
	TargetRepCompClass = InCompClass;
	RepCompName = InCompName;
	FString Params = CommandletHelpers::BuildCommandletProcessArguments(
		TEXT("CookAndFilterRepActor"),
		*FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())),
		TEXT(" -targetplatform=WindowsServer -skipcompile -SkipShaderCompile -nop4 -cook -skipstage -utf8output -stdout")
	);
	FString Cmd = ChanneldReplicatorGeneratorUtils::GetUECmdBinary();

	if (FPaths::FileExists(Cmd))
	{
		SpawnRunningFilterRepActorNotification();

		FPlatformProcess::CreatePipe(FilterRepActorProcReadPipe, FilterRepActorProcWritePipe);

		FilterRepActorProcessHandle = FPlatformProcess::CreateProc(
			*Cmd,
			*Params,
			false,
			true,
			true,
			&FilterRepActorProcessID,
			1,
			NULL,
			FilterRepActorProcWritePipe,
			FilterRepActorProcReadPipe
		);
	}
}

void UAddCompToBPSubsystem::CancelFilterRepActor()
{
	if (FilterRepActorProcessStatus != Busy)
	{
		return;
	}

	FilterRepActorProcessStatus = Canceling;
	if (FilterRepActorProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(FilterRepActorProcessID))
	{
		FPlatformProcess::TerminateProc(FilterRepActorProcessHandle, true);
		FilterRepActorProcessHandle.Reset();
		FilterRepActorProcessID = 0;
	}
	FilterRepActorProcessStatus = Canceled;
}

void UAddCompToBPSubsystem::AddCompToActorBPInternal(bool ShowDialog)
{
	TArray<FString> Result;
	bool bLoadSuccess;
	UCookAndFilterRepActorCommandlet::LoadResult(Result, bLoadSuccess);
	TArray<FString> TargetAssetPathList;
	for (FString ClassPath : Result)
	{
		if (ClassPath.RemoveFromEnd(TEXT("_C")))
		{
			UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassPath);
			if (BP == nullptr)
			{
				UE_LOG(LogAddRepCompToBP, Warning, TEXT("We could not load blueprint: %s"), *ClassPath);
				continue;
			}
			if (BP->GeneratedClass != nullptr && !ChanneldReplicatorGeneratorUtils::HasRepComponent(BP->GeneratedClass))
			{
				TargetAssetPathList.Add(ClassPath);
				AActor* TargetCDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
				TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(TargetCDO, false);

				UActorComponent* NewInstanceComponent = NewObject<UActorComponent>(TargetCDO, TargetRepCompClass, RepCompName, RF_Transactional);
				// NewInstanceComponent->RegisterComponentWithWorld()
				TargetCDO->Modify();
				TargetCDO->AddInstanceComponent(NewInstanceComponent);
				NewInstanceComponent->OnComponentCreated();
				NewInstanceComponent->RegisterComponentWithWorld(TargetCDO->GetWorld());
				// NewInstanceComponent->RegisterComponent();
				// TargetCDO->RerunConstructionScripts();

				ApplyChangesToBP(TargetCDO);
			}
		}
	}

	if (ShowDialog)
	{
		FString TargetAssetPathStr;
		for (FString& TargetAssetPath : TargetAssetPathList)
		{
			TargetAssetPathStr.Append(FString::Printf(TEXT("  - %s\n"), *TargetAssetPath));
		}
		FText DialogText = FText::Format(
			LOCTEXT("PluginButtonDialogText", "A total of {0} blueprints have been added replication component [{1}].\n\nBelow blueprint assets are unsaved, please save them manually!!!\n\n{2}"),
			FText::FromString(FString::Printf(TEXT("%d"), TargetAssetPathList.Num())),
			FText::FromString(TargetRepCompClass->GetPathName()),
			FText::FromString(TargetAssetPathStr)
		);
		FMessageDialog::Open(EAppMsgType::Ok, DialogText);
	}
}

void UAddCompToBPSubsystem::SpawnRunningFilterRepActorNotification()
{
	UAddCompToBPSubsystem* AddCompToBPSubsystem = this;
	AsyncTask(ENamedThreads::GameThread, [AddCompToBPSubsystem]()
	{
		if (AddCompToBPSubsystem->FilterRepActorNotiPtr.IsValid())
		{
			AddCompToBPSubsystem->FilterRepActorNotiPtr.Pin()->ExpireAndFadeout();
		}
		FNotificationInfo Info(FText::FromString(TEXT("Adding replicaiton components")));

		Info.bFireAndForget = false;
		Info.Hyperlink = FSimpleDelegate::CreateStatic([]() { FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog")); });
		Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("RunningFilterRepActorNotificationCancelButton", "Cancel"), FText(),
		                                               FSimpleDelegate::CreateLambda([AddCompToBPSubsystem]() { AddCompToBPSubsystem->CancelFilterRepActor(); }),
		                                               SNotificationItem::CS_Pending
		));

		AddCompToBPSubsystem->FilterRepActorNotiPtr = FSlateNotificationManager::Get().AddNotification(Info);

		AddCompToBPSubsystem->FilterRepActorNotiPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	});
}


void UAddCompToBPSubsystem::SpawnFilterRepActorSucceedNotification()
{
	UAddCompToBPSubsystem* AddCompToBPSubsystem = this;
	AsyncTask(ENamedThreads::GameThread, [AddCompToBPSubsystem]()
	{
		TSharedPtr<SNotificationItem> NotificationItem = AddCompToBPSubsystem->FilterRepActorNotiPtr.Pin();

		if (NotificationItem.IsValid())
		{
			NotificationItem->SetText(FText::FromString(TEXT("Added replicaiton components")));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
			NotificationItem->ExpireAndFadeout();

			AddCompToBPSubsystem->FilterRepActorNotiPtr.Reset();
		}
	});
}

void UAddCompToBPSubsystem::SpawnFilterRepActorFailedNotification()
{
	UAddCompToBPSubsystem* AddCompToBPSubsystem = this;
	AsyncTask(ENamedThreads::GameThread, [AddCompToBPSubsystem]()
	{
		TSharedPtr<SNotificationItem> NotificationItem = AddCompToBPSubsystem->FilterRepActorNotiPtr.Pin();

		if (NotificationItem.IsValid())
		{
			NotificationItem->SetText(FText::FromString(TEXT("Filter replicaiton actor is failed")));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();

			AddCompToBPSubsystem->FilterRepActorNotiPtr.Reset();
			UE_LOG(LogAddRepCompToBP, Error, TEXT("Filter replicaiton actor is failed"))
		}
	});
}

#undef LOCTEXT_NAMESPACE
