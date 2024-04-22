/*=============================================================================
	UnDistributions.cpp: Implementation of distribution classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

IMPLEMENT_CLASS(UDistributionFloat);
IMPLEMENT_CLASS(UDistributionVector);

DWORD GDistributionType = 1;
#define GLookupTableMaxFrames 100
#define GLookupTableFrameRate 20.0f

/*-----------------------------------------------------------------------------
	FDistribution
-----------------------------------------------------------------------------*/

void FRawDistribution::GetValue1(FLOAT Time, FLOAT* Value, INT Extreme, class FRandomStream* InRandomStream)
{
	switch (Op)
	{
	case RDO_None:
		GetValue1None(Time,Value);
		break;
	case RDO_Extreme:
		GetValue1Extreme(Time,Value,Extreme,InRandomStream);
		break;
	case RDO_Random:
		GetValue1Random(Time,Value,InRandomStream);
		break;
	case RDO_RandomRange:
		GetValue1RandomRange(Time,Value,InRandomStream);
		break;
	default: // compiler complains
		checkSlow(0);
		*Value = 0.0f;
		break;
	}
}

void FRawDistribution::GetValue3(FLOAT Time, FLOAT* Value, INT Extreme, class FRandomStream* InRandomStream)
{
	switch (Op)
	{
	case RDO_None:
		GetValue3None(Time,Value);
		break;
	case RDO_Extreme:
		GetValue3Extreme(Time,Value,Extreme,InRandomStream);
		break;
	case RDO_Random:
		GetValue3Random(Time,Value,InRandomStream);
		break;
	case RDO_RandomRange:
		GetValue3RandomRange(Time,Value,InRandomStream);
		break;
	default: // compiler complains
		checkSlow(0);
		*Value = 0.0f;
		break;
	}
}

void FRawDistribution::GetValue1Extreme(FLOAT Time, FLOAT* InValue, INT Extreme, class FRandomStream* InRandomStream)
{
	FLOAT* RESTRICT Value = InValue;
	const LOOKUPVALUE* Entry1;
	const LOOKUPVALUE* Entry2;
	FLOAT LerpAlpha = 0.0f;
	FLOAT RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
	GetEntry( Time, Entry1, Entry2, LerpAlpha );
	const LOOKUPVALUE* RESTRICT NewEntry1 = Entry1;
	const LOOKUPVALUE* RESTRICT NewEntry2 = Entry2;
	INT InitialElement = ((Extreme > 0) || ((Extreme == 0) && (RandValue > 0.5f)));
	Value[0] = Lerp(NewEntry1[InitialElement + 0], NewEntry2[InitialElement + 0], LerpAlpha);
}

void FRawDistribution::GetValue3Extreme(FLOAT Time, FLOAT* InValue, INT Extreme, class FRandomStream* InRandomStream)
{
	FLOAT* RESTRICT Value = InValue;
	const LOOKUPVALUE* Entry1;
	const LOOKUPVALUE* Entry2;
	FLOAT LerpAlpha = 0.0f;
	FLOAT RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
	GetEntry( Time, Entry1, Entry2, LerpAlpha );
	const LOOKUPVALUE* RESTRICT NewEntry1 = Entry1;
	const LOOKUPVALUE* RESTRICT NewEntry2 = Entry2;
	INT InitialElement = ((Extreme > 0) || ((Extreme == 0) && (RandValue > 0.5f)));
	InitialElement *= 3;
	FLOAT T0 = Lerp(NewEntry1[InitialElement + 0], NewEntry2[InitialElement + 0], LerpAlpha);
	FLOAT T1 = Lerp(NewEntry1[InitialElement + 1], NewEntry2[InitialElement + 1], LerpAlpha);
	FLOAT T2 = Lerp(NewEntry1[InitialElement + 2], NewEntry2[InitialElement + 2], LerpAlpha);
	Value[0] = T0;
	Value[1] = T1;
	Value[2] = T2;
}

void FRawDistribution::GetValue1Random(FLOAT Time, FLOAT* InValue, class FRandomStream* InRandomStream)
{
	FLOAT* RESTRICT Value = InValue;
	const LOOKUPVALUE* Entry1;
	const LOOKUPVALUE* Entry2;
	FLOAT LerpAlpha = 0.0f;
	FLOAT RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
	GetEntry( Time, Entry1, Entry2, LerpAlpha );
	const LOOKUPVALUE* RESTRICT NewEntry1 = Entry1;
	const LOOKUPVALUE* RESTRICT NewEntry2 = Entry2;
	LOOKUPVALUE Value1,Value2;
	Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
	Value2 = Lerp(NewEntry1[1 + 0], NewEntry2[1 + 0], LerpAlpha);
	Value[0] = Value1 + (Value2 - Value1) * RandValue;
}

void FRawDistribution::GetValue3Random(FLOAT Time, FLOAT* InValue, class FRandomStream* InRandomStream)
{
	FLOAT* RESTRICT Value = InValue;
	const LOOKUPVALUE* Entry1;
	const LOOKUPVALUE* Entry2;
	FLOAT LerpAlpha = 0.0f;
	FVector RandValues(
		DIST_GET_RANDOM_VALUE(InRandomStream),
		DIST_GET_RANDOM_VALUE(InRandomStream),
		DIST_GET_RANDOM_VALUE(InRandomStream)
		);
	GetEntry( Time, Entry1, Entry2, LerpAlpha );
	const LOOKUPVALUE* RESTRICT NewEntry1 = Entry1;
	const LOOKUPVALUE* RESTRICT NewEntry2 = Entry2;
	LOOKUPVALUE Value1,Value2;
	FLOAT T0;
	FLOAT T1;
	FLOAT T2;

	if (DIST_IS_UNIFORMCURVE(Type))
	{
		FLOAT X0, Y0, Z0;
		FLOAT X1, Y1, Z1;

		switch (DIST_GET_LOCKFLAG_0(Type))
		{
		case RDL_XY:
			X0 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Y0 = X0;
			Z0 = Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
			break;
		case RDL_XZ:
			X0 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Y0 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Z0 = X0;
			break;
		case RDL_YZ:
			X0 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Y0 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Z0 = Y0;
			break;
		case RDL_XYZ:
			X0 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Y0 = X0;
			Z0 = X0;
			break;
		case RDL_None:
		default:
			X0 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Y0 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Z0 = Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
			break;
		}

		switch (DIST_GET_LOCKFLAG_1(Type))
		{
		case RDL_XY:
			X1 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			Y1 = X1;
			Z1 = Lerp(NewEntry1[3 + 2], NewEntry2[3 + 2], LerpAlpha);
			break;
		case RDL_XZ:
			X1 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			Y1 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			Z1 = X1;
			break;
		case RDL_YZ:
			X1 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			Y1 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			Z1 = Y1;
			break;
		case RDL_XYZ:
			X1 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			Y1 = X1;
			Z1 = X1;
			break;
		case RDL_None:
		default:
			X1 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			Y1 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			Z1 = Lerp(NewEntry1[3 + 2], NewEntry2[3 + 2], LerpAlpha);
			break;
		}

		Value[0] = X0 + (X1 - X0) * RandValues[0];
		Value[1] = Y0 + (Y1 - Y0) * RandValues[1];
		Value[2] = Z0 + (Z1 - Z0) * RandValues[2];
	}
	else
	{
		switch (DIST_GET_LOCKFLAG_0(Type))
		{
		case RDL_XY:
			Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			T0 = Value1 + (Value2 - Value1) * RandValues[0];
			Value1 = Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 2], NewEntry2[3 + 2], LerpAlpha);
			T2 = Value1 + (Value2 - Value1) * RandValues[2];
			Value[0] = T0;
			Value[1] = T0;
			Value[2] = T2;
			break;
		case RDL_XZ:
			Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			T0 = Value1 + (Value2 - Value1) * RandValues[0];
			Value1 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			T1 = Value1 + (Value2 - Value1) * RandValues[1];
			Value[0] = T0;
			Value[1] = T1;
			Value[2] = T0;
			break;
		case RDL_YZ:
			Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			T0 = Value1 + (Value2 - Value1) * RandValues[1];
			Value1 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			T1 = Value1 + (Value2 - Value1) * RandValues[2];
			Value[0] = T0;
			Value[1] = T1;
			Value[2] = T1;
			break;
		case RDL_XYZ:
			Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			T0 = Value1 + (Value2 - Value1) * RandValues[0];
			Value[0] = T0;
			Value[1] = T0;
			Value[2] = T0;
			break;
		case RDL_None:
		default:
			Value1 = Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
			T0 = Value1 + (Value2 - Value1) * RandValues[0];
			Value1 = Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
			T1 = Value1 + (Value2 - Value1) * RandValues[1];
			Value1 = Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
			Value2 = Lerp(NewEntry1[3 + 2], NewEntry2[3 + 2], LerpAlpha);
			T2 = Value1 + (Value2 - Value1) * RandValues[2];
			Value[0] = T0;
			Value[1] = T1;
			Value[2] = T2;
			break;
		}

	}
}

void FRawDistribution::GetValue1RandomRange(FLOAT Time, FLOAT* InValue, class FRandomStream* InRandomStream)
{
	//@todo. Implement this when baking of UniformRange is supported
}

void FRawDistribution::GetValue3RandomRange(FLOAT Time, FLOAT* InValue, class FRandomStream* InRandomStream)
{
	//@todo. Implement this when baking of UniformRange is supported
}

/**
 * Calcuate the float or vector value at the given time 
 * @param Time The time to evaluate
 * @param Value An array of (1 or 3) FLOATs to receivet the values
 * @param NumCoords The number of floats in the Value array
 * @param Extreme For distributions that use one of the extremes, this is which extreme to use
 */
void FRawDistribution::GetValue(FLOAT Time, FLOAT* Value, INT NumCoords, INT Extreme, class FRandomStream* InRandomStream)
{
	checkSlow(NumCoords == 3 || NumCoords == 1);
	switch (Op)
	{
	case RDO_None:
		if (NumCoords == 1)
		{
			GetValue1None(Time,Value);
		}
		else
		{
			GetValue3None(Time,Value);
		}
		break;
	case RDO_Extreme:
		if (NumCoords == 1)
		{
			GetValue1Extreme(Time,Value,Extreme,InRandomStream);
		}
		else
		{
			GetValue3Extreme(Time,Value,Extreme,InRandomStream);
		}
		break;
	case RDO_Random:
		if (NumCoords == 1)
		{
			GetValue1Random(Time,Value,InRandomStream);
		}
		else
		{
			GetValue3Random(Time,Value,InRandomStream);
		}
		break;
	}
}

/**
 * Return the UDistribution* variable if the given StructProperty
 * points to a FRawDistribution* struct
 * @param Property Some UStructProperty
 * @param Data Memory that owns the property
 * @return The UDisitribution* object if this is a FRawDistribution* struct, 
 *         or NULL otherwise
 */
UObject* FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(UStructProperty* Property, BYTE* Data)
{
	// if the struct in this property is of type FRawDistributionFloat
	if (Property->Struct->GetFName() == NAME_RawDistributionFloat)
	{
		// then return the UDistribution pointed to by the FRawDistributionFloat
		return ((FRawDistributionFloat*)(Data + Property->Offset))->Distribution;
	}
	// if the struct in this property is of type FRawDistributionVector
	else if (Property->Struct->GetFName() == NAME_RawDistributionVector)
	{
		// then return the UDistribution pointed to by the FRawDistributionVector
		return ((FRawDistributionVector*)(Data + Property->Offset))->Distribution;
	}

	// if this wasn't a FRawDistribution*, return NULL
	return NULL;
}


/*-----------------------------------------------------------------------------
	FDistributionFloat
-----------------------------------------------------------------------------*/

#if !CONSOLE
/** 
 * Fill out the RawDistrubutions with data appropriate to this distribution 
 */
void FRawDistributionFloat::Initialize()
{
	// Nothing to do if we don't have a distribution.
	if( Distribution == NULL )
	{
		return;
	}

	// does this FRawDist need updating? (if UDist is dirty or somehow the distribution wasn't dirty, but we have no data)
	UBOOL bNeedsUpdating = FALSE;
	if (Distribution->bIsDirty || (LookupTable.Num() == 0 && Distribution->CanBeBaked()))
	{
		if (!Distribution->bIsDirty)
		{
			debugf(TEXT("Somehow Distribution %s wasn't dirty, but its FRawDistribution wasn't ever initialized!"), *Distribution->GetFullName());
			
		}
		bNeedsUpdating = TRUE;
	}

	// only initialize if we need to
	if (!bNeedsUpdating)
	{
		return;
	}

	// always empty out the lookup table
	LookupTable.Empty();

	// distribution is no longer dirty (if it was)
	// template objects aren't marked as dirty, because any UDists that uses this as an archetype, 
	// aren't the default values, and has already been saved, needs to know to build the FDist
	if (!Distribution->IsTemplate())
	{
		Distribution->bIsDirty = FALSE;
	}

	// if the distribution can't be baked out, then we do nothing here
	if (!Distribution->CanBeBaked())
	{
		return;
	}

	FLOAT MinIn, MaxIn;
	FLOAT MinOut, MaxOut;
	// fill out our min/max
	Distribution->GetInRange(MinIn, MaxIn);
	Distribution->GetOutRange(MinOut, MaxOut);

	DWORD NumPoints;
	FLOAT TimeScale;

	// check for some special cases

	// first check for linear line between two points - this there's no need for intermediate values at all
	if (Distribution->GetNumKeys() == 2 && Distribution->GetKeyInterpMode(0) == CIM_Linear)
	{
		NumPoints = 2;
		TimeScale = MaxIn - MinIn;
	}
	// next check for entire distribution fitting inside one time slice
	else if (MaxIn - MinIn < 1.0f / GLookupTableFrameRate)
	{
		NumPoints = 2;
		TimeScale = MaxIn - MinIn;
	}
	// otherwise, normal
	else
	{
		// we sample at GLookupTableFrameRate fps, or with a max of GLookupTableMaxFrames sample points, making sure there's always 1
		NumPoints = Min<DWORD>((DWORD)((MaxIn - MinIn) * GLookupTableFrameRate), (GLookupTableMaxFrames - 1)) + 1;

		// calculate time scale (1 point doesn't need a scale)
		TimeScale = (NumPoints > 1) ? (MaxIn - MinIn) / (FLOAT)(NumPoints - 1) : 0;
	}


	// get the operation to use, and calculate the number of elements needed for that operation
	Op = Distribution->GetOperation();
	LookupTableNumElements = (Op == RDO_None) ? 1 : 2;

	// Need to ensure this is 0 now as we use it for lock flags and such...
	Type = 0;

	// store our min/max as the first 2 entries in the table
	// note that there is only one element per entry here
	// @GEMINI_TODO: For (non-uniform) constants, we don't need these at 
	LookupTable.AddItem(MinOut);
	LookupTable.AddItem(MaxOut);

	// bake out all the points
	for (DWORD Sample = 0; Sample < NumPoints; Sample++)
	{
		// time for this point
		FLOAT Time = MinIn + Sample * TimeScale;
		// get all elements at this time point
		FLOAT Values[4];
		// get values, and remember how many were used
		Distribution->InitializeRawEntry(Time, Values);

		// add each one
		for (DWORD Element = 0; Element < LookupTableNumElements; Element++)
		{
			LookupTable.AddItem(Values[Element]);
		}
	}

	// fill out the raw distrib structure
	LookupTableChunkSize = LookupTableNumElements; 
	LookupTableTimeScale = TimeScale;
	if (TimeScale != 0.0f)
	{
		LookupTableTimeScale = 1.0f / TimeScale;
	}
	LookupTableStartTime = MinIn;
}
#endif

FLOAT FRawDistributionFloat::GetValue(FLOAT F, UObject* Data, class FRandomStream* InRandomStream)
{
#if !CONSOLE
	// make sure it's up to date
	if( GIsEditor || (Distribution && Distribution->bIsDirty) )
	{
		Initialize();
	}
#endif

	// if this distribution is in memory, that means the package wasn't saved, or we
	// want to use it (or its the editor, and we are in "Use Original Distribution" mode
	if (Distribution 
#if !CONSOLE
		&& (GIsGame || GDistributionType == 0 || LookupTable.Num() == 0)
#endif
		)
	{
		return Distribution->GetValue(F, Data, InRandomStream);
	}

	// if we get here, we better have been initialized!
	check(LookupTable.Num());

	FLOAT Value;
	FRawDistribution::GetValue1(F, &Value, 0, InRandomStream);
	return Value;
}

const FRawDistribution *FRawDistributionFloat::GetFastRawDistribution()
{
#if !CONSOLE
	// make sure it's up to date
	if( GIsEditor )
	{
		Initialize();
	}
#endif

	if (!IsSimple())
	{
		return 0;
	}

	// if this distribution is in memory, that means the package wasn't saved, or we
	// want to use it (or its the editor, and we are in "Use Original Distribution" mode
	if (Distribution 
#if !CONSOLE
		&& (GIsGame || GDistributionType == 0 || LookupTable.Num() == 0)
#endif
		)
	{
		return 0;
	}

	// if we get here, we better have been initialized!
	check(LookupTable.Num());

	return this;
}

/**
 * Get the min and max values
 */
void FRawDistributionFloat::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	if (LookupTable.Num() == 0 || GDistributionType == 0)
	{
		check(Distribution);
		Distribution->GetOutRange(MinOut, MaxOut);
	}
	else
	{
		MinOut = LookupTable(0);
		MaxOut = LookupTable(1);
	}
}

/*-----------------------------------------------------------------------------
	FMatineeRawDistributionFloat
 -----------------------------------------------------------------------------*/

/**
 * Get the value at the specified F
 */
FLOAT FMatineeRawDistributionFloat::GetValue(FLOAT F /*=0.0f*/, UObject* Data /*=NULL*/, class FRandomStream* InRandomStream /*= NULL*/)
{
	if (bInMatinee)
	{
		return MatineeValue;
	}

#if !CONSOLE
	// make sure it's up to date
	if( GIsEditor || (Distribution && Distribution->bIsDirty) )
	{
		Initialize();
	}
#endif

	// if this distribution is in memory, that means the package wasn't saved, or we
	// want to use it (or its the editor, and we are in "Use Original Distribution" mode
	if (Distribution 
#if !CONSOLE
		&& (GIsGame || GDistributionType == 0 || LookupTable.Num() == 0)
#endif
		)
	{
		return Distribution->GetValue(F, Data, InRandomStream);
	}

	// if we get here, we better have been initialized!
	check(LookupTable.Num());

	FLOAT Value;
	FRawDistribution::GetValue1(F, &Value, 0, InRandomStream);
	return Value;
}


/*-----------------------------------------------------------------------------
	UDistributionFloat implementation.
-----------------------------------------------------------------------------*/

/** UObject interface */
void UDistributionFloat::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

UBOOL UDistributionFloat::NeedsLoadForClient() const
{
	return !CanBeBaked() || IsTemplate();
}

UBOOL UDistributionFloat::NeedsLoadForServer() const 
{
	return NeedsLoadForClient();
}

void UDistributionFloat::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bIsDirty = TRUE;
}

FLOAT UDistributionFloat::GetValue( FLOAT F, UObject* Data, class FRandomStream* InRandomStream )
{
	return 0.0;
}

/** Script-accessible way to query a float distribution */
FLOAT UDistributionFloat::GetFloatValue(FLOAT F)
{
	return GetValue(F);
}

void UDistributionFloat::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn	= 0.0f;
	MaxIn	= 0.0f;
}

void UDistributionFloat::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	MinOut	= 0.0f;
	MaxOut	= 0.0f;
}

#if !CONSOLE
/**
 * Fill out an array of floats and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 floats
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionFloat::InitializeRawEntry(FLOAT Time, FLOAT* Values)
{
	Values[0] = GetValue(Time);
	return 1;
}

#endif

/*-----------------------------------------------------------------------------
	FRawDistributionVector
-----------------------------------------------------------------------------*/
#if !CONSOLE
/** 
 * Fill out the RawDistrubutions with data appropriate to this distribution 
 */
void FRawDistributionVector::Initialize()
{
	// Nothing to do if we don't have a distribution.
	if( Distribution == NULL )
	{
		return;
	}

	// does this FRawDist need updating? (if UDist is dirty or somehow the distribution wasn't dirty, but we have no data)
	UBOOL bNeedsUpdating = FALSE;
	if (Distribution->bIsDirty || (LookupTable.Num() == 0 && Distribution->CanBeBaked()))
	{
		if (!Distribution->bIsDirty)
		{
			debugf(TEXT("Somehow Distribution %s wasn't dirty, but its FRawDistribution wasn't ever initialized!"), *Distribution->GetFullName());
		}
		bNeedsUpdating = TRUE;
	}

	// only initialize if we need to
	if (!bNeedsUpdating)
	{
		return;
	}

	// always empty out the lookup table
	LookupTable.Empty();

	// distribution is no longer dirty (if it was)
	// template objects aren't marked as dirty, because any UDists that uses this as an archetype, 
	// aren't the default values, and has already been saved, needs to know to build the FDist
	if (!Distribution->IsTemplate())
	{
		Distribution->bIsDirty = FALSE;	
	}

	// if the distribution can't be baked out, then we do nothing here
	if (!Distribution->CanBeBaked())
	{
		return;
	}

	FLOAT MinIn, MaxIn;
	FLOAT MinOut, MaxOut;
	// fill out our min/max
	Distribution->GetInRange(MinIn, MaxIn);
	Distribution->GetOutRange(MinOut, MaxOut);

	DWORD NumPoints;
	FLOAT TimeScale;

	// check for some special cases

	// first check for linear line between two points - this there's no need for intermediate values at all
	if (Distribution->GetNumKeys() == 2 && Distribution->GetKeyInterpMode(0) == CIM_Linear)
	{
		NumPoints = 2;
		TimeScale = MaxIn - MinIn;
	}
	// next check for entire distribution fitting inside one time slice
	else if (MaxIn - MinIn < 1.0f / GLookupTableFrameRate)
	{
		NumPoints = 2;
		TimeScale = MaxIn - MinIn;
	}
	// otherwise, normal
	else
	{
		// we sample at GLookupTableFrameRate fps, or with a max of GLookupTableMaxFrames sample points, making sure there's always 1
		NumPoints = Min<DWORD>((DWORD)((MaxIn - MinIn) * GLookupTableFrameRate), (GLookupTableMaxFrames - 1)) + 1;

		// calculate time scale (1 point doesn't need a scale)
		TimeScale = (NumPoints > 1) ? (MaxIn - MinIn) / (FLOAT)(NumPoints - 1) : 0;
	}

	// get the operation to use, and calculate the number of elements needed for that operation
	Op = Distribution->GetOperation();
	LookupTableNumElements = (Op == RDO_None) ? 1 : 2;

	Type = 0;
	DIST_SET_LOCKFLAG_0(Distribution->GetLockFlags(0), Type);
	DIST_SET_LOCKFLAG_1(Distribution->GetLockFlags(1), Type);
	DIST_SET_UNIFORMCURVE(Distribution->IsUniformCurve(), Type);

	// store our min/max as the first 2 values in the table
	// note that there is only one element per entry here
	// @GEMINI_TODO: For (non-uniform) constants, we don't need these at 
	LookupTable.AddItem(MinOut);
	LookupTable.AddItem(MaxOut);

	// bake out all the points
	for (DWORD Sample = 0; Sample < NumPoints; Sample++)
	{
		// time for this point
		FLOAT Time = MinIn + Sample * TimeScale;
		// get all elements at this time point
		FVector Values[2];
		// get values, and remember how many were used
		Distribution->InitializeRawEntry(Time, Values);

		// add each one
		for (DWORD Element = 0; Element < LookupTableNumElements; Element++)
		{
			LookupTable.AddItem(Values[Element].X);
			LookupTable.AddItem(Values[Element].Y);
			LookupTable.AddItem(Values[Element].Z);
		}
	}

	// fill out the raw distrib structure
	LookupTableChunkSize = LookupTableNumElements * 3;
	LookupTableTimeScale = TimeScale;
	if (TimeScale != 0.0f)
	{
		LookupTableTimeScale = 1.0f / TimeScale;
	}
	LookupTableStartTime = MinIn;
}
#endif

FVector FRawDistributionVector::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
#if !CONSOLE
	// make sure it's up to date
	if( GIsEditor )
	{
		Initialize();
	}
#endif

	// if this distribution is in memory, that means the package wasn't saved, or we
	// want to use it (or its the editor, and we are in "Use Original Distribution" mode
	if (Distribution 
#if !CONSOLE
		&& (GIsGame || GDistributionType == 0 || LookupTable.Num() == 0)
#endif
		)
	{
		return Distribution->GetValue(F, Data, Extreme, InRandomStream);
	}

	// if we get here, we better have been initialized!
	check(LookupTable.Num());

	FVector Value;
	FRawDistribution::GetValue3(F, &Value.X, Extreme, InRandomStream);
	return Value;
}

const FRawDistribution *FRawDistributionVector::GetFastRawDistribution()
{
#if !CONSOLE
	// make sure it's up to date
	if( GIsEditor || (Distribution && Distribution->bIsDirty) )
	{
		Initialize();
	}
#endif

	if (!IsSimple()) 
	{
		return 0;
	}

	// if this distribution is in memory, that means the package wasn't saved, or we
	// want to use it (or its the editor, and we are in "Use Original Distribution" mode
	if (Distribution
#if !CONSOLE
		&& (GIsGame || GDistributionType == 0 || LookupTable.Num() == 0)
#endif
		)
	{
		return 0;
	}

	// if we get here, we better have been initialized!
	check(LookupTable.Num());

	return this;
}

/**
 * Get the min and max values
 */
void FRawDistributionVector::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	if (LookupTable.Num() == 0 || GDistributionType == 0)
	{
		check(Distribution);
		Distribution->GetOutRange(MinOut, MaxOut);
	}
	else
	{
		MinOut = LookupTable(0);
		MaxOut = LookupTable(1);
	}
}

/*-----------------------------------------------------------------------------
	UDistributionVector implementation.
-----------------------------------------------------------------------------*/

/** UObject interface */
void UDistributionVector::Serialize(FArchive& Ar)
{
	// this will load all of the distribution values to initialize the FDistribution below
	Super::Serialize(Ar);
}

UBOOL UDistributionVector::NeedsLoadForClient() const
{
	return !CanBeBaked() || IsTemplate();
}

UBOOL UDistributionVector::NeedsLoadForServer() const 
{
	return NeedsLoadForClient();
}

void UDistributionVector::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bIsDirty = TRUE;
}

FVector UDistributionVector::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
	return FVector(0,0,0);
}

/** Script-accessible way to query a vector distribution */
FVector UDistributionVector::GetVectorValue(FLOAT F, INT LastExtreme)
{
	return GetValue(F, NULL, LastExtreme);
}

void UDistributionVector::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn	= 0.0f;
	MaxIn	= 0.0f;
}

void UDistributionVector::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	MinOut	= 0.0f;
	MaxOut	= 0.0f;
}

void UDistributionVector::GetRange(FVector& OutMin, FVector& OutMax)
{
	OutMin	= FVector(0.0f, 0.0f, 0.0f);
	OutMax	= FVector(0.0f, 0.0f, 0.0f);
}

#if !CONSOLE
/**
 * Fill out an array of vectors and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
 * @param Op Out value that can override the default operation to apply to the values at GetValue time
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionVector::InitializeRawEntry(FLOAT Time, FVector* Values)
{
	Values[0] = GetValue(Time);
	return 1;
}
#endif
