/*=============================================================================
	UnTex.cpp: Unreal texture loading/saving/processing functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "UnLinker.h"

IMPLEMENT_CLASS(UTexture);
IMPLEMENT_CLASS(UTextureMovie);
IMPLEMENT_CLASS(UTexture2D);
IMPLEMENT_CLASS(UTexture3D);
IMPLEMENT_CLASS(UTextureCube);

IMPLEMENT_RESOURCE_TYPE(FStaticTexture2D);
IMPLEMENT_RESOURCE_TYPE(FStaticTexture3D);
IMPLEMENT_RESOURCE_TYPE(UShadowMap);

//
//	Pixel format information.
//

FPixelFormatInfo	GPixelFormats[] =
{
	// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	PlatformFlags	Supported	EPixelFormat

	{ TEXT("unknown"),			0,			0,			0,			0,			0,				0,				0,				0			},	//	PF_Unknown
	{ TEXT("A32B32G32R32F"),	1,			1,			1,			16,			4,				0,				0,				1			},	//	PF_A32B32G32R32F
	{ TEXT("A8R8G8B8"),			1,			1,			1,			4,			4,				0,				0,				1			},	//	PF_A8R8G8B8
	{ TEXT("G8"),				1,			1,			1,			1,			1,				0,				0,				1			},	//	PF_G8
	{ TEXT("G16"),				1,			1,			1,			2,			1,				0,				0,				1			},	//	PF_G16
	{ TEXT("DXT1"),				4,			4,			1,			8,			3,				0,				0,				1			},	//	PF_DXT1
	{ TEXT("DXT3"),				4,			4,			1,			16,			4,				0,				0,				1			},	//	PF_DXT3
	{ TEXT("DXT5"),				4,			4,			1,			16,			4,				0,				0,				1			},	//	PF_DXT5
	{ TEXT("UYVY"),				2,			1,			1,			4,			4,				0,				0,				1			},	//	PF_UYVY
};

//
//	CalculateImageBytes
//

SIZE_T CalculateImageBytes(UINT SizeX,UINT SizeY,UINT SizeZ,EPixelFormat Format)
{
	if( SizeZ > 0 )
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * (SizeZ / GPixelFormats[Format].BlockSizeZ) * GPixelFormats[Format].BlockBytes;
	else
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockBytes;
}

/*-----------------------------------------------------------------------------
	UTextureMovie implementation.
-----------------------------------------------------------------------------*/

/**
 * Updates the movie texture if necessary by requesting a new frame from the decoder taking into account both
 * game and movie framerate.
 *
 * @param DeltaTime		Time (in seconds) that has passed since the last time this function has been called.
 */
void UTextureMovie::Tick( FLOAT DeltaTime )
{
	if( !Paused && !Stopped )
	{
		TimeIntoMovie		+= DeltaTime;
		TimeSinceLastFrame	+= DeltaTime;

		check(Decoder);

		// @todo movie textures: need to find a good way to always ensure that the resource is created with the dynamic flag set.
		Dynamic = 1;

		// Respect the movie streams frame rate for playback/ frame update.
		if( TimeSinceLastFrame >= (1.f / Decoder->GetFrameRate()) )
		{
			//@todo movie textures: first frame is displayed "twice".
			FLOAT DecoderTime = Decoder->BlockUntilIdle();
			GResourceManager->UpdateResource(this);

			// This can happen if PF_UYVY is not supported as querying for format support can occur after PostLoad 
			// is called for movies that are referenced by the first loaded level.
			if( Format != Decoder->GetFormat() )
				UpdateTextureFormat();
	
			UBOOL EndOfStream = !Decoder->GetFrame( CurrentFrame.GetData() );

			if( EndOfStream )
			{
				if( Looping )
				{
					// Reset the stream, start playback from the beginning and request the next frame.
					Decoder->ResetStream();
					TimeIntoMovie	= 0;
					DecoderTime		= 0;
					Decoder->GetFrame( CurrentFrame.GetData() );
				}
				else
				{
					// No longer update texture.
					Stopped = TRUE;
				}
			}
	
			// Avoid getting out of sync by adjusting TimeSinceLastFrame to take into account difference
			// between where we think the movie should be at and what the decoder thinks.
			TimeSinceLastFrame = TimeIntoMovie - DecoderTime;
		}
	}
}

/**
 * Updates texture format from decoder and re- allocates memory for the intermediate buffer to hold one frame in the decoders texture format.
 */
void UTextureMovie::UpdateTextureFormat()
{
	Format = Decoder->GetFormat();

	// Create a large enough buffer to hold a frame of uncompressed data.
	DWORD Size = (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockBytes;
	CurrentFrame.Empty( Size );
	CurrentFrame.AddZeroed( Size );
}

/**
 * Sets initial default values.
 */
void UTextureMovie::StaticConstructor()
{
	SRGB		= 1;
	UnpackMin	= FPlane(0,0,0,0);
	UnpackMax	= FPlane(1,1,1,1);
}

/**
 * Serializes the compressed movie data.
 *
 * @param Ar	FArchive to serialize RawData with.
 */
void UTextureMovie::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << RawData;
}

/**
 * Postload initialization of movie texture. Creates decoder object and retriever first frame.
 */
void UTextureMovie::PostLoad()
{
	Super::PostLoad();

	// Constructs a new decoder of the appropriate class (set by the UTextureMovieFactory).
	check( Decoder == NULL );
	Decoder = ConstructObject<UCodecMovie>( DecoderClass );

	UBOOL ValidStream = 0;
	if( Decoder )
	{
		// Movie textures imported before RawSize was added need to determine RawSize on load which is slow. This can be avoided
		// by saving 
		if( RawSize == 0 && RawData.Num() == 0 )
		{
			warnf(TEXT("%s needs to be resaved in order for %s to hit fast loading path."),*(GetOutermost()->GetPathName()),*GetPathName());
			RawData.Load();
			RawSize = RawData.Num();
			if( !GIsEditor && GetLinker() )
				RawData.Unload();
		}

		// Newly imported objects won't have a linker till they are saved so we have to play from memory. We also play
		// from memory in the Editor to avoid having an open file handle to an existing package.
		if( !GIsEditor && GetLinker() )
		{
			// Have the decoder open the file itself. RawData does NOT need to be loaded as
			// the data is going to be streamed in from disk.
			ValidStream = Decoder->Open( GetLinker()->Filename, RawData.GetOffset(), RawSize );
		}
		else
		{
			// Never unload RawData as the decoder doesn't duplicate the memory!
			RawData.Load();
			check(RawData.Num());
			ValidStream = Decoder->Open( RawData.GetData(), RawData.Num() );
		}
	}
	else
	{
		ValidStream = 0;
	}

	// The engine always expects a valid decoder so we're going to revert to a fallback decoder
	// if we couldn't associate a decoder with this stream.
	if( !ValidStream )
	{
		delete Decoder;
		Decoder = ConstructObject<UCodecMovieFallback>( UCodecMovieFallback::StaticClass() );
		verify( Decoder->Open( NULL, 0 ) );
	}

	// Update the internal size and format with what the decoder provides. This is not necessarily a power of two.
	check( Decoder );
	SizeX	= Decoder->GetSizeX();
	SizeY	= Decoder->GetSizeY();
	NumMips	= 1;
	UpdateTextureFormat();

	// Retrieve the first frame into CurrentFrame and block till we're done so we have something to upload to the card.
	Decoder->GetFrame( CurrentFrame.GetData() );
	Decoder->BlockUntilIdle();

	Stopped = !AutoPlay;
	Dynamic = 1;
}

/**
 * Copies the current uncompressed movie frame to the passed in Buffer.
 *
 * @param Buffer	Pointer to destination data to upload uncompressed movie frame
 * @param MipIndex	unused
 * @param StrideY	StrideY for destination data.
 */
void UTextureMovie::GetData(void* Buffer,UINT MipIndex,UINT StrideY)
{
	check( Decoder );
	
	DWORD	RowSize = (SizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes,
			NumRows = (SizeY / GPixelFormats[Format].BlockSizeY);

	if( StrideY == RowSize )
	{
		appMemcpy( Buffer, CurrentFrame.GetData(), RowSize * NumRows );
	}
	else
	{
		for( DWORD RowIndex=0; RowIndex<NumRows; RowIndex++ )
		{
			appMemcpy( (BYTE*)Buffer + RowIndex*StrideY, (BYTE*)CurrentFrame.GetData() + RowIndex*RowSize, RowSize );
		}
	}
}

/**
 * PostEditChange - gets called whenever a property is either edited via the Editor or the "set" console command.
 * In this particular case we just make sure that non power of two textures have ClampX/Y set to true as wrapping
 * isn't supported on them.
 *
 * @param PropertyThatChanged	Property that changed
 */
void UTextureMovie::PostEditChange(UProperty* PropertyThatChanged)
{
	// Non power of two textures need to use clamping.
	if( SizeX & (SizeX - 1) )
		ClampX = 1;
	if( SizeY & (SizeY - 1) )
		ClampY = 1;	
}

/**
 * Plays the movie and also unpauses.
 */
void UTextureMovie::execPlay( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Paused	= 0;
	Stopped = 0;
}
IMPLEMENT_FUNCTION(UTextureMovie,INDEX_NONE,execPlay);

/**
 * Pauses the movie.
 */
void UTextureMovie::execPause( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Paused = 1;
}
IMPLEMENT_FUNCTION(UTextureMovie,INDEX_NONE,execPause);

/**
 * Stops movie playback.
 */
void UTextureMovie::execStop( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Stopped				= 1;
	TimeIntoMovie		= 0.f;
	TimeSinceLastFrame	= 0.f;
	// Prepare the stream so we can call "Play" on it again.
	Decoder->ResetStream();
}
IMPLEMENT_FUNCTION(UTextureMovie,INDEX_NONE,execStop);

/**
 * Destroy - gets called by ConditionalDestroy from within destructor. We need to ensure that the decoder 
 * doesn't have any references to RawData before destructing it.
 */
void UTextureMovie::Destroy()
{
	Super::Destroy();

	// We need to make sure that the Decoder object detaches itself from the movie texture in case it
	// decodes directly from memory (UTextureMovie->RawData). Note that ConditionalDestroy only calls
	// the Decoder's Destroy function if it hasn't been already called.
	check( Decoder );
	Decoder->ConditionalDestroy();
}

FString UTextureMovie::GetDesc()
{
	check(Decoder);
	return FString::Printf( TEXT("%dx%d [%s], %.1f sec"), SizeX, SizeY, GPixelFormats[Format].Name, Decoder->GetDuration() );
}

void UTextureMovie::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > SizeX ) ? SizeX : InViewport->GetSizeX() : SizeX*InZoom,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > SizeY ) ? SizeY : InViewport->GetSizeY() : SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		this,
		0
	);
}

FThumbnailDesc UTextureMovie::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : SizeY*InZoom;
	return td;
}

INT UTextureMovie::GetThumbnailLabels( TArray<FString>* InLabels )
{
	check(Decoder);
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%dx%d [%s], %.1f sec"), SizeX, SizeY, GPixelFormats[Format].Name, Decoder->GetDuration() ) );

	return InLabels->Num();
}

/*-----------------------------------------------------------------------------
	FStaticTexture2D implementation.
-----------------------------------------------------------------------------*/

FStaticTexture2D::FStaticTexture2D()
{
	SizeX = 0;
	SizeY = 0;
	Format = PF_Unknown;
	NumMips = 0;
}

void FStaticTexture2D::GetData(void* Buffer,UINT MipIndex,UINT StrideY)
{
	check(Mips.Num() == NumMips);
	check(MipIndex < (UINT)Mips.Num());
#ifndef XBOX
	BYTE*	Dest			= (BYTE*)Buffer;
	UINT	USize			= Align( Mips(MipIndex).SizeX, GPixelFormats[Format].BlockSizeX ),
			VSize			= Align( Mips(MipIndex).SizeY, GPixelFormats[Format].BlockSizeY ),
			RowSize			= (USize / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes,
			NumRows			= (VSize / GPixelFormats[Format].BlockSizeY);

	UINT	FirstColoredMip	= GEngine->DebugManager ? GEngine->DebugManager->FirstColoredMip : MAXINT;

	if( (MipIndex >= FirstColoredMip) && (Format == PF_DXT1 || Format == PF_DXT3 || Format == PF_DXT5) )
	{
		FColor	MipColor( 255, 0, 0, 127 );

		QWORD	ColorPattern	= ((MipColor.R >> 3) << 11) + ((MipColor.G >> 2) << 5) + (MipColor.B >> 3),
				AlphaPattern	= (Format == PF_DXT3) ? 0x7777777777777777 : 0x8081ffffffffffff;
		BYTE*	Dummy			= (BYTE*) Dest;

		if( Format == PF_DXT1 )
		{
			for( UINT Y=0; Y<NumRows; Y++ )
			{
				QWORD* DestData = (QWORD*) Dummy;
				for( UINT X=0; X<RowSize/8; X++ )
				{
					*(DestData++) = ColorPattern;
				}
				Dummy += StrideY;
			}
		}
		else
		{
			for( UINT Y=0; Y<NumRows; Y++ )
			{
				QWORD* DestData = (QWORD*) Dummy;
				for( UINT X=0; X<RowSize/16; X++ )
				{
					*(DestData++) = AlphaPattern;
					*(DestData++) = ColorPattern;
				}
				Dummy += StrideY;
			}
		}
	}
	else
	{
		Mips(MipIndex).Data.Load();
		for( UINT Y=0; Y<NumRows; Y++ )
			appMemcpy(Dest + Y * StrideY,&Mips(MipIndex).Data(Y * RowSize),RowSize);
		Mips(MipIndex).Data.Unload();
	}
#else
	// The cooked data is a 1:1 mapping.
	Mips(MipIndex).Data.Load();
	appMemcpy(Buffer,Mips(MipIndex).Data.GetData(),Mips(MipIndex).Data.Num());
	Mips(MipIndex).Data.Unload();
#endif
}

void FStaticTexture2D::Serialize(FArchive& Ar)
{
	//@legacy: serialize Format as DWORD
	INT Dummy = Format;
	Ar << SizeX << SizeY << Dummy;
	Format = Dummy;

	// Serialize the mipmaps.

	if(Ar.IsLoading())
	{
		Ar << NumMips;
		Mips.Empty(NumMips);
		for(UINT MipIndex = 0;MipIndex < NumMips;MipIndex++)
			Ar << *new(Mips)FStaticMipMap2D;
	}
	else
	{
		Ar << NumMips;
		for(INT MipIndex = 0;MipIndex < Mips.Num();MipIndex++)
			Ar << Mips(MipIndex);
	}

	GResourceManager->UpdateResource(this);
}

/*-----------------------------------------------------------------------------
	UTexture2D implementation.
-----------------------------------------------------------------------------*/

void UTexture2D::StaticConstructor()
{
	SRGB = 1;
	UnpackMin = FPlane(0,0,0,0);
	UnpackMax = FPlane(1,1,1,1);
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	FStaticTexture2D::Serialize(Ar);
}

void UTexture2D::PostLoad()
{
	Super::PostLoad();
	GResourceManager->UpdateResource(this);
}

//@warning:	Do NOT call Compress from within Init as it could cause an infinite recursion.
void UTexture2D::Init(UINT InSizeX,UINT InSizeY,EPixelFormat InFormat)
{
	// Check that the dimensions are powers of two and evenly divisible by the format block size.

	check(!(InSizeX & (InSizeX - 1)));
	check(!(InSizeY & (InSizeY - 1)));
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));

	SizeX = InSizeX;
	SizeY = InSizeY;
	Format = InFormat;
	NumMips = 1;

	// Allocate first mipmap.

	Mips.Empty();
	new(Mips) FStaticMipMap2D(SizeX,SizeY,CalculateImageBytes(SizeX,SizeY,0,(EPixelFormat)Format));

	GResourceManager->UpdateResource(this);
}

void UTexture2D::CreateMips(UINT NumNewMips,UBOOL Generate)
{
	// Remove any existing non-toplevel mipmaps.

	if(Mips.Num() > 1)
		Mips.Remove(1,Mips.Num() - 1);

	// Allocate the new mipmaps.

	for(UINT MipIndex = 1;MipIndex <= NumNewMips;MipIndex++)
	{
		UINT		DestSizeX = Max(GPixelFormats[Format].BlockSizeX,SizeX >> MipIndex),
					DestSizeY = Max(GPixelFormats[Format].BlockSizeY,SizeY >> MipIndex);

		new(Mips) FStaticMipMap2D(
			DestSizeX,
			DestSizeY,
			(DestSizeX / GPixelFormats[Format].BlockSizeX) * (DestSizeY / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockBytes
			);

		if(Generate)
		{
			FStaticMipMap2D*	SourceMip = &Mips(MipIndex - 1);
			FStaticMipMap2D*	DestMip = &Mips(MipIndex);

			UINT	SubSizeX = SourceMip->SizeX / DestMip->SizeX,
					SubSizeY = SourceMip->SizeY / DestMip->SizeY,
					SubShift = appCeilLogTwo(SubSizeX) + appCeilLogTwo(SubSizeY);
			FLOAT	SubScale = 1.f / (SubSizeX + SubSizeY);

			switch(Format)
			{
			case PF_A8R8G8B8:
			{
				FColor*	DestPtr	= (FColor*)&DestMip->Data(0);
				FColor*	SrcPtr = (FColor*)&SourceMip->Data(0);

				if( RGBE )
				{
					for(UINT Y = 0;Y < DestMip->SizeY;Y++)
					{
						for(UINT X = 0;X < DestMip->SizeX;X++)
						{
							FLinearColor HDRColor = FLinearColor(0,0,0,0);

							for(UINT SubX = 0;SubX < SubSizeX;SubX++)
								for(UINT SubY = 0;SubY < SubSizeY;SubY++)
									HDRColor += SrcPtr[SubY * SourceMip->SizeX + SubX].FromRGBE();

							*DestPtr = (HDRColor * SubScale).ToRGBE();

							DestPtr++;
							SrcPtr += 2;
						}

						SrcPtr += SourceMip->SizeX;
					}
				}
				else
				{
					for(UINT Y = 0;Y < DestMip->SizeY;Y++)
					{
						for(UINT X = 0;X < DestMip->SizeX;X++)
						{
							INT	R = 0,
								G = 0,
								B = 0,
								A = 0;

							for(UINT SubX = 0;SubX < SubSizeX;SubX++)
							{
								for(UINT SubY = 0;SubY < SubSizeY;SubY++)
								{
									R += SrcPtr[SubY * SourceMip->SizeX + SubX].R;
									G += SrcPtr[SubY * SourceMip->SizeX + SubX].G;
									B += SrcPtr[SubY * SourceMip->SizeX + SubX].B;
									A += SrcPtr[SubY * SourceMip->SizeX + SubX].A;
								}
							}

							DestPtr->R = R >> SubShift;
							DestPtr->G = G >> SubShift;
							DestPtr->B = B >> SubShift;
							DestPtr->A = A >> SubShift;

							DestPtr++;
							SrcPtr += 2;
						}

						SrcPtr += SourceMip->SizeX;
					}
				}
				break;
			}
			default:
				appErrorf(TEXT("Generating mipmaps for %s format textures isn't supported."),GPixelFormats[Format].Name);
			};
		}
	}

	NumMips = Mips.Num();

	GResourceManager->UpdateResource(this);

}

void UTexture2D::Clear(FColor Color)
{
	switch(Format)
	{
	case PF_A8R8G8B8:
		for(UINT MipIndex = 0;MipIndex < (UINT)Mips.Num();MipIndex++)
			for(UINT TexelIndex = 0;TexelIndex < SizeX * SizeY;TexelIndex++)
				((FColor*)(&Mips(0).Data(0)))[TexelIndex] = Color;
		break;
	default:
		appErrorf(TEXT("Clearing %s format textures isn't supported."),GPixelFormats[Format].Name);
	}

	GResourceManager->UpdateResource(this);

}

FString UTexture2D::GetDesc()
{
	return FString::Printf( TEXT("%dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}

void UTexture2D::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > SizeX ) ? SizeX : InViewport->GetSizeX() : SizeX*InZoom,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > SizeY ) ? SizeY : InViewport->GetSizeY() : SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		this,
		0
	);
}

FThumbnailDesc UTexture2D::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : SizeY*InZoom;
	return td;
}

INT UTexture2D::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name ) );

	return InLabels->Num();
}

/*-----------------------------------------------------------------------------
	FStaticTexture3D implementation.
-----------------------------------------------------------------------------*/

FStaticTexture3D::FStaticTexture3D()
{
	SizeX	= 0;
	SizeY	= 0;
	SizeZ	= 0;
	Format	= PF_Unknown;
	NumMips = 0;
}

void FStaticTexture3D::GetData(void* Buffer,UINT MipIndex,UINT StrideY,UINT StrideZ)
{
	check(Mips.Num() == NumMips);
	check(MipIndex < (UINT)Mips.Num());
#ifndef XBOX
	Mips(MipIndex).Data.Load();

	BYTE*	Dest			= (BYTE*)Buffer;
	UINT	USize			= Align( Mips(MipIndex).SizeX, GPixelFormats[Format].BlockSizeX ),
			VSize			= Align( Mips(MipIndex).SizeY, GPixelFormats[Format].BlockSizeY ),
			WSize			= Align( Mips(MipIndex).SizeZ, GPixelFormats[Format].BlockSizeZ ),
			RowSize			= (USize / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes,
			NumRows			= (VSize / GPixelFormats[Format].BlockSizeY),
			NumSlices		= (WSize / GPixelFormats[Format].BlockSizeZ);

	INT NumBytes = Mips(MipIndex).Data.Num();

	for( UINT Z=0; Z<NumSlices; Z++ )
		for( UINT Y=0; Y<NumRows; Y++ )
			appMemcpy(Dest + Y * StrideY + Z * StrideZ,&Mips(MipIndex).Data(Y * RowSize + Z * NumRows * RowSize),RowSize);

	Mips(MipIndex).Data.Unload();
#else
	// The cooked data is a 1:1 mapping.
	Mips(MipIndex).Data.Load();
	appMemcpy(Buffer,Mips(MipIndex).Data.GetData(),Mips(MipIndex).Data.Num());
	Mips(MipIndex).Data.Unload();
#endif
}

void FStaticTexture3D::Serialize(FArchive& Ar)
{
	//@legacy: serialize Format as DWORD
	INT Dummy = Format;
	Ar << SizeX << SizeY << SizeZ << Dummy;
	Format = Dummy;

	// Serialize the mipmaps.

	if(Ar.IsLoading())
	{
		Ar << NumMips;
		Mips.Empty(NumMips);
		for(UINT MipIndex = 0;MipIndex < NumMips;MipIndex++)
			Ar << *new(Mips)FStaticMipMap3D;
	}
	else
	{
		Ar << NumMips;
		for(INT MipIndex = 0;MipIndex < Mips.Num();MipIndex++)
			Ar << Mips(MipIndex);
	}

	GResourceManager->UpdateResource(this);
}

/*-----------------------------------------------------------------------------
	UTexture3D implementation.
-----------------------------------------------------------------------------*/

void UTexture3D::StaticConstructor()
{
	SRGB = 1;
	UnpackMin = FPlane(0,0,0,0);
	UnpackMax = FPlane(1,1,1,1);
}

void UTexture3D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	FStaticTexture3D::Serialize(Ar);
}

void UTexture3D::PostLoad()
{
	Super::PostLoad();
	GResourceManager->UpdateResource(this);
}

//	Do NOT call Compress from within Init as it could cause an infinite recursion.
void UTexture3D::Init(UINT InSizeX,UINT InSizeY,UINT InSizeZ,EPixelFormat InFormat)
{
	// Check that the dimensions are powers of two and evenly divisible by the format block size.

	check(!(InSizeX & (InSizeX - 1)));
	check(!(InSizeY & (InSizeY - 1)));
	check(!(InSizeZ & (InSizeZ - 1)));
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));
	check(!(InSizeZ % GPixelFormats[InFormat].BlockSizeZ));

	SizeX	= InSizeX;
	SizeY	= InSizeY;
	SizeZ	= InSizeZ;
	Format	= InFormat;
	NumMips = 1;

	// Allocate first mipmap.

	Mips.Empty();
	new(Mips) FStaticMipMap3D(SizeX,SizeY,SizeZ,CalculateImageBytes(SizeX,SizeY,SizeZ,(EPixelFormat)Format));

	GResourceManager->UpdateResource(this);

}

FString UTexture3D::GetDesc()
{
	return FString::Printf( TEXT("%dx%dx%d [%s]"), SizeX, SizeY, SizeZ, GPixelFormats[Format].Name );
}

void UTexture3D::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > SizeX ) ? SizeX : InViewport->GetSizeX() : SizeX*InZoom,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > SizeY ) ? SizeY : InViewport->GetSizeY() : SizeY*InZoom,
		0.0f,
		0.0f,
		appFractional( appSeconds() / 10.f ),
		1.0f,
		1.0f,
		FLinearColor::White,
		this,
		0
	);
}

FThumbnailDesc UTexture3D::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : SizeY*InZoom;
	return td;
}

INT UTexture3D::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%dx%dx%d [%s]"), SizeX, SizeY, SizeZ, GPixelFormats[Format].Name ) );

	return InLabels->Num();
}

/*-----------------------------------------------------------------------------
	UTextureCube implementation.
-----------------------------------------------------------------------------*/

void UTextureCube::GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY)
{
	if(Valid && Faces[FaceIndex])
		Faces[FaceIndex]->GetData(Buffer,MipIndex,StrideY);
}

void UTextureCube::StaticConstructor()
{
	SRGB = 1;
	UnpackMin = FPlane(0,0,0,0);
	UnpackMax = FPlane(1,1,1,1);
}

void UTextureCube::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << SizeX << SizeY << Format << NumMips;
}

void UTextureCube::PostLoad()
{
	Super::PostLoad();

	UTexture2D*	FirstTexture = NULL;
	Valid = 1;
	for(UINT FaceIndex = 0;FaceIndex < 6;FaceIndex++)
	{
		if(!FirstTexture)
			FirstTexture = Faces[FaceIndex];
		else if(Faces[FaceIndex])
		{
			if(Faces[FaceIndex]->SizeX != FirstTexture->SizeX || Faces[FaceIndex]->SizeY != FirstTexture->SizeY || Faces[FaceIndex]->NumMips != FirstTexture->NumMips || Faces[FaceIndex]->Format != FirstTexture->Format)
				Valid = 0;
		}
	}

	if(FirstTexture && Valid)
	{
		SizeX = FirstTexture->SizeX;
		SizeY = FirstTexture->SizeY;
		Format = FirstTexture->Format;
		NumMips = FirstTexture->NumMips;
	}
	else
	{
		SizeX = 1;
		SizeY = 1;
		NumMips = 1;
		Valid = 0;
	}

	GResourceManager->UpdateResource(this);

}

void UTextureCube::PostEditChange(UProperty* PropertyThatChanged)
{
	PostLoad();
	Super::PostEditChange(PropertyThatChanged);
}

FTextureBase* UTextureCube::GetTexture()
{
	if(Valid)
		return this;
	else
	{
		PostLoad();

		if(!Valid)
			return LoadObject<UTexture2D>(NULL,TEXT("Engine.DefaultTexture"),NULL,LOAD_NoFail,NULL);
		else
			return this;
	}
}

FString UTextureCube::GetDesc()
{
	return FString::Printf( TEXT("Cube: %dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}

void UTextureCube::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	if( Faces[0] )
		InRI->DrawTile
		(
			InX,
			InY,
			InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > SizeX ) ? SizeX : InViewport->GetSizeX() : SizeX*InZoom,
			InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > SizeY ) ? SizeY : InViewport->GetSizeY() : SizeY*InZoom,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			FLinearColor::White,
			Faces[0],
			0
		);
}

FThumbnailDesc UTextureCube::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : SizeY*InZoom;
	return td;
}

INT UTextureCube::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( FString::Printf( TEXT("Cube: %s"), GetName() ) );
	new( *InLabels )FString( FString::Printf( TEXT("%dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name ) );

	return InLabels->Num();
}


/*-----------------------------------------------------------------------------
	UShadowMap implementation.
-----------------------------------------------------------------------------*/

/**
 * Initializes the texture and allocates room in SourceArt array for visibility and coverage data. Please
 * note that the texture is not ready for rendering at this point as there aren't ANY miplevels yet.
 *
 * @param	Size	Width/ height of lightmap
 * @param	Light	Light component to associate with texture
 * @return			Offset into SourceArt where coverage information is expected.
 */
UINT UShadowMap::Init( UINT InSize, ULightComponent* InLight )
{
	SizeX	= InSize;
	SizeY	= InSize;
	Format	= PF_G8;
	NumMips = 0;

	Light	= InLight;

	SourceArt.Empty();
	// We also store coverage information as a byte per pixel after the visibility information so we need to allocate twice as much data.
	SourceArt.AddZeroed( SizeX * (SizeY * 2) ); 

	return SizeX * SizeY; // CoverageOffset
}

/**
 * Serializes the lightmap data.
 *
 * @param Ar	FArchive to serialize object with.
 */
void UShadowMap::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	FStaticTexture2D::Serialize(Ar);
}

/**
 * Downsample the passed in source coverage and visibility information. The filter used is a simple box
 * filter albeit only taking into account covered samples.
 *
 * @param	SrcCoverage		Pointer to the source coverage data
 * @param	SrcVisibility	Pointer to the source visibility data
 * @param	DstCoverage		Pointer to the destination coverage data
 * @param	DstVisibility	Pointer to the destination visibility data
 * @param	DstDimension	Width and height of the destination image
 */
void UShadowMap::DownSample( BYTE* SrcCoverage, BYTE* SrcVisibility, BYTE* DstCoverage, BYTE* DstVisibility, UINT DstDimension )
{
	for( UINT Y=0; Y<DstDimension; Y++ )
	{
		for( UINT X=0; X<DstDimension; X++ )
		{
			// Number of pixels coveraged in quad.
			UINT Coverage	=	SrcCoverage[ (2*X)   + (2*Y)   * (2*DstDimension) ]
							+	SrcCoverage[ (2*X)   + (2*Y+1) * (2*DstDimension) ]
							+	SrcCoverage[ (2*X+1) + (2*Y)   * (2*DstDimension) ]
							+	SrcCoverage[ (2*X+1) + (2*Y+1) * (2*DstDimension) ];

			// Combined sum of covered pixels.
			UINT Combined	= 	SrcCoverage[ (2*X)   + (2*Y)   * (2*DstDimension) ] * SrcVisibility[ (2*X)   + (2*Y)   * (2*DstDimension) ]
							+	SrcCoverage[ (2*X)   + (2*Y+1) * (2*DstDimension) ] * SrcVisibility[ (2*X)   + (2*Y+1) * (2*DstDimension) ]
							+	SrcCoverage[ (2*X+1) + (2*Y)   * (2*DstDimension) ] * SrcVisibility[ (2*X+1) + (2*Y)   * (2*DstDimension) ]
							+	SrcCoverage[ (2*X+1) + (2*Y+1) * (2*DstDimension) ] * SrcVisibility[ (2*X+1) + (2*Y+1) * (2*DstDimension) ];

			// Visibility == weighted average of covered pixels.
			DstVisibility[ X + Y * DstDimension ] = Coverage ? appRound((FLOAT)Combined / Coverage) : 0;
			// Only propagate 0/1 as we use the sum above to determine number of covered pixels.
			DstCoverage  [ X + Y * DstDimension ] = Coverage ? 1 : 0;
		}
	}
}

/**
 * Upsample the passed in source coverage and visibility information and fill in uncovered areas in the
 * destination image. The source image has full coverage and we fill in uncovered areas in the destination
 * by looking at the source. Please note that this function does NOT change any already covered pixels.
 *
 * @param	SrcCoverage		Pointer to the source coverage data
 * @param	SrcVisibility	Pointer to the source visibility data
 * @param	DstCoverage		Pointer to the destination coverage data
 * @param	DstVisibility	Pointer to the destination visibility data
 * @param	DstDimension	Width and height of the destination image
 */
void UShadowMap::UpSample( BYTE* SrcCoverage, BYTE* SrcVisibility, BYTE* DstCoverage, BYTE* DstVisibility, UINT DstDimension )
{
	UINT SrcDimension = DstDimension / 2;
	for( UINT Y=0; Y<DstDimension; Y++ )
	{
		for( UINT X=0; X<DstDimension; X++ )
		{
			if( DstCoverage[ X + Y * DstDimension ] == 0 )
			{
				// Source area is always covered.
				check( SrcCoverage  [ X/2 + Y/2 * SrcDimension ] );

				// Smear/ upsample source region across destination one.
				DstCoverage  [ X + Y * DstDimension ] = 1;
				DstVisibility[ X + Y * DstDimension ] = SrcVisibility[ X/2 + Y/2 * SrcDimension ];
			}
		}
	}
}

/**
 * Creates one or more miplevels based on visibility and potentially coverage information stored in the
 * SourceArt array and cleans out SourceArt afterwards as it doesn't make sense to store three times the
 * data necessary for something that can easily be recalculated by rebuilding lighting.
 *
 * @param	CreateMips	Whether to create miplevels or not
 */
void UShadowMap::CreateFromSourceArt( UBOOL CreateMips )
{
	check( SizeX == SizeY );
	check( Format == PF_G8 );
	check( SourceArt.Num() );
	
	UINT	MipIndex		= 0;
	UINT	MipSize			= SizeX;
	BYTE*	MipCoverage[20] = { 0 }; // 2^(20-1) texture size seems like a reasonable limitation for now

	check( appCeilLogTwo( MipSize ) < ARRAY_COUNT(MipCoverage) );

	// Start from scratch, create toplevel mipmap and copy base visibility data over from source art.
	Mips.Empty();
	new(Mips) FStaticMipMap2D(MipSize,MipSize,CalculateImageBytes(MipSize,MipSize,0,PF_G8));
	appMemcpy( Mips(0).Data.GetData(), SourceArt.GetData(), CalculateImageBytes(MipSize,MipSize,0,PF_G8) );

	// Copy base coverage information over from source art array into newly allocated coverage info.
	MipCoverage[0] = (BYTE*) appMalloc( CalculateImageBytes(MipSize,MipSize,0,PF_G8) );
	appMemcpy( MipCoverage[0], (BYTE*) SourceArt.GetData() + MipSize * MipSize, MipSize * MipSize );

	if( CreateMips )
	{
		// Downsample and create miplevels.
		while( MipSize > 1 )
		{
			MipSize /= 2;
			MipIndex++;

			new(Mips) FStaticMipMap2D(MipSize,MipSize,CalculateImageBytes(MipSize,MipSize,0,PF_G8));
			MipCoverage[MipIndex] = (BYTE*) appMalloc( CalculateImageBytes(MipSize,MipSize,0,PF_G8) );

			DownSample( MipCoverage[MipIndex-1], (BYTE*) Mips(MipIndex-1).Data.GetData(), MipCoverage[MipIndex], (BYTE*) Mips(MipIndex).Data.GetData(), MipSize );
		}

		// "Upsample" to fill holes in the coverage.
		while( MipSize < SizeX )
		{
			check( MipIndex > 0 );
			UpSample( MipCoverage[MipIndex], (BYTE*) Mips(MipIndex).Data.GetData(), MipCoverage[MipIndex-1], (BYTE*) Mips(MipIndex-1).Data.GetData(), MipSize * 2 );
			MipIndex--;
			MipSize *= 2;
		}

		// And finally free intermediate data.
		for( INT i=0; i<ARRAY_COUNT(MipCoverage); i++ )
		{
			appFree( MipCoverage[i] );
		}
	}

	// We don't keep the source art around as it's just a waste of disk space and is easily re-generated by rebuilding lighting.
	SourceArt.Empty();

	// Update resource information.
	NumMips = Mips.Num();
	GResourceManager->UpdateResource(this);
}

// Editor thumbnail viewer interface. Unfortunately this is duplicated across all UTexture implementations.

FString UShadowMap::GetDesc()
{
	return FString::Printf( TEXT("Lightmap: %dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}
void UShadowMap::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeX() > SizeX ) ? SizeX : InViewport->GetSizeX() : SizeX*InZoom,
		InFixedSz ? InFixedSz : ( InZoom == -1 ) ? ( InViewport->GetSizeY() > SizeY ) ? SizeY : InViewport->GetSizeY() : SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		this,
		0
	);
}
FThumbnailDesc UShadowMap::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;
	td.Width = InFixedSz ? InFixedSz : SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : SizeY*InZoom;
	return td;
}
INT UShadowMap::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name ) );
	return InLabels->Num();
}

/*-----------------------------------------------------------------------------
	UTexture implementation.
-----------------------------------------------------------------------------*/

void UTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);	
	Ar << SourceArt;
}

void UTexture::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	// Determine whether any property that requires recompression of the texture has changed.
	UBOOL RequiresRecompression = 0;
	if( PropertyThatChanged )
	{
		const TCHAR* PropertyName = PropertyThatChanged->GetName();

		if( appStricmp( PropertyName, TEXT("RGBE")							) == 0
		||	appStricmp( PropertyName, TEXT("CompressionNoAlpha")			) == 0
		||	appStricmp( PropertyName, TEXT("CompressionNone")				) == 0
		||	appStricmp( PropertyName, TEXT("CompressionNoMipmaps")			) == 0
		||	appStricmp( PropertyName, TEXT("CompressionFullDynamicRange")	) == 0
		||	appStricmp( PropertyName, TEXT("CompressionSettings")			) == 0
		)
		{
			RequiresRecompression = 1;
		}
	}
	else
	{
		RequiresRecompression = 1;
	}

	// Only compress when we really need to to avoid lag when level designers/ artists manipulate properties like clamping in the editor.
	if( RequiresRecompression )
		Compress();

	// Update the resource with the resource manager.
	if(GetTexture())
		GResourceManager->UpdateResource(GetTexture());

	// Update materials that sample this texture.
	for(TObjectIterator<UMaterialExpressionTextureSample> It;It;++It)
		if(It->Texture == this)
			It->PostEditChange(NULL);
}

/**
 * @todo: PostLoad currently ensures that RGBE is correctly set to work around a bug in older versions of the engine.
 */
void UTexture::PostLoad()
{
	Super::PostLoad();

	// High dynamic range textures are currently always stored as RGBE (shared exponent) textures.
	// We explicitely set this here as older versions of the engine didn't correctly update the RGBE field.
	RGBE = (CompressionSettings == TC_HighDynamicRange);
}

