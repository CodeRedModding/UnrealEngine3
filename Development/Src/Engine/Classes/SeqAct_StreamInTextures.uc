/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_StreamInTextures extends SeqAct_Latent
	native(Sequence);

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
	void DeActivated();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();
	virtual void UpdateObject();

	virtual void ApplyForceMipSettings( UBOOL bEnable, FLOAT Duration );

	/**
	 * Adds an error message to the map check dialog if Duration is invalid
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

/** Whether we should stream in textures based on location or usage. If TRUE, textures surrounding the attached actors will start to stream in. If FALSE, textures used by the attached actors will start to stream in. */
var	deprecated bool	bLocationBased;

/** Number of seconds to force the streaming system to stream in all of the target's textures or enforce bForceMiplevelsToBeResident */
var()	float	Seconds;

/**
 * Allows adjusting the desired streaming distance around the specified Location.
 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
 * A lower value (0.0-1.0) makes the textures stream in later and use less memory.
 */
var()	float	StreamingDistanceMultiplier;

/** Is this streaming currently active? */
var const bool	bStreamingActive;

/** Whether the AllLoaded output has been triggered. */
var const bool	bHasTriggeredAllLoaded;

/**
 * ID for when we started checking for NumWantingResources,
 * to make sure we've let the streaming system update it before we trigger the AllLoaded output.
 */
var const int NumWantingResourcesID;

/** Timestamp for when we should stop the forced texture streaming. */
var const float StopTimestamp;

/** Textures surrounding the LocationActors will begin to stream in */
var() array<Object> LocationActors;

/** Array of Materials to set bForceMiplevelsToBeResident on their textures for the duration of this action. */
var() array<MaterialInterface> ForceMaterials;

/** Texture groups that will use extra (higher resolution) mip-levels. */
var(CinematicMipLevels) const TextureGroupContainer	CinematicTextureGroups;

/** Internal bitfield representing the selection in CinematicTextureGropus. */
var native private transient const int		SelectedCinematicTextureGroups;


/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 2;
}

defaultproperties
{
	ObjName="Stream In Textures"
	ObjCategory="Actor"
	Seconds=15.0
	StreamingDistanceMultiplier=1.0
	bStreamingActive=false
	StopTimestamp=0.0
	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")
	OutputLinks(0)=(LinkDesc="Out")				// always fires on activation
	OutputLinks(2)=(LinkDesc="All Loaded")		// fire when all textures are loaded, or duration is over, whichever comes first

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Actor",PropertyName=Targets)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Location",PropertyName=LocationActors)
}
