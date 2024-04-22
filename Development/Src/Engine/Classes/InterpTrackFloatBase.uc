class InterpTrackFloatBase extends InterpTrack
	native(Interpolation)
	abstract;

/** 
 * InterpTrackFloatBase
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
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);
	virtual FColor GetKeyframeColor(INT KeyIndex) const;

	// FCurveEdInterface interface
	virtual INT		GetNumKeys();
	virtual INT		GetNumSubCurves() const;
	virtual FLOAT	GetKeyIn(INT KeyIndex);
	virtual FLOAT	GetKeyOut(INT SubIndex, INT KeyIndex);
	virtual void	GetInRange(FLOAT& MinIn, FLOAT& MaxIn);
	virtual void	GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);
	virtual BYTE	GetKeyInterpMode(INT KeyIndex);
	virtual void	GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent);
	virtual FLOAT	EvalSub(INT SubIndex, FLOAT InVal);

	virtual INT		CreateNewKey(FLOAT KeyIn);
	virtual void	DeleteKey(INT KeyIndex);

	virtual INT		SetKeyIn(INT KeyIndex, FLOAT NewInVal);
	virtual void	SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal);
	virtual void	SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode);
	virtual void	SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent);

	/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
	virtual UBOOL	UsingLegacyInterpMethod() const;

	/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
	virtual void	UpgradeInterpMethod();
}

/** Actually track data containing keyframes of float as it varies over time. */
var		InterpCurveFloat	FloatTrack;

/** Tension of curve, used for keypoints using automatic tangents. */
var()	float				CurveTension;

defaultproperties
{
	TrackTitle="Generic Float Track"
	CurveTension=0.0
}
