#include "Persistence/StaticObjectExportController.h"

bool UStaticObjectExportController::SaveStaticObjectExportInfo(const TArray<const UObject*>& InStaticObjects)
{
	TArray<FStaticObjectInfo> StaticObjectInfos;
	TMap<const UObject*, int32> StaticObjectExportID;
	int32 ExportID = GenManager_ChannelStaticObjectExportStartID;
	for (auto Obj : InStaticObjects)
	{
		StaticObjectExportID.Add(Obj, ExportID);
		ExportID += 2;
	}
	for (auto Obj : InStaticObjects)
	{
		int32 OuterExportID = 0;
		if (Obj->GetOuter() && StaticObjectExportID.Contains(Obj->GetOuter()))
		{
			OuterExportID = StaticObjectExportID[Obj->GetOuter()];
		}
		StaticObjectInfos.Add(FStaticObjectInfo(Obj->GetPathName(), StaticObjectExportID[Obj], OuterExportID));
	}
	return FStaticObjectInfoModel.SaveData(FStaticObjectCache(StaticObjectInfos));
}