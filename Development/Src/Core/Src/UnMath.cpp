/*=============================================================================
	UnMath.cpp: Unreal math routines, implementation of FGlobalMath class
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* FMatrix (4x4 matrices) and FQuat (quaternions) classes added - Erik de Neve
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FGlobalMath constructor.
-----------------------------------------------------------------------------*/

// Constructor.
FGlobalMath::FGlobalMath()
{
	// Init base angle table.
	{for( INT i=0; i<NUM_ANGLES; i++ )
		TrigFLOAT[i] = appSin((FLOAT)i * 2.f * PI / (FLOAT)NUM_ANGLES);}
}

/*-----------------------------------------------------------------------------
	Conversion functions.
-----------------------------------------------------------------------------*/

// Return the FRotator corresponding to the direction that the vector
// is pointing in.  Sets Yaw and Pitch to the proper numbers, and sets
// roll to zero because the roll can't be determined from a vector.
FRotator FVector::Rotation()
{
	FRotator R;

	// Find yaw.
	R.Yaw = appRound(appAtan2(Y,X) * (FLOAT)MAXWORD / (2.f*PI));

	// Find pitch.
	R.Pitch = appRound(appAtan2(Z,appSqrt(X*X+Y*Y)) * (FLOAT)MAXWORD / (2.f*PI));

	// Find roll.
	R.Roll = 0;

	return R;
}

//	FColor->FLinearColor conversion.
FLinearColor::FLinearColor(const FColor& C):
	R(appPow(C.R / 255.0f,2.2f)),
	G(appPow(C.G / 255.0f,2.2f)),
	B(appPow(C.B / 255.0f,2.2f)),
	A(C.A / 255.0f)
{}

//	FLinearColor->FColor conversion.
FColor::FColor(const FLinearColor& C):
	R(Clamp(appTrunc(appPow(C.R,1.0f / 2.2f) * 255.0f),0,255)),
	G(Clamp(appTrunc(appPow(C.G,1.0f / 2.2f) * 255.0f),0,255)),
	B(Clamp(appTrunc(appPow(C.B,1.0f / 2.2f) * 255.0f),0,255)),
	A(Clamp(appTrunc(appPow(C.A,1.0f / 2.2f) * 255.0f),0,255))
{}

// Convert from float to RGBE as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80.
FColor FLinearColor::ToRGBE() const
{
	FLOAT	Primary = Max3( R, G, B );
	FColor	Color;

	if( Primary < 1E-32 )
	{
		Color = FColor(0,0,0,0);
	}
	else
	{
		INT	Exponent;
		FLOAT Scale;
				
		Scale		= frexp(Primary, &Exponent) / Primary * 255.f;
		Color.R		= Clamp(R * Scale, 0.f, 255.f);
		Color.G		= Clamp(G * Scale, 0.f, 255.f);
		Color.B		= Clamp(B * Scale, 0.f, 255.f);
		Color.A		= Clamp(Exponent,-128,127) + 128;
	}

	return Color;
}

// Convert from RGBE to float as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80.
FLinearColor FColor::FromRGBE() const
{
	if( A == 0 )
		return FLinearColor::Black;
	else
	{
		FLOAT Scale = ldexp( 1 / 255.0, A - 128 );
		return FLinearColor( R * Scale, G * Scale, B * Scale, 1.0f );
	}
}

// Common colors.
const FLinearColor FLinearColor::White(1,1,1);
const FLinearColor FLinearColor::Black(0,0,0);

//
//	FGetHSV - Convert byte hue-saturation-brightness to floating point red-green-blue.
//

FLinearColor FGetHSV( BYTE H, BYTE S, BYTE V )
{
	FLOAT Brightness = V * 1.4f / 255.f;
	Brightness *= 0.7f/(0.01f + appSqrt(Brightness));
	Brightness  = Clamp(Brightness,0.f,1.f);
	FVector Hue = (H<86) ? FVector((85-H)/85.f,(H-0)/85.f,0) : (H<171) ? FVector(0,(170-H)/85.f,(H-85)/85.f) : FVector((H-170)/85.f,0,(255-H)/84.f);
	FVector ColorVector = (Hue + S/255.f * (FVector(1,1,1) - Hue)) * Brightness;
	return FLinearColor(ColorVector.X,ColorVector.Y,ColorVector.Z,1);
}

// Make a random but quite nice color.

FColor MakeRandomColor()
{
	BYTE Hue = (BYTE)( appFrand()*255.f );
	return FColor( FGetHSV(Hue, 0, 255) );
}

//
// Find good arbitrary axis vectors to represent U and V axes of a plane
// given just the normal.
//
void FVector::FindBestAxisVectors( FVector& Axis1, FVector& Axis2 ) const
{
	FLOAT NX = Abs(X);
	FLOAT NY = Abs(Y);
	FLOAT NZ = Abs(Z);

	// Find best basis vectors.
	if( NZ>NX && NZ>NY )	Axis1 = FVector(1,0,0);
	else					Axis1 = FVector(0,0,1);

	Axis1 = (Axis1 - *this * (Axis1 | *this)).SafeNormal();
	Axis2 = Axis1 ^ *this;
}

/*-----------------------------------------------------------------------------
	FSphere implementation.
-----------------------------------------------------------------------------*/

//
// Compute a bounding sphere from an array of points.
//
FSphere::FSphere( const FVector* Pts, INT Count )
: FPlane(0,0,0,0)
{
	if( Count )
	{
		FBox Box( Pts, Count );
		*this = FSphere( (Box.Min+Box.Max)/2, 0 );
		for( INT i=0; i<Count; i++ )
		{
			FLOAT Dist = FDistSquared(Pts[i],*this);
			if( Dist > W )
				W = Dist;
		}
		W = appSqrt(W) * 1.001f;
	}
}

/*-----------------------------------------------------------------------------
	FBox implementation.
-----------------------------------------------------------------------------*/

FBox::FBox( const FVector* Points, INT Count )
: Min(0,0,0), Max(0,0,0), IsValid(0)
{
	for( INT i=0; i<Count; i++ )
		*this += Points[i];
}

/*-----------------------------------------------------------------------------
	FRotator functions.
-----------------------------------------------------------------------------*/

FRotator::FRotator(const FQuat& Quat)
{
	FMatrix RotMat( FVector(0.f), Quat);
	*this = RotMat.Rotator();
}

//
// Convert a rotation into a vector facing in its direction.
//
FVector FRotator::Vector()
{
	FRotationMatrix R( *this );
	return R.GetAxis(0);
}

//
// Convert a rotation into a quaternion.
//

FQuat FRotator::Quaternion()
{
	FRotationMatrix R( *this );
	return FQuat( R );
}

/** Convert a Rotator into floating-point Euler angles (in degrees). */
FVector FRotator::Euler() const
{
	return FVector( Roll * (180.f / 32768.f), Pitch * (180.f / 32768.f), Yaw * (180.f / 32768.f) );
}

/** Convert a vector of floating-point Euler angles (in degrees) into a Rotator. */
FRotator FRotator::MakeFromEuler(const FVector& Euler)
{
	return FRotator( Euler.Y * (32768.f / 180.f), Euler.Z * (32768.f / 180.f), Euler.X * (32768.f / 180.f) );
}



/** 
 *	Decompose this Rotator into a Winding part (multiples of 65535) and a Remainder part. 
 *	Remainder will always be in -32768 -> 32768 range.
 */
void FRotator::GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const
{
	//// YAW
	Remainder.Yaw = Yaw % 65535;
	Winding.Yaw = Yaw - Remainder.Yaw;

	if(Remainder.Yaw > 32768)
	{
		Winding.Yaw += 65535;
		Remainder.Yaw -= 65535;
	}
	else if(Remainder.Yaw < -32768)
	{
		Winding.Yaw -= 65535;
		Remainder.Yaw += 65535;
	}

	//// PITCH
	Remainder.Pitch = Pitch % 65535;
	Winding.Pitch = Pitch - Remainder.Pitch;

	if(Remainder.Pitch > 32768)
	{
		Winding.Pitch += 65535;
		Remainder.Pitch -= 65535;
	}
	else if(Remainder.Pitch < -32768)
	{
		Winding.Pitch -= 65535;
		Remainder.Pitch += 65535;
	}

	//// ROLL
	Remainder.Roll = Roll % 65535;
	Winding.Roll = Roll - Remainder.Roll;

	if(Remainder.Roll > 32768)
	{
		Winding.Roll += 65535;
		Remainder.Roll -= 65535;
	}
	else if(Remainder.Roll < -32768)
	{
		Winding.Roll -= 65535;
		Remainder.Roll += 65535;
	}
}

/** 
 *	Alter this Rotator to form the 'shortest' rotation that will have the affect. 
 *	First clips the rotation between +/- 65535, then checks direction for each component.
 */
void FRotator::MakeShortestRoute()
{
	//// YAW

	// Clip rotation to +/- 65535 range
	Yaw = Yaw % 65535;

	// Then ensure it takes the 'shortest' route.
	if(Yaw > 32768)
		Yaw -= 65535;
	else if(Yaw < -32768)
		Yaw += 65535;


	//// PITCH

	Pitch = Pitch % 65535;

	if(Pitch > 32768)
		Pitch -= 65535;
	else if(Pitch < -32768)
		Pitch += 65535;


	//// ROLL

	Roll = Roll % 65535;

	if(Roll > 32768)
		Roll -= 65535;
	else if(Roll < -32768)
		Roll += 65535;
}

/*-----------------------------------------------------------------------------
	FQuaternion and FMatrix support functions
-----------------------------------------------------------------------------*/

FMatrix::FMatrix(const FVector& Origin, const FRotator& Rotation)
{
	*this = FRotationMatrix(Rotation) * FTranslationMatrix(Origin);
}

FRotator FMatrix::Rotator() const
{
	FVector		XAxis	= GetAxis( 0 );
	FVector		YAxis	= GetAxis( 1 );
	FVector		ZAxis	= GetAxis( 2 );

	FRotator	Rotator	= FRotator( 
									appRound(appAtan2( XAxis.Z, appSqrt(Square(XAxis.X)+Square(XAxis.Y)) ) * 32768.f / PI), 
									appRound(appAtan2( XAxis.Y, XAxis.X ) * 32768.f / PI), 
									0 
								);
	
	FVector		SYAxis	= FRotationMatrix( Rotator ).GetAxis(1);
	Rotator.Roll		= appRound(appAtan2( ZAxis | SYAxis, YAxis | SYAxis ) * 32768.f / PI);
	return Rotator;
}

const FMatrix FMatrix::Identity(FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,1,0),FPlane(0,0,0,1));

const FQuat FQuat::Identity(0,0,0,1);


FQuat::FQuat( const FMatrix& M )
{
	//const MeReal *const t = (MeReal *) tm;
	FLOAT	tr, s;

	static const INT nxt[3] = { 1, 2, 0 };

	// Check diagonal (trace)
	tr = M.M[0][0] + M.M[1][1] + M.M[2][2];

	if (tr > 0.0f) 
	{
		s = appSqrt(tr + 1.0f);

		this->W = s * 0.5f;

		s = 0.5f / s;

		this->X = (M.M[1][2] - M.M[2][1]) * s;
		this->Y = (M.M[2][0] - M.M[0][2]) * s;
		this->Z = (M.M[0][1] - M.M[1][0]) * s;
	} 
	else 
	{
		// diagonal is negative

		INT i, j, k;
		FLOAT qt[4];

		i = 0;

		if (M.M[1][1] > M.M[0][0])
			i = 1;

		if (M.M[2][2] > M.M[i][i])
			i = 2;

		j = nxt[i];
		k = nxt[j];

		s = M.M[i][i] - M.M[j][j] - M.M[k][k] + 1.0f;
		s = appSqrt(s);

		qt[i] = s * 0.5f;

		if (s != 0.0f)
			s = 0.5f / s;

		qt[3] = (M.M[j][k] - M.M[k][j]) * s;
		qt[j] = (M.M[i][j] + M.M[j][i]) * s;
		qt[k] = (M.M[i][k] + M.M[k][i]) * s;

		this->X = qt[0];
		this->Y = qt[1];
		this->Z = qt[2];
		this->W = qt[3];
	}
}

/** Convert a vector of floating-point Euler angles (in degrees) into a Quaternion. */
FQuat FQuat::MakeFromEuler(const FVector& Euler)
{
	FMatrix RotMatrix( FVector(0.f), FRotator::MakeFromEuler(Euler) );
	return FQuat(RotMatrix);
}

/** Convert a Quaternion into floating-point Euler angles (in degrees). */
FVector FQuat::Euler() const
{
	FMatrix RotMatrix( FVector(0.f), *this );
	return RotMatrix.Rotator().Euler();
}

FQuat FQuatFindBetween(const FVector& vec1, const FVector& vec2)
{
	FVector cross = vec1 ^ vec2;
	FLOAT crossMag = cross.Size();

	// If these vectors are basically parallel - just return identity quaternion (ie no rotation).
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		return FQuat::Identity;
	}

	FLOAT angle = appAsin(crossMag);

	FLOAT dot = vec1 | vec2;
	if(dot < 0.0f)
		angle = PI - angle;

	FLOAT sinHalfAng = appSin(0.5f * angle);
	FLOAT cosHalfAng = appCos(0.5f * angle);
	FVector axis = cross / crossMag;

	return FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );
}

// Returns quaternion with W=0 and V=theta*v.

FQuat FQuat::Log()
{
	FQuat Result;
	Result.W = 0.f;

	if ( Abs(W) < 1.f )
	{
		FLOAT Angle = appAcos(W);
		FLOAT SinAngle = appSin(Angle);

		if ( Abs(SinAngle) >= SMALL_NUMBER )
		{
			FLOAT Scale = Angle/SinAngle;
			Result.X = Scale*X;
			Result.Y = Scale*Y;
			Result.Z = Scale*Z;

			return Result;
		}
	}

	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;

	return Result;
}

// Assumes a quaternion with W=0 and V=theta*v (where |v| = 1).
// Exp(q) = (sin(theta)*v, cos(theta))

FQuat FQuat::Exp()
{
	FLOAT Angle = appSqrt(X*X + Y*Y + Z*Z);
	FLOAT SinAngle = appSin(Angle);

	FQuat Result;
	Result.W = appCos(Angle);

	if ( Abs(SinAngle) >= SMALL_NUMBER )
	{
		FLOAT Scale = SinAngle/Angle;
		Result.X = Scale*X;
		Result.Y = Scale*Y;
		Result.Z = Scale*Z;
	}
	else
	{
		Result.X = X;
		Result.Y = Y;
		Result.Z = Z;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	Swept-Box vs Box test.
-----------------------------------------------------------------------------*/

/* Line-extent/Box Test Util */
#define BOX_SIDE_THRESHOLD	0.1f

UBOOL FLineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 FLOAT& HitTime)
{
	FBox box = inBox;
	box.Max.X += Extent.X;
	box.Max.Y += Extent.Y;
	box.Max.Z += Extent.Z;
	
	box.Min.X -= Extent.X;
	box.Min.Y -= Extent.Y;
	box.Min.Z -= Extent.Z;

	FVector Dir = (End - Start);
	
	FVector	Time;
	UBOOL	Inside = 1;
	FLOAT   faceDir[3] = {1, 1, 1};
	
	/////////////// X
	if(Start.X < box.Min.X)
	{
		if(Dir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[0] = -1;
			Time.X = (box.Min.X - Start.X) / Dir.X;
		}
	}
	else if(Start.X > box.Max.X)
	{
		if(Dir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (box.Max.X - Start.X) / Dir.X;
		}
	}
	else
		Time.X = 0.0f;
	
	/////////////// Y
	if(Start.Y < box.Min.Y)
	{
		if(Dir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[1] = -1;
			Time.Y = (box.Min.Y - Start.Y) / Dir.Y;
		}
	}
	else if(Start.Y > box.Max.Y)
	{
		if(Dir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (box.Max.Y - Start.Y) / Dir.Y;
		}
	}
	else
		Time.Y = 0.0f;
	
	/////////////// Z
	if(Start.Z < box.Min.Z)
	{
		if(Dir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[2] = -1;
			Time.Z = (box.Min.Z - Start.Z) / Dir.Z;
		}
	}
	else if(Start.Z > box.Max.Z)
	{
		if(Dir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (box.Max.Z - Start.Z) / Dir.Z;
		}
	}
	else
		Time.Z = 0.0f;
	
	// If the line started inside the box (ie. player started in contact with the fluid)
	if(Inside)
	{
		HitLocation = Start;
		HitNormal = FVector(0, 0, 1);
		HitTime = 0;
		return 1;
	}
	// Otherwise, calculate when hit occured
	else
	{	
		if(Time.Y > Time.Z)
		{
			HitTime = Time.Y;
			HitNormal = FVector(0, faceDir[1], 0);
		}
		else
		{
			HitTime = Time.Z;
			HitNormal = FVector(0, 0, faceDir[2]);
		}
		
		if(Time.X > HitTime)
		{
			HitTime = Time.X;
			HitNormal = FVector(faceDir[0], 0, 0);
		}
		
		if(HitTime >= 0.0f && HitTime <= 1.0f)
		{
			HitLocation = Start + Dir * HitTime;
			
			if(	HitLocation.X > box.Min.X - BOX_SIDE_THRESHOLD && HitLocation.X < box.Max.X + BOX_SIDE_THRESHOLD &&
				HitLocation.Y > box.Min.Y - BOX_SIDE_THRESHOLD && HitLocation.Y < box.Max.Y + BOX_SIDE_THRESHOLD &&
				HitLocation.Z > box.Min.Z - BOX_SIDE_THRESHOLD && HitLocation.Z < box.Max.Z + BOX_SIDE_THRESHOLD)
			{				
				return 1;
			}
		}
		
		return 0;
	}
}

/*-----------------------------------------------------------------------------
	Complex number algebra
-----------------------------------------------------------------------------*/

FComplex Exp(const FComplex& Exponent)
{
	return FComplex(appCos(Exponent.Imag), appSin(Exponent.Imag)) * appExp(Exponent.Real);
}

/*-----------------------------------------------------------------------------
	Fourier transform
-----------------------------------------------------------------------------*/

// Input[LowerIndex...LowerIndex + Length] is Fourier-transformed and the result is placed in Output[LowerIndex...LowerIndex + Length].
// The discrete Fourier transform is currently used; it will be replaced with the fast Fourier transform at some point.

void FourierTransform(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT LowerIndex, UINT Length)
{
	FComplex ExpMultiplier(0.0, -2.0 * PI / Length);

	for (UINT OutputIndex = LowerIndex; OutputIndex < LowerIndex + Length; OutputIndex++)
	{
		Output(OutputIndex) = FComplex();
		for (UINT InputIndex = LowerIndex; InputIndex < LowerIndex + Length; InputIndex++)
			Output(OutputIndex) += Input(InputIndex) * Exp(ExpMultiplier * InputIndex * OutputIndex);
	}
}

void InverseFourierTransform(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT LowerIndex, UINT Length)
{
	FLOAT InvLength = 1/Length;
	FComplex ExpMultiplier(0.0, 2.0 * PI / Length);

	for (UINT OutputIndex = LowerIndex; OutputIndex < LowerIndex + Length; OutputIndex++)
	{
		Output(OutputIndex) = FComplex();
		for (UINT InputIndex = LowerIndex; InputIndex < LowerIndex + Length; InputIndex++)
			Output(OutputIndex) += Input(InputIndex) * Exp(ExpMultiplier * InputIndex * OutputIndex);
		Output(OutputIndex) *= InvLength;
	}
}

void FourierTransform2D(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT NumRows, UINT NumColumns)
{
	for (UINT RowIndex = 0; RowIndex < NumRows; RowIndex++)
		FourierTransform(Input, Output, RowIndex * NumColumns, NumColumns);
}

void InverseFourierTransform2D(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT NumRows, UINT NumColumns)
{
	for (UINT RowIndex = 0; RowIndex < NumRows; RowIndex++)
		InverseFourierTransform(Input, Output, RowIndex * NumColumns, NumColumns);
}

/*-----------------------------------------------------------------------------
	Probability distributions
-----------------------------------------------------------------------------*/

// Generates a random number according to a Gaussian probability distribution with a specified mean and standard deviation.
FLOAT GaussianRandom(FLOAT Mean, FLOAT StandardDeviation)
{
	FVector2D Point;
	FLOAT SqrMag;
	// Rejection-acceptance sampling. Keep generating points in the unit square until we get one within the unit circle.
	do {
		Point.X = 2.0 * appFrand() - 1.0;
		Point.Y = 2.0 * appFrand() - 1.0;
		SqrMag = Point.SizeSquared();
	} while (SqrMag >= 1.0);
	// The Gaussian-distributed number is generated by a Box-Mueller transformation of the squared magnitude.
	return appSqrt((-2.0 * appLoge(SqrMag)) / SqrMag) * StandardDeviation + Mean;
}

/*-----------------------------------------------------------------------------
	Cubic Quaternion interpolation support functions
-----------------------------------------------------------------------------*/

// Simpler Slerp that doesn't do any checks for 'shortest distance' etc.
// We need this for the cubic interpolation stuff so that the multiple Slerps dont go in different directions.
FQuat SlerpQuatFullPath(const FQuat &quat1, const FQuat &quat2, float Alpha)
{
	FLOAT CosAngle = Clamp(quat1 | quat2, -1.f, 1.f);
	FLOAT Angle = appAcos(CosAngle);

	//debugf( TEXT("CosAngle: %f Angle: %f"), CosAngle, Angle );

	if ( Abs(Angle) < KINDA_SMALL_NUMBER )
		return quat1;

	FLOAT SinAngle = appSin(Angle);
	FLOAT InvSinAngle = 1.f/SinAngle;

	FLOAT Scale0 = appSin((1.0f-Alpha)*Angle)*InvSinAngle;
	FLOAT Scale1 = appSin(Alpha*Angle)*InvSinAngle;

	return quat1*Scale0 + quat2*Scale1;
}

// Given start and end quaternions of quat1 and quat2, and tangents at those points tang1 and tang2, calculate the point at Alpha (between 0 and 1) between them.
FQuat SquadQuat(const FQuat& quat1, const FQuat& tang1, const FQuat& quat2, const FQuat& tang2, FLOAT Alpha)
{
	FQuat Q1 = SlerpQuatFullPath(quat1, quat2, Alpha);
	//debugf(TEXT("Q1: %f %f %f %f"), Q1.X, Q1.Y, Q1.Z, Q1.W);

	FQuat Q2 = SlerpQuatFullPath(tang1, tang2, Alpha);
	//debugf(TEXT("Q2: %f %f %f %f"), Q2.X, Q2.Y, Q2.Z, Q2.W);

	FQuat Result = SlerpQuatFullPath(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//FQuat Result = SlerpQuat(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//debugf(TEXT("Result: %f %f %f %f"), Result.X, Result.Y, Result.Z, Result.W);

	return Result;
}

void CalcQuatTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, FLOAT Tension, FQuat& OutTan )
{
	FQuat InvP = -P;
	FQuat Part1 = (InvP * PrevP).Log();
	FQuat Part2 = (InvP * NextP).Log();

	FQuat PreExp = (Part1 + Part2) * -0.25f;

	OutTan = P * PreExp.Exp();
}

static void FindBounds( FLOAT& OutMin, FLOAT& OutMax,  FLOAT Start, FLOAT StartLeaveTan, FLOAT StartT, FLOAT End, FLOAT EndArriveTan, FLOAT EndT, UBOOL bCurve )
{
	OutMin = Min( Start, End );
	OutMax = Max( Start, End );

	// Do we need to consider extermeties of a curve?
	if(bCurve)
	{
		FLOAT a = 6.f*Start + 3.f*StartLeaveTan + 3.f*EndArriveTan - 6.f*End;
		FLOAT b = -6.f*Start - 4.f*StartLeaveTan - 2.f*EndArriveTan + 6.f*End;
		FLOAT c = StartLeaveTan;

		FLOAT Discriminant = (b*b) - (4.f*a*c);
		if(Discriminant > 0.f)
		{
			FLOAT SqrtDisc = appSqrt( Discriminant );

			FLOAT x0 = (-b + SqrtDisc)/(2.f*a); // x0 is the 'Alpha' ie between 0 and 1
			FLOAT t0 = StartT + x0*(EndT - StartT); // Then t0 is the actual 'time' on the curve
			if(t0 > StartT && t0 < EndT)
			{
				FLOAT Val = CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x0 );

				OutMin = ::Min( OutMin, Val );
				OutMax = ::Max( OutMax, Val );
			}

			FLOAT x1 = (-b - SqrtDisc)/(2.f*a);
			FLOAT t1 = StartT + x1*(EndT - StartT);
			if(t1 > StartT && t1 < EndT)
			{
				FLOAT Val = CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x1 );

				OutMin = ::Min( OutMin, Val );
				OutMax = ::Max( OutMax, Val );
			}
		}
	}
}

void CurveFloatFindIntervalBounds( const FInterpCurvePoint<FLOAT>& Start, const FInterpCurvePoint<FLOAT>& End, FLOAT& CurrentMin, FLOAT& CurrentMax )
{
	UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal, Start.LeaveTangent, Start.InVal, End.OutVal, End.ArriveTangent, End.InVal, bIsCurve);

	CurrentMin = ::Min( CurrentMin, OutMin );
	CurrentMax = ::Max( CurrentMax, OutMax );
}

void CurveVectorFindIntervalBounds( const FInterpCurvePoint<FVector>& Start, const FInterpCurvePoint<FVector>& End, FVector& CurrentMin, FVector& CurrentMax )
{
	UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.X, Start.LeaveTangent.X, Start.InVal, End.OutVal.X, End.ArriveTangent.X, End.InVal, bIsCurve);
	CurrentMin.X = ::Min( CurrentMin.X, OutMin );
	CurrentMax.X = ::Max( CurrentMax.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Y, Start.LeaveTangent.Y, Start.InVal, End.OutVal.Y, End.ArriveTangent.Y, End.InVal, bIsCurve);
	CurrentMin.Y = ::Min( CurrentMin.Y, OutMin );
	CurrentMax.Y = ::Max( CurrentMax.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Z, Start.LeaveTangent.Z, Start.InVal, End.OutVal.Z, End.ArriveTangent.Z, End.InVal, bIsCurve);
	CurrentMin.Z = ::Min( CurrentMin.Z, OutMin );
	CurrentMax.Z = ::Max( CurrentMax.Z, OutMax );
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

