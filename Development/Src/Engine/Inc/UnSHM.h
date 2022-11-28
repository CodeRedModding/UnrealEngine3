/*=============================================================================
	UnSHM.h: Spherical harmonic map definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//	Constants.

#define MAX_SH_ORDER	6
#define MAX_SH_BASIS	(MAX_SH_ORDER*MAX_SH_ORDER)

//	Forward declarations.

class USphericalHarmonicMap;

//
//	FSHVector
//

struct FSHVector
{
	FLOAT	V[MAX_SH_BASIS];

	FSHVector()
	{
		for(INT BasisIndex = 0;BasisIndex < MAX_SH_BASIS;BasisIndex++)
			V[BasisIndex] = 0.0f;
	}
};

//
//	SHGetBasisIndex
//

FORCEINLINE INT SHGetBasisIndex(INT L,INT M)
{
	return L * (L + 1) + M;
}

//
//	SHBasisFunction - Returns the value of the SH basis L,M at the point on the sphere defined by the unit vector Vector.
//

FSHVector SHBasisFunction(FVector Vector);

//
//	FSHTexture
//

struct FSHTexture2D : public FStaticTexture2D
{
	DECLARE_RESOURCE_TYPE(FSHTexture2D,FStaticTexture2D);

	FPlane	CoefficientScale,
			CoefficientBias;

	//	Constructors.

	FSHTexture2D() {}
	FSHTexture2D(UINT InSizeX,UINT InSizeY,EPixelFormat InFormat)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		Format = InFormat;
		NumMips = 0;
	}

	//	Serializer.

	friend FArchive& operator<<(FArchive& Ar,FSHTexture2D& Texture)
	{
		Ar << Texture.CoefficientScale << Texture.CoefficientBias;
		Texture.Serialize(Ar);
		return Ar;
	}
};

//
//	FSHBasisCubemap
//

struct FSHBasisCubemap : public FTextureCube
{
	DECLARE_RESOURCE_TYPE(FSHBasisCubemap,FTextureCube);

	USphericalHarmonicMap*	SHM;
	INT						BaseCoefficientIndex;

	// Constructor.

	FSHBasisCubemap(USphericalHarmonicMap* InSHM,INT InBaseCoefficientIndex);

	// FTextureCube interface.

	virtual void GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY);
};

//
//	FSHSkyBasisCubemap
//

struct FSHSkyBasisCubemap : public FTextureCube
{
	DECLARE_RESOURCE_TYPE(FSHSkyBasisCubemap,FTextureCube);

	USphericalHarmonicMap*	SHM;
	INT						BaseCoefficientIndex;

	// Constructor.

	FSHSkyBasisCubemap(USphericalHarmonicMap* InSHM,INT InBaseCoefficientIndex);

	// FTextureCube interface.

	virtual void GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY);
};

//
//	USphericalHarmonicMap
//

class USphericalHarmonicMap : public UObject
{
	DECLARE_CLASS(USphericalHarmonicMap,UObject,0,Engine);
public:

	TArray<FSHTexture2D>		CoefficientTextures;
	TArray<FSHBasisCubemap>		OptimizedBasisCubemaps;
	TArray<FSHSkyBasisCubemap>	OptimizedSkyCubemaps;
	INT							NumCoefficients,
								NumUnoptimizedCoefficients;
	TArray<FLOAT>				OptimizationMatrix;
	TArray<FLOAT>				MeanVector;

	//	UObject interface.

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};