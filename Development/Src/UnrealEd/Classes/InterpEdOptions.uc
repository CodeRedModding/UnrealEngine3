/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpEdOptions extends Object
	hidecategories(Object)
	config(Editor)
	native;

var		array<InterpGroup.InterpEdSelKey>	SelectedKeys;

// Are we currently editing the value of a keyframe. This should only be true if there is one keyframe selected and the time is currently set to it.
var		bool bAdjustingKeyframe;

// Are we currently editing the values of a groups keyframe. This should only be true if the keyframes that are selected belong to the same group.
var		bool bAdjustingGroupKeyframes;
