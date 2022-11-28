/*=============================================================================
	UnSHM.cpp: Spherical harmonic map implementation.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(USphericalHarmonicMap);

IMPLEMENT_RESOURCE_TYPE(FSHTexture2D);
IMPLEMENT_RESOURCE_TYPE(FSHBasisCubemap);
IMPLEMENT_RESOURCE_TYPE(FSHSkyBasisCubemap);

//
//	Spherical harmonic constants.
//

FLOAT	NormalizationConstants[MAX_SH_BASIS];
INT		BasisL[MAX_SH_BASIS];
INT		BasisM[MAX_SH_BASIS];

//
//	Factorial
//

INT Factorial(INT A)
{
	if(A == 0)
		return 1;
	else
		return A * Factorial(A - 1);
}

//
//	InitSHTables - Initializes the tables used to calculate SH values.
//

INT InitSHTables()
{
	INT	L = 0,
		M = 0;

	for(INT BasisIndex = 0;BasisIndex < MAX_SH_BASIS;BasisIndex++)
	{
		BasisL[BasisIndex] = L;
		BasisM[BasisIndex] = M;

		NormalizationConstants[BasisIndex] = appSqrt(
			(FLOAT(2 * L + 1) / FLOAT(4 * PI)) *
			(FLOAT(Factorial(L - Abs(M))) / FLOAT(Factorial(L + Abs(M))))
			);

		if(M != 0)
			NormalizationConstants[BasisIndex] *= appSqrt(2.f);

		M++;
		if(M > L)
		{
			L++;
			M = -L;
		}
	}

	return 0;
}
INT	InitDummy = InitSHTables();

//
//	LegendrePolynomial
//

FORCEINLINE FLOAT LegendrePolynomial(INT L,INT M,FLOAT X)
{
	switch(L)
	{
	case 0:
		return 1;
	case 1:
		if(M == 0)
			return X;
		else if(M == 1)
			return -appSqrt(1 - X * X);
		break;
	case 2:
		if(M == 0)
			return -0.5f + (3 * X * X) / 2;
		else if(M == 1)
			return -3 * X * appSqrt(1 - X * X);
		else if(M == 2)
			return -3 * (-1 + X * X);
		break;
	case 3:
		if(M == 0)
			return -(3 * X) / 2 + (5 * X * X * X) / 2;
		else if(M == 1)
			return -3 * appSqrt(1 - X * X) / 2 * (-1 + 5 * X * X);
		else if(M == 2)
			return -15 * (-X + X * X * X);
		else if(M == 3)
			return -15 * appPow(1 - X * X,1.5f);
		break;
	case 4:
		if(M == 0)
			return 0.125f * (3.0f - 30.0f * X * X + 35.0f * X * X * X * X);
		else if(M == 1)
			return -2.5f * X * appSqrt(1.0f - X * X) * (7.0f * X * X - 3.0f);
		else if(M == 2)
			return -7.5f * (1.0f - 8.0f * X * X + 7.0f * X * X * X * X);
		else if(M == 3)
			return -105.0f * X * appPow(1 - X * X,1.5f);
		else if(M == 4)
			return 105.0f * Square(X * X - 1.0f);
		break;
	case 5:
		if(M == 0)
			return 0.125f * X * (15.0f - 70.0f * X * X + 63.0f * X * X * X * X);
		else if(M == 1)
			return -1.875f * appSqrt(1.0f - X * X) * (1.0f - 14.0f * X * X + 21.0f * X * X * X * X);
		else if(M == 2)
			return -52.5f * (X - 4.0f * X * X * X + 3.0f * X * X * X * X * X);
		else if(M == 3)
			return -52.5f * appPow(1.0f - X * X,1.5f) * (9.0f * X * X - 1.0f);
		else if(M == 4)
			return 945.0f * X * Square(X * X - 1);
		else if(M == 5)
			return -945.0f * appPow(1.0f - X * X,2.5f);
		break;
	};

	return 0.0f;
}

//
//	SHBasisFunction
//

FSHVector SHBasisFunction(FVector Vector)
{
	FSHVector	Result;

	// Initialize the result to the normalization constant.

	for(INT BasisIndex = 0;BasisIndex < MAX_SH_BASIS;BasisIndex++)
		Result.V[BasisIndex] = NormalizationConstants[BasisIndex];

	// Multiply the result by the phi-dependent part of the SH bases.

	FLOAT	Phi = appAtan2(Vector.Y,Vector.X);
	for(INT BandIndex = 1;BandIndex < MAX_SH_ORDER;BandIndex++)
	{
		FLOAT	SinPhiM = GMath.SinFloat(BandIndex * Phi),
				CosPhiM = GMath.CosFloat(BandIndex * Phi);

		for(INT RecurrentBandIndex = BandIndex;RecurrentBandIndex < MAX_SH_ORDER;RecurrentBandIndex++)
		{
			Result.V[SHGetBasisIndex(RecurrentBandIndex,-BandIndex)] *= SinPhiM;
			Result.V[SHGetBasisIndex(RecurrentBandIndex,+BandIndex)] *= CosPhiM;
		}
	}

	// Multiply the result by the theta-dependent part of the SH bases.

	for(INT BasisIndex = 1;BasisIndex < MAX_SH_BASIS;BasisIndex++)
		Result.V[BasisIndex] *= LegendrePolynomial(BasisL[BasisIndex],Abs(BasisM[BasisIndex]),Vector.Z);

	return Result;
}

//
//	FSHBasisCubemap::FSHBasisCubemap
//

FSHBasisCubemap::FSHBasisCubemap(USphericalHarmonicMap* InSHM,INT InBaseCoefficientIndex):
	SHM(InSHM),
	BaseCoefficientIndex(InBaseCoefficientIndex)
{
	SizeX = 64;
	SizeY = 64;
	NumMips = appCeilLogTwo(64);
	Format = PF_A8R8G8B8;
}

//
//	FSHBasisCubemap::GetData
//

void FSHBasisCubemap::GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY)
{
	UINT	MipWidth = Max<UINT>(SizeX >> MipIndex,1),
			MipHeight = Max<UINT>(SizeY >> MipIndex,1);

	static FVector	FaceBases[6][3] =
	{
		{ FVector(+1,+1,+1), FVector(+0,+0,-2), FVector(+0,-2,+0) },
		{ FVector(-1,+1,-1), FVector(+0,+0,+2), FVector(+0,-2,+0) },
		{ FVector(-1,+1,-1), FVector(+2,+0,+0), FVector(+0,+0,+2) },
		{ FVector(-1,-1,+1), FVector(+2,+0,+0), FVector(+0,+0,-2) },
		{ FVector(-1,+1,+1), FVector(+2,+0,+0), FVector(+0,-2,+0) },
		{ FVector(+1,+1,-1), FVector(-2,+0,+0), FVector(+0,-2,+0) }
	};

	FVector	FaceX = FaceBases[FaceIndex][1] / (MipWidth - 1),
			FaceY = FaceBases[FaceIndex][2] / (MipHeight - 1);

	FLOAT	Scale = 255.0f / 2.0f,
			Bias = 1.0f;

	for(UINT Y = 0;Y < MipHeight;Y++)
	{
		FColor*	DestColor = (FColor*) (((BYTE*) Buffer) + Y * StrideY);

		for(UINT X = 0;X < MipWidth;X++)
		{
			FSHVector	SH = SHBasisFunction((FaceBases[FaceIndex][0] + FaceX * X + FaceY * Y).SafeNormal());
			FLOAT		OptimizedSH[4];

			for(INT CoefficientIndex = 0;CoefficientIndex < 3;CoefficientIndex++)
			{
				OptimizedSH[CoefficientIndex] = 0.0f;
				if(BaseCoefficientIndex + CoefficientIndex < SHM->NumCoefficients)
					for(INT UnoptimizedCoefficientIndex = 0;UnoptimizedCoefficientIndex < SHM->NumUnoptimizedCoefficients;UnoptimizedCoefficientIndex++)
						OptimizedSH[CoefficientIndex] += SH.V[UnoptimizedCoefficientIndex] * SHM->OptimizationMatrix((BaseCoefficientIndex + CoefficientIndex) * SHM->NumUnoptimizedCoefficients + UnoptimizedCoefficientIndex);
			}
			OptimizedSH[3] = 0.0f;
			if(SHM->MeanVector.Num() == SHM->NumUnoptimizedCoefficients)
			{
				for(INT UnoptimizedCoefficientIndex = 0;UnoptimizedCoefficientIndex < SHM->NumUnoptimizedCoefficients;UnoptimizedCoefficientIndex++)
					OptimizedSH[3] += SH.V[UnoptimizedCoefficientIndex] * SHM->MeanVector(UnoptimizedCoefficientIndex);
			}

			*DestColor++ = FColor(
				BYTE(Clamp<INT>((OptimizedSH[0] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[1] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[2] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[3] + Bias) * Scale,0,255))
				);
		}
	}

}

//
//	FSHSkyBasisCubemap::FSHSkyBasisCubemap
//

#define SKY_DIRECTIONS	256
static FVector	SkySampleDirections[SKY_DIRECTIONS];

FSHSkyBasisCubemap::FSHSkyBasisCubemap(USphericalHarmonicMap* InSHM,INT InBaseCoefficientIndex):
	SHM(InSHM),
	BaseCoefficientIndex(InBaseCoefficientIndex)
{
	SizeX = 8;
	SizeY = 8;
	NumMips = appCeilLogTwo(Max(SizeX,SizeY));
	Format = PF_A8R8G8B8;

	for(UINT DirectionIndex = 0;DirectionIndex < SKY_DIRECTIONS;DirectionIndex++)
		SkySampleDirections[DirectionIndex] = VRand();
}

//
//	FSHSkyBasisCubemap::GetData
//

void FSHSkyBasisCubemap::GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY)
{
	UINT	MipWidth = Max<UINT>(SizeX >> MipIndex,1),
			MipHeight = Max<UINT>(SizeY >> MipIndex,1);

	static FVector	FaceBases[6][3] =
	{
		{ FVector(+1,+1,+1), FVector(+0,+0,-2), FVector(+0,-2,+0) },
		{ FVector(-1,+1,-1), FVector(+0,+0,+2), FVector(+0,-2,+0) },
		{ FVector(-1,+1,-1), FVector(+2,+0,+0), FVector(+0,+0,+2) },
		{ FVector(-1,-1,+1), FVector(+2,+0,+0), FVector(+0,+0,-2) },
		{ FVector(-1,+1,+1), FVector(+2,+0,+0), FVector(+0,-2,+0) },
		{ FVector(+1,+1,-1), FVector(-2,+0,+0), FVector(+0,-2,+0) }
	};

	FVector	FaceX = FaceBases[FaceIndex][1] / (MipWidth - 1),
			FaceY = FaceBases[FaceIndex][2] / (MipHeight - 1);

	FLOAT	Scale = 255.0f / 2.0f,
			Bias = 1.0f;

	for(UINT Y = 0;Y < MipHeight;Y++)
	{
		FColor*	DestColor = (FColor*) (((BYTE*) Buffer) + Y * StrideY);

		for(UINT X = 0;X < MipWidth;X++)
		{
			FVector		SkyDirection = (FaceBases[FaceIndex][0] + FaceX * X + FaceY * Y).SafeNormal();
			FSHVector	SH;

			for(UINT DirectionIndex = 0;DirectionIndex < SKY_DIRECTIONS;DirectionIndex++)
			{
				FLOAT	Dot = SkyDirection | SkySampleDirections[DirectionIndex];

				if(Dot > 0.0f)
				{
					FSHVector	SampleBasis = SHBasisFunction(SkySampleDirections[DirectionIndex]);
					FLOAT		SampleScale = ((FLOAT)(4.0 * PI) / (FLOAT)SKY_DIRECTIONS) * Max<FLOAT>(0,Dot);
					for(UINT CoefficientIndex = 0;CoefficientIndex < MAX_SH_BASIS;CoefficientIndex++)
						SH.V[CoefficientIndex] += SampleBasis.V[CoefficientIndex] * SampleScale;
				}
			}

			FLOAT		OptimizedSH[4];
			if(SHM)
			{
				for(INT CoefficientIndex = 0;CoefficientIndex < 4;CoefficientIndex++)
				{
					OptimizedSH[CoefficientIndex] = 0.0f;
					if(BaseCoefficientIndex + CoefficientIndex < SHM->NumCoefficients)
						for(INT UnoptimizedCoefficientIndex = 0;UnoptimizedCoefficientIndex < SHM->NumUnoptimizedCoefficients;UnoptimizedCoefficientIndex++)
							OptimizedSH[CoefficientIndex] += SH.V[UnoptimizedCoefficientIndex] * SHM->OptimizationMatrix((BaseCoefficientIndex + CoefficientIndex) * SHM->NumUnoptimizedCoefficients + UnoptimizedCoefficientIndex);
				}
			}
			else
			{
				for(INT CoefficientIndex = 0;CoefficientIndex < 4;CoefficientIndex++)
					OptimizedSH[CoefficientIndex] = SH.V[CoefficientIndex];
			}

			*DestColor++ = FColor(
				BYTE(Clamp<INT>((OptimizedSH[0] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[1] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[2] + Bias) * Scale,0,255)),
				BYTE(Clamp<INT>((OptimizedSH[3] + Bias) * Scale,0,255))
				);
		}
	}

}

//
//	USphericalHarmonicMap::Serialize
//

void USphericalHarmonicMap::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << CoefficientTextures << NumCoefficients << NumUnoptimizedCoefficients << OptimizationMatrix;
	Ar << MeanVector;
}

//
//	USphericalHarmonicMap::PostLoad
//

void USphericalHarmonicMap::PostLoad()
{
	Super::PostLoad();

	OptimizedBasisCubemaps.Empty(CoefficientTextures.Num());
	OptimizedSkyCubemaps.Empty(CoefficientTextures.Num());

	UINT	BaseCoefficientIndex = 0;
	for(UINT TextureIndex = 0;TextureIndex < (UINT)CoefficientTextures.Num();TextureIndex++)
	{
		new(OptimizedBasisCubemaps) FSHBasisCubemap(this,BaseCoefficientIndex);
		new(OptimizedSkyCubemaps) FSHSkyBasisCubemap(this,BaseCoefficientIndex);
		BaseCoefficientIndex += GPixelFormats[CoefficientTextures(TextureIndex).Format].NumComponents;
	}

}

//
//	USphericalHarmonicMap::GetDesc
//

FString USphericalHarmonicMap::GetDesc()
{
	check(CoefficientTextures.Num());
	return FString::Printf(TEXT("%dx%d"),CoefficientTextures(0).SizeX,CoefficientTextures(0).SizeY);
}

//
//	USphericalHarmonicMap::DrawThumbnail
//

void USphericalHarmonicMap::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	check(CoefficientTextures.Num());
	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > CoefficientTextures(0).SizeX ) ? CoefficientTextures(0).SizeX : InViewport->GetSizeX() : CoefficientTextures(0).SizeX*InZoom,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > CoefficientTextures(0).SizeY ) ? CoefficientTextures(0).SizeY : InViewport->GetSizeY() : CoefficientTextures(0).SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		&CoefficientTextures(0),
		0
	);
}

//
//	USphericalHarmonicMap::GetThumbnailDesc
//

FThumbnailDesc USphericalHarmonicMap::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	check(CoefficientTextures.Num());
	FThumbnailDesc td;
	td.Width = InFixedSz ? InFixedSz : CoefficientTextures(0).SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : CoefficientTextures(0).SizeY*InZoom;
	return td;
}

//
//	USphericalHarmonicMap::GetThumbnailLabels
//

INT USphericalHarmonicMap::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( GetDesc() );

	return InLabels->Num();
}
