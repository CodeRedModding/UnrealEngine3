/*=============================================================================
	UnClient.cpp: UClient implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "EnginePrivate.h"

#if !CONSOLE && !__LINUX__
#include "NVTriStrip.h"
#endif

IMPLEMENT_RESOURCE_TYPE(FSurface);
IMPLEMENT_RESOURCE_TYPE(FTextureBase);
IMPLEMENT_RESOURCE_TYPE(FTexture2D);
IMPLEMENT_RESOURCE_TYPE(FTexture3D);
IMPLEMENT_RESOURCE_TYPE(FTextureCube);
IMPLEMENT_RESOURCE_TYPE(FMaterial);
IMPLEMENT_RESOURCE_TYPE(FIndexBuffer);
IMPLEMENT_RESOURCE_TYPE(FVertexBuffer);
IMPLEMENT_RESOURCE_TYPE(FVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FLocalVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FLocalShadowVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FTerrainVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FFoliageVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FParticleVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FParticleSubUVVertexFactory);
IMPLEMENT_RESOURCE_TYPE(FParticleTrailVertexFactory);

IMPLEMENT_CLASS(UClient);
IMPLEMENT_CLASS(URenderDevice);

//
//	FWindPointSource::GetWind
//

FVector FWindPointSource::GetWind(const FVector& Location) const
{
	FVector	WindVector = Location - SourceLocation;
	FLOAT	Distance = WindVector.Size(),
			LocalPhase = Phase - Distance / 512.0f;

	if(LocalPhase < 0.0f)
		return FVector(0,0,0);
	else
		return (FVector(WindVector.X,WindVector.Y,0) / Distance) * Strength * appSin(LocalPhase * Frequency * 2.0f * PI) * (1.0f - LocalPhase * InvDuration) * Max(1.0f - Distance * InvRadius,0.0f);
}

//
//	FWindDirectionSource::GetWind
//

FVector FWindDirectionSource::GetWind(const FVector& Location) const
{
	FLOAT	Distance = Location | Direction,
			LocalPhase = Phase - Distance / 512.0f;

	return FVector(Direction.X,Direction.Y,0) * (Strength * appSin(LocalPhase * Frequency * 2.0f * PI));
}

//
//	FPackedNormal::operator=
//

void FPackedNormal::operator=(const FVector& InVector)
{
	Vector.X = Clamp(appTrunc(InVector.X * 127.5f + 127.5f),0,255);
	Vector.Y = Clamp(appTrunc(InVector.Y * 127.5f + 127.5f),0,255);
	Vector.Z = Clamp(appTrunc(InVector.Z * 127.5f + 127.5f),0,255);
	Vector.W = 0;
}

//
//	FPackedNormal::operator FVector
//

FPackedNormal::operator FVector() const
{
	return FVector(
		(FLOAT)Vector.X / 127.5f - 1.0f,
		(FLOAT)Vector.Y / 127.5f - 1.0f,
		(FLOAT)Vector.Z / 127.5f - 1.0f
		);
}

//
//	FMaterialCompiler::Errorf
//

INT FMaterialCompiler::Errorf(const TCHAR* Format,...)
{
	TCHAR	ErrorText[2048];
	GET_VARARGS(ErrorText,ARRAY_COUNT(ErrorText),Format,Format);
	return Error(ErrorText);
}

//
//	FMaterial::GetPropertyType
//

EMaterialCodeType FMaterial::GetPropertyType(EMaterialProperty Property)
{
	switch(Property)
	{
	case MP_EmissiveColor: return MCT_Float3;
	case MP_Opacity: return MCT_Float;
	case MP_OpacityMask: return MCT_Float;
	case MP_Distortion: return MCT_Float2;
	case MP_TwoSidedLightingMask: return MCT_Float3;
	case MP_DiffuseColor: return MCT_Float3;
	case MP_SpecularColor: return MCT_Float3;
	case MP_SpecularPower: return MCT_Float;
	case MP_Normal: return MCT_Float3;
	case MP_SHM: return MCT_Float4;
	};
	return MCT_Unknown;
}

//
//	FChildViewport::IsCtrlDown
//

UBOOL FChildViewport::IsCtrlDown()
{
	return (KeyState(KEY_LeftControl) || KeyState(KEY_RightControl));
}

//
//	FChildViewport::IsShiftDown
//

UBOOL FChildViewport::IsShiftDown()
{
	return (KeyState(KEY_LeftShift) || KeyState(KEY_RightShift));
}

//
//	FChildViewport::IsAltDown
//

UBOOL FChildViewport::IsAltDown()
{
    return (KeyState(KEY_LeftAlt) || KeyState(KEY_RightAlt));
}

//
//	FChildViewport::GetHitProxy
//

HHitProxy* FChildViewport::GetHitProxy(INT X,INT Y)
{
	// Compute a 5x5 test region with the center at (X,Y).

	INT		MinX = X - 2,
			MinY = Y - 2,
			MaxX = X + 2,
			MaxY = Y + 2;

	// Clip the region to the viewport bounds.

	MinX = Max(MinX,0);
	MinY = Max(MinY,0);
	MaxX = Min(MaxX,(INT)GetSizeX() - 1);
	MaxY = Min(MaxY,(INT)GetSizeY() - 1);

	INT	TestSizeX = MaxX - MinX + 1,
		TestSizeY = MaxY - MinY + 1;

	if(TestSizeX > 0 && TestSizeY > 0)
	{
		// Read the hit proxy map from the device.

		TArray<HHitProxy*>	ProxyMap;
		GetHitProxyMap((UINT)MinX,(UINT)MinY,(UINT)MaxX,(UINT)MaxY,ProxyMap);
		check(ProxyMap.Num() == TestSizeX * TestSizeY);

		// Find the hit proxy in the test region with the highest order.

		HHitProxy*	HitProxy = NULL;
		for(INT Y = 0;Y < TestSizeY;Y++)
		{
			for(INT X = 0;X < TestSizeX;X++)
			{
				HHitProxy*	TestProxy = ProxyMap(Y * TestSizeX + X);
				if(TestProxy && (!HitProxy || TestProxy->Order > HitProxy->Order))
					HitProxy = TestProxy;
			}
		}

		return HitProxy;
	}
	else
		return NULL;
}

//
//	UClient::StaticConstructor
//

void UClient::StaticConstructor()
{
	new(GetClass(),TEXT("MinDesiredFrameRate"),	RF_Public)UFloatProperty(CPP_PROPERTY(MinDesiredFrameRate	), TEXT("Display"), CPF_Config );

	new(GetClass(),TEXT("StartupFullscreen"),		RF_Public)UBoolProperty	(CPP_PROPERTY(StartupFullscreen			), TEXT("Display"), CPF_Config );
	new(GetClass(),TEXT("EditorPreviewFullscreen"),	RF_Public)UBoolProperty	(CPP_PROPERTY(EditorPreviewFullscreen	), TEXT("Display"), CPF_Config );
	new(GetClass(),TEXT("StartupResolutionX"),		RF_Public)UIntProperty	(CPP_PROPERTY(StartupResolutionX		), TEXT("Display"), CPF_Config );
	new(GetClass(),TEXT("StartupResolutionY"),		RF_Public)UIntProperty	(CPP_PROPERTY(StartupResolutionY		), TEXT("Display"), CPF_Config );

}

//
//	FRenderInterface::DrawStringCentered
//

INT FRenderInterface::DrawStringCentered(INT StartX,INT StartY,const TCHAR* Text,UFont* Font,const FLinearColor& Color)
{
	INT XL, YL;
	StringSize( Font, XL, YL, Text );

	return DrawString( StartX-(XL/2), StartY, Text, Font, Color );

}

//
//	FRenderInterface::DrawString
//

INT FRenderInterface::DrawString(INT StartX,INT StartY,const TCHAR* Text,UFont* Font,const FLinearColor& Color)
{
	if( !Font )
		return 0;

	// Draw all characters in string.
	INT LineX = 0;
	INT bDrawUnderline = 0;
	INT UnderlineWidth = 0;
	for( INT i=0; Text[i]; i++ )
	{
		INT bUnderlineNext = 0;
		INT Ch = (TCHARU)Font->RemapChar(Text[i]);

		// Handle ampersand underlining.
		if( bDrawUnderline )
			Ch = (TCHARU)Font->RemapChar('_');
		if( Text[i]=='&' )
		{
			if( !Text[i+1] )
				break; 
			if( Text[i+1]!='&' )
			{
				bUnderlineNext = 1;
				Ch = (TCHARU)Font->RemapChar(Text[i+1]);
			}
		}

		// Process character if it's valid.
		if( Ch < Font->Characters.Num() )
		{
			FFontCharacter& Char = Font->Characters(Ch);
			UTexture2D* Tex;
			if( Char.TextureIndex < Font->Textures.Num() && (Tex=Font->Textures(Char.TextureIndex))!=NULL )
			{
				// Compute character width.
				INT CharWidth;
				if( bDrawUnderline )
					CharWidth = Min(UnderlineWidth, Char.USize);
				else
					CharWidth = Char.USize;

				// Prepare for clipping.
				INT X      = LineX + StartX;
				INT Y      = StartY;
				INT CU     = Char.StartU;
				INT CV     = Char.StartV;
				INT CUSize = CharWidth;
				INT CVSize = Char.VSize;

				// Draw.
				DrawTile(
					X,
					Y,
					CUSize,
					CVSize,
					(FLOAT)CU / (FLOAT)Tex->SizeX,
					(FLOAT)CV / (FLOAT)Tex->SizeY,
					(FLOAT)CUSize / (FLOAT)Tex->SizeX,
					(FLOAT)CVSize / (FLOAT)Tex->SizeY,
					Color,
					Tex
					);

				// Update underline status.
				if( bDrawUnderline )
					CharWidth = UnderlineWidth;

				if( !bUnderlineNext )
					LineX += (INT) (CharWidth);
				else
					UnderlineWidth = Char.USize;

				bDrawUnderline = bUnderlineNext;
			}
		}
	}

	return LineX;

}

//
//	FRenderInterface::DrawShadowedString
//

INT FRenderInterface::DrawShadowedString(INT StartX,INT StartY,const TCHAR* Text,UFont* Font,const FLinearColor& Color)
{
	// Draw a shadow of the text offset by 1 pixel in X and Y.

	DrawString(StartX + 1,StartY + 1,Text,Font,FLinearColor::Black);

	// Draw the text.

	return DrawString(StartX,StartY,Text,Font,Color);
}

//
//	FRenderInterface::StringSize
//

void FRenderInterface::StringSize(UFont* Font,INT& XL,INT &YL,const TCHAR* Format,...)
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Format, Format );

	XL = 0;
	YL = 0;

	// Draw all characters in string.
	if( Font )
	{
		for( INT i=0; Text[i]; i++ )
		{
			INT Ch = (TCHARU)Font->RemapChar(Text[i]);

			// Process character if it's valid.
			if( Ch < Font->Characters.Num() )
			{
				FFontCharacter& Char = Font->Characters(Ch);
				UTexture2D* Tex;
				if( Char.TextureIndex < Font->Textures.Num() && (Tex=Font->Textures(Char.TextureIndex))!=NULL )
				{
					XL += Char.USize;
					YL = Max(YL,Char.VSize);
				}
			}
		}
	}
}


//
//  FRawIndexBuffer::Stripify
//

INT FRawIndexBuffer::Stripify()
{
#if !CONSOLE && !__LINUX__

	PrimitiveGroup*	PrimitiveGroups = NULL;
	_WORD			NumPrimitiveGroups = 0;

	SetListsOnly(false);
	GenerateStrips(&Indices(0),Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

	Indices.Empty();
	Indices.Add(PrimitiveGroups->numIndices);

	appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(_WORD));

	delete [] PrimitiveGroups;

	GResourceManager->UpdateResource(this);

	return Indices.Num() - 2;

#else
	return 0;
#endif

}

//
//  FRawIndexBuffer::CacheOptimize
//

void FRawIndexBuffer::CacheOptimize()
{
#if !CONSOLE && !__LINUX__

	PrimitiveGroup*	PrimitiveGroups = NULL;
	_WORD			NumPrimitiveGroups = 0;

	SetListsOnly(true);
	GenerateStrips(&Indices(0),Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

	Indices.Empty();
	Indices.Add(PrimitiveGroups->numIndices);

	appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(_WORD));

	delete [] PrimitiveGroups;

	GResourceManager->UpdateResource(this);
#else
	check(0);
#endif

}

//
//  FRawIndexBuffer operator<<
//

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I)
{
	Ar << I.Indices << I.Size;
	if(Ar.IsLoading())
		GResourceManager->UpdateResource(&I);
	return Ar;
}

//
//	FRawIndexBuffer::GetData
//

void FRawIndexBuffer::GetData(void* Buffer)
{
	appMemcpy(Buffer,&Indices(0),Size);
}

//
//  FRawIndexBuffer32 operator<<
//

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer32& I)
{
	Ar << I.Indices << I.Size;
	if(Ar.IsLoading())
		GResourceManager->UpdateResource(&I);
	return Ar;
}

//
//	FRawIndexBuffer32::GetData
//

void FRawIndexBuffer32::GetData(void* Buffer)
{
	appMemcpy(Buffer,&Indices(0),Size);
}