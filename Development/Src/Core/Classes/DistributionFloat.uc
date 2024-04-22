/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionFloat extends Component
	inherits(FCurveEdInterface)
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew
	abstract;

struct native RawDistributionFloat extends RawDistribution
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
		 * Gets a pointer to the raw distribution if you can just call FRawDistribution::GetValue1 on it, otherwise NULL 
		 */
		const FRawDistribution* GetFastRawDistribution();

		/**
		 * Get the value at the specified F
		 */
		FLOAT GetValue(FLOAT F=0.0f, UObject* Data=NULL, class FRandomStream* InRandomStream = NULL);

		/**
		 * Get the min and max values
		 */
		void GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);

		/**
		 * Is this distribution a uniform type? (ie, does it have two values per entry?)
		 */
		inline UBOOL IsUniform() { return LookupTableNumElements == 2; }
	}

	var() export noclear DistributionFloat Distribution;
};

struct native MatineeRawDistributionFloat extends RawDistributionFloat
{
	structcpptext
	{
		/**
		 * Get the value at the specified F
		 */
		FLOAT GetValue(FLOAT F=0.0f, UObject* Data=NULL, class FRandomStream* InRandomStream = NULL);
	}

	/** The value that will be modified by Matinee. */
	var float MatineeValue;

	/** TRUE when the value returned is controlled by matinee. */
	var bool bInMatinee;

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
	 * Fill out an array of floats and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 values
	 * @return The number of elements (values) set in the array
	 */
	virtual DWORD InitializeRawEntry(FLOAT Time, FLOAT* Values);
#endif

	virtual FLOAT	GetValue( FLOAT F = 0.f, UObject* Data = NULL, class FRandomStream* InRandomStream = NULL );

	virtual void	GetInRange(FLOAT& MinIn, FLOAT& MaxIn);
	virtual void	GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);
	
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

/** Script-accessible way to query a float distribution */
native function float GetFloatValue(optional float F = 0.0);


defaultproperties
{
	bCanBeBaked=true
	// make sure the FRawDistribution is initialized
	bIsDirty=true 
}
