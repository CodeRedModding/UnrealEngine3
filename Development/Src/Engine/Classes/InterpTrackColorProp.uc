/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackColorProp extends InterpTrackVectorBase
	native(Interpolation);

cpptext
{
	/** Returns the property name */
	virtual UBOOL GetPropertyName( FName& PropertyNameOut ) const { PropertyNameOut = PropertyName; return TRUE; }

	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	
	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
}

/** Name of property in Group Actor which this track mill modify over time. */
var()	editconst	name		PropertyName;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstColorProp'
	TrackTitle="Color Property"
}
