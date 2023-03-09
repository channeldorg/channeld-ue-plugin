#pragma once
#include "ReplicatedActorDecorator.h"

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

	FString ChannelDataProcessor_PathFNameVarDecl;
	FString ChannelDataProcessor_MergeCode;
	FString ChannelDataProcessor_GetStateCode;
	FString ChannelDataProcessor_SetStateCode;
};

struct FGeneratedCodeBundle
{
	FString TypeDefinitionsHeadCode;
	FString TypeDefinitionsCppCode;

	FString ReplicatorRegistrationHeadCode;

	TArray<FReplicatorCode> ReplicatorCodes;

	FString GlobalStructCodes;

	FString GlobalStructProtoDefinitions;

	FString ChannelDataProcessorHeadCode;
	FString ChannelDataProtoDefsFile;

	FString ChannelDataMerge_GoCode;
	FString ChannelDataRegistration_GoCode;
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
	 * @param ReplicationActorClasses The actor classes to generate replicators and channeld data fields.
	 * @param DefaultModuleDir The default module directory. The channel data processor will use default module name.
	 * @param ProtoPackageName All generated proto files will use this package name.
	 * @param GoPackageImportPath Be used to set 'option go_package='
	 * @param ReplicatorCodeBundle The generated replicator codes (.h, .cpp, .proto) .
	 * @return true on success, false otherwise.
	 */
	bool Generate(
		TArray<const UClass*> ReplicationActorClasses,
		const FString& DefaultModuleDir,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		FGeneratedCodeBundle& ReplicatorCodeBundle
	);

	/**
	 * Generate replicator code for the specified actor.
	 *
	 * @param TargetActorClass The actor to generate replicator for.
	 * @param GeneratedResult The generated replicator code (.h, .cpp, .proto) .
	 * @param ProtoPackageName All replicator's proto files will use this package name.
	 * @param GoPackageImportPath Be used to set 'option go_package='
	 * @param ResultMessage The result message.
	 * @return true on success, false otherwise.
	 */
	bool GenerateActorCode(
		const UClass* TargetActorClass,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		FReplicatorCode& GeneratedResult,
		FString& ResultMessage
	);

	bool GenerateChannelDataCode(
		TArray<const UClass*> ReplicationActorClasses,
		const FString& ChannelDataProtoMsgName,
		const FString& ChannelDataProcessorNamespace,
		const FString& ChannelDataProcessorClassName,
		const FString& ChannelDataProtoHeadFileName,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		FGeneratedCodeBundle& GeneratedCodeBundle,
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

protected:
	TMap<FString, FModuleInfo> ModuleInfoByClassName;
	TMap<FString, FCPPClassInfo> CPPClassInfoMap;

	int32 IllegalClassNameIndex = 0;
	TMap<FString, int32> TargetActorSameNameCounter;

	inline void ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule);

	inline bool CreateDecorateActor(
		TSharedPtr<FReplicatedActorDecorator>& OutActorDecorator,
		FString& OutResultMessage,
		const UClass* TargetActor,
		const FString& ProtoPackageName,
		const FString& GoPackageImportPath,
		bool bInitPropertiesAndRPCs = true,
		bool bIncrementIfSameName = true
	);
};
