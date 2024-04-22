/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionFloatUniformRange extends DistributionFloat
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;
	
/** 
 *	High value of the maximum end of the output float distribution. 
 *	Final value will be between [MinHigh..MinLow] or [MaxHigh..MaxLow]
 */
var()	float	MaxHigh;
/** 
 *	Low value of the maximum end of the output float distribution.
 *	Final value will be between [MinHigh..MinLow] or [MaxHigh..MaxLow]
 */
var()	float	MaxLow;

/** 
 *	High value of the minimum end of the output float distribution. 
 *	Final value will be between [MinHigh..MinLow] or [MaxHigh..MaxLow]
 */
var()	float	MinHigh;
/** 
 *	Low value of the minimum end of the output float distribution.
 *	Final value will be between [MinHigh..MinLow] or [MaxHigh..MaxLow]
 */
var()	float	MinLow;

/** 
 *	If TRUE, mirror the MaxHigh, MinHigh values in the MaxLow, MinLow values 
 *	ie, MinHigh = -MaxHigh; MinLow = -MaxLow
 */
var()	bool	bMirrorMaxMin;

cpptext
{
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual FLOAT GetValue( FLOAT F = 0.f, UObject* Data = NULL, class FRandomStream* InRandomStream = NULL );

#if !CONSOLE
	/**
	 * Return the operation used at runtime to calculate the final value
	 */
	virtual ERawDistributionOperation GetOperation();
	
	/**
	 * Fill out an array of floats and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 floats
	 * @return The number of elements (values) set in the array
	 */
	virtual DWORD InitializeRawEntry(FLOAT Time, FLOAT* Values);
#endif

	// FCurveEdInterface interface
	virtual INT		GetNumKeys();
	virtual INT		GetNumSubCurves() const;

	/**
	 * Provides the color for the sub-curve button that is present on the curve tab.
	 *
	 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
  	 * @param	bIsSubCurveHidden	Is the curve hidden?
	 * @return						The color associated to the given sub-curve index.
	 */
	virtual FColor	GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const;

	virtual FLOAT	GetKeyIn(INT KeyIndex);
	virtual FLOAT	GetKeyOut(INT SubIndex, INT KeyIndex);

	/**
	 * Provides the color for the given key at the given sub-curve.
	 *
	 * @param		SubIndex	The index of the sub-curve
	 * @param		KeyIndex	The index of the key in the sub-curve
	 * @param[in]	CurveColor	The color of the curve
	 * @return					The color that is associated the given key at the given sub-curve
	 */
	virtual FColor	GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor);

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
}

defaultproperties
{
	bCanBeBaked=false
}
