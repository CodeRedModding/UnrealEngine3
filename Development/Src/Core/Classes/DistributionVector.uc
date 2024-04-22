/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionVector extends Component
	inherits(FCurveEdInterface)
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew
	abstract;

enum EDistributionVectorLockFlags
{
    EDVLF_None,
    EDVLF_XY,
    EDVLF_XZ,
    EDVLF_YZ,
    EDVLF_XYZ
};

enum EDistributionVectorMirrorFlags
{
	EDVMF_Same,
	EDVMF_Different,
	EDVMF_Mirror
};

struct native RawDistributionVector extends RawDistribution
{
	structcpptext
	{
	#if !CONSOLE
		/**
		 * Initialize a raw distribution from the original Unreal distribution
		 */
		void Initialize();
	#endif

		/**
 		 * Gets a pointer to the raw distribution if you can just call FRawDistribution::GetValue3 on it, otherwise NULL 
 		 */
 		const FRawDistribution *GetFastRawDistribution();

		/**
		 * Get the value at the specified F
		 */
		FVector GetValue(FLOAT F=0.0f, UObject* Data=NULL, INT LastExtreme=0, class FRandomStream* InRandomStream = NULL);

		/**
		 * Get the min and max values
		 */
		void GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);

		/**
		 * Is this distribution a uniform type? (ie, does it have two values per entry?)
		 */
		inline UBOOL IsUniform() { return LookupTableNumElements == 2; }
	}

	var() export noclear DistributionVector Distribution;
};

cpptext
{

#if !CONSOLE
	/**
	 * Return the operation used at runtime to calculate the final value
	 */
	virtual ERawDistributionOperation GetOperation() { return RDO_None; }
	
	/**
	 * Return the lock flags used at runtime to calculate the final value
	 */
	virtual ERawDistributionLockFlags GetLockFlags(INT InIndex) { return RDL_None; }

	/**
	 * Return true if the distribution is a uniform curve
	 */
	virtual UBOOL IsUniformCurve() { return FALSE; }

	/**
	 * Fill out an array of vectors and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
	 * @return The number of elements (values) set in the array
	 */
	virtual DWORD InitializeRawEntry(FLOAT Time, FVector* Values);
#endif

	virtual FVector	GetValue( FLOAT F = 0.f, UObject* Data = NULL, INT LastExtreme = 0, class FRandomStream* InRandomStream = NULL );

	virtual void	GetInRange(FLOAT& MinIn, FLOAT& MaxIn);
	virtual void	GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);
	virtual	void	GetRange(FVector& OutMin, FVector& OutMax);

	/**
	 * Return whether or not this distribution can be baked into a FRawDistribution lookup table
	 */
	virtual UBOOL CanBeBaked() const 
	{
		return bCanBeBaked; 
	}

	/** UObject interface */
	virtual void Serialize(FArchive& Ar);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	
	/**
	 * If the distribution can be baked, then we don't need it on the client or server
	 */
	virtual UBOOL NeedsLoadForClient() const;
	virtual UBOOL NeedsLoadForServer() const;
}


/** Can this variable be baked out to a FRawDistribution? Should be TRUE 99% of the time*/
var(Baked) bool bCanBeBaked;

/** Set internally when the distribution is updated so that that FRawDistribution can know to update itself*/
var bool bIsDirty;

/** Script-accessible way to query a vector distribution */
native function vector GetVectorValue(optional float F = 0.0, optional INT LastExtreme = 0);

defaultproperties
{
	bCanBeBaked=true
	// make sure the FRawDistribution is initialized
	bIsDirty=true 
}