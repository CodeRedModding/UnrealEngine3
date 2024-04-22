/*=============================================================================
	UnDistributions.cpp: Implementation of distribution classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UDistributionFloatConstant);
IMPLEMENT_CLASS(UDistributionFloatConstantCurve);
IMPLEMENT_CLASS(UDistributionFloatUniform);
IMPLEMENT_CLASS(UDistributionFloatUniformCurve);
IMPLEMENT_CLASS(UDistributionFloatUniformRange);
IMPLEMENT_CLASS(UDistributionFloatParameterBase);

IMPLEMENT_CLASS(UDistributionVectorConstant);
IMPLEMENT_CLASS(UDistributionVectorConstantCurve);
IMPLEMENT_CLASS(UDistributionVectorUniform);
IMPLEMENT_CLASS(UDistributionVectorUniformCurve);
IMPLEMENT_CLASS(UDistributionVectorUniformRange);
IMPLEMENT_CLASS(UDistributionVectorParameterBase);

/*-----------------------------------------------------------------------------
	UDistributionFloatConstant implementation.
-----------------------------------------------------------------------------*/

FLOAT UDistributionFloatConstant::GetValue( FLOAT F, UObject* Data, class FRandomStream* InRandomStream )
{
	return Constant;
}


//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatConstant::GetNumKeys()
{
	return 1;
}

INT UDistributionFloatConstant::GetNumSubCurves() const
{
	return 1;
}

FLOAT UDistributionFloatConstant::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionFloatConstant::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex == 0 );
	check( KeyIndex == 0 );
	return Constant;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionFloatConstant::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	// There can be only be one sub-curve for this distribution.
	check( SubIndex == 0 );
	// There can be only be one key for this distribution.
	check( KeyIndex == 0 );

	// Always return RED since there is only one key
	return FColor(255,0,0);
}

void UDistributionFloatConstant::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionFloatConstant::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	MinOut = Constant;
	MaxOut = Constant;
}

BYTE UDistributionFloatConstant::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return CIM_Constant;
}

void UDistributionFloatConstant::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex == 0 );
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionFloatConstant::EvalSub(INT SubIndex, FLOAT InVal)
{
	check(SubIndex == 0);
	return Constant;
}

INT UDistributionFloatConstant::CreateNewKey(FLOAT KeyIn)
{	
	return 0;
}

void UDistributionFloatConstant::DeleteKey(INT KeyIndex)
{
	check( KeyIndex == 0 );
}

INT UDistributionFloatConstant::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex == 0 );
	return 0;
}

void UDistributionFloatConstant::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex == 0 );
	check( KeyIndex == 0 );
	Constant = NewOutVal;

	bIsDirty = TRUE;
}

void UDistributionFloatConstant::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex == 0 );
}

void UDistributionFloatConstant::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex == 0 );
}

/*-----------------------------------------------------------------------------
	UDistributionFloatConstantCurve implementation.
-----------------------------------------------------------------------------*/

FLOAT UDistributionFloatConstantCurve::GetValue( FLOAT F, UObject* Data, class FRandomStream* InRandomStream )
{
	return ConstantCurve.Eval(F, 0.f);
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatConstantCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionFloatConstantCurve::GetNumSubCurves() const
{
	return 1;
}

FLOAT UDistributionFloatConstantCurve::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).InVal;
}

FLOAT UDistributionFloatConstantCurve::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).OutVal;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionFloatConstantCurve::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	// There can be only be one sub-curve for this distribution.
	check( SubIndex == 0 );
	// There can be only be one key for this distribution.
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );

	// Always return RED since there is only one sub-curve.
	return FColor(255,0,0);
}

void UDistributionFloatConstantCurve::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if(ConstantCurve.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		FLOAT Min = BIG_NUMBER;
		FLOAT Max = -BIG_NUMBER;
		for (INT Index = 0; Index < ConstantCurve.Points.Num(); Index++)
		{
			FLOAT Value = ConstantCurve.Points(Index).InVal;
			if (Value < Min)
			{
				Min = Value;
			}
			if (Value > Max)
			{
				Max = Value;
			}
		}
		MinIn = Min;
		MaxIn = Max;
	}
}

void UDistributionFloatConstantCurve::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	ConstantCurve.CalcBounds(MinOut, MaxOut, 0.f);
}

BYTE UDistributionFloatConstantCurve::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).InterpMode;
}

void UDistributionFloatConstantCurve::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent;
	LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent;
}

FLOAT UDistributionFloatConstantCurve::EvalSub(INT SubIndex, FLOAT InVal)
{
	check(SubIndex == 0);
	return ConstantCurve.Eval(InVal, 0.f);
}

INT UDistributionFloatConstantCurve::CreateNewKey(FLOAT KeyIn)
{
	FLOAT NewKeyOut = ConstantCurve.Eval(KeyIn, 0.f);
	INT NewPointIndex = ConstantCurve.AddPoint(KeyIn, NewKeyOut);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionFloatConstantCurve::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

INT UDistributionFloatConstantCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionFloatConstantCurve::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).OutVal = NewOutVal;
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionFloatConstantCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionFloatConstantCurve::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).ArriveTangent = ArriveTangent;
	ConstantCurve.Points(KeyIndex).LeaveTangent = LeaveTangent;

	bIsDirty = TRUE;
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UDistributionFloatConstantCurve::UsingLegacyInterpMethod() const
{
	return ConstantCurve.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UDistributionFloatConstantCurve::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		ConstantCurve.UpgradeInterpMethod();

		bIsDirty = TRUE;
	}
}


/*-----------------------------------------------------------------------------
	UDistributionFloatUniform implementation.
-----------------------------------------------------------------------------*/
void UDistributionFloatUniform::PostLoad()
{
	if (GetLinker() && (GetLinker()->Ver() < VER_UNIFORM_DISTRIBUTION_BAKING_UPDATE))
	{
		bIsDirty = TRUE;
		MarkPackageDirty();
	}

	Super::PostLoad();
}

FLOAT UDistributionFloatUniform::GetValue( FLOAT F, UObject* Data, class FRandomStream* InRandomStream )
{
	return Max + (Min - Max) * DIST_GET_RANDOM_VALUE(InRandomStream);
}

#if !CONSOLE
/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionFloatUniform::GetOperation()
{
	if (Min == Max)
	{
		// This may as well be a constant - don't bother doing the appSRand scaling on it.
		return RDO_None;
	}
	return RDO_Random;
}
	
/**
 * Fill out an array of floats and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 floats
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionFloatUniform::InitializeRawEntry(FLOAT Time, FLOAT* Values)
{
	Values[0] = Min;
	Values[1] = Max;
	return 2;
}
#endif

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatUniform::GetNumKeys()
{
	return 1;
}

INT UDistributionFloatUniform::GetNumSubCurves() const
{
	return 2;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionFloatUniform::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionFloatUniform::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionFloatUniform::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex == 0 || SubIndex == 1);
	check( KeyIndex == 0 );
	return (SubIndex == 0) ? Min : Max;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionFloatUniform::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	// There can only be as many as two sub-curves for this distribution.
	check( SubIndex == 0 || SubIndex == 1);
	// There can be only be one key for this distribution.
	check( KeyIndex == 0 );

	FColor KeyColor;

	if( 0 == SubIndex )
	{
		// RED
		KeyColor = FColor(255,0,0);
	} 
	else
	{
		// GREEN
		KeyColor = FColor(0,255,0);
	}

	return KeyColor;
}

void UDistributionFloatUniform::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionFloatUniform::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	MinOut = Min;
	MaxOut = Max;
}

BYTE UDistributionFloatUniform::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return CIM_Constant;
}

void UDistributionFloatUniform::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex == 0 || SubIndex == 1);
	check( KeyIndex == 0 );
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionFloatUniform::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( SubIndex == 0 || SubIndex == 1);
	return (SubIndex == 0) ? Min : Max;
}

INT UDistributionFloatUniform::CreateNewKey(FLOAT KeyIn)
{	
	return 0;
}

void UDistributionFloatUniform::DeleteKey(INT KeyIndex)
{
	check( KeyIndex == 0 );
}

INT UDistributionFloatUniform::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex == 0 );
	return 0;
}

void UDistributionFloatUniform::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex == 0 || SubIndex == 1);
	check( KeyIndex == 0 );

	// We ensure that we can't move the Min past the Max.
	if(SubIndex == 0)
	{	
		Min = ::Min<FLOAT>(NewOutVal, Max);
	}
	else
	{
		Max = ::Max<FLOAT>(NewOutVal, Min);
	}

	bIsDirty = TRUE;
}

void UDistributionFloatUniform::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex == 0 );
}

void UDistributionFloatUniform::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex == 0 || SubIndex == 1);
	check( KeyIndex == 0 );
}

/*-----------------------------------------------------------------------------
	UDistributionFloatUniformCurve implementation.
-----------------------------------------------------------------------------*/
void UDistributionFloatUniformCurve::PostLoad()
{
	if (GetLinker() && (GetLinker()->Ver() < VER_UNIFORM_DISTRIBUTION_BAKING_UPDATE))
	{
		bIsDirty = TRUE;
		MarkPackageDirty();
	}

	Super::PostLoad();
}

FLOAT UDistributionFloatUniformCurve::GetValue(FLOAT F, UObject* Data, class FRandomStream* InRandomStream)
{
	FVector2D Val = ConstantCurve.Eval(F, FVector2D(0.f, 0.f));
	return Val.X + (Val.Y - Val.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
}

#if !CONSOLE
/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionFloatUniformCurve::GetOperation()
{
	if (ConstantCurve.Points.Num() == 1)
	{
		// Only a single point - so see if Min == Max
		FInterpCurvePoint<FVector2D>& Value = ConstantCurve.Points(0);
		if (Value.OutVal.X == Value.OutVal.Y)
		{
			// This may as well be a constant - don't bother doing the appSRand scaling on it.
			return RDO_None;
		}
	}
	return RDO_Random;
}

/**
 * Fill out an array of floats and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 floats
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionFloatUniformCurve::InitializeRawEntry(FLOAT Time, FLOAT* Values)
{
	FVector2D MinMax = GetMinMaxValue(Time, NULL);
	Values[0] = MinMax.X;
	Values[1] = MinMax.Y;
	return 2;
}
#endif

FVector2D UDistributionFloatUniformCurve::GetMinMaxValue(FLOAT F, UObject* Data)
{
	return ConstantCurve.Eval(F, FVector2D(0.f, 0.f));
}

//////////////////////// FCurveEdInterface ////////////////////////
INT UDistributionFloatUniformCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionFloatUniformCurve::GetNumSubCurves() const
{
	return 2;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionFloatUniformCurve::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionFloatUniformCurve::GetKeyIn(INT KeyIndex)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	return ConstantCurve.Points(KeyIndex).InVal;
}

FLOAT UDistributionFloatUniformCurve::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check((SubIndex >= 0) && (SubIndex < 2));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	
	if (SubIndex == 0)
	{
		return ConstantCurve.Points(KeyIndex).OutVal.X;
	}
	else
	{
		return ConstantCurve.Points(KeyIndex).OutVal.Y;
	}
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionFloatUniformCurve::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check((SubIndex >= 0) && (SubIndex < 2));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		return FColor(255,0,0);
	}
	else
	{
		return FColor(0,255,0);
	}
}

void UDistributionFloatUniformCurve::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if (ConstantCurve.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		FLOAT Min = BIG_NUMBER;
		FLOAT Max = -BIG_NUMBER;
		for (INT Index = 0; Index < ConstantCurve.Points.Num(); Index++)
		{
			FLOAT Value = ConstantCurve.Points(Index).InVal;
			if (Value < Min)
			{
				Min = Value;
			}
			if (Value > Max)
			{
				Max = Value;
			}
		}
		MinIn = Min;
		MaxIn = Max;
	}
}

void UDistributionFloatUniformCurve::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector2D MinVec, MaxVec;
	ConstantCurve.CalcBounds(MinVec, MaxVec, FVector2D(0.f,0.f));
	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

BYTE UDistributionFloatUniformCurve::GetKeyInterpMode(INT KeyIndex)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	return ConstantCurve.Points(KeyIndex).InterpMode;
}

void UDistributionFloatUniformCurve::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check((SubIndex >= 0) && (SubIndex < 2));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.X;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.X;
	}
	else
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.Y;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.Y;
	}
}

FLOAT UDistributionFloatUniformCurve::EvalSub(INT SubIndex, FLOAT InVal)
{
	check((SubIndex >= 0) && (SubIndex < 2));

	FVector2D OutVal = ConstantCurve.Eval(InVal, FVector2D(0.f,0.f));

	if (SubIndex == 0)
	{
		return OutVal.X;
	}
	else
	{
		return OutVal.Y;
	}
}

INT UDistributionFloatUniformCurve::CreateNewKey(FLOAT KeyIn)
{	
	FVector2D NewKeyVal = ConstantCurve.Eval(KeyIn, FVector2D(0.f,0.f));
	INT NewPointIndex = ConstantCurve.AddPoint(KeyIn, NewKeyVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionFloatUniformCurve::DeleteKey(INT KeyIndex)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

INT UDistributionFloatUniformCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionFloatUniformCurve::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check((SubIndex >= 0) && (SubIndex < 2));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		ConstantCurve.Points(KeyIndex).OutVal.X = NewOutVal;
	}
	else
	{
		ConstantCurve.Points(KeyIndex).OutVal.Y = NewOutVal;
	}

	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionFloatUniformCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionFloatUniformCurve::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check((SubIndex >= 0) && (SubIndex < 2));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if(SubIndex == 0)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.X = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.X = LeaveTangent;
	}
	else
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.Y = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.Y = LeaveTangent;
	}

	bIsDirty = TRUE;
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UDistributionFloatUniformCurve::UsingLegacyInterpMethod() const
{
	return ConstantCurve.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UDistributionFloatUniformCurve::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		ConstantCurve.UpgradeInterpMethod();

		bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UDistributionFloatUniformRange implementation.
-----------------------------------------------------------------------------*/
void UDistributionFloatUniformRange::PostLoad()
{
	Super::PostLoad();
}

void UDistributionFloatUniformRange::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// For now, do *not* allow these to be baked...
	bCanBeBaked = FALSE;
}

FLOAT UDistributionFloatUniformRange::GetValue(FLOAT F, UObject* Data, class FRandomStream* InRandomStream)
{
	INT MaxOrMinValue = appRound(DIST_GET_RANDOM_VALUE(InRandomStream));
	if (MaxOrMinValue == 0)
	{
		return (MaxHigh + ((MaxLow - MaxHigh) * DIST_GET_RANDOM_VALUE(InRandomStream)));
	}
	return (MinHigh  + ((MinLow - MinHigh) * DIST_GET_RANDOM_VALUE(InRandomStream)));
}

#if !CONSOLE
/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionFloatUniformRange::GetOperation()
{
	return RDO_RandomRange;
}
	
/**
 * Fill out an array of floats and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 floats
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionFloatUniformRange::InitializeRawEntry(FLOAT Time, FLOAT* Values)
{
	Values[0] = MaxHigh;
	Values[1] = MaxLow;
	Values[2] = MinHigh;
	Values[3] = MinLow;
	return 4;
}
#endif

// FCurveEdInterface interface
INT UDistributionFloatUniformRange::GetNumKeys()
{
	return 1;
}

INT UDistributionFloatUniformRange::GetNumSubCurves() const
{
	return 4;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionFloatUniformRange::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		break;
	case 2:
		// Dark red
		ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
		break;
	case 3:
		// Dark green
		ButtonColor = bIsSubCurveHidden ? FColor(  0, 28,  0) : FColor(0 , 196, 0);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionFloatUniformRange::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionFloatUniformRange::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check((SubIndex >= 0) && (SubIndex <= 3));
	check(KeyIndex == 0);
	switch (SubIndex)
	{
	case 0:		return MaxHigh;
	case 1:		return MinHigh;
	case 2:		return MaxLow;
	case 3:		return MinLow;
	}
	return MaxHigh;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionFloatUniformRange::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	// There can only be as many as two sub-curves for this distribution.
	check(SubIndex >= 0 && SubIndex <= 3);
	// There can be only be one key for this distribution.
	check(KeyIndex == 0);

	FColor KeyColor;
	switch (SubIndex)
	{
	case 0:		// Red
		KeyColor = FColor(255, 0, 0);
		break;
	case 1:		// Green
		KeyColor = FColor(0, 255, 0);
		break;
	case 2:		// Dark red
		KeyColor = FColor(196, 0, 0);
		break;
	case 3:		// Dark green
		KeyColor = FColor(0 , 196, 0);
		break;
	}

	return KeyColor;
}


void UDistributionFloatUniformRange::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionFloatUniformRange::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FLOAT MaxMax = Max<FLOAT>(MaxHigh, MaxLow);
	FLOAT MaxMin = Max<FLOAT>(MinHigh, MinLow);
	FLOAT MinMax = Min<FLOAT>(MaxHigh, MaxLow);
	FLOAT MinMin = Min<FLOAT>(MinHigh, MinLow);
	MinOut = Min<FLOAT>(MinMax, MinMin);
	MaxOut = Max<FLOAT>(MaxMax, MaxMin);
}

BYTE UDistributionFloatUniformRange::GetKeyInterpMode(INT KeyIndex)
{
	check(KeyIndex == 0);
	return CIM_Constant;
}

void UDistributionFloatUniformRange::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check(SubIndex >= 0 && SubIndex <= 3);
	check(KeyIndex == 0);
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionFloatUniformRange::EvalSub(INT SubIndex, FLOAT InVal)
{
	check(SubIndex >= 0 && SubIndex <= 3);
	switch (SubIndex)
	{
	case 0:		return MaxHigh;
	case 1:		return MinHigh;
	case 2:		return MaxLow;
	case 3:		return MinLow;
	}
	return MaxHigh;
}


INT UDistributionFloatUniformRange::CreateNewKey(FLOAT KeyIn)
{
	return 0;
}
void UDistributionFloatUniformRange::DeleteKey(INT KeyIndex)
{
	check(KeyIndex == 0);
}

INT UDistributionFloatUniformRange::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check(KeyIndex == 0);
	return 0;
}

void UDistributionFloatUniformRange::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal)
{
	check(SubIndex >= 0 && SubIndex <= 3);
	check(KeyIndex == 0);

	// We ensure that we can't move the Min past the Max.
	switch (SubIndex)
	{
	case 0:	// MaxHigh
		MaxHigh = ::Max<FLOAT>(NewOutVal, MaxLow);
		break;
	case 1:	// MinHigh
		MinHigh	= ::Max<FLOAT>(NewOutVal, MinLow);
		break;
	case 2:	// MaxLow
		MaxLow = ::Min<FLOAT>(NewOutVal, MaxHigh);
		break;
	case 3:	// MinLow
		MinLow = ::Min<FLOAT>(NewOutVal, MinHigh);
		break;
	}

	bIsDirty = TRUE;
}

void UDistributionFloatUniformRange::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode)
{
	check( KeyIndex == 0 );
}

void UDistributionFloatUniformRange::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check(SubIndex >= 0 && SubIndex <= 3);
	check( KeyIndex == 0 );
}

/*-----------------------------------------------------------------------------
	UDistributionVectorConstant implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVectorConstant::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
	switch (LockedAxes)
	{
    case EDVLF_XY:
		return FVector(Constant.X, Constant.X, Constant.Z);
    case EDVLF_XZ:
		return FVector(Constant.X, Constant.Y, Constant.X);
    case EDVLF_YZ:
		return FVector(Constant.X, Constant.Y, Constant.Y);
	case EDVLF_XYZ:
		return FVector(Constant.X);
    case EDVLF_None:
	default:
		return Constant;
	}
}

void UDistributionVectorConstant::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}


//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionVectorConstant::GetNumKeys()
{
	return 1;
}

INT UDistributionVectorConstant::GetNumSubCurves() const
{
	switch (LockedAxes)
	{
    case EDVLF_XY:
    case EDVLF_XZ:
    case EDVLF_YZ:
		return 2;
	case EDVLF_XYZ:
		return 1;
	}
	return 3;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionVectorConstant::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		break;
	case 2:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor(0, 0, 255);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionVectorConstant::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionVectorConstant::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex == 0 );
	
	if (SubIndex == 0)
	{
		return Constant.X;
	}
	else 
	if (SubIndex == 1)
	{
		if ((LockedAxes == EDVLF_XY) || (LockedAxes == EDVLF_XYZ))
		{
			return Constant.X;
		}
		else
		{
			return Constant.Y;
		}
	}
	else 
	{
		if ((LockedAxes == EDVLF_XZ) || (LockedAxes == EDVLF_XYZ))
		{
			return Constant.X;
		}
		else
		if (LockedAxes == EDVLF_YZ)
		{
			return Constant.Y;
		}
		else
		{
			return Constant.Z;
		}
	}
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionVectorConstant::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex == 0 );

	if(SubIndex == 0)
		return FColor(255,0,0);
	else if(SubIndex == 1)
		return FColor(0,255,0);
	else
		return FColor(0,0,255);
}

void UDistributionVectorConstant::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionVectorConstant::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector Local;

	switch (LockedAxes)
	{
    case EDVLF_XY:
		Local = FVector(Constant.X, Constant.X, Constant.Z);
		break;
    case EDVLF_XZ:
		Local = FVector(Constant.X, Constant.Y, Constant.X);
		break;
    case EDVLF_YZ:
		Local = FVector(Constant.X, Constant.Y, Constant.Y);
		break;
    case EDVLF_XYZ:
		Local = FVector(Constant.X);
		break;
	case EDVLF_None:
	default:
		Local = FVector(Constant);
		break;
	}

	MinOut = Local.GetMin();
	MaxOut = Local.GetMax();
}

BYTE UDistributionVectorConstant::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return CIM_Constant;
}

void UDistributionVectorConstant::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex == 0 );
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionVectorConstant::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( SubIndex >= 0 && SubIndex < 3);
	return GetKeyOut(SubIndex, 0);
}

INT UDistributionVectorConstant::CreateNewKey(FLOAT KeyIn)
{	
	return 0;
}

void UDistributionVectorConstant::DeleteKey(INT KeyIndex)
{
	check( KeyIndex == 0 );
}

INT UDistributionVectorConstant::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex == 0 );
	return 0;
}

void UDistributionVectorConstant::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex == 0 );

	if(SubIndex == 0)
		Constant.X = NewOutVal;
	else if(SubIndex == 1)
		Constant.Y = NewOutVal;
	else if(SubIndex == 2)
		Constant.Z = NewOutVal;

	bIsDirty = TRUE;
}

void UDistributionVectorConstant::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex == 0 );
}

void UDistributionVectorConstant::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex == 0 );
}

// DistributionVector interface
/** GetRange - in the case of a constant curve, this will not be exact!				*/
void UDistributionVectorConstant::GetRange(FVector& OutMin, FVector& OutMax)
{
	OutMin	= Constant;
	OutMax	= Constant;
}

/*-----------------------------------------------------------------------------
	UDistributionVectorConstantCurve implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVectorConstantCurve::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
	FVector Val = ConstantCurve.Eval(F, FVector(0.f));
	switch (LockedAxes)
	{
    case EDVLF_XY:
		return FVector(Val.X, Val.X, Val.Z);
    case EDVLF_XZ:
		return FVector(Val.X, Val.Y, Val.X);
    case EDVLF_YZ:
		return FVector(Val.X, Val.Y, Val.Y);
	case EDVLF_XYZ:
		return FVector(Val.X);
    case EDVLF_None:
	default:
		return Val;
	}
}

void UDistributionVectorConstantCurve::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionVectorConstantCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionVectorConstantCurve::GetNumSubCurves() const
{
	switch (LockedAxes)
	{
    case EDVLF_XY:
    case EDVLF_XZ:
    case EDVLF_YZ:
		return 2;
	case EDVLF_XYZ:
		return 1;
	}
	return 3;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionVectorConstantCurve::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		break;
	case 2:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor(0, 0, 255);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionVectorConstantCurve::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).InVal;
}

FLOAT UDistributionVectorConstantCurve::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	
	if (SubIndex == 0)
	{
		return ConstantCurve.Points(KeyIndex).OutVal.X;
	}
	else
	if(SubIndex == 1)
	{
		if ((LockedAxes == EDVLF_XY) || (LockedAxes == EDVLF_XYZ))
		{
			return ConstantCurve.Points(KeyIndex).OutVal.X;
		}

		return ConstantCurve.Points(KeyIndex).OutVal.Y;
	}
	else 
	{
		if ((LockedAxes == EDVLF_XZ) || (LockedAxes == EDVLF_XYZ))
		{
			return ConstantCurve.Points(KeyIndex).OutVal.X;
		}
		else
		if (LockedAxes == EDVLF_YZ)
		{
			return ConstantCurve.Points(KeyIndex).OutVal.Y;
		}

		return ConstantCurve.Points(KeyIndex).OutVal.Z;
	}
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionVectorConstantCurve::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );

	if(SubIndex == 0)
		return FColor(255,0,0);
	else if(SubIndex == 1)
		return FColor(0,255,0);
	else
		return FColor(0,0,255);
}

void UDistributionVectorConstantCurve::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if( ConstantCurve.Points.Num() == 0 )
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		FLOAT Min = BIG_NUMBER;
		FLOAT Max = -BIG_NUMBER;
		for (INT Index = 0; Index < ConstantCurve.Points.Num(); Index++)
		{
			FLOAT Value = ConstantCurve.Points(Index).InVal;
			if (Value < Min)
			{
				Min = Value;
			}
			if (Value > Max)
			{
				Max = Value;
			}
		}
		MinIn = Min;
		MaxIn = Max;
	}
}

void UDistributionVectorConstantCurve::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector MinVec, MaxVec;
	ConstantCurve.CalcBounds(MinVec, MaxVec, FVector(0.f));

	switch (LockedAxes)
	{
    case EDVLF_XY:
		MinVec.Y = MinVec.X;
		MaxVec.Y = MaxVec.X;
		break;
    case EDVLF_XZ:
		MinVec.Z = MinVec.X;
		MaxVec.Z = MaxVec.X;
		break;
    case EDVLF_YZ:
		MinVec.Z = MinVec.Y;
		MaxVec.Z = MaxVec.Y;
		break;
    case EDVLF_XYZ:
		MinVec.Y = MinVec.X;
		MinVec.Z = MinVec.X;
		MaxVec.Y = MaxVec.X;
		MaxVec.Z = MaxVec.X;
		break;
    case EDVLF_None:
	default:
		break;
	}

	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

BYTE UDistributionVectorConstantCurve::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).InterpMode;
}

void UDistributionVectorConstantCurve::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );

	if(SubIndex == 0)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.X;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.X;
	}
	else if(SubIndex == 1)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.Y;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.Y;
	}
	else if(SubIndex == 2)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.Z;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.Z;
	}
}

FLOAT UDistributionVectorConstantCurve::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( SubIndex >= 0 && SubIndex < 3);

	FVector OutVal = ConstantCurve.Eval(InVal, FVector(0.f));

	if(SubIndex == 0)
		return OutVal.X;
	else if(SubIndex == 1)
		return OutVal.Y;
	else
		return OutVal.Z;
}

INT UDistributionVectorConstantCurve::CreateNewKey(FLOAT KeyIn)
{	
	FVector NewKeyVal = ConstantCurve.Eval(KeyIn, FVector(0.f));
	INT NewPointIndex = ConstantCurve.AddPoint(KeyIn, NewKeyVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionVectorConstantCurve::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

INT UDistributionVectorConstantCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionVectorConstantCurve::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );

	if(SubIndex == 0)
		ConstantCurve.Points(KeyIndex).OutVal.X = NewOutVal;
	else if(SubIndex == 1)
		ConstantCurve.Points(KeyIndex).OutVal.Y = NewOutVal;
	else 
		ConstantCurve.Points(KeyIndex).OutVal.Z = NewOutVal;

	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionVectorConstantCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	
	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionVectorConstantCurve::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );

	if(SubIndex == 0)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.X = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.X = LeaveTangent;
	}
	else if(SubIndex == 1)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.Y = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.Y = LeaveTangent;
	}
	else if(SubIndex == 2)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.Z = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.Z = LeaveTangent;
	}

	bIsDirty = TRUE;
}

// DistributionVector interface
/** GetRange - in the case of a constant curve, this will not be exact!				*/
void UDistributionVectorConstantCurve::GetRange(FVector& OutMin, FVector& OutMax)
{
	FVector MinVec, MaxVec;
	ConstantCurve.CalcBounds(MinVec, MaxVec, FVector(0.f));

	switch (LockedAxes)
	{
    case EDVLF_XY:
		MinVec.Y = MinVec.X;
		MaxVec.Y = MaxVec.X;
		break;
    case EDVLF_XZ:
		MinVec.Z = MinVec.X;
		MaxVec.Z = MaxVec.X;
		break;
    case EDVLF_YZ:
		MinVec.Z = MinVec.Y;
		MaxVec.Z = MaxVec.Y;
		break;
    case EDVLF_XYZ:
		MinVec.Y = MinVec.X;
		MinVec.Z = MinVec.X;
		MaxVec.Y = MaxVec.X;
		MaxVec.Z = MaxVec.X;
		break;
    case EDVLF_None:
	default:
		break;
	}

	OutMin = MinVec;
	OutMax = MaxVec;
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UDistributionVectorConstantCurve::UsingLegacyInterpMethod() const
{
	return ConstantCurve.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UDistributionVectorConstantCurve::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		ConstantCurve.UpgradeInterpMethod();

		bIsDirty = TRUE;
	}
}


/*-----------------------------------------------------------------------------
	UDistributionVectorUniform implementation.
-----------------------------------------------------------------------------*/
void UDistributionVectorUniform::PostLoad()
{
	if (GetLinker() && (GetLinker()->Ver() < VER_UNIFORM_DISTRIBUTION_BAKING_UPDATE))
	{
		bIsDirty = TRUE;
		MarkPackageDirty();
	}

	if (GetLinker() && (GetLinker()->Ver() < VER_LOCKED_UNIFORM_DISTRIBUTION_BAKING))
	{
		if (LockedAxes != EDVLF_None)
		{
			bIsDirty = TRUE;
		}
	}

	Super::PostLoad();
}

FVector UDistributionVectorUniform::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
	FVector LocalMax = Max;
	FVector LocalMin = Min;

	LocalMin.X = (MirrorFlags[0] == EDVMF_Different) ? LocalMin.X : ((MirrorFlags[0] == EDVMF_Mirror) ? -LocalMax.X : LocalMax.X);
	LocalMin.Y = (MirrorFlags[1] == EDVMF_Different) ? LocalMin.Y : ((MirrorFlags[1] == EDVMF_Mirror) ? -LocalMax.Y : LocalMax.Y);
	LocalMin.Z = (MirrorFlags[2] == EDVMF_Different) ? LocalMin.Z : ((MirrorFlags[2] == EDVMF_Mirror) ? -LocalMax.Z : LocalMax.Z);

	FLOAT fX;
	FLOAT fY;
	FLOAT fZ;

	UBOOL bMin = TRUE;
	if (bUseExtremes)
	{
		if (Extreme == 0)
		{
			if (DIST_GET_RANDOM_VALUE(InRandomStream) > 0.5f)
			{
				bMin = FALSE;
			}
		}
		else if (Extreme > 0)
		{
			bMin = FALSE;
		}
	}

	switch (LockedAxes)
	{
    case EDVLF_XY:
		if (bUseExtremes)
		{
			if (bMin)
			{
				fX = LocalMin.X;
				fZ = LocalMin.Z;
			}
			else
			{
				fX = LocalMax.X;
				fZ = LocalMax.Z;
			}
		}
		else
		{
			fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
			fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
		}
		fY = fX;
		break;
    case EDVLF_XZ:
		if (bUseExtremes)
		{
			if (bMin)
			{
				fX = LocalMin.X;
				fY = LocalMin.Y;
			}
			else
			{
				fX = LocalMax.X;
				fY = LocalMax.Y;
			}
		}
		else
		{
			fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
			fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
		}
		fZ = fX;
		break;
    case EDVLF_YZ:
		if (bUseExtremes)
		{
			if (bMin)
			{
				fX = LocalMin.X;
				fY = LocalMin.Y;
			}
			else
			{
				fX = LocalMax.X;
				fY = LocalMax.Y;
			}
		}
		else
		{
			fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
			fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
		}
		fZ = fY;
		break;
	case EDVLF_XYZ:
		if (bUseExtremes)
		{
			if (bMin)
			{
				fX = LocalMin.X;
			}
			else
			{
				fX = LocalMax.X;
			}
		}
		else
		{
			fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
		}
		fY = fX;
		fZ = fX;
		break;
    case EDVLF_None:
	default:
		if (bUseExtremes)
		{
			if (bMin)
			{
				fX = LocalMin.X;
				fY = LocalMin.Y;
				fZ = LocalMin.Z;
			}
			else
			{
				fX = LocalMax.X;
				fY = LocalMax.Y;
				fZ = LocalMax.Z;
			}
		}
		else
		{
			fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
			fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
			fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
		}
		break;
	}

	return FVector(fX, fY, fZ);
}

#if !CONSOLE

/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionVectorUniform::GetOperation()
{
	if (Min == Max)
	{
		// This may as well be a constant - don't bother doing the appSRand scaling on it.
		return RDO_None;
	}
	// override the operation to use
	return bUseExtremes ? RDO_Extreme : RDO_Random;
}

/**
 * Fill out an array of vectors and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionVectorUniform::InitializeRawEntry(FLOAT Time, FVector* Values)
{
	// get the locked/mirrored min and max
	Values[0] = GetMinValue();
	Values[1] = GetMaxValue();

	// two elements per value
	return 2;
}

#endif

FVector UDistributionVectorUniform::GetMinValue()
{
	FVector LocalMax = Max;
	FVector LocalMin = Min;

	for (INT i = 0; i < 3; i++)
	{
		switch (MirrorFlags[i])
		{
		case EDVMF_Same:	LocalMin[i] =  LocalMax[i];		break;
		case EDVMF_Mirror:	LocalMin[i] = -LocalMax[i];		break;
		}
	}

	FLOAT fX;
	FLOAT fY;
	FLOAT fZ;

	switch (LockedAxes)
	{
    case EDVLF_XY:
		fX = LocalMin.X;
		fY = LocalMin.X;
		fZ = LocalMin.Z;
		break;
    case EDVLF_XZ:
		fX = LocalMin.X;
		fY = LocalMin.Y;
		fZ = fX;
		break;
    case EDVLF_YZ:
		fX = LocalMin.X;
		fY = LocalMin.Y;
		fZ = fY;
		break;
	case EDVLF_XYZ:
		fX = LocalMin.X;
		fY = fX;
		fZ = fX;
		break;
    case EDVLF_None:
	default:
		fX = LocalMin.X;
		fY = LocalMin.Y;
		fZ = LocalMin.Z;
		break;
	}

	return FVector(fX, fY, fZ);
}

FVector UDistributionVectorUniform::GetMaxValue()
{
	FVector LocalMax = Max;

	FLOAT fX;
	FLOAT fY;
	FLOAT fZ;

	switch (LockedAxes)
	{
    case EDVLF_XY:
		fX = LocalMax.X;
		fY = LocalMax.X;
		fZ = LocalMax.Z;
		break;
    case EDVLF_XZ:
		fX = LocalMax.X;
		fY = LocalMax.Y;
		fZ = fX;
		break;
    case EDVLF_YZ:
		fX = LocalMax.X;
		fY = LocalMax.Y;
		fZ = fY;
		break;
	case EDVLF_XYZ:
		fX = LocalMax.X;
		fY = fX;
		fZ = fX;
		break;
    case EDVLF_None:
	default:
		fX = LocalMax.X;
		fY = LocalMax.Y;
		fZ = LocalMax.Z;
		break;
	}

	return FVector(fX, fY, fZ);
}

void UDistributionVectorUniform::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

//////////////////////// FCurveEdInterface ////////////////////////

// We have 6 subs, 3 mins and three maxes. They are assigned as:
// 0,1 = min/max x
// 2,3 = min/max y
// 4,5 = min/max z

INT UDistributionVectorUniform::GetNumKeys()
{
	return 1;
}

INT UDistributionVectorUniform::GetNumSubCurves() const
{
	switch (LockedAxes)
	{
    case EDVLF_XY:
    case EDVLF_XZ:
    case EDVLF_YZ:
		return 4;
	case EDVLF_XYZ:
		return 2;
	}
	return 6;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionVectorUniform::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	const INT SubCurves = GetNumSubCurves();

	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < SubCurves);

	const UBOOL bShouldGroupMinAndMax = ( (SubCurves == 4) || (SubCurves == 6) );
	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0, 0) : FColor(255, 0, 0);
		break;
	case 1:
		if (bShouldGroupMinAndMax)
		{
			// Dark red
			ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
		}
		else
		{
			// Green
			ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor(0, 255, 0);
		}
		break;
	case 2:
		if (bShouldGroupMinAndMax)
		{
			// Green
			ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor(0, 255, 0);
		}
		else
		{
			// Blue
			ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor(0, 0, 255);
		}
		break;
	case 3:
		// Dark green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 28, 0) : FColor(0, 196, 0);
		break;
	case 4:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor(0, 0, 255);
		break;
	case 5:
		// Dark blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 28) : FColor(0, 0, 196);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionVectorUniform::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionVectorUniform::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 6 );
	check( KeyIndex == 0 );

	FVector LocalMax = Max;
	FVector LocalMin = Min;

	for (INT i = 0; i < 3; i++)
	{
		switch (MirrorFlags[i])
		{
		case EDVMF_Same:	LocalMin[i] =  LocalMax[i];		break;
		case EDVMF_Mirror:	LocalMin[i] = -LocalMax[i];		break;
		}
	}

	switch (LockedAxes)
	{
    case EDVLF_XY:
		LocalMin.Y = LocalMin.X;
		break;
    case EDVLF_XZ:
		LocalMin.Z = LocalMin.X;
		break;
    case EDVLF_YZ:
		LocalMin.Z = LocalMin.Y;
		break;
	case EDVLF_XYZ:
		LocalMin.Y = LocalMin.X;
		LocalMin.Z = LocalMin.X;
		break;
    case EDVLF_None:
	default:
		break;
	}

	switch (SubIndex)
	{
	case 0:		return LocalMin.X;
	case 1:		return LocalMax.X;
	case 2:		return LocalMin.Y;
	case 3:		return LocalMax.Y;
	case 4:		return LocalMin.Z;
	}
	return LocalMax.Z;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionVectorUniform::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex == 0 );

	if(SubIndex == 0)
		return FColor(128,0,0);
	else if(SubIndex == 1)
		return FColor(255,0,0);
	else if(SubIndex == 2)
		return FColor(0,128,0);
	else if(SubIndex == 3)
		return FColor(0,255,0);
	else if(SubIndex == 4)
		return FColor(0,0,128);
	else
		return FColor(0,0,255);
}

void UDistributionVectorUniform::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionVectorUniform::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector LocalMax = Max;
	FVector LocalMin = Min;

	for (INT i = 0; i < 3; i++)
	{
		switch (MirrorFlags[i])
		{
		case EDVMF_Same:	LocalMin[i] =  LocalMax[i];		break;
		case EDVMF_Mirror:	LocalMin[i] = -LocalMax[i];		break;
		}
	}

	FVector LocalMin2;
	FVector LocalMax2;

	switch (LockedAxes)
	{
    case EDVLF_XY:
		LocalMin2 = FVector(LocalMin.X, LocalMin.X, LocalMin.Z);
		LocalMax2 = FVector(LocalMax.X, LocalMax.X, LocalMax.Z);
		break;
    case EDVLF_XZ:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.X);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.X);
		break;
    case EDVLF_YZ:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Y);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Y);
		break;
    case EDVLF_XYZ:
		LocalMin2 = FVector(LocalMin.X);
		LocalMax2 = FVector(LocalMax.X);
		break;
    case EDVLF_None:
	default:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Z);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Z);
		break;
	}

	MinOut = LocalMin2.GetMin();
	MaxOut = LocalMax2.GetMax();
}

BYTE UDistributionVectorUniform::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return CIM_Constant;
}

void UDistributionVectorUniform::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex == 0 );
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionVectorUniform::EvalSub(INT SubIndex, FLOAT InVal)
{
	return GetKeyOut(SubIndex, 0);
}

INT UDistributionVectorUniform::CreateNewKey(FLOAT KeyIn)
{	
	return 0;
}

void UDistributionVectorUniform::DeleteKey(INT KeyIndex)
{
	check( KeyIndex == 0 );
}

INT UDistributionVectorUniform::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex == 0 );
	return 0;
}

void UDistributionVectorUniform::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 6 );
	check( KeyIndex == 0 );

	if(SubIndex == 0)
		Min.X = ::Min<FLOAT>(NewOutVal, Max.X);
	else if(SubIndex == 1)
		Max.X = ::Max<FLOAT>(NewOutVal, Min.X);
	else if(SubIndex == 2)
		Min.Y = ::Min<FLOAT>(NewOutVal, Max.Y);
	else if(SubIndex == 3)
		Max.Y = ::Max<FLOAT>(NewOutVal, Min.Y);
	else if(SubIndex == 4)
		Min.Z = ::Min<FLOAT>(NewOutVal, Max.Z);
	else
		Max.Z = ::Max<FLOAT>(NewOutVal, Min.Z);

	bIsDirty = TRUE;
}

void UDistributionVectorUniform::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex == 0 );
}

void UDistributionVectorUniform::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex == 0 );
}

// DistributionVector interface
/** GetRange - in the case of a constant curve, this will not be exact!				*/
void UDistributionVectorUniform::GetRange(FVector& OutMin, FVector& OutMax)
{
	OutMin	= Min;
	OutMax	= Max;
}

//
//	UDistributionVectorUniformCurve
//
void UDistributionVectorUniformCurve::PostLoad()
{
	if (GetLinker() && (GetLinker()->Ver() < VER_UNIFORM_DISTRIBUTION_BAKING_UPDATE))
	{
		bIsDirty = TRUE;
		MarkPackageDirty();
	}

	if (GetLinker() && (GetLinker()->Ver() < VER_LOCKED_UNIFORM_DISTRIBUTION_BAKING))
	{
		if ((LockedAxes[0] != EDVLF_None) || (LockedAxes[1] != EDVLF_None))
		{
			bIsDirty = TRUE;
		}
	}

	Super::PostLoad();
}

FVector UDistributionVectorUniformCurve::GetValue(FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream)
{
	FTwoVectors	Val = ConstantCurve.Eval(F, FTwoVectors());

	UBOOL bMin = TRUE;
	if (bUseExtremes)
	{
		if (Extreme == 0)
		{
			if (DIST_GET_RANDOM_VALUE(InRandomStream) > 0.5f)
			{
				bMin = FALSE;
			}
		}
		else if (Extreme < 0)
		{
			bMin = FALSE;
		}
	}

	LockAndMirror(Val);
	if (bUseExtremes)
	{
		return ((bMin == TRUE) ? FVector(Val.v2.X, Val.v2.Y, Val.v2.Z) : FVector(Val.v1.X, Val.v1.Y, Val.v1.Z));
	}
	else
	{
		return FVector(
			Val.v1.X + (Val.v2.X - Val.v1.X) * DIST_GET_RANDOM_VALUE(InRandomStream),
			Val.v1.Y + (Val.v2.Y - Val.v1.Y) * DIST_GET_RANDOM_VALUE(InRandomStream),
			Val.v1.Z + (Val.v2.Z - Val.v1.Z) * DIST_GET_RANDOM_VALUE(InRandomStream));
	}
}

#if !CONSOLE

/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionVectorUniformCurve::GetOperation()
{
	if (ConstantCurve.Points.Num() == 1)
	{
		// Only a single point - so see if Min == Max
		FInterpCurvePoint<FTwoVectors>& Value = ConstantCurve.Points(0);
		if (Value.OutVal.v1 == Value.OutVal.v2)
		{
			// This may as well be a constant - don't bother doing the appSRand scaling on it.
			return RDO_None;
		}
	}
	return bUseExtremes ? RDO_Extreme : RDO_Random;
}

/**
 * Fill out an array of vectors and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionVectorUniformCurve::InitializeRawEntry(FLOAT Time, FVector* Values)
{
	// get the min and max values at the current time (just Eval the curve)
	FTwoVectors MinMax = GetMinMaxValue(Time, NULL);
	// apply any axis locks and mirroring (in place)
	LockAndMirror(MinMax);

	// copy out the values
	Values[0] = MinMax.v1;
	Values[1] = MinMax.v2;

	// we wrote two elements
	return 2;
}

#endif

/**
 *	This function will retrieve the max and min values at the given time.
 */
FTwoVectors UDistributionVectorUniformCurve::GetMinMaxValue(FLOAT F, UObject* Data)
{
	return ConstantCurve.Eval(F, FTwoVectors());
}

/** These two functions will retrieve the Min/Max values respecting the Locked and Mirror flags. */
FVector UDistributionVectorUniformCurve::GetMinValue()
{
	check(!TEXT("Don't call me!"));
	return FVector(0.0f);
}

FVector UDistributionVectorUniformCurve::GetMaxValue()
{
	check(!TEXT("Don't call me!"));
	return FVector(0.0f);
}

// UObject interface
void UDistributionVectorUniformCurve::Serialize(FArchive& Ar)
{
	// No need to override old versions - this is a new class...
	Super::Serialize(Ar);
}

// FCurveEdInterface interface
INT UDistributionVectorUniformCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionVectorUniformCurve::GetNumSubCurves() const
{
	INT Count = 0;
/***
	switch (LockedAxes[0])
	{
    case EDVLF_XY:	Count += 2;	break;
    case EDVLF_XZ:	Count += 2;	break;
    case EDVLF_YZ:	Count += 2;	break;
	case EDVLF_XYZ:	Count += 1;	break;
	default:		Count += 3;	break;
	}

	switch (LockedAxes[1])
	{
    case EDVLF_XY:	Count += 2;	break;
    case EDVLF_XZ:	Count += 2;	break;
    case EDVLF_YZ:	Count += 2;	break;
	case EDVLF_XYZ:	Count += 1;	break;
	default:		Count += 3;	break;
	}
***/
	Count = 6;
	return Count;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionVectorUniformCurve::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	const INT SubCurves = GetNumSubCurves();

	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < SubCurves);

	const UBOOL bShouldGroupMinAndMax = ( (SubCurves == 4) || (SubCurves == 6) );
	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor(255, 0, 0);
		break;
	case 1:
		if (bShouldGroupMinAndMax)
		{
			// Dark red
			ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
		}
		else
		{
			// Green
			ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor(0, 255, 0);
		}
		break;
	case 2:
		if (bShouldGroupMinAndMax)
		{
			// Green
			ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor(0, 255, 0);
		}
		else
		{
			// Blue
			ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor(0, 0, 255);
		}
		break;
	case 3:
		// Dark green
		ButtonColor = bIsSubCurveHidden ? FColor(  0, 28,  0) : FColor(0 , 196, 0);
		break;
	case 4:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(  0,  0, 32) : FColor(0, 0, 255);
		break;
	case 5:
		// Dark blue
		ButtonColor = bIsSubCurveHidden ? FColor(  0,  0, 28) : FColor(0, 0, 196);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UDistributionVectorUniformCurve::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	return ConstantCurve.Points(KeyIndex).InVal;
}

FLOAT UDistributionVectorUniformCurve::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check((SubIndex >= 0) && (SubIndex < 6));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));


	// Grab the value
	FInterpCurvePoint<FTwoVectors>	Point = ConstantCurve.Points(KeyIndex);

	FTwoVectors	Val	= Point.OutVal;
	LockAndMirror(Val);
	if ((SubIndex % 2) == 0)
	{
		return Val.v1[SubIndex / 2];
	}
	return Val.v2[SubIndex / 2];
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionVectorUniformCurve::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check((SubIndex >= 0) && (SubIndex < 6));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		return FColor(255,0,0);
	}
	else 
	if (SubIndex == 1)
	{
		return FColor(128,0,0);
	}
	else
	if (SubIndex == 2)
	{
		return FColor(0,255,0);
	}
	else
	if (SubIndex == 3)
	{
		return FColor(0,128,0);
	}
	else
	if (SubIndex == 4)
	{
		return FColor(0,0,255);
	}
	else
	{
		return FColor(0,0,128);
	}
}

void UDistributionVectorUniformCurve::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if (ConstantCurve.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		FLOAT Min = BIG_NUMBER;
		FLOAT Max = -BIG_NUMBER;
		for (INT Index = 0; Index < ConstantCurve.Points.Num(); Index++)
		{
			FLOAT Value = ConstantCurve.Points(Index).InVal;
			if (Value < Min)
			{
				Min = Value;
			}
			if (Value > Max)
			{
				Max = Value;
			}
		}
		MinIn = Min;
		MaxIn = Max;
	}
}

void UDistributionVectorUniformCurve::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FTwoVectors	MinVec, MaxVec;

	ConstantCurve.CalcBounds(MinVec, MaxVec, FTwoVectors());
	LockAndMirror(MinVec);
	LockAndMirror(MaxVec);

	MinOut = ::Min<FLOAT>(MinVec.GetMin(), MaxVec.GetMin());
	MaxOut = ::Max<FLOAT>(MinVec.GetMax(), MaxVec.GetMax());
}

BYTE UDistributionVectorUniformCurve::GetKeyInterpMode(INT KeyIndex)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	return ConstantCurve.Points(KeyIndex).InterpMode;
}

void UDistributionVectorUniformCurve::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check((SubIndex >= 0) && (SubIndex < 6));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v1.X;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v1.X;
	}
	else
	if (SubIndex == 1)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v2.X;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v2.X;
	}
	else
	if (SubIndex == 2)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v1.Y;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v1.Y;
	}
	else
	if (SubIndex == 3)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v2.Y;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v2.Y;
	}
	else
	if (SubIndex == 4)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v1.Z;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v1.Z;
	}
	else
	if (SubIndex == 5)
	{
		ArriveTangent = ConstantCurve.Points(KeyIndex).ArriveTangent.v2.Z;
		LeaveTangent = ConstantCurve.Points(KeyIndex).LeaveTangent.v2.Z;
	}
}

FLOAT UDistributionVectorUniformCurve::EvalSub(INT SubIndex, FLOAT InVal)
{
	check((SubIndex >= 0) && (SubIndex < 6));

	FTwoVectors Default;
	FTwoVectors OutVal = ConstantCurve.Eval(InVal, Default);
	LockAndMirror(OutVal);
	if ((SubIndex % 2) == 0)
	{
		return OutVal.v1[SubIndex / 2];
	}
	else
	{
		return OutVal.v2[SubIndex / 2];
	}
}

INT UDistributionVectorUniformCurve::CreateNewKey(FLOAT KeyIn)
{
	FTwoVectors NewKeyVal = ConstantCurve.Eval(KeyIn, FTwoVectors());
	INT NewPointIndex = ConstantCurve.AddPoint(KeyIn, NewKeyVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionVectorUniformCurve::DeleteKey(INT KeyIndex)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

INT UDistributionVectorUniformCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;

	return NewPointIndex;
}

void UDistributionVectorUniformCurve::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal)
{
	check((SubIndex >= 0) && (SubIndex < 6));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	FLOAT Value;

	FInterpCurvePoint<FTwoVectors>*	Point = &(ConstantCurve.Points(KeyIndex));
	check(Point);

	if (SubIndex == 0)
	{
		Value = ::Max<FLOAT>(NewOutVal, Point->OutVal.v2.X);
		Point->OutVal.v1.X = Value;
	}
	else
	if (SubIndex == 1)
	{
		Value = ::Min<FLOAT>(NewOutVal, Point->OutVal.v1.X);
		Point->OutVal.v2.X = Value;
	}
	else
	if (SubIndex == 2)
	{
		Value = ::Max<FLOAT>(NewOutVal, Point->OutVal.v2.Y);
		Point->OutVal.v1.Y = Value;
	}
	else
	if (SubIndex == 3)
	{
		Value = ::Min<FLOAT>(NewOutVal, Point->OutVal.v1.Y);
		Point->OutVal.v2.Y = Value;
	}
	else
	if (SubIndex == 4)
	{
		Value = ::Max<FLOAT>(NewOutVal, Point->OutVal.v2.Z);
		Point->OutVal.v1.Z = Value;
	}
	else
	if (SubIndex == 5)
	{
		Value = ::Min<FLOAT>(NewOutVal, Point->OutVal.v1.Z);
		Point->OutVal.v2.Z = Value;
	}

	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionVectorUniformCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode)
{
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);

	bIsDirty = TRUE;
}

void UDistributionVectorUniformCurve::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check((SubIndex >= 0) && (SubIndex < 6));
	check((KeyIndex >= 0) && (KeyIndex < ConstantCurve.Points.Num()));

	if (SubIndex == 0)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v1.X = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v1.X = LeaveTangent;
	}
	else
	if (SubIndex == 1)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v2.X = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v2.X = LeaveTangent;
	}
	else
	if (SubIndex == 2)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v1.Y = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v1.Y = LeaveTangent;
	}
	else
	if (SubIndex == 3)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v2.Y = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v2.Y = LeaveTangent;
	}
	else
	if (SubIndex == 4)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v1.Z = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v1.Z = LeaveTangent;
	}
	else
	if (SubIndex == 5)
	{
		ConstantCurve.Points(KeyIndex).ArriveTangent.v2.Z = ArriveTangent;
		ConstantCurve.Points(KeyIndex).LeaveTangent.v2.Z = LeaveTangent;
	}

	bIsDirty = TRUE;
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UDistributionVectorUniformCurve::UsingLegacyInterpMethod() const
{
	return ConstantCurve.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UDistributionVectorUniformCurve::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		ConstantCurve.UpgradeInterpMethod();

		bIsDirty = TRUE;
	}
}


void UDistributionVectorUniformCurve::LockAndMirror(FTwoVectors& Val)
{
	// Handle the mirror flags...
	for (INT i = 0; i < 3; i++)
	{
		switch (MirrorFlags[i])
		{
		case EDVMF_Same:		Val.v2[i]	=  Val.v1[i];	break;
		case EDVMF_Mirror:		Val.v2[i]	= -Val.v1[i];	break;
		}
	}

	// Handle the lock axes flags
	switch (LockedAxes[0])
	{
	case EDVLF_XY:
		Val.v1.Y	= Val.v1.X;
		break;
	case EDVLF_XZ:
		Val.v1.Z	= Val.v1.X;
		break;
	case EDVLF_YZ:
		Val.v1.Z	= Val.v1.Y;
		break;
	case EDVLF_XYZ:
		Val.v1.Y	= Val.v1.X;
		Val.v1.Z	= Val.v1.X;
		break;
	}

	switch (LockedAxes[0])
	{
	case EDVLF_XY:
		Val.v2.Y	= Val.v2.X;
		break;
	case EDVLF_XZ:
		Val.v2.Z	= Val.v2.X;
		break;
	case EDVLF_YZ:
		Val.v2.Z	= Val.v2.Y;
		break;
	case EDVLF_XYZ:
		Val.v2.Y	= Val.v2.X;
		Val.v2.Z	= Val.v2.X;
		break;
	}
}

// DistributionVector interface
/** GetRange - in the case of a constant curve, this will not be exact!				*/
void UDistributionVectorUniformCurve::GetRange(FVector& OutMin, FVector& OutMax)
{
	FTwoVectors	MinVec, MaxVec;

	ConstantCurve.CalcBounds(MinVec, MaxVec, FTwoVectors());
	LockAndMirror(MinVec);
	LockAndMirror(MaxVec);

	if (MinVec.v1.X < MaxVec.v1.X)	OutMin.X = MinVec.v1.X;
	else							OutMin.X = MaxVec.v1.X;
	if (MinVec.v1.Y < MaxVec.v1.Y)	OutMin.Y = MinVec.v1.Y;
	else							OutMin.Y = MaxVec.v1.Y;
	if (MinVec.v1.Z < MaxVec.v1.Z)	OutMin.Z = MinVec.v1.Z;
	else							OutMin.Z = MaxVec.v1.Z;

	if (MinVec.v2.X > MaxVec.v2.X)	OutMax.X = MinVec.v2.X;
	else							OutMax.X = MaxVec.v2.X;
	if (MinVec.v2.Y > MaxVec.v2.Y)	OutMax.Y = MinVec.v2.Y;
	else							OutMax.Y = MaxVec.v2.Y;
	if (MinVec.v2.Z > MaxVec.v2.Z)	OutMax.Z = MinVec.v2.Z;
	else							OutMax.Z = MaxVec.v2.Z;
}

//
//	UDistributionVectorUniformRange
//
void UDistributionVectorUniformRange::PostLoad()
{
	Super::PostLoad();
}

void UDistributionVectorUniformRange::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// For now, do *not* allow these to be baked...
	bCanBeBaked = FALSE;
}

FVector	UDistributionVectorUniformRange::GetValue(FLOAT F, UObject* Data, INT LastExtreme, class FRandomStream* InRandomStream)
{
	FLOAT fX;
	FLOAT fY;
	FLOAT fZ;

	INT MaxOrMin = appRound(DIST_GET_RANDOM_VALUE(InRandomStream));

	if (MaxOrMin == 0)
	{
		fX = MaxHigh.X + (MaxLow.X - MaxHigh.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
		fY = MaxHigh.Y + (MaxLow.Y - MaxHigh.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
		fZ = MaxHigh.Z + (MaxLow.Z - MaxHigh.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
	}
	else
	{
		fX = MinHigh.X + (MinLow.X - MinHigh.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
		fY = MinHigh.Y + (MinLow.Y - MinHigh.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
		fZ = MinHigh.Z + (MinLow.Z - MinHigh.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
	}

	return FVector(fX, fY, fZ);
}

#if !CONSOLE
/**
 * Return the operation used at runtime to calculate the final value
 */
ERawDistributionOperation UDistributionVectorUniformRange::GetOperation()
{
	return RDO_RandomRange;
}

/**
 * Fill out an array of vectors and return the number of elements in the entry
 *
 * @param Time The time to evaluate the distribution
 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
 * @return The number of elements (values) set in the array
 */
DWORD UDistributionVectorUniformRange::InitializeRawEntry(FLOAT Time, FVector* Values)
{
	//@todo. Fill this in!
	// get the locked/mirrored min and max
	Values[0] = GetMinValue();
	Values[1] = GetMaxValue();
	// two elements per value
	return 2;
}

#endif
	
/** These two functions will retrieve the Min/Max values respecting the Locked and Mirror flags. */
FVector UDistributionVectorUniformRange::GetMinValue()
{
	return MinLow;
}

FVector UDistributionVectorUniformRange::GetMaxValue()
{
	return MaxHigh;
}

// UObject interface
void UDistributionVectorUniformRange::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

// FCurveEdInterface interface
INT UDistributionVectorUniformRange::GetNumKeys()
{
	return 1;
}

INT UDistributionVectorUniformRange::GetNumSubCurves() const
{
	return 12;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UDistributionVectorUniformRange::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
{
	check( SubCurveIndex >= 0 && SubCurveIndex < 12);

	INT CompIdx = SubCurveIndex % 3;
	INT ValueIdx = SubCurveIndex / 3;

	FColor ReturnColor;

	switch (CompIdx)
	{
	case 0:		ReturnColor = FColor(255,0,0);	break;
	case 1:		ReturnColor = FColor(0,255,0);	break;
	case 2:		ReturnColor = FColor(0,0,255);	break;
	}

	switch (ValueIdx)
	{
	case 1:
		{
			ReturnColor.R = appRound((FLOAT)ReturnColor.R * 0.75f);
			ReturnColor.G = appRound((FLOAT)ReturnColor.G * 0.75f);
			ReturnColor.B = appRound((FLOAT)ReturnColor.B * 0.75f);
		}
		break;
	case 2:
		{
			ReturnColor.R = appRound((FLOAT)ReturnColor.R * 0.5f);
			ReturnColor.G = appRound((FLOAT)ReturnColor.G * 0.5f);
			ReturnColor.B = appRound((FLOAT)ReturnColor.B * 0.5f);
		}
		break;
	case 3:
		{
			ReturnColor.R = appRound((FLOAT)ReturnColor.R * 0.25f);
			ReturnColor.G = appRound((FLOAT)ReturnColor.G * 0.25f);
			ReturnColor.B = appRound((FLOAT)ReturnColor.B * 0.25f);
		}
		break;
	}

	return ReturnColor;
}

FLOAT UDistributionVectorUniformRange::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return 0.f;
}

FLOAT UDistributionVectorUniformRange::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 12 );
	check( KeyIndex == 0 );

	INT CompIdx = SubIndex % 3;
	INT ValueIdx = SubIndex / 3;

	switch (ValueIdx)
	{
	case 0:		return MaxHigh[CompIdx];
	case 1:		return MaxLow[CompIdx];
	case 2:		return MinHigh[CompIdx];
	case 3:		return MinLow[CompIdx];
	}

	return 0.0f;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UDistributionVectorUniformRange::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	FColor ReturnColor = GetSubCurveButtonColor(SubIndex, FALSE);
	return ReturnColor;
}

void UDistributionVectorUniformRange::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	MinIn = 0.f;
	MaxIn = 0.f;
}

void UDistributionVectorUniformRange::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector MaxValue;
	FVector MinValue;
	GetRange(MinValue, MaxValue);

	MaxOut = ::Max<FLOAT>(MaxValue.X, MaxValue.Y);
	MaxOut = ::Max<FLOAT>(MaxOut, MaxValue.Z);
	MinOut = ::Max<FLOAT>(MinValue.X, MinValue.Y);
	MinOut = ::Max<FLOAT>(MinOut, MinValue.Z);
}

BYTE UDistributionVectorUniformRange::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex == 0 );
	return CIM_Constant;
}

void UDistributionVectorUniformRange::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex == 0 );
	ArriveTangent = 0.f;
	LeaveTangent = 0.f;
}

FLOAT UDistributionVectorUniformRange::EvalSub(INT SubIndex, FLOAT InVal)
{
	return GetKeyOut(SubIndex, 0);
}

INT UDistributionVectorUniformRange::CreateNewKey(FLOAT KeyIn)
{	
	return 0;
}

void UDistributionVectorUniformRange::DeleteKey(INT KeyIndex)
{
	check( KeyIndex == 0 );
}

INT UDistributionVectorUniformRange::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex == 0 );
	return 0;
}

void UDistributionVectorUniformRange::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal)
{
	check( SubIndex >= 0 && SubIndex < 12);
	check( KeyIndex == 0 );

	INT CompIdx = SubIndex % 3;
	if ((SubIndex >= 0) && (SubIndex <= 2))
	{
		MaxHigh[CompIdx] = ::Max<FLOAT>(NewOutVal, MaxLow[CompIdx]);
	}
	else if ((SubIndex >= 3) && (SubIndex <= 5))
	{
		MaxLow[CompIdx] = ::Min<FLOAT>(NewOutVal, MaxHigh[CompIdx]);
	}
	if ((SubIndex >= 6) && (SubIndex <= 8))
	{
		MinHigh[CompIdx] = ::Max<FLOAT>(NewOutVal, MinLow[CompIdx]);
	}
	else if ((SubIndex >= 9) && (SubIndex <= 11))
	{
		MinLow[CompIdx] = ::Min<FLOAT>(NewOutVal, MinHigh[CompIdx]);
	}

	bIsDirty = TRUE;
}

void UDistributionVectorUniformRange::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode)
{
	check( KeyIndex == 0 );
}

void UDistributionVectorUniformRange::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 4);
	check( KeyIndex == 0 );
}


// DistributionVector interface
void UDistributionVectorUniformRange::GetRange(FVector& OutMin, FVector& OutMax)
{
	OutMax = FVector(MaxHigh);
	OutMin = FVector(MinLow);

	if (MinHigh.X < OutMin.X)		OutMin.X = MaxHigh.X;
	if (MinHigh.Y < OutMin.Y)		OutMin.Y = MaxHigh.Y;
	if (MinHigh.Z < OutMin.Z)		OutMin.Z = MaxHigh.Z;
	if (MaxHigh.X < OutMin.X)		OutMin.X = MaxHigh.X;
	if (MaxHigh.Y < OutMin.Y)		OutMin.Y = MaxHigh.Y;
	if (MaxHigh.Z < OutMin.Z)		OutMin.Z = MaxHigh.Z;
	if (MaxLow.X < OutMin.X)		OutMin.X = MaxLow.X;
	if (MaxLow.Y < OutMin.Y)		OutMin.Y = MaxLow.Y;
	if (MaxLow.Z < OutMin.Z)		OutMin.Z = MaxLow.Z;

	if (MaxLow.X > OutMax.X)		OutMax.X = MaxLow.X;
	if (MaxLow.Y > OutMax.Y)		OutMax.Y = MaxLow.Y;
	if (MaxLow.Z > OutMax.Z)		OutMax.Z = MaxLow.Z;
	if (MinHigh.X > OutMax.X)		OutMax.X = MinHigh.X;
	if (MinHigh.Y > OutMax.Y)		OutMax.Y = MinHigh.Y;
	if (MinHigh.Z > OutMax.Z)		OutMax.Z = MinHigh.Z;
	if (MinLow.X > OutMax.X)		OutMax.X = MinLow.X;
	if (MinLow.Y > OutMax.Y)		OutMax.Y = MinLow.Y;
	if (MinLow.Z > OutMax.Z)		OutMax.Z = MinLow.Z;
}

/*-----------------------------------------------------------------------------
	UDistributionFloatParameterBase implementation
-----------------------------------------------------------------------------*/
void UDistributionFloatParameterBase::GetOutRange(float &MinOut,float &MaxOut)
{
	MinOut = MinOutput;
	MaxOut = MaxOutput;
}

//
//	UDistributionFloatParameterBase
//
FLOAT UDistributionFloatParameterBase::GetValue( FLOAT F, UObject* Data, class FRandomStream* InRandomStream )
{
	FLOAT ParamFloat = 0.f;
	UBOOL bFoundParam = GetParamValue(Data, ParameterName, ParamFloat);
	if(!bFoundParam)
	{
		ParamFloat = Constant;
	}

	if(ParamMode == DPM_Direct)
	{
		return ParamFloat;
	}
	else if(ParamMode == DPM_Abs)
	{
		ParamFloat = Abs(ParamFloat);
	}

	FLOAT Gradient;
	if(MaxInput <= MinInput)
		Gradient = 0.f;
	else
		Gradient = (MaxOutput - MinOutput)/(MaxInput - MinInput);

	FLOAT ClampedParam = ::Clamp(ParamFloat, MinInput, MaxInput);
	FLOAT Output = MinOutput + ((ClampedParam - MinInput) * Gradient);

	return Output;
}

void UDistributionVectorParameterBase::GetOutRange(float &MinOut,float &MaxOut)
{
	FVector LocalMax = MaxOutput;
	FVector LocalMin = MinInput;

	FVector LocalMin2;
	FVector LocalMax2;

	switch (LockedAxes)
	{
    case EDVLF_XY:
		LocalMin2 = FVector(LocalMin.X, LocalMin.X, LocalMin.Z);
		LocalMax2 = FVector(LocalMax.X, LocalMax.X, LocalMax.Z);
		break;
    case EDVLF_XZ:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.X);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.X);
		break;
    case EDVLF_YZ:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Y);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Y);
		break;
    case EDVLF_XYZ:
		LocalMin2 = FVector(LocalMin.X);
		LocalMax2 = FVector(LocalMax.X);
		break;
    case EDVLF_None:
	default:
		LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Z);
		LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Z);
		break;
	}

	MinOut = LocalMin2.GetMin();
	MaxOut = LocalMax2.GetMax();
}

FVector UDistributionVectorParameterBase::GetValue( FLOAT F, UObject* Data, INT Extreme, class FRandomStream* InRandomStream )
{
	FVector ParamVector(0.f);
	UBOOL bFoundParam = GetParamValue(Data, ParameterName, ParamVector);
	if(!bFoundParam)
	{
		ParamVector = Constant;
	}

	if(ParamModes[0] == DPM_Abs)
	{
		ParamVector.X = Abs(ParamVector.X);
	}

	if(ParamModes[1] == DPM_Abs)
	{
		ParamVector.Y = Abs(ParamVector.Y);
	}

	if(ParamModes[2] == DPM_Abs)
	{
		ParamVector.Z = Abs(ParamVector.Z);
	}

	FVector Gradient;
	if(MaxInput.X <= MinInput.X)
		Gradient.X = 0.f;
	else
		Gradient.X = (MaxOutput.X - MinOutput.X)/(MaxInput.X - MinInput.X);

	if(MaxInput.Y <= MinInput.Y)
		Gradient.Y = 0.f;
	else
		Gradient.Y = (MaxOutput.Y - MinOutput.Y)/(MaxInput.Y - MinInput.Y);

	if(MaxInput.Z <= MinInput.Z)
		Gradient.Z = 0.f;
	else
		Gradient.Z = (MaxOutput.Z - MinOutput.Z)/(MaxInput.Z - MinInput.Z);

	FVector ClampedParam;
	ClampedParam.X = ::Clamp(ParamVector.X, MinInput.X, MaxInput.X);
	ClampedParam.Y = ::Clamp(ParamVector.Y, MinInput.Y, MaxInput.Y);
	ClampedParam.Z = ::Clamp(ParamVector.Z, MinInput.Z, MaxInput.Z);

	FVector Output = MinOutput + ((ClampedParam - MinInput) * Gradient);

	if(ParamModes[0] == DPM_Direct)
	{
		Output.X = ParamVector.X;
	}

	if(ParamModes[1] == DPM_Direct)
	{
		Output.Y = ParamVector.Y;
	}

	if(ParamModes[2] == DPM_Direct)
	{
		Output.Z = ParamVector.Z;
	}

	return Output;
}

