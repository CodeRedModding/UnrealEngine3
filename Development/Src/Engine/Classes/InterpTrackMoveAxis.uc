class InterpTrackMoveAxis extends InterpTrackFloatBase
	dependson(InterpTrackMove)
	native(Interpolation);

/** 
  * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
  *
  * Subtrack for InterpTrackMove
  * Transforms an interp actor on one axis
  */

/** List of axies this track can use */
enum EInterpMoveAxis
{
	AXIS_TranslationX,
	AXIS_TranslationY,
	AXIS_TranslationZ,
	AXIS_RotationX,
	AXIS_RotationY,
	AXIS_RotationZ,
};

/** The axis which this track will use when transforming an actor */
var EInterpMoveAxis MoveAxis;

/** Lookup track to use when looking at different groups for transform information*/
var	InterpLookupTrack LookupTrack;

cpptext
{
	virtual INT GetKeyframeIndex( FLOAT KeyTime ) const;
	virtual INT AddKeyframe( FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode );
	virtual void UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst);
	virtual INT SetKeyframeTime( INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder );
	virtual void RemoveKeyframe( INT KeyIndex );
	virtual INT DuplicateKeyframe( INT KeyIndex, FLOAT NewKeyTime );
	FName GetLookupKeyGroupName( INT KeyIndex );
	void SetLookupKeyGroupName( INT KeyIndex, const FName& NewGroupName );
	void ClearLookupKeyGroupName( INT KeyIndex );
	
	// FCurveEdInterface interface
	virtual FColor GetSubCurveButtonColor( INT SubCurveIndex, UBOOL bIsSubCurveHidden ) const;
	virtual INT CreateNewKey( FLOAT KeyIn );
	virtual void DeleteKey( INT KeyIndex );
	virtual INT SetKeyIn( INT KeyIndex, FLOAT NewInVal );

	/**
	 * Provides the color for the given key at the given sub-curve.
	 *
	 * @param		SubIndex	The index of the sub-curve
	 * @param		KeyIndex	The index of the key in the sub-curve
	 * @param[in]	CurveColor	The color of the curve
	 * @return					The color that is associated the given key at the given sub-curve
	 */
	virtual FColor GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor);
	void GetKeyframeValue( UInterpTrackInst* TrInst, INT KeyIndex, FLOAT& OutTime, FLOAT &OutValue, FLOAT* OutArriveTangent, FLOAT* OutLeaveTangent );
	FLOAT EvalValueAtTime( UInterpTrackInst* TrInst, FLOAT Time );

	virtual class UMaterial* GetTrackIcon() const;

	/** 
	 * Reduce Keys within Tolerance
	 *
	 * @param bIntervalStart	start of the key to reduce
	 * @param bIntervalEnd		end of the key to reduce
	 * @param Tolerance			tolerance
	 */
	virtual void ReduceKeys( FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance );
};

defaultproperties
{
	CurveTension=0.0
	bSubTrackOnly=true;
	TrackTitle="Move Axis Track"
}

