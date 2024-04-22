/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SoundClass extends Object
	hidecategories( object )
	dontsortcategories( SoundClass )
	native( AudioDevice );

struct native export SoundClassEditorData
{
	var	native const int NodePosX;
	var native const int NodePosY;
};
	
/**
 * Structure containing configurable properties of a sound class.
 */
struct native SoundClassProperties
{
	/** Volume multiplier. */
	var() float Volume;
	/** Pitch multiplier. */
	var() float Pitch;
	/** The amount of stereo sounds to bleed to the rear speakers */
	var() float StereoBleed;
	/** The amount of a sound to bleed to the LFE channel */
	var() float LFEBleed;
	/** Voice center channel volume - Not a multiplier (no propagation)	*/
	var() float VoiceCenterChannelVolume;
	/** Volume of the radio filter effect */
	var() float RadioFilterVolume;
	/** Volume at which the radio filter kicks in */
	var() float RadioFilterVolumeThreshold;

	/** Sound mode voice - whether to apply audio effects */
	var() bool bApplyEffects;
	/** Whether to artificially prioritise the component to play */
	var() bool bAlwaysPlay;
	/** Whether or not this sound plays when the game is paused in the UI */
	var() bool bIsUISound;
	/** Whether or not this is music (propagates only if parent is TRUE) */
	var() bool bIsMusic;
	/** Whether or not this sound class has reverb applied */
	var() bool bReverb;
	/** Whether or not this sound class forces sounds to the center channel */
	var() bool bCenterChannelOnly;
	/** Whether the Interior/Exterior volume and LPF modifiers should be applied */
	var() bool bApplyAmbientVolumes;

	structdefaultproperties
	{
		Volume=1
		Pitch=1
		StereoBleed=0.25
		LFEBleed=0.5
		VoiceCenterChannelVolume=0
		RadioFilterVolume=0
		RadioFilterVolumeThreshold=0
		bApplyEffects=FALSE
		bAlwaysPlay=FALSE
		bIsUISound=FALSE
		bIsMusic=FALSE
		bReverb=TRUE
		bCenterChannelOnly=FALSE
		bApplyAmbientVolumes=FALSE
	}
};
	
/** Configurable properties like volume and priority. */
var()				SoundClassProperties			Properties;
/** Array of names of child sound classes. Empty for leaf classes. */
var()				array<name>						ChildClassNames;
/** Whether this class is referenced by another class */
var					bool							bIsChild;
/** ID used in menus in the editor */
var	editoronly		int								MenuID;
/** Editor data for all sound classes; only used in the master sound class */	
var	native			const 	Map{USoundClass*, FSoundClassEditorData}	EditorData;

defaultproperties
{
}
