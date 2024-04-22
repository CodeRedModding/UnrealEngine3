/** 
 * InterpTrackNotify
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class InterpTrackNotify extends InterpTrack
	native(Interpolation);
 
cpptext
{
	// UObject interface
	//virtual void PostLoad();

	// InterpTrack interface
	virtual INT GetNumKeyframes() const;
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const;
	virtual FLOAT GetTrackEndTime() const;
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const;
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);
	//virtual FColor GetKeyframeColor(INT KeyIndex) const;

	virtual void PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
}

/** Sequence node to pass into the Notify function */
var AnimNodeSequence    Node;

/** Name of the parent node - used for the Notify function */
var name                ParentNodeName;

/** Sequence to be the Outer of the Notifies */
var AnimSequence        OuterSequence;

/** AnimSet to be the Outer of the OuterSequence */
var AnimSet             OuterSet;

/** Information for one notify in the track. */
struct native NotifyTrackKey
{
	var float	    Time;
	var AnimNotify	Notify;
};

/** Array of notifies to fire off. */
var	array<NotifyTrackKey>	NotifyTrack;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstNotify'
	TrackTitle="Notify"
}
