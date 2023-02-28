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
	FString ProtoDefinitions;

	FString IncludeActorCode;
	FString RegisterReplicatorCode;

	FString ChannelDataProcessor_PathFNameVarDecl;
	FString ChannelDataProcessor_MergeCode;
	FString ChannelDataProcessor_GetStateCode;
	FString ChannelDataProcessor_SetStateCode;
};

struct FGeneratedCodeBundle
{
	FString RegisterReplicatorFileCode;

	TArray<FReplicatorCode> ReplicatorCodes;

	FString GlobalStructCodes;

	FString GlobalStructProtoDefinitions;

	FString ChannelDataProcessorClassName;
	FString ChannelDataProcessorHeadCode;
	FString ChannelDataProcessorProtoMsgFullName;
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
	 * @param TargetActors The actor classes to generate replicators for.
	 * @param GetGoPackage The function to get the 'go_package' for proto file. The parameter is the package name of the proto file. If it is nullptr, the 'go_package' will be empty.
	 * @param ReplicatorCodeBundle The generated replicator codes (.h, .cpp, .proto) .
	 * @return true on success, false otherwise.
	 */
	bool Generate(
		TArray<const UClass*> TargetActors,
		const TFunction<FString(const FString& PackageName)>* GetGoPackage,
		FGeneratedCodeBundle& ReplicatorCodeBundle
	);

	bool GenerateChannelDataProcessor(FGeneratedCodeBundle& GeneratedCodeBundle, TArray<const UClass*> TargetActors);

	/**
	 * Generate replicator code for the specified actor.
	 *
	 * @param TargetActorClass The actor to generate replicator for.
	 * @param GeneratedResult The generated replicator code (.h, .cpp, .proto) .
	 * @param GetGoPackage The function to get the 'go_package' for proto file.
	 * @param ResultMessage The result message.
	 * @return true on success, false otherwise.
	 */
	bool GenerateActorCode(const UClass* TargetActorClass, const TFunction<FString(const FString& PackageName)>* GetGoPackage, FReplicatorCode& GeneratedResult, FString& ResultMessage);

protected:
	TMap<FString, FModuleInfo> ModuleInfoByClassName;
	TMap<FString, FCPPClassInfo> CPPClassInfoMap;

	int32 IllegalClassNameIndex = 0;
	TMap<FString, int32> TargetActorSameNameCounter;

	inline void ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule);

	inline bool CreateDecorateActor(TSharedPtr<FReplicatedActorDecorator>& OutActorDecorator, FString& OutResultMessage, const UClass* TargetActor, bool bInitPropertiesAndRPCs = true, bool bIncrementIfSameName = true);
};
