/*=============================================================================
	UnInterpolationCurveEd.cpp: Implementation of CurveEdInterface for various track types.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"

/*-----------------------------------------------------------------------------
 UInterpTrackMove
-----------------------------------------------------------------------------*/

INT UInterpTrackMove::CalcSubIndex(UBOOL bPos, INT InIndex) const
{
	if(bPos)
	{
		if(bShowTranslationOnCurveEd)
		{
			return InIndex;
		}
		else
		{
			return INDEX_NONE;
		}
	}
	else
	{
		// Only allow showing rotation curve if not using Quaternion interpolation method.
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			if(bShowTranslationOnCurveEd)
			{
				return InIndex + 3;
			}
			else
			{
				return InIndex;
			}
		}
	}
	return INDEX_NONE;
}


INT UInterpTrackMove::GetNumKeys()
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	return PosTrack.Points.Num();
}

INT UInterpTrackMove::GetNumSubCurves() const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );

	INT NumSubs = 0;

	if(bShowTranslationOnCurveEd)
	{
		NumSubs += 3;
	}

	if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
	{
		NumSubs += 3;
	}

	return NumSubs;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UInterpTrackMove::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
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
	case 3:
		// Dark red
		ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
		break;
	case 4:
		// Dark green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 28, 0) : FColor(0, 196, 0);
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

FLOAT UInterpTrackMove::GetKeyIn(INT KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	return PosTrack.Points(KeyIndex).InVal;
}

FLOAT UInterpTrackMove::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	
	if(SubIndex == CalcSubIndex(true,0))
		return PosTrack.Points(KeyIndex).OutVal.X;
	else if(SubIndex == CalcSubIndex(true,1))
		return PosTrack.Points(KeyIndex).OutVal.Y;
	else if(SubIndex == CalcSubIndex(true,2))
		return PosTrack.Points(KeyIndex).OutVal.Z;
	else if(SubIndex == CalcSubIndex(false,0))
		return EulerTrack.Points(KeyIndex).OutVal.X;
	else if(SubIndex == CalcSubIndex(false,1))
		return EulerTrack.Points(KeyIndex).OutVal.Y;
	else if(SubIndex == CalcSubIndex(false,2))
		return EulerTrack.Points(KeyIndex).OutVal.Z;
	else
	{
		check(0);
		return 0.f;
	}
}

void UInterpTrackMove::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	if( PosTrack.Points.Num() == 0 )
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = PosTrack.Points( 0 ).InVal;
		MaxIn = PosTrack.Points( PosTrack.Points.Num()-1 ).InVal;
	}
}

void UInterpTrackMove::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	FVector PosMinVec, PosMaxVec;
	PosTrack.CalcBounds(PosMinVec, PosMaxVec, FVector(0.f));

	FVector EulerMinVec, EulerMaxVec;
	EulerTrack.CalcBounds(EulerMinVec, EulerMaxVec, FVector(0.f));

	// Only output bounds for curve currently being displayed.
	if(bShowTranslationOnCurveEd)
	{
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			MinOut = ::Min( PosMinVec.GetMin(), EulerMinVec.GetMin() );
			MaxOut = ::Max( PosMaxVec.GetMax(), EulerMaxVec.GetMax() );
		}
		else
		{
			MinOut = PosMinVec.GetMin();
			MaxOut = PosMaxVec.GetMax();
		}
	}
	else
	{
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			MinOut = EulerMinVec.GetMin();
			MaxOut = EulerMaxVec.GetMax();
		}
		else
		{
			MinOut = 0.f;
			MaxOut = 0.f;
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
FColor UInterpTrackMove::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
		return FColor(255,0,0);
	else if(SubIndex == CalcSubIndex(true,1))
		return FColor(0,200,0);
	else if(SubIndex == CalcSubIndex(true,2))
		return FColor(0,0,255);
	else if(SubIndex == CalcSubIndex(false,0))
		return FColor(255,128,128);
	else if(SubIndex == CalcSubIndex(false,1))
		return FColor(128,255,128);
	else if(SubIndex == CalcSubIndex(false,2))
		return FColor(128,128,255);
	else
	{
		check(0);
		return FColor(0,0,0);
	}
}

BYTE UInterpTrackMove::GetKeyInterpMode(INT KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	check( PosTrack.Points(KeyIndex).InterpMode == EulerTrack.Points(KeyIndex).InterpMode );

	return PosTrack.Points(KeyIndex).InterpMode;
}

void UInterpTrackMove::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
	{
		ArriveTangent = PosTrack.Points(KeyIndex).ArriveTangent.X;
		LeaveTangent = PosTrack.Points(KeyIndex).LeaveTangent.X;
	}
	else if(SubIndex == CalcSubIndex(true,1))
	{
		ArriveTangent = PosTrack.Points(KeyIndex).ArriveTangent.Y;
		LeaveTangent = PosTrack.Points(KeyIndex).LeaveTangent.Y;
	}
	else if(SubIndex == CalcSubIndex(true,2))
	{
		ArriveTangent = PosTrack.Points(KeyIndex).ArriveTangent.Z;
		LeaveTangent = PosTrack.Points(KeyIndex).LeaveTangent.Z;
	}
	else if(SubIndex == CalcSubIndex(false,0))
	{
		ArriveTangent = EulerTrack.Points(KeyIndex).ArriveTangent.X;
		LeaveTangent = EulerTrack.Points(KeyIndex).LeaveTangent.X;
	}
	else if(SubIndex == CalcSubIndex(false,1))
	{
		ArriveTangent = EulerTrack.Points(KeyIndex).ArriveTangent.Y;
		LeaveTangent = EulerTrack.Points(KeyIndex).LeaveTangent.Y;
	}
	else if(SubIndex == CalcSubIndex(false,2))
	{
		ArriveTangent = EulerTrack.Points(KeyIndex).ArriveTangent.Z;
		LeaveTangent = EulerTrack.Points(KeyIndex).LeaveTangent.Z;
	}
	else
	{
		check(0);
	}
}

FLOAT UInterpTrackMove::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);

	FVector OutPos = PosTrack.Eval(InVal, FVector(0.f));
	FVector OutEuler = EulerTrack.Eval(InVal, FVector(0.f));

	if(SubIndex == CalcSubIndex(true,0))
		return OutPos.X;
	else if(SubIndex == CalcSubIndex(true,1))
		return OutPos.Y;
	else if(SubIndex == CalcSubIndex(true,2))
		return OutPos.Z;
	else if(SubIndex == CalcSubIndex(false,0))
		return OutEuler.X;
	else if(SubIndex == CalcSubIndex(false,1))
		return OutEuler.Y;
	else if(SubIndex == CalcSubIndex(false,2))
		return OutEuler.Z;
	else
	{
		check(0);
		return 0.f;
	}
}


INT UInterpTrackMove::CreateNewKey(FLOAT KeyIn)
{	
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );

	FVector NewKeyPos = PosTrack.Eval(KeyIn, FVector(0.f));
	INT NewPosIndex = PosTrack.AddPoint(KeyIn, NewKeyPos);
	PosTrack.AutoSetTangents(LinCurveTension);

	FVector NewKeyEuler = EulerTrack.Eval(KeyIn, FVector(0.f));
	INT NewEulerIndex = EulerTrack.AddPoint(KeyIn, NewKeyEuler);
	EulerTrack.AutoSetTangents(AngCurveTension);

	FName DefaultName(NAME_None);
	INT NewLookupKeyIndex = LookupTrack.AddPoint(KeyIn, DefaultName);

	check((NewPosIndex == NewEulerIndex) && (NewEulerIndex == NewLookupKeyIndex));

	return NewPosIndex;
}

void UInterpTrackMove::DeleteKey(INT KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	PosTrack.Points.Remove(KeyIndex);
	PosTrack.AutoSetTangents(LinCurveTension);

	EulerTrack.Points.Remove(KeyIndex);
	EulerTrack.AutoSetTangents(AngCurveTension);

	LookupTrack.Points.Remove(KeyIndex);
}

INT UInterpTrackMove::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	INT NewPosIndex = PosTrack.MovePoint(KeyIndex, NewInVal);
	PosTrack.AutoSetTangents(LinCurveTension);

	INT NewEulerIndex = EulerTrack.MovePoint(KeyIndex, NewInVal);
	EulerTrack.AutoSetTangents(AngCurveTension);

	INT NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewInVal);

	check((NewPosIndex == NewEulerIndex) && (NewEulerIndex == NewLookupKeyIndex));

	return NewPosIndex;
}

void UInterpTrackMove::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
		PosTrack.Points(KeyIndex).OutVal.X = NewOutVal;
	else if(SubIndex == CalcSubIndex(true,1))
		PosTrack.Points(KeyIndex).OutVal.Y = NewOutVal;
	else if(SubIndex == CalcSubIndex(true,2))
		PosTrack.Points(KeyIndex).OutVal.Z = NewOutVal;
	else if(SubIndex == CalcSubIndex(false,0))
		EulerTrack.Points(KeyIndex).OutVal.X = NewOutVal;
	else if(SubIndex == CalcSubIndex(false,1))
		EulerTrack.Points(KeyIndex).OutVal.Y = NewOutVal;
	else  if(SubIndex == CalcSubIndex(false,2))
		EulerTrack.Points(KeyIndex).OutVal.Z = NewOutVal;
	else
		check(0);

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

void UInterpTrackMove::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	
	PosTrack.Points(KeyIndex).InterpMode = NewMode;
	PosTrack.AutoSetTangents(LinCurveTension);

	EulerTrack.Points(KeyIndex).InterpMode = NewMode;
	EulerTrack.AutoSetTangents(AngCurveTension);
}

void UInterpTrackMove::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
	{
		PosTrack.Points(KeyIndex).ArriveTangent.X = ArriveTangent;
		PosTrack.Points(KeyIndex).LeaveTangent.X = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(true,1))
	{
		PosTrack.Points(KeyIndex).ArriveTangent.Y = ArriveTangent;
		PosTrack.Points(KeyIndex).LeaveTangent.Y = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(true,2))
	{
		PosTrack.Points(KeyIndex).ArriveTangent.Z = ArriveTangent;
		PosTrack.Points(KeyIndex).LeaveTangent.Z = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(false,0))
	{
		EulerTrack.Points(KeyIndex).ArriveTangent.X = ArriveTangent;
		EulerTrack.Points(KeyIndex).LeaveTangent.X = LeaveTangent;
	}	
	else if(SubIndex == CalcSubIndex(false,1))
	{
		EulerTrack.Points(KeyIndex).ArriveTangent.Y = ArriveTangent;
		EulerTrack.Points(KeyIndex).LeaveTangent.Y = LeaveTangent;
	}	
	else if(SubIndex == CalcSubIndex(false,2))
	{
		EulerTrack.Points(KeyIndex).ArriveTangent.Z = ArriveTangent;
		EulerTrack.Points(KeyIndex).LeaveTangent.Z = LeaveTangent;
	}
	else
	{
		check(0);
	}
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UInterpTrackMove::UsingLegacyInterpMethod() const
{
	return PosTrack.UsingLegacyInterpMethod() || EulerTrack.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UInterpTrackMove::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		PosTrack.UpgradeInterpMethod();
		EulerTrack.UpgradeInterpMethod();
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackMoveAxis
-----------------------------------------------------------------------------*/

FColor UInterpTrackMoveAxis::GetSubCurveButtonColor( INT SubCurveIndex, UBOOL bIsSubCurveHidden ) const
{
	check( SubCurveIndex >= 0 && SubCurveIndex < GetNumSubCurves() );

	// Determine the color based on what axis they represent.
	// X = Red, Y = Green, Z = Blue.
	FColor ButtonColor;
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
	case AXIS_RotationX:
		ButtonColor = bIsSubCurveHidden ? FColor(32,0,0) : FColor(255,0,0);
		break;
	case AXIS_TranslationY:
	case AXIS_RotationY:
		ButtonColor = bIsSubCurveHidden ? FColor(0,32,0) : FColor(0,255,0);
		break;
	case AXIS_TranslationZ:
	case AXIS_RotationZ:
		ButtonColor = bIsSubCurveHidden ? FColor(0,0,32) : FColor(0,0,255);
		break;
	default:
		checkf(FALSE, TEXT("Invalid axis") );
	}

	return ButtonColor;
}

FColor UInterpTrackMoveAxis::GetKeyColor( INT SubIndex, INT KeyIndex, const FColor& CurveColor )
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < GetNumSubCurves() );

	// Determine the color based on what axis they represent.
	// X = Red, Y = Green, Z = Blue.
	FColor KeyColor;
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
		KeyColor = FColor(255,0,0);
		break;
	case AXIS_TranslationY:
		KeyColor = FColor(0,255,0);
		break;
	case AXIS_TranslationZ:
		KeyColor = FColor(0,0,255);
		break;
	case AXIS_RotationX:
		KeyColor = FColor(255,128,128);
		break;
	case AXIS_RotationY:
		KeyColor = FColor(128,255,128);
		break;
	case AXIS_RotationZ:
		KeyColor = FColor(128,128,255);
		break;
	default:
		checkf(FALSE, TEXT("Invalid axis") );
	}

	return KeyColor;
}

INT UInterpTrackMoveAxis::CreateNewKey(FLOAT KeyIn)
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	
	INT NewKeyIndex = Super::CreateNewKey( KeyIn );

	FName DefaultName(NAME_None);
	INT NewLookupKeyIndex = LookupTrack.AddPoint( KeyIn, DefaultName );
	check( NewKeyIndex == NewLookupKeyIndex );

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::DeleteKey( INT KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	Super::DeleteKey( KeyIndex );

	LookupTrack.Points.Remove(KeyIndex);
}

INT UInterpTrackMoveAxis::SetKeyIn( INT KeyIndex, FLOAT NewInVal )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	
	INT NewIndex = Super::SetKeyIn( KeyIndex, NewInVal );

	INT NewLookupKeyIndex = LookupTrack.MovePoint( KeyIndex, NewInVal );

	check( NewIndex == NewLookupKeyIndex );

	return NewIndex;
}

UMaterial* UInterpTrackMoveAxis::GetTrackIcon() const
{
	return NULL;
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

INT UInterpTrackFloatBase::GetNumKeys()
{
	return FloatTrack.Points.Num();
}

INT UInterpTrackFloatBase::GetNumSubCurves() const
{
	return 1;
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackFloatBase::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( FloatTrack.Points.Num() )
	{
		EndTime = FloatTrack.Points( FloatTrack.Points.Num()-1 ).InVal;
	}

	return EndTime;
}

FLOAT UInterpTrackFloatBase::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	return FloatTrack.Points(KeyIndex).InVal;
}

FLOAT UInterpTrackFloatBase::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	return FloatTrack.Points(KeyIndex).OutVal;
}

void UInterpTrackFloatBase::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if(FloatTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = FloatTrack.Points( 0 ).InVal;
		MaxIn = FloatTrack.Points( FloatTrack.Points.Num()-1 ).InVal;
	}
}

void UInterpTrackFloatBase::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FloatTrack.CalcBounds(MinOut, MaxOut, 0.f);
}

BYTE UInterpTrackFloatBase::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	return FloatTrack.Points(KeyIndex).InterpMode;
}

void UInterpTrackFloatBase::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	ArriveTangent = FloatTrack.Points(KeyIndex).ArriveTangent;
	LeaveTangent = FloatTrack.Points(KeyIndex).LeaveTangent;
}

FLOAT UInterpTrackFloatBase::EvalSub(INT SubIndex, FLOAT InVal)
{
	check(SubIndex == 0);
	return FloatTrack.Eval(InVal, 0.f);
}

INT UInterpTrackFloatBase::CreateNewKey(FLOAT KeyIn)
{
	FLOAT NewKeyOut = FloatTrack.Eval(KeyIn, 0.f);
	INT NewPointIndex = FloatTrack.AddPoint(KeyIn, NewKeyOut);
	FloatTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackFloatBase::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points.Remove(KeyIndex);
	FloatTrack.AutoSetTangents(CurveTension);
}

INT UInterpTrackFloatBase::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	INT NewPointIndex = FloatTrack.MovePoint(KeyIndex, NewInVal);
	FloatTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackFloatBase::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points(KeyIndex).OutVal = NewOutVal;
	FloatTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackFloatBase::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points(KeyIndex).InterpMode = NewMode;
	FloatTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackFloatBase::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points(KeyIndex).ArriveTangent = ArriveTangent;
	FloatTrack.Points(KeyIndex).LeaveTangent = LeaveTangent;
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UInterpTrackFloatBase::UsingLegacyInterpMethod() const
{
	return FloatTrack.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UInterpTrackFloatBase::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		FloatTrack.UpgradeInterpMethod();
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorBase
-----------------------------------------------------------------------------*/

INT UInterpTrackVectorBase::GetNumKeys()
{
	return VectorTrack.Points.Num();
}

INT UInterpTrackVectorBase::GetNumSubCurves() const
{
	return 3;
}

/**
 * Provides the color for the sub-curve button that is present on the curve tab.
 *
 * @param	SubCurveIndex		The index of the sub-curve. Cannot be negative nor greater or equal to the number of sub-curves.
 * @param	bIsSubCurveHidden	Is the curve hidden?
 * @return						The color associated to the given sub-curve index.
 */
FColor UInterpTrackVectorBase::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
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

FLOAT UInterpTrackVectorBase::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	return VectorTrack.Points(KeyIndex).InVal;
}

FLOAT UInterpTrackVectorBase::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
		return VectorTrack.Points(KeyIndex).OutVal.X;
	else if(SubIndex == 1)
		return VectorTrack.Points(KeyIndex).OutVal.Y;
	else
		return VectorTrack.Points(KeyIndex).OutVal.Z;
}

void UInterpTrackVectorBase::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if(VectorTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = VectorTrack.Points( 0 ).InVal;
		MaxIn = VectorTrack.Points( VectorTrack.Points.Num()-1 ).InVal;
	}
}

void UInterpTrackVectorBase::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector MinVec, MaxVec;
	VectorTrack.CalcBounds(MinVec, MaxVec, FVector(0.f));

	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

/**
 * @return	The ending time of the track. 
 */
FLOAT UInterpTrackVectorBase::GetTrackEndTime() const
{
	FLOAT EndTime = 0.0f;

	if( VectorTrack.Points.Num() )
	{
		EndTime = VectorTrack.Points( VectorTrack.Points.Num()-1 ).InVal;
	}

	return EndTime;
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UInterpTrackVectorBase::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
		return FColor(255,0,0);
	else if(SubIndex == 1)
		return FColor(0,255,0);
	else
		return FColor(0,0,255);
}

BYTE UInterpTrackVectorBase::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	return VectorTrack.Points(KeyIndex).InterpMode;
}

void UInterpTrackVectorBase::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		ArriveTangent = VectorTrack.Points(KeyIndex).ArriveTangent.X;
		LeaveTangent = VectorTrack.Points(KeyIndex).LeaveTangent.X;
	}
	else if(SubIndex == 1)
	{
		ArriveTangent = VectorTrack.Points(KeyIndex).ArriveTangent.Y;
		LeaveTangent = VectorTrack.Points(KeyIndex).LeaveTangent.Y;
	}
	else if(SubIndex == 2)
	{
		ArriveTangent = VectorTrack.Points(KeyIndex).ArriveTangent.Z;
		LeaveTangent = VectorTrack.Points(KeyIndex).LeaveTangent.Z;
	}
}

FLOAT UInterpTrackVectorBase::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( SubIndex >= 0 && SubIndex < 3 );

	FVector OutVal = VectorTrack.Eval(InVal, FVector(0.f));

	if(SubIndex == 0)
		return OutVal.X;
	else if(SubIndex == 1)
		return OutVal.Y;
	else
		return OutVal.Z;
}

INT UInterpTrackVectorBase::CreateNewKey(FLOAT KeyIn)
{
	FVector NewKeyOut = VectorTrack.Eval(KeyIn, FVector(0.f));
	INT NewPointIndex = VectorTrack.AddPoint(KeyIn, NewKeyOut);
	VectorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackVectorBase::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	VectorTrack.Points.Remove(KeyIndex);
	VectorTrack.AutoSetTangents(CurveTension);
}

INT UInterpTrackVectorBase::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	INT NewPointIndex = VectorTrack.MovePoint(KeyIndex, NewInVal);
	VectorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackVectorBase::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
		VectorTrack.Points(KeyIndex).OutVal.X = NewOutVal;
	else if(SubIndex == 1)
		VectorTrack.Points(KeyIndex).OutVal.Y = NewOutVal;
	else 
		VectorTrack.Points(KeyIndex).OutVal.Z = NewOutVal;

	VectorTrack.AutoSetTangents(0.f);
}

void UInterpTrackVectorBase::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	VectorTrack.Points(KeyIndex).InterpMode = NewMode;
	VectorTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackVectorBase::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		VectorTrack.Points(KeyIndex).ArriveTangent.X = ArriveTangent;
		VectorTrack.Points(KeyIndex).LeaveTangent.X = LeaveTangent;
	}
	else if(SubIndex == 1)
	{
		VectorTrack.Points(KeyIndex).ArriveTangent.Y = ArriveTangent;
		VectorTrack.Points(KeyIndex).LeaveTangent.Y = LeaveTangent;
	}
	else if(SubIndex == 2)
	{
		VectorTrack.Points(KeyIndex).ArriveTangent.Z = ArriveTangent;
		VectorTrack.Points(KeyIndex).LeaveTangent.Z = LeaveTangent;
	}
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UInterpTrackVectorBase::UsingLegacyInterpMethod() const
{
	return VectorTrack.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UInterpTrackVectorBase::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		VectorTrack.UpgradeInterpMethod();
	}
}



/*-----------------------------------------------------------------------------
UInterpTrackLinearColorBase
-----------------------------------------------------------------------------*/

INT UInterpTrackLinearColorBase::GetNumKeys()
{
	return LinearColorTrack.Points.Num();
}

INT UInterpTrackLinearColorBase::GetNumSubCurves() const
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
FColor UInterpTrackLinearColorBase::GetSubCurveButtonColor(INT SubCurveIndex, UBOOL bIsSubCurveHidden) const
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
	case 3:
		// Dark red
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 0) : FColor(255, 255, 255);
		break;
	default:
		// A bad sub-curve index was given. 
		check(FALSE);
		break;
	}

	return ButtonColor;
}

FLOAT UInterpTrackLinearColorBase::GetKeyIn(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	return LinearColorTrack.Points(KeyIndex).InVal;
}

FLOAT UInterpTrackLinearColorBase::GetKeyOut(INT SubIndex, INT KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
		return LinearColorTrack.Points(KeyIndex).OutVal.R;
	else if(SubIndex == 1)
		return LinearColorTrack.Points(KeyIndex).OutVal.G;
	else if(SubIndex == 2)
		return LinearColorTrack.Points(KeyIndex).OutVal.B;
	else
		return LinearColorTrack.Points(KeyIndex).OutVal.A;
}

void UInterpTrackLinearColorBase::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if(LinearColorTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = LinearColorTrack.Points( 0 ).InVal;
		MaxIn = LinearColorTrack.Points( LinearColorTrack.Points.Num()-1 ).InVal;
	}
}

void UInterpTrackLinearColorBase::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FLinearColor MinVec, MaxVec;
	LinearColorTrack.CalcBounds(MinVec, MaxVec, FLinearColor(0.f, 0.f, 0.f, 0.f));

	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

/**
 * Provides the color for the given key at the given sub-curve.
 *
 * @param		SubIndex	The index of the sub-curve
 * @param		KeyIndex	The index of the key in the sub-curve
 * @param[in]	CurveColor	The color of the curve
 * @return					The color that is associated the given key at the given sub-curve
 */
FColor UInterpTrackLinearColorBase::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 4);
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
		return FColor(255,0,0);
	else if(SubIndex == 1)
		return FColor(0,255,0);
	else if(SubIndex == 2)
		return FColor(0,0,255);
	else
		return FColor(255,255,255);
}

BYTE UInterpTrackLinearColorBase::GetKeyInterpMode(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	return LinearColorTrack.Points(KeyIndex).InterpMode;
}

void UInterpTrackLinearColorBase::GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		ArriveTangent = LinearColorTrack.Points(KeyIndex).ArriveTangent.R;
		LeaveTangent = LinearColorTrack.Points(KeyIndex).LeaveTangent.R;
	}
	else if(SubIndex == 1)
	{
		ArriveTangent = LinearColorTrack.Points(KeyIndex).ArriveTangent.G;
		LeaveTangent = LinearColorTrack.Points(KeyIndex).LeaveTangent.G;
	}
	else if(SubIndex == 2)
	{
		ArriveTangent = LinearColorTrack.Points(KeyIndex).ArriveTangent.B;
		LeaveTangent = LinearColorTrack.Points(KeyIndex).LeaveTangent.B;
	}
	else if(SubIndex == 3)
	{
		ArriveTangent = LinearColorTrack.Points(KeyIndex).ArriveTangent.A;
		LeaveTangent = LinearColorTrack.Points(KeyIndex).LeaveTangent.A;
	}
}

FLOAT UInterpTrackLinearColorBase::EvalSub(INT SubIndex, FLOAT InVal)
{
	check( SubIndex >= 0 && SubIndex < 4 );

	FLinearColor OutVal = LinearColorTrack.Eval(InVal, FLinearColor(0.f, 0.f, 0.f, 0.f));

	if(SubIndex == 0)
		return OutVal.R;
	else if(SubIndex == 1)
		return OutVal.G;
	else if(SubIndex == 2)
		return OutVal.B;
	else
		return OutVal.A;
}

INT UInterpTrackLinearColorBase::CreateNewKey(FLOAT KeyIn)
{
	FLinearColor NewKeyOut = LinearColorTrack.Eval(KeyIn, FLinearColor(0.f, 0.f, 0.f, 0.f));
	INT NewPointIndex = LinearColorTrack.AddPoint(KeyIn, NewKeyOut);
	LinearColorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackLinearColorBase::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	LinearColorTrack.Points.Remove(KeyIndex);
	LinearColorTrack.AutoSetTangents(CurveTension);
}

INT UInterpTrackLinearColorBase::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	INT NewPointIndex = LinearColorTrack.MovePoint(KeyIndex, NewInVal);
	LinearColorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackLinearColorBase::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
		LinearColorTrack.Points(KeyIndex).OutVal.R = NewOutVal;
	else if(SubIndex == 1)
		LinearColorTrack.Points(KeyIndex).OutVal.G = NewOutVal;
	else if(SubIndex == 2)
		LinearColorTrack.Points(KeyIndex).OutVal.B = NewOutVal;
	else 
		LinearColorTrack.Points(KeyIndex).OutVal.A = NewOutVal;

	LinearColorTrack.AutoSetTangents(0.f);
}

void UInterpTrackLinearColorBase::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	LinearColorTrack.Points(KeyIndex).InterpMode = NewMode;
	LinearColorTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackLinearColorBase::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		LinearColorTrack.Points(KeyIndex).ArriveTangent.R = ArriveTangent;
		LinearColorTrack.Points(KeyIndex).LeaveTangent.R = LeaveTangent;
	}
	else if(SubIndex == 1)
	{
		LinearColorTrack.Points(KeyIndex).ArriveTangent.G = ArriveTangent;
		LinearColorTrack.Points(KeyIndex).LeaveTangent.G = LeaveTangent;
	}
	else if(SubIndex == 2)
	{
		LinearColorTrack.Points(KeyIndex).ArriveTangent.B = ArriveTangent;
		LinearColorTrack.Points(KeyIndex).LeaveTangent.B = LeaveTangent;
	}	
	else if(SubIndex == 3)
	{
		LinearColorTrack.Points(KeyIndex).ArriveTangent.A = ArriveTangent;
		LinearColorTrack.Points(KeyIndex).LeaveTangent.A = LeaveTangent;
	}
}


/** Returns TRUE if this curve uses legacy tangent/interp algorithms and may be 'upgraded' */
UBOOL UInterpTrackLinearColorBase::UsingLegacyInterpMethod() const
{
	return LinearColorTrack.UsingLegacyInterpMethod();
}


/** 'Upgrades' this curve to use the latest tangent/interp algorithms (usually, will 'bake' key tangents.) */
void UInterpTrackLinearColorBase::UpgradeInterpMethod()
{
	if( UsingLegacyInterpMethod() )
	{
		LinearColorTrack.UpgradeInterpMethod();
	}
}
