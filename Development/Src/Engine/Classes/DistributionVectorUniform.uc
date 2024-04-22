/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionVectorUniform extends DistributionVector
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** Upper end of vector magnitude range. */
var() vector	Max;
/** Lower end of vector magnitude range. */
var() vector	Min;

/** If true, X == Y == Z ie. only one degree of freedom. If false, each axis is picked independently. */ 
var		bool							bLockAxes;
var()	EDistributionVectorLockFlags	LockedAxes;
var()	EDistributionVectorMirrorFlags	MirrorFlags[3];
var()	bool							bUseExtremes;

cpptext
{
	virtual void PostLoad();

	virtual FVector	GetValue( FLOAT F = 0.f, UObject* Data = NULL, INT LastExtreme = 0, class FRandomStream* InRandomStream = NULL );

#if !CONSOLE
	/**
	 * Return the operation used at runtime to calculate the final value
	 */
	virtual ERawDistributionOperation GetOperation();
	
	/**
	 * Return the lock flags used at runtime to calculate the final value
	 */
	virtual ERawDistributionLockFlags GetLockFlags(INT InIndex) 
	{
		if (InIndex != 0)
		{
			return RDL_None;
		}

		switch (LockedAxes)
		{
		case EDVLF_XY:		return RDL_XY;
		case EDVLF_XZ:		return RDL_XZ;
		case EDVLF_YZ:		return RDL_YZ;
		case EDVLF_XYZ:		return RDL_XYZ;
		}
		return RDL_None;
	}

	/**
	 * Fill out an array of vectors and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
	 * @return The number of elements (values) set in the array
	 */
	virtual DWORD InitializeRawEntry(FLOAT Time, FVector* Values);
#endif
	
	/** These two functions will retrieve the Min/Max values respecting the Locked and Mirror flags. */
	virtual FVector GetMinValue();
	virtual FVector GetMaxValue();

	// UObject interface
	virtual void Serialize(FArchive& Ar);

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

	// DistributionVector interface
	virtual	void	GetRange(FVector& OutMin, FVector& OutMax);
}

defaultproperties
{
	MirrorFlags[0] = EDVMF_Different
	MirrorFlags[1] = EDVMF_Different
	MirrorFlags[2] = EDVMF_Different
	
	bUseExtremes = false
}
