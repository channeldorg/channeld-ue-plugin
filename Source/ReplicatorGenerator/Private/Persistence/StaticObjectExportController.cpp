#include "Persistence/StaticObjectExportController.h"

bool UStaticObjectExportController::SaveStaticObjectExportInfo(const TArray<const UObject*>& InStaticObjects)
{
	TArray<FStaticObjectInfo> StaticObjectInfos;
	TMap<const UObject*, uint32> StaticObjectExportID;
	uint32 ExportID = GenManager_ChanneldStaticObjectExportStartID;
	for (auto Obj : InStaticObjects)
	{
		StaticObjectExportID.Add(Obj, ExportID);
		ExportID += 2;
	}
	for (auto Obj : InStaticObjects)
	{
		uint32 OuterExportID = 0;
		if (Obj->GetOuter() && StaticObjectExportID.Contains(Obj->GetOuter()))
		{
			OuterExportID = StaticObjectExportID[Obj->GetOuter()];
		}
		StaticObjectInfos.Add(FStaticObjectInfo(Obj->GetPathName(), StaticObjectExportID[Obj], OuterExportID));
	}
	return FStaticObjectInfoModel.SaveData(FStaticObjectCache(StaticObjectInfos));
}