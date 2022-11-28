/*=============================================================================
	UnEditSHM.cpp: Spherical harmonic map editing code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EditorPrivate.h"

IMPLEMENT_CLASS(USphericalHarmonicMapFactorySHM);

USphericalHarmonicMapFactorySHM::USphericalHarmonicMapFactorySHM()
{
	bEditorImport = 1;
}

//
//	USphericalHarmonicMapFactorySHM::StaticConstructor()
//

void USphericalHarmonicMapFactorySHM::StaticConstructor()
{
	SupportedClass = USphericalHarmonicMap::StaticClass();
	new(Formats)FString(TEXT("shm;Spherical harmonic map files"));
	bCreateNew = 0;

}

//
//	FSHMHeader
//

struct FSHMHeader
{
	DWORD	Version;
	DWORD	Width;
	DWORD	Height;
	DWORD	NumCoefficients;
	DWORD	NumUnoptimizedCoefficients;
	DWORD	NumMips;
};

//
//	USphericalHarmonicMapFactorySHM::FactoryCreateBinary
//

UObject* USphericalHarmonicMapFactorySHM::FactoryCreateBinary(
	UClass* InClass,
	UObject* InOuter,
	FName InName,
	DWORD InFlags,
	UObject* Context,
	const TCHAR* Type,
	const BYTE*& Buffer,
	const BYTE* BufferEnd,
	FFeedbackContext* Warn
	)
{
	// Read the file header.

	FSHMHeader*	Header = (FSHMHeader*)Buffer;

	if(Header->Version != 0xffff0002 && Header->Version != 0xffff0003)
	{
		Warn->Logf(TEXT("Error importing SHM: The file's version is unsupported."));
		return NULL;
	}

	FLOAT*	Coefficients = (FLOAT*)(Header+1);

	// Construct the spherical harmonic map object.

	USphericalHarmonicMap*	Result = CastChecked<USphericalHarmonicMap>(StaticConstructObject(
		InClass,
		InOuter,
		InName,
		InFlags
		));

	Result->NumUnoptimizedCoefficients = Header->NumUnoptimizedCoefficients;
	Result->NumCoefficients = Header->NumCoefficients;

	// Calculate the minimum and maximum coefficients in the SHM.

	TArray<FLOAT>	MinCoefficients,
					MaxCoefficients;

	for(DWORD CoefficientIndex = 0;CoefficientIndex < Header->NumCoefficients;CoefficientIndex++)
	{
		MinCoefficients.AddItem(100.0f);
		MaxCoefficients.AddItem(-100.0f);
	}

	for(DWORD Y = 0;Y < Header->Height;Y++)
	{
		for(DWORD X = 0;X < Header->Width;X++)
		{
			for(DWORD CoefficientIndex = 0;CoefficientIndex < Header->NumCoefficients;CoefficientIndex++)
			{
				FLOAT	Coefficient = Coefficients[(Y * Header->Width + X) * Header->NumCoefficients + CoefficientIndex];

				if(Coefficient < MinCoefficients(CoefficientIndex))
					MinCoefficients(CoefficientIndex) = Coefficient;

				if(Coefficient > MaxCoefficients(CoefficientIndex))
					MaxCoefficients(CoefficientIndex) = Coefficient;
			}
		}
	}

	// Read the optimization matrix.

	FLOAT*	MatrixPtr = Coefficients;
	for(DWORD MipIndex = 0;MipIndex < Header->NumMips;MipIndex++)
		MatrixPtr += Max<INT>(1,Header->Width >> MipIndex) * Max<INT>(1,Header->Height >> MipIndex) * Header->NumCoefficients;

	Result->OptimizationMatrix.Add(Result->NumCoefficients * Result->NumUnoptimizedCoefficients);
	appMemcpy(&Result->OptimizationMatrix(0),MatrixPtr,Result->OptimizationMatrix.Num() * sizeof(FLOAT));

	// Read the mean vector.

	if(Header->Version == 0xffff0003)
	{
		FLOAT*	MeanPtr = MatrixPtr;
		MeanPtr += Result->NumCoefficients * Result->NumUnoptimizedCoefficients;
		Result->MeanVector.Empty(Result->NumUnoptimizedCoefficients);
		Result->MeanVector.Add(Result->NumUnoptimizedCoefficients);
		appMemcpy(&Result->MeanVector(0),MeanPtr,sizeof(FLOAT) * Result->NumUnoptimizedCoefficients);
	}

	// Create the coefficient textures.

	for(INT BaseCoefficientIndex = 0;BaseCoefficientIndex < Result->NumCoefficients;BaseCoefficientIndex += 3)
	{
		EPixelFormat	PixelFormat = PF_DXT1;
		FSHTexture2D*	Texture = new(Result->CoefficientTextures) FSHTexture2D(Header->Width,Header->Height,PixelFormat);

		Texture->CoefficientScale = FPlane(
			MaxCoefficients(BaseCoefficientIndex + 0) - MinCoefficients(BaseCoefficientIndex + 0),
			MaxCoefficients(BaseCoefficientIndex + 1) - MinCoefficients(BaseCoefficientIndex + 1),
			MaxCoefficients(BaseCoefficientIndex + 2) - MinCoefficients(BaseCoefficientIndex + 2),
			0
			);

		Texture->CoefficientBias = FPlane(
			MinCoefficients(BaseCoefficientIndex + 0),
			MinCoefficients(BaseCoefficientIndex + 1),
			MinCoefficients(BaseCoefficientIndex + 2),
			1
			);

		FLOAT*	SrcPtr = Coefficients + BaseCoefficientIndex;
		FPlane	InvCoefficientBias(
			-Texture->CoefficientBias.X,
			-Texture->CoefficientBias.Y,
			-Texture->CoefficientBias.Z,
			0.0f
			);
		FPlane	InvCoefficientScale(
			255.0f / Texture->CoefficientScale.X,
			255.0f / Texture->CoefficientScale.Y,
			255.0f / Texture->CoefficientScale.Z,
			0.0f
			);
		for(DWORD MipIndex = 0;MipIndex < Header->NumMips;MipIndex++)
		{
			UINT	MipSizeX = Max<UINT>(Header->Width >> MipIndex,4),
					MipSizeY =  Max<UINT>(Header->Height >> MipIndex,4);

			// Quantize the coefficients.

			TArray<FColor>	UncompressedTexture(MipSizeX * MipSizeY);
			FColor*			DestPtr = &UncompressedTexture(0);
			for(UINT Y = 0;Y < MipSizeY;Y++)
			{
				for(UINT X = 0;X < MipSizeX;X++)
				{
					*DestPtr++ = FColor(
						(BYTE)Clamp<INT>((SrcPtr[0] + InvCoefficientBias.X) * InvCoefficientScale.X,0,255),
						(BYTE)Clamp<INT>((SrcPtr[1] + InvCoefficientBias.Y) * InvCoefficientScale.Y,0,255),
						(BYTE)Clamp<INT>((SrcPtr[2] + InvCoefficientBias.Z) * InvCoefficientScale.Z,0,255),
						0
						);
					SrcPtr += Header->NumCoefficients;
				}
			}

			// DXT compress the coefficients.

			FStaticMipMap2D*	TextureMip = new(Texture->Mips) FStaticMipMap2D(MipSizeX,MipSizeY,CalculateImageBytes(MipSizeX,MipSizeY,0,(EPixelFormat)Texture->Format));
			DXTCompress(
				&UncompressedTexture(0),
				MipSizeX,
				MipSizeY,
				PixelFormat,
				TextureMip->Data
				);
		}
		Texture->NumMips = Texture->Mips.Num();
		GResourceManager->UpdateResource(Texture);

		new(Result->OptimizedBasisCubemaps) FSHBasisCubemap(Result,BaseCoefficientIndex);
		new(Result->OptimizedSkyCubemaps) FSHSkyBasisCubemap(Result,BaseCoefficientIndex);
	}

	return Result;

}
