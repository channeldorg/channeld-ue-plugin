#pragma once

static const TCHAR* CodeGen_Go_Data_ImportTemplate =
	LR"EOF(
import (
	"errors"

	"github.com/channeldorg/channeld/pkg/channeld"
	"github.com/channeldorg/channeld/pkg/channeldpb"
	"github.com/channeldorg/channeld/pkg/common"
	"github.com/channeldorg/channeld/pkg/unreal"
	"github.com/channeldorg/channeld/pkg/unrealpb"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	{Code_AnypbImport}
)
)EOF";

static const TCHAR* CodeGen_Go_CollectStatesTemplate = LR"EOF(
// Implement [channeld.ChannelDataCollector]
func (to *{Definition_ChannelDataMsgName}) CollectStates(netId uint32, src common.Message) error {
{Decl_ChannelDataMsgVar}, ok := src.(*{Definition_ChannelDataMsgName})
if !ok {
	return errors.New("src is not a {Definition_ChannelDataMsgName}")
}

)EOF";

static const TCHAR* CodeGen_Go_CollectStateInMapTemplate = LR"EOF(
{Definition_StateVarName}, exists := from.{Definition_StateMapName}[netId]
if exists {
	if to.{Definition_StateMapName} == nil {
		to.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackagePath}{Definition_StateClassName})
	}
	to.{Definition_StateMapName}[netId] = {Definition_StateVarName}
}

)EOF";

static const TCHAR* CodeGen_Go_MergeTemplate = LR"EOF(
// Implement [channeld.MergeableChannelData]
func (dst *{Definition_ChannelDataMsgName}) Merge(src common.ChannelDataMessage, options *channeldpb.ChannelDataMergeOptions, spatialNotifier common.SpatialInfoChangedNotifier) error {
	{Decl_ChannelDataMsgVar}, ok := src.(*{Definition_ChannelDataMsgName})
	if !ok {
		return errors.New("src is not a {Definition_ChannelDataMsgName}")
	}

	{Code_CheckHandover}
	{Code_MergeStates}
	{Code_NotifyHandover}

	return nil
}
)EOF";

static const TCHAR* CodeGen_Go_CheckHandoverTemplate = LR"EOF(
	hasHandover := false
	var oldInfo, newInfo *common.SpatialInfo
	if spatialNotifier != nil && dst.ObjRef != nil {
		{Code_CheckHandoverInStates}
	}
)EOF";

static const TCHAR* CodeGen_Go_ActorCheckHandoverTemplate = LR"EOF(
		if srcData.ActorState != nil && srcData.ActorState.ReplicatedMovement != nil && srcData.ActorState.ReplicatedMovement.Location != nil &&
			dst.ActorState != nil && dst.ActorState.ReplicatedMovement != nil && dst.ActorState.ReplicatedMovement.Location != nil {
			hasHandover, oldInfo, newInfo = unreal.CheckEntityHandover(*dst.ObjRef.NetGUID, srcData.ActorState.ReplicatedMovement.Location, dst.ActorState.ReplicatedMovement.Location)
		}
)EOF";

static const TCHAR* CodeGen_Go_SceneCompCheckHandoverTemplate = LR"EOF(
		if !hasHandover && srcData.SceneComponentState != nil && srcData.SceneComponentState.RelativeLocation != nil &&
			dst.SceneComponentState != nil && dst.SceneComponentState.RelativeLocation != nil {
			hasHandover, oldInfo, newInfo = unreal.CheckEntityHandover(*dst.ObjRef.NetGUID, srcData.SceneComponentState.RelativeLocation, dst.SceneComponentState.RelativeLocation)
		}
)EOF";

static const TCHAR* CodeGen_Go_NotifyHandover = LR"EOF(
	if hasHandover {
		spatialNotifier.Notify(*oldInfo, *newInfo,
			func(srcChannelId common.ChannelId, dstChannelId common.ChannelId, handoverData interface{}) {
				entityId, ok := handoverData.(*channeld.EntityId)
				if !ok {
					channeld.RootLogger().Error("handover data is not an entityId",
						zap.Uint32("srcChannelId", uint32(srcChannelId)),
						zap.Uint32("dstChannelId", uint32(dstChannelId)),
					)
					return
				}
				*entityId = channeld.EntityId(*dst.ObjRef.NetGUID)
			},
		)
	}
)EOF";

static const TCHAR* CodeGen_Go_MergeStateTemplate = LR"EOF(
	if srcData.{Definition_StateVarName} != nil {
		if dst.{Definition_StateVarName} == nil {
			dst.{Definition_StateVarName} = &{Definition_StatePackagePath}{Definition_StateClassName}{}
		}
		proto.Merge(dst.{Definition_StateVarName}, srcData.{Definition_StateVarName})
	}

)EOF";

static const TCHAR* CodeGen_Go_MergeStateInMapTemplate = LR"EOF(
	for netId, {Definition_NewStateVarName} := range srcData.{Definition_StateMapName} {
		{Definition_OldStateVarName}, exists := dst.{Definition_StateMapName}[netId]
		if exists {
			proto.Merge({Definition_OldStateVarName}, {Definition_NewStateVarName})
		} else {
			if dst.{Definition_StateMapName} == nil {
				dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackagePath}{Definition_StateClassName})
			}
			dst.{Definition_StateMapName}[netId] = {Definition_NewStateVarName}
		}
	}

)EOF";

static const TCHAR* CodeGen_Go_MergeCompStateInMapTemplate = LR"EOF(
	for netId, {Definition_NewStateVarName} := range srcData.{Definition_StateMapName} {
		if {Definition_NewStateVarName}.Removed {
			delete(dst.{Definition_StateMapName}, netId)
		} else {
			{Definition_OldStateVarName}, exists := dst.{Definition_StateMapName}[netId]
			if exists {
				proto.Merge({Definition_OldStateVarName}, {Definition_NewStateVarName})
			} else {
				if dst.{Definition_StateMapName} == nil {
					dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackagePath}{Definition_StateClassName})
				}
				dst.{Definition_StateMapName}[netId] = {Definition_NewStateVarName}
			}
		}
	}

)EOF";

static const TCHAR* CodeGen_Go_MergeActorStateInMapTemplate = LR"EOF(
	for netId, {Definition_NewStateVarName} := range srcData.{Definition_StateMapName} {
		if {Definition_NewStateVarName}.Removed {
			{Code_DeleteFromStates}
			channeld.RootLogger().Debug("removed actor state", zap.Uint32("netId", netId))
			continue
		} else {
			{Definition_OldStateVarName}, exists := dst.{Definition_StateMapName}[netId]
			if exists {
				proto.Merge({Definition_OldStateVarName}, {Definition_NewStateVarName})
			} else {
				if dst.{Definition_StateMapName} == nil {
					dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackagePath}{Definition_StateClassName})
				}
				dst.{Definition_StateMapName}[netId] = {Definition_NewStateVarName}
			}
		}
	}

)EOF";

static const TCHAR* CodeGen_Go_DeleteStateInMapTemplate = LR"EOF(
	delete(dst.{Definition_StateMapName}, netId)
)EOF";

static const TCHAR* CodeGen_Go_RegistrationTemplate = LR"EOF(
package main

import (
	"{Definition_GoImportPath}"
	"github.com/channeldorg/channeld/pkg/channeld"
	"github.com/channeldorg/channeld/pkg/channeldpb"
)

func InitChannelDataTypes() {
	{Code_Registration}
}
)EOF";

static const TCHAR* CodeGen_Go_EntityChannelDataTemplate = LR"EOF(
// Implement [channeld.HandoverDataMerger]
func (entityData *{Definition_ChannelDataMsgName}) MergeTo(msg common.Message, fullData bool) error {
	handoverData, ok := msg.(*unrealpb.SpatialChannelData)
	if !ok {
		return errors.New("msg is not a SpatialChannelData")
	}

	entityState := &unrealpb.SpatialEntityState{
		ObjRef: entityData.ObjRef,
	}

	if fullData {
		anyData, err := anypb.New(entityData)
		if err != nil {
			return err
		}
		entityState.EntityData = anyData
	}

	if handoverData.Entities == nil {
		handoverData.Entities = make(map[uint32]*unrealpb.SpatialEntityState)
	}
	handoverData.Entities[*entityData.ObjRef.NetGUID] = entityState

	return nil
}

// Implement [unreal.UnrealObjectEntityData]
func (entityData *{Definition_ChannelDataMsgName}) SetObjRef(objRef *unrealpb.UnrealObjectRef) {
	entityData.ObjRef = objRef
}
)EOF";

static const TCHAR* CodeGen_Go_EntityGetSpatialInfoTemplate = LR"EOF(
// Implement [channeld.EntityChannelDataWithSpatialInfo]
func (entityData *{Definition_ChannelDataMsgName}) GetSpatialInfo() *common.SpatialInfo {
	{Code_GetSpatialInfo}
	return nil
}
)EOF";

static const TCHAR* CodeGen_Go_GetActorSpatialInfo = LR"EOF(
	if entityData.ActorState != nil && entityData.ActorState.ReplicatedMovement != nil {
		return entityData.ActorState.ReplicatedMovement.Location.ToSpatialInfo()
	}
)EOF";

static const TCHAR* CodeGen_Go_GetSceneComponentSpatialInfo = LR"EOF(
	if entityData.SceneComponentState != nil && (entityData.SceneComponentState.BAbsoluteLocation == nil || *entityData.SceneComponentState.BAbsoluteLocation) {
		return entityData.SceneComponentState.RelativeLocation.ToSpatialInfo()
	}
)EOF";