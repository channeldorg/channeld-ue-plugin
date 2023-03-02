#pragma once

static const TCHAR* CodeGen_Go_ImportTemplate =
	LR"EOF(
import (
	"errors"

	"channeld.clewcat.com/channeld/pkg/channeld"
	"channeld.clewcat.com/channeld/pkg/channeldpb"
	"channeld.clewcat.com/channeld/pkg/common"
	"channeld.clewcat.com/channeld/pkg/unreal"
	"channeld.clewcat.com/channeld/pkg/unrealpb"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
)
)EOF";

static const TCHAR* CodeGen_Go_CollectStatesTemplate = LR"EOF(
// Implement [channeld.ChannelDataCollector]
func (to *{Definition_ChannelDataMsgName}) CollectStates(netId uint32, src common.Message) error {
from, ok := src.(*{Definition_ChannelDataMsgName})
if !ok {
	return errors.New("src is not a {Definition_ChannelDataMsgName}")
}

)EOF";

static const TCHAR* CodeGen_Go_CollectStateInMapTemplate = LR"EOF(
{Definition_StateVarName}, exists := from.{Definition_StateMapName}[netId]
if exists {
	if to.{Definition_StateMapName} == nil {
		to.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackageName}.{Definition_StateClassName})
	}
	to.{Definition_StateMapName}[netId] = {Definition_StateVarName}
}

)EOF";

static const TCHAR* CodeGen_Go_MergeTemplate = LR"EOF(
// Implement [channeld.MergeableChannelData]
func (dst *{Definition_ChannelDataMsgName}) Merge(src common.ChannelDataMessage, options *channeldpb.ChannelDataMergeOptions, spatialNotifier common.SpatialInfoChangedNotifier) error {
	srcData, ok := src.(*{Definition_ChannelDataMsgName})
	if !ok {
		return errors.New("src is not a {Definition_ChannelDataMsgName}")
	}

	if spatialNotifier != nil {
		// src = the incoming update, dst = existing channel data
		for netId, newActorState := range srcData.ActorStates {
			oldActorState, exists := dst.ActorStates[netId]
			if exists {
				if newActorState.ReplicatedMovement != nil && newActorState.ReplicatedMovement.Location != nil &&
					oldActorState.ReplicatedMovement != nil && oldActorState.ReplicatedMovement.Location != nil {
					unreal.CheckSpatialInfoChange(netId, newActorState.ReplicatedMovement.Location, oldActorState.ReplicatedMovement.Location, spatialNotifier)
				}
			}
		}

		for netId, newSceneCompState := range srcData.SceneComponentStates {
			oldSceneCompState, exists := dst.SceneComponentStates[netId]
			if exists {
				if newSceneCompState.RelativeLocation != nil && oldSceneCompState.RelativeLocation != nil {
					unreal.CheckSpatialInfoChange(netId, newSceneCompState.RelativeLocation, oldSceneCompState.RelativeLocation, spatialNotifier)
				}
			}
		}
	}

)EOF";

static const TCHAR* CodeGen_Go_MergeStateTemplate = LR"EOF(
	if srcData.{Definition_StateVarName} != nil {
		if dst.{Definition_StateVarName} == nil {
			dst.{Definition_StateVarName} = &{Definition_StatePackageName}.{Definition_StateClassName}{}
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
				dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackageName}.{Definition_StateClassName})
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
					dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackageName}.{Definition_StateClassName})
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
					dst.{Definition_StateMapName} = make(map[uint32]*{Definition_StatePackageName}.{Definition_StateClassName})
				}
				dst.{Definition_StateMapName}[netId] = {Definition_NewStateVarName}
			}
	}

)EOF";

static const TCHAR* CodeGen_Go_DeleteStateInMapTemplate = LR"EOF(
	delete(dst.{Definition_StateMapName}, netId)
)EOF";