class InterpTrackFaceFX extends InterpTrack
	native(Interpolation);
	
/** 
 * InterpTrackFaceFX
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
 
cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// InterpTrack interface
	virtual INT GetNumKeyframes() const;
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const;
	virtual FLOAT GetTrackEndTime() const;
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const;
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	//virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);
	//virtual FColor GetKeyframeColor(INT KeyIndex) const;

	virtual void PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	virtual void PreviewStopPlayback(class UInterpTrackInst* TrInst);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
	
	// InterpTrackFaceFX interface
	void GetSeqInfoForTime( FLOAT InTime, FString& OutGroupName, FString& OutSeqName, FLOAT& OutPosition, FLOAT& OutSeqStart, USoundCue*& OutSoundCue );

	/** Updates references to sound cues for all of this track's FaceFX animation keys.  Should be called at
		load time in the editor as well as whenever the track's data is changed. */
	void UpdateFaceFXSoundCueReferences( class UFaceFXAsset* FaceFXAsset );
}

/** Structure used for holding information for one FaceFX animation played by the track. */
struct native FaceFXTrackKey
{
	/** Position in the Matinee sequence to start playing this FaceFX animation. */
	var		float	StartTime;

	/** Name of FaceFX group containing sequence to play. */
	var		string	FaceFXGroupName;

	/** Name of FaceFX sequence to play. */
	var		string	FaceFXSeqName;
};	

/** Extra sets of animation that you wish to use on this Group's Actor during the matinee sequence. */
var()	array<FaceFXAnimSet>	FaceFXAnimSets;

/** Track of different animations to play and when to start playing them. */
var	array<FaceFXTrackKey>	FaceFXSeqs;

/** In Matinee, cache a pointer to the Actor's FaceFXAsset, so we can get info like anim lengths. */
var transient FaceFXAsset	CachedActorFXAsset;


/** Structure used for holding information for one FaceFX animation played by the track. */
struct native FaceFXSoundCueKey
{
	/** Sound cue associated with this key's FaceFX sequence.  Currently this is maintained automatically by
	    the editor and saved out when the map is saved to disk.  The game requires the sound cue reference
		in order to play FaceFX animations with audio. */
	var private const SoundCue FaceFXSoundCue;
};	


/** One key for each key in the associated FaceFX track's array of keys */
var	private const array< FaceFXSoundCueKey > FaceFXSoundCueKeys;


defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstFaceFX'
	TrackTitle="FaceFX"
	bOnePerGroup=true
}
