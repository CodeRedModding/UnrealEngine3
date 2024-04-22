/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** Thread-safe appSRand based RNG. */
class FRandomStream
{
public:

	/** Default constructor - should set seed prior to use. */
	FRandomStream() :
	  Seed(0)
	{
	}

	/** Initialization constructor. */
	FRandomStream(INT InSeed):
		Seed(InSeed)
	{}

	void Initialize(INT InSeed)
	{
		Seed = InSeed;
	}

	/** @return A random number between 0 and 1. */
	FLOAT GetFraction()
	{
		MutateSeed();

		//@todo fix type aliasing
		const FLOAT SRandTemp = 1.0f;
		FLOAT Result;
		*(INT*)&Result = (*(INT*)&SRandTemp & 0xff800000) | (Seed & 0x007fffff);
		return appFractional(Result); 
	}

	/** @return A random number between 0 and MAXUINT. */
	UINT GetUnsignedInt()
	{
		MutateSeed();

		return Seed;
	}

	FVector GetUnitVector()
	{
		FVector Result;
		do
		{
			// Check random vectors in the unit sphere so result is statistically uniform.
			Result.X = GetFraction() * 2 - 1;
			Result.Y = GetFraction() * 2 - 1;
			Result.Z = GetFraction() * 2 - 1;
		} while( Result.SizeSquared() > 1.f );
		return Result.UnsafeNormal();
	}

private:

	/** Mutates the current seed into the next seed. */
	void MutateSeed()
	{
		Seed = (Seed * 196314165) + 907633515; 
	}

	UINT Seed;
};
