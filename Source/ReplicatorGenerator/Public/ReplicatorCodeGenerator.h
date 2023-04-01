#pragma once
#include "ReplicatedActorDecorator.h"
#include "Persistence/ChannelDataSchemaController.h"

struct FCPPClassInfo
{
	FString HeadFilePath;
	FModuleInfo ModuleInfo;
};

struct FReplicatorCode
{
	TSharedPtr<FReplicatedActorDecorator> ActorDecorator;

	FString HeadFileName;
	FString HeadCode;

	FString CppFileName;
	FString CppCode;

	FString ProtoFileName;
	FString ProtoDefinitionsFile;

	FString IncludeActorCode;
	FString RegisterReplicatorCode;
};

struct FChannelDataCode
{
	FString ProcessorHeadFileName;
	FString ProcessorHeadCode;

	FString ProtoFileName;
	FString ProtoDefsFile;

	FString IncludeProcessorCode;
	FString RegisterProcessorCode;
	FString DeleteProcessorPtrCode;
	FString ProcessorPtrDecl;

	FString Merge_GoCode;
	FString Registration_GoCode;
};

struct FGeneratedCodeBundle
{
	FString TypeDefinitionsHeadCode;
	FString TypeDefinitionsCppCode;

	FString ReplicatorRegistrationHeadCode;

	TArray<FReplicatorCode> ReplicatorCodes;

	FString GlobalStructCodes;

	FString GlobalStructProtoDefinitions;

	TArray<FChannelDataCode> ChannelDataCodes;
};


struct FChannelDataInfo
{
	struct FStateInfo
	{
		const UClass* RepActorClass;
		FChannelDataStateSchema Setting;

		FStateInfo() = default;

		FStateInfo(const UClass* InRepActorClass, const FChannelDataStateSchema& InSetting) : RepActorClass(InRepActorClass), Setting(InSetting)
		{
		}
	};

	FChannelDataSchema Schema;

	TArray<FStateInfo> StateInfos;

	FChannelDataInfo() = default;

	FChannelDataInfo(const FChannelDataSchema& InSetting) : Schema(InSetting)
	{
		for (const FChannelDataStateSchema& StateSetting : InSetting.StateSchemata)
		{
			if (const UClass* TargetClass = LoadClass<UObject>(nullptr, *StateSetting.ReplicationClassPath, nullptr, LOAD_None, nullptr))
			{
				StateInfos.Add(FStateInfo(TargetClass, StateSetting));
			}
			else
			{
				UE_LOG(LogChanneldRepGenerator, Warning, TEXT("The target class [%s] was not found."), *StateSetting.ReplicationClassPath);
			}
		}
	}
};


class REPLICATORGENERATOR_API FReplicatorCodeGenerator
{
public:
	/**
	 * Load the related information of the module of the class from '.uhtmanifest'.
	 * 
	 * @return true on success, false otherwise.
	 */
	bool RefreshModuleInfoByClassName();

	/**
	 * Get the path of the header file by class name.
	 * Before calling this function, you need to call RefreshModuleInfoByClassName() at least once.
	 * 
	 * @param ClassName The name of the class, without UE prefix.
	 * @return Absolute path of the header file.
	 */
	FString GetClassHeadFilePath(const FString& ClassName);

	/**
	 * Generate replicator codes for the specified actors.
	 *
	 * @param DefaultModuleDir The default module directory. The channel data processor will use default module name.
	 * @param ProtoPackageName All generated proto files will use this package name.
	 * @param GoPackageImportPath Be used to set 'option go_package='
	 * @param ReplicatorCodeBundle The generated replicator codes (.h, .cpp, .proto) .
	 * @return true on success, false otherwise.
	 */
	bool Generate(
		const TArray<FChannelDataInfo>& ChannelDataInfos,
		const FString& DefaultModuleDir,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		FGeneratedCodeBundle& ReplicatorCodeBundle
	);

protected:
	TMap<FString, FModuleInfo> ModuleInfoByClassName;
	TMap<FString, FCPPClassInfo> CPPClassInfoMap;

	TMap<FString, int32> TargetActorSameNameCounter;
	TMap<const UClass*, int32> TargetClassSameNameNumber;

	/**
	 * Generate replicator code for the specified actor.
	 *
	 * @param ActorDecorator The actor to generate replicator for.
	 * @param GeneratedResult The generated replicator code (.h, .cpp, .proto) .
	 * @param ResultMessage The result message.
	 * @return true on success, false otherwise.
	 */
	bool GenerateReplicatorCode(
		const TSharedPtr<FReplicatedActorDecorator>& ActorDecorator,
		FReplicatorCode& GeneratedResult,
		FString& ResultMessage
	);

	bool GenerateChannelDataCode(
		const FChannelDataInfo& ChannelDataInfo,
		const FString& GoPackageImportPath,
		const FString& ProtoPackageName,
		FChannelDataCode& GeneratedResult,
		FString& ResultMessage
	);

	bool GenerateChannelDataProcessorCode(
		const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
		const TArray<TSharedPtr<FReplicatedActorDecorator>>& ChildrenOfAActor,
		const FString& ChannelDataMessageName,
		const FString& ChannelDataProcessorNamespace,
		const FString& ChannelDataProcessorClassName,
		const FString& ChannelDataProtoHeadFileName,
		const FString& ProtoPackageName,
		FString& ChannelDataProcessorCode
	);

	bool GenerateChannelDataProtoDefFile(
		const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
		const FString& ChannelDataMessageName,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		FString& ChannelDataProtoFile
	);

	bool GenerateChannelDataMerge_GoCode(
		const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
		const TArray<TSharedPtr<FReplicatedActorDecorator>>& ChildrenOfAActor,
		const FString& ChannelDataMessageName,
		const FString& ProtoPackageName,
		FString& GoCode
	);

	bool GenerateChannelDataRegistration_GoCode(
		const FString& GoImportPath,
		const FString& ChannelDataMessageName,
		const FString& ProtoPackageName,
		FString& GoCode
	);

	inline void ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule);

	inline bool CreateDecorateActor(
		TSharedPtr<FReplicatedActorDecorator>& OutActorDecorator,
		FString& OutResultMessage,
		const UClass* TargetActorClass,
		const FChannelDataStateSchema& ChannelDataStateSchema,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		bool bInitPropertiesAndRPCs = true
	);

	inline TArray<TSharedPtr<FStructPropertyDecorator>> GetAllStructPropertyDecorators(const TArray<TSharedPtr<FReplicatedActorDecorator>>& ActorDecorator) const;
};
