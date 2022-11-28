/*=============================================================================
	UnDistributions.cpp: Implementation of distribution classes.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UDistributionFloat);
IMPLEMENT_CLASS(UDistributionFloatConstant);
IMPLEMENT_CLASS(UDistributionFloatConstantCurve);
IMPLEMENT_CLASS(UDistributionFloatUniform);

IMPLEMENT_CLASS(UDistributionVector);
IMPLEMENT_CLASS(UDistributionVectorConstant);
IMPLEMENT_CLASS(UDistributionVectorConstantCurve);
IMPLEMENT_CLASS(UDistributionVectorUniform);


/*-----------------------------------------------------------------------------
	UDistributionFloat implementation.
-----------------------------------------------------------------------------*/

FLOAT UDistributionFloat::GetValue( FLOAT F )
{
	return 0.f;
}

/*-----------------------------------------------------------------------------
	UDistributionFloatConstant implementation.
-----------------------------------------------------------------------------*/

FLOAT UDistributionFloatConstant::GetValue( FLOAT F )
{
	return Constant;
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatConstant::GetNumKeys()
{
	return 1;
}

INT UDistributionFloatConstant::GetNumSubCurves()
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

FLOAT UDistributionFloatConstantCurve::GetValue( FLOAT F )
{
	return ConstantCurve.Eval(F, 0.f);
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatConstantCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionFloatConstantCurve::GetNumSubCurves()
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

void UDistributionFloatConstantCurve::GetInRange(FLOAT& MinIn, FLOAT& MaxIn)
{
	if(ConstantCurve.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = ConstantCurve.Points( 0 ).InVal;
		MaxIn = ConstantCurve.Points( ConstantCurve.Points.Num()-1 ).InVal;
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
	return NewPointIndex;
}

void UDistributionFloatConstantCurve::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);
}

INT UDistributionFloatConstantCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);
	return NewPointIndex;
}

void UDistributionFloatConstantCurve::SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) 
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).OutVal = NewOutVal;
	ConstantCurve.AutoSetTangents(0.f);
}

void UDistributionFloatConstantCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);
}

void UDistributionFloatConstantCurve::SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points(KeyIndex).ArriveTangent = ArriveTangent;
	ConstantCurve.Points(KeyIndex).LeaveTangent = LeaveTangent;
}

/*-----------------------------------------------------------------------------
	UDistributionFloatUniform implementation.
-----------------------------------------------------------------------------*/

FLOAT UDistributionFloatUniform::GetValue( FLOAT F )
{
	return Max + (Min - Max) * appFrand();
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionFloatUniform::GetNumKeys()
{
	return 1;
}

INT UDistributionFloatUniform::GetNumSubCurves()
{
	return 2;
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
	UDistributionVector implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVector::GetValue( FLOAT F )
{
	return FVector(0,0,0);
}

/*-----------------------------------------------------------------------------
	UDistributionVectorConstant implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVectorConstant::GetValue( FLOAT F )
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
	if (Ar.IsLoading() && (Ar.Ver() < 183))
	{
		if (bLockAxes)
		{
			LockedAxes = EDVLF_XYZ;
		}
		else
		{
			LockedAxes = EDVLF_None;
		}
	}
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionVectorConstant::GetNumKeys()
{
	return 1;
}

INT UDistributionVectorConstant::GetNumSubCurves()
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

/*-----------------------------------------------------------------------------
	UDistributionVectorConstantCurve implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVectorConstantCurve::GetValue( FLOAT F )
{
	FVector Val = ConstantCurve.Eval(F, 0.f);
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
	if (Ar.IsLoading() && (Ar.Ver() < 183))
	{
		if (bLockAxes)
		{
			LockedAxes = EDVLF_XYZ;
		}
		else
		{
			LockedAxes = EDVLF_None;
		}
	}
}

//////////////////////// FCurveEdInterface ////////////////////////

INT UDistributionVectorConstantCurve::GetNumKeys()
{
	return ConstantCurve.Points.Num();
}

INT UDistributionVectorConstantCurve::GetNumSubCurves()
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
		MinIn = ConstantCurve.Points( 0 ).InVal;
		MaxIn = ConstantCurve.Points( ConstantCurve.Points.Num()-1 ).InVal;
	}
}

void UDistributionVectorConstantCurve::GetOutRange(FLOAT& MinOut, FLOAT& MaxOut)
{
	FVector MinVec, MaxVec;
	ConstantCurve.CalcBounds(MinVec, MaxVec, 0.f);

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

	FVector OutVal = ConstantCurve.Eval(InVal, 0.f);

	if(SubIndex == 0)
		return OutVal.X;
	else if(SubIndex == 1)
		return OutVal.Y;
	else
		return OutVal.Z;
}

INT UDistributionVectorConstantCurve::CreateNewKey(FLOAT KeyIn)
{	
	FVector NewKeyVal = ConstantCurve.Eval(KeyIn, 0.f);
	INT NewPointIndex = ConstantCurve.AddPoint(KeyIn, NewKeyVal);
	ConstantCurve.AutoSetTangents(0.f);
	return NewPointIndex;
}

void UDistributionVectorConstantCurve::DeleteKey(INT KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	ConstantCurve.Points.Remove(KeyIndex);
	ConstantCurve.AutoSetTangents(0.f);
}

INT UDistributionVectorConstantCurve::SetKeyIn(INT KeyIndex, FLOAT NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	INT NewPointIndex = ConstantCurve.MovePoint(KeyIndex, NewInVal);
	ConstantCurve.AutoSetTangents(0.f);
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
}

void UDistributionVectorConstantCurve::SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < ConstantCurve.Points.Num() );
	
	ConstantCurve.Points(KeyIndex).InterpMode = NewMode;
	ConstantCurve.AutoSetTangents(0.f);
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
}

/*-----------------------------------------------------------------------------
	UDistributionVectorUniform implementation.
-----------------------------------------------------------------------------*/

FVector UDistributionVectorUniform::GetValue( FLOAT F )
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
		fX = LocalMax.X + (LocalMin.X - LocalMax.X) * appFrand();
		fY = fX;
		fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * appFrand();
		break;
    case EDVLF_XZ:
		fX = LocalMax.X + (LocalMin.X - LocalMax.X) * appFrand();
		fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * appFrand();
		fZ = fX;
		break;
    case EDVLF_YZ:
		fX = LocalMax.X + (LocalMin.X - LocalMax.X) * appFrand();
		fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * appFrand();
		fZ = fY;
		break;
	case EDVLF_XYZ:
		fX = LocalMax.X + (LocalMin.X - LocalMax.X) * appFrand();
		fY = fX;
		fZ = fX;
		break;
    case EDVLF_None:
	default:
		fX = LocalMax.X + (LocalMin.X - LocalMax.X) * appFrand();
		fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * appFrand();
		fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * appFrand();
		break;
	}

	return FVector(fX, fY, fZ);
}

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
	if (Ar.IsLoading() && (Ar.Ver() < 183))
	{
		if (bLockAxes)
		{
			LockedAxes = EDVLF_XYZ;
		}
		else
		{
			LockedAxes = EDVLF_None;
		}

		MirrorFlags[0] = EDVMF_Different;
		MirrorFlags[1] = EDVMF_Different;
		MirrorFlags[2] = EDVMF_Different;
	}
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

INT UDistributionVectorUniform::GetNumSubCurves()
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
