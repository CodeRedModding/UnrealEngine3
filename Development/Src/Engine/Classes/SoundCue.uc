/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SoundCue extends Object
	dependson( AudioDevice )
	hidecategories( object )
	native;

struct native export SoundNodeEditorData
{
	var	native const int NodePosX;
	var native const int NodePosY;
};

/** Sound group this sound cue belongs to */
var()						Name									SoundClass;
var							ESoundClassName							SoundClassName;

/** For debugging purpose only . */
var()						bool									bDebug;							

var							SoundNode								FirstNode;
var		native		  		const Map{USoundNode*,FSoundNodeEditorData}	EditorData;
var		transient			float									MaxAudibleDistance;
var()						float									VolumeMultiplier;
var()						float									PitchMultiplier;
var							float									Duration;

/** Reference to FaceFX AnimSet package the animation is in */
//var() editoronly notforconsole FaceFXAnimSet						FaceFXAnimSetRef;
var()						FaceFXAnimSet							FaceFXAnimSetRef;
/** Name of the FaceFX Group the animation is in */
var()						string									FaceFXGroupName;
/** Name of the FaceFX Animation */
var()						string									FaceFXAnimName;

/** Maximum number of times this cue can be played concurrently. */
var()						int										MaxConcurrentPlayCount;
/** Number of times this cue is currently being played. */
var	const transient duplicatetransient int							CurrentPlayCount;

var	 deprecated	Name												SoundGroup;

native final function float GetCueDuration();

defaultproperties
{
	VolumeMultiplier=0.75
	PitchMultiplier=1
	MaxConcurrentPlayCount=16
}
