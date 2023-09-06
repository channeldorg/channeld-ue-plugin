#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "EditorSubsystem.h"
#include "JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "StaticObjectExportController.generated.h"

/**<channeld
 * 
 */
UCLASS(BlueprintType)
class REPLICATORGENERATOR_API UStaticObjectExportController : public UEditorSubsystem
{
	GENERATED_BODY()
	TJsonModel<FStaticObjectCache> FStaticObjectInfoModel = GenManager_ChannelStaticObjectExportPath;
public:
	bool SaveStaticObjectExportInfo(const TArray<const UObject*>& InStaticObjects);
};
