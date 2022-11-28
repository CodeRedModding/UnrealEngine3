/*=============================================================================
	UnInterpolationCurveEd.cpp: Implementation of CurveEdInterface for various track types.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineInterpolationClasses.h"

/*-----------------------------------------------------------------------------
 UInterpTrackMove
-----------------------------------------------------------------------------*/

INT UInterpTrackMove::CalcSubIndex(UBOOL bPos, INT Index)
{
	if(bPos)
	{
		if(bShowTranslationOnCurveEd)
		{
			return Index;
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
				return Index + 3;
			}
			else
			{
				return Index;
			}
		}
		else
		{
			return INDEX_NONE;
		}
	}
}


INT UInterpTrackMove::GetNumKeys()
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	return PosTrack.Points.Num();
}

INT UInterpTrackMove::GetNumSubCurves()
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
	PosTrack.CalcBounds(PosMinVec, PosMaxVec, 0.f);

	FVector EulerMinVec, EulerMaxVec;
	EulerTrack.CalcBounds(EulerMinVec, EulerMaxVec, 0.f);

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

FColor UInterpTrackMove::GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
		return FColor(255,0,0);
	else if(SubIndex == CalcSubIndex(true,1))
		return FColor(0,255,0);
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

	FVector OutPos = PosTrack.Eval(InVal, 0.f);
	FVector OutEuler = EulerTrack.Eval(InVal, 0.f);

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

	FVector NewKeyPos = PosTrack.Eval(KeyIn, 0.f);
	INT NewPosIndex = PosTrack.AddPoint(KeyIn, NewKeyPos);
	PosTrack.AutoSetTangents(LinCurveTension);

	FVector NewKeyEuler = EulerTrack.Eval(KeyIn, 0.f);
	INT NewEulerIndex = EulerTrack.AddPoint(KeyIn, NewKeyEuler);
	EulerTrack.AutoSetTangents(AngCurveTension);

	check(NewPosIndex == NewEulerIndex);
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
}

INT UInterpTrackMove::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	INT NewPosIndex = PosTrack.MovePoint(KeyIndex, NewInVal);
	PosTrack.AutoSetTangents(LinCurveTension);

	INT NewEulerIndex = EulerTrack.MovePoint(KeyIndex, NewInVal);
	EulerTrack.AutoSetTangents(AngCurveTension);

	check(NewPosIndex == NewEulerIndex);
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

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

INT UInterpTrackFloatBase::GetNumKeys()
{
	return FloatTrack.Points.Num();
}

INT UInterpTrackFloatBase::GetNumSubCurves()
{
	return 1;
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
