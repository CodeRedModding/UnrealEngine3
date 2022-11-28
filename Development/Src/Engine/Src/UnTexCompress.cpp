/*=============================================================================
	UnTexCompress.cpp: Unreal texture compression functions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

#if _MSC_VER && !CONSOLE
#pragma pack (push,4)
#include "nvtt\nvtt.h"
#include "nvtt\nvtt_wrapper.h"
#pragma pack (pop)
#endif


/*-----------------------------------------------------------------------------
	DXT functions.
-----------------------------------------------------------------------------*/

// Callbacks required by nvDXT library.
#if _MSC_VER && !CONSOLE
//static HRESULT CompressionCallback(void * Data, INT MipIndex, DWORD NumBytes, INT Width, INT Height, void* UserData )
//{
//	UTexture2D* Texture2D = Cast<UTexture2D>((UObject*)UserData);
//	UTexture3D* Texture3D = Cast<UTexture3D>((UObject*)UserData);
//	if( Texture2D )
//	{
//		new(Texture2D->Mips)FStaticMipMap2D(Max<UINT>(Texture2D->SizeX >> MipIndex,4),Max<UINT>(Texture2D->SizeY >> MipIndex,4),NumBytes);
//		appMemcpy(&Texture2D->Mips(MipIndex).Data(0),Data,NumBytes);
//	}
//	else
//	if( Texture3D )
//	{
//		appErrorf(TEXT("nvDXT v6.67 doesn't correctly handle creating compressed volume textures and this codepath should never have been called!"));
//		new(Texture3D->Mips)FStaticMipMap3D(Max<UINT>(Texture3D->SizeX >> MipIndex,4),Max<UINT>(Texture3D->SizeY >> MipIndex,4),Max<UINT>(Texture3D->SizeZ >> MipIndex,1),NumBytes);
//		appMemcpy(&Texture3D->Mips(MipIndex).Data(0),Data,NumBytes);
//	}
//	return 0;
//}
//void WriteDTXnFile(DWORD Count, void* Buffer, void* UserData){}
//void ReadDTXnFile(DWORD Count, void* Buffer, void* UserData){}

class CompressDXTError
    : public nvtt::ErrorHandler
{
public:
    virtual void error( nvtt::Error e )
    {
        FString     str = "CompressDXTError ";
        switch ( e )
        {
        case nvtt::Error_Unknown : str += "Error_Unknown";
        case nvtt::Error_InvalidInput: str += "Error_InvalidInput";
        case nvtt::Error_UnsupportedFeature: str += "Error_UnsupportedFeature";
        case nvtt::Error_CudaError: str += "Error_CudaError";
        case nvtt::Error_FileOpen: str += "Error_FileOpen";
        case nvtt::Error_FileWrite: str += "Error_FileWrite";
        }

        warnf( *str );
    }
};

class CompressDXTOutput
    : public nvtt::OutputHandler
{
public:
    CompressDXTOutput( UTexture2D * pTexture )
        : m_pTexture( pTexture )
        , m_pCurMipMap( NULL )
        , m_iCurMipLevel( 0 )
        , m_iCurDataSize( 0 )
    {
    }

    virtual ~CompressDXTOutput(){}

    virtual void beginImage( int size, int width, int height, int depth, int face, int miplevel )
    {
        if ( m_pTexture != NULL )
        {
            FStaticMipMap2D * MipMap = new(m_pTexture->Mips)FStaticMipMap2D( Max<UINT>( m_pTexture->SizeX >> miplevel, 4 ), Max<UINT>( m_pTexture->SizeY >> miplevel, 4 ), size );

            m_pCurMipMap   = MipMap;
            m_iCurDataSize = 0;
        }
    }

    virtual bool writeData( const void * data, int size )
    {
        if ( m_pCurMipMap != NULL )
        {
            appMemcpy( (BYTE *)&m_pCurMipMap->Data(0) + m_iCurDataSize, data, size );

            m_iCurDataSize += size;
        }

        return true;
    }

private:
    UTexture2D *        m_pTexture;
    int                 m_iCurMipLevel;
    FStaticMipMap2D *   m_pCurMipMap;
    unsigned int        m_iCurDataSize;
};

#endif

/**
 * Shared compression functionality. 
 */
void UTexture::Compress()
{
	// High dynamic range textures are currently always stored as RGBE (shared exponent) textures.
	RGBE = (CompressionSettings == TC_HighDynamicRange);
}

void UTexture2D::Compress()
{
#if _MSC_VER && !CONSOLE
	Super::Compress();

	switch( Format )
	{
	case PF_A8R8G8B8:
	case PF_G8:
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
		// Handled formats, break.
		break;

	case PF_Unknown:
	case PF_A32B32G32R32F:
	case PF_G16:
	default:
		// Unhandled, return.
		return;
	}

	// Load lazy loaders.
	SourceArt.Load();
	Mips(0).Data.Load();

	// Return if no source art is present (maybe old package).
	if( !SourceArt.Num() )
		return;

	// Decompress source art.
	FPNGHelper PNG;
	PNG.InitCompressed( &SourceArt(0), SourceArt.Num(), SizeX, SizeY );
	TArray<BYTE> RawData = PNG.GetRawData();

	// Unload source art as we have raw uncompressed data now.
	SourceArt.Unload();

	// Don't compress textures smaller than DXT blocksize.
	if( SizeX < 4 || SizeY < 4 )
		CompressionNone = 1;

	// Displacement maps get stored as PF_G8
	if( CompressionSettings == TC_Displacementmap )
	{
		Init(SizeX,SizeY,PF_G8);
		PostLoad();
		
		FColor* RawColor	= (FColor*) &RawData(0);
		BYTE*	DestColor	= &Mips(0).Data(0);

		for( UINT i=0; i<SizeX * SizeY; i++ )
			*(DestColor++)	= (RawColor++)->A;
	}
	// Grayscale textures are stored uncompressed.
	else
	if( CompressionSettings == TC_Grayscale )
	{
		Init(SizeX,SizeY,PF_G8);
		PostLoad();
		
		FColor* RawColor	= (FColor*) &RawData(0);
		BYTE*	DestColor	= &Mips(0).Data(0);

		for( UINT i=0; i<SizeX * SizeY; i++ )
			*(DestColor++) = (RawColor++)->R;

		//@todo compression: need to support mipmaps
	}
	// Certain textures (icons in Editor) need to be accessed by code so we can't compress them.
	else
	if( CompressionNone || (CompressionSettings == TC_HighDynamicRange && CompressionFullDynamicRange ) )
	{
		Init(SizeX,SizeY,PF_A8R8G8B8);
		PostLoad();
		
		check( Mips(0).Data.Num() == RawData.Num() );
		appMemcpy( &Mips(0).Data(0), &RawData(0), RawData.Num() );

		if( !CompressionNoMipmaps )
			CreateMips(Max(appCeilLogTwo(SizeX),appCeilLogTwo(SizeY)),1);
	}
	// Regular textures.
	else
	{
		UBOOL	Opaque			= 1,
				FreeSourceData	= 0;
		FColor*	SourceData		= (FColor*) &RawData(0);

		// Artists sometimes have alpha channel in source art though don't want to use it.
		if( ! (CompressionNoAlpha || CompressionSettings == TC_Normalmap) )
		{
			// Figure out whether texture is opaque or not.
			FColor*	Color = SourceData;
			for( UINT y=0; y<SizeY; y++ )
				for( UINT x=0; x<SizeX; x++ )
					if( (Color++)->A != 255 )
					{
						Opaque = 0;
						break;
					}
		}

		// We need to fiddle with the exponent for RGBE textures.
		if( CompressionSettings == TC_HighDynamicRange && RGBE )
		{
			FreeSourceData	= 1;
			SourceData		= new FColor[SizeY*SizeX];
			appMemcpy( SourceData, &RawData(0), SizeY * SizeX * sizeof(FColor) );

			// Clamp exponent to -8, 7 range, translate into 0..15 and shift into most significant bits so compressor doesn't throw the data away.
			FColor*	Color = SourceData;
			for( UINT y=0; y<SizeY; y++ )
			{
				for( UINT x=0; x<SizeX; x++ )
				{
					Color->A = (Clamp(Color->A - 128, -8, 7) + 8) * 16;
					Color++;
				}
			}
		}

		// DXT1 if opaque (or override) and DXT5 otherwise. DXT3 is only suited for masked textures though DXT5 works fine for this purpose as well.
		EPixelFormat PixelFormat = Opaque ? PF_DXT1 : PF_DXT5;
		
		// DXT3's explicit 4 bit alpha works well with RGBE textures as we can limit the exponent to 4 bit.
		if( RGBE )
			PixelFormat = PF_DXT3;

		UBOOL	IsNormalMap = (CompressionSettings == TC_Normalmap) || (CompressionSettings == TC_NormalmapAlpha);

		nvtt::Format TextureFormat;
		if ( PixelFormat == PF_DXT1 )
		{
			if ( CompressionNoAlpha )
			{
				TextureFormat = nvtt::Format_DXT1;
			}
			else
			{
				TextureFormat = nvtt::Format_DXT1a;
			}
		}
		else if ( PixelFormat == PF_DXT3 )
		{
			TextureFormat = nvtt::Format_DXT3;
		}
		else
		{
			TextureFormat = nvtt::Format_DXT5;
		}

		// Start with a clean plate.
		Mips.Empty();
        Format = PixelFormat;

		nvtt::InputOptions		nvttIOptions;
		nvttIOptions.setTextureLayout( nvtt::TextureType_2D, SizeX, SizeY );
		nvttIOptions.setFormat( nvtt::InputFormat_BGRA_8UB );

        if ( IsNormalMap )
        {
            nvttIOptions.setNormalMap( true );   
            nvttIOptions.setNormalizeMipmaps( true );
        }
        else
        {
            nvttIOptions.setNormalMap( false );
            nvttIOptions.setNormalizeMipmaps( false );
        }
        nvttIOptions.setWrapMode( nvtt::WrapMode_Clamp );
		nvttIOptions.setMipmapGeneration( CompressionNoMipmaps || RGBE ? false : true );
		nvttIOptions.setMipmapFilter( nvtt::MipmapFilter_Triangle );
		nvttIOptions.setConvertToNormalMap( false ); // ?这个方法！！！

		if ( SRGB ) nvttIOptions.setGamma( 2.2f, 2.2f );
		nvttIOptions.setMipmapData( SourceData, SizeX, SizeY );

		CompressDXTOutput		nvttOutputHandler( this );
		CompressDXTError		nvttOutputError;
		nvtt::OutputOptions		nvttOOptions;
		nvttOOptions.setOutputHandler( &nvttOutputHandler );
		nvttOOptions.setErrorHandler( &nvttOutputError );

		nvtt::CompressionOptions nvttOptions;
		nvttOptions.setFormat( TextureFormat );
        nvttOptions.setQuality( nvtt::Quality_Normal );

		nvtt::Compressor		nvttCompressor;

		if ( !nvttCompressor.process( nvttIOptions, nvttOptions, nvttOOptions ) )
		{
			warnf( TEXT("Texture compressor failing") );
		}

		if( FreeSourceData )
			delete [] SourceData;
	}

	NumMips = Mips.Num();
	GResourceManager->UpdateResource( this );
#endif
}

void UTexture3D::Compress()
{
#if 0
#if _MSC_VER && !CONSOLE
	Super::Compress();

	switch( Format )
	{
	case PF_A8R8G8B8:
	case PF_G8:
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
		// Handled formats, break.
		break;

	case PF_Unknown:
	case PF_A32B32G32R32F:
	case PF_G16:
	default:
		// Unhandled, return.
		return;
	}

	// Load lazy loaders.
	SourceArt.Load();
	Mips(0).Data.Load();

	// Return if no source art is present (maybe old package).
	if( !SourceArt.Num() )
		return;

	// Decompress source art.
	FPNGHelper PNG;
	PNG.InitCompressed( &SourceArt(0), SourceArt.Num(), SizeX, SizeY * SizeZ );
	TArray<BYTE> RawData = PNG.GetRawData();

	// Unload source art as we have raw uncompressed data now.
	SourceArt.Unload();

	// Grayscale textures are stored uncompressed.
	if( CompressionSettings == TC_Grayscale )
	{
		Init(SizeX,SizeY,SizeZ,PF_G8);
		PostLoad();
		
		FColor* RawColor	= (FColor*) &RawData(0);
		BYTE*	DestColor	= &Mips(0).Data(0);

		check( RawData.Num() == SizeX * SizeY * SizeZ * 4 );
		for( UINT i=0; i<SizeX * SizeY * SizeZ; i++ )
			*(DestColor++) = (RawColor++)->R;

		//@todo compression: need to support mipmaps
	}
	// Certain textures (icons in Editor) need to be accessed by code so we can't compress them.
	else
#if 0
	if( CompressionNone )
#else
	// Temporary workaround for nvDXT 6.67
	if( 1 )
#endif
	{
		Init(SizeX,SizeY,SizeZ,PF_A8R8G8B8);
		PostLoad();

		check( RawData.Num() == SizeX * SizeY * SizeZ * 4 );
		appMemcpy( &Mips(0).Data(0), &RawData(0), RawData.Num() );

		//@todo compression: need to support mipmaps
	}
	// Regular textures.
	else
	{
		UBOOL	Opaque			= 1;
		FColor*	SourceData		= (FColor*) &RawData(0);

		// Artists sometimes have alpha channel in source art though don't want to use it.
		if( !CompressionNoAlpha )
		{
			// Figure out whether texture is opaque or not.
			FColor*	Color = SourceData;
			for( UINT z=0; z<SizeZ; z++ )
				for( UINT y=0; y<SizeY; y++ )
					for( UINT x=0; x<SizeX; x++ )
						if( (Color++)->A != 255 )
						{
							Opaque = 0;
							break;
						}
		}

		// DXT1 if opaque (or override) and DXT5 otherwise. DXT3 is only suited for masked textures though DXT5 works fine for this purpose as well.
		EPixelFormat	PixelFormat		= Opaque ? PF_DXT1 : PF_DXT5;	
		TextureFormats	TextureFormat	= PixelFormat == PF_DXT1 ? kDXT1 : PixelFormat == PF_DXT3 ? kDXT3 : kDXT5;
		
		// Start with a clean plate.
		Mips.Empty();

		// Constructor fills in default data for CompressionOptions.
		CompressionOptions nvOptions; 
		nvOptions.MipMapType		= CompressionNoMipmaps ? dNoMipMaps : dGenerateMipMaps;
		nvOptions.MIPFilterType		= kMIPFilterBox;
		nvOptions.TextureType		= kTextureTypeVolume;
		nvOptions.TextureFormat		= TextureFormat;
		nvOptions.normalMapType		= kColorTextureMap;
		nvOptions.user_data			= this;

		Format						= PixelFormat;

		// Compress...
		nvDXTcompressVolume(
			(BYTE*)SourceData,		// src
			SizeX,					// width
			SizeY,					// height
			SizeZ,					// depth
			SizeX * 4,				// pitch
			&nvOptions,				// compression options
			4,						// color components
			CompressionCallback		// callback
			);

		NumMips			= Mips.Num();
	}

	GResourceManager->UpdateResource(this);
#endif

#endif
}

// The below is only needed for compressing SHM's mipmaps invididually and will be cleaned up at some point.

//
//	CompressionCallback2
//

#if _MSC_VER && !CONSOLE
static HRESULT CompressionCallback2(void * Data, INT MipIndex, DWORD NumBytes, INT Width, INT Height, void* UserData )
{
	TArray<BYTE>* DestData = (TArray<BYTE>*)UserData;
	check(DestData);

	DestData->Add(NumBytes);
	appMemcpy(&(*DestData)(0),Data,NumBytes);

	return 0;
}

class CompressDXTNoMipmap
    : public nvtt::OutputHandler
{
public:
    CompressDXTNoMipmap( TArray<BYTE> * pData )
        : m_pData( pData )
        , m_iCurDataSize( 0 )
    {
    }

    virtual ~CompressDXTNoMipmap(){}

    virtual void beginImage( int size, int width, int height, int depth, int face, int miplevel )
    {
        if ( m_pData != NULL )
        {
            m_pData->AddZeroed( size );

            m_iCurDataSize = 0;
        }
    }

    virtual bool writeData( const void * data, int size )
    {
        if ( m_pData != NULL )
        {
            appMemcpy( (BYTE *)&(*m_pData)(0) + m_iCurDataSize, data, size );

            m_iCurDataSize += size;
        }

        return true;
    }

private:
    TArray<BYTE> *      m_pData;
    unsigned int        m_iCurDataSize;
};

#endif

//
//	DXTCompress
//

void DXTCompress(FColor* SrcData,INT Width,INT Height,EPixelFormat DestFormat,TArray<BYTE>& DestData)
{
#if _MSC_VER && !CONSOLE
	// Only compresses to DXTx
	nvtt::Format TextureFormat;
	switch( DestFormat )
	{
	case PF_DXT1:
		TextureFormat = nvtt::Format_DXT1;
		break;
	case PF_DXT3:
		TextureFormat = nvtt::Format_DXT3;
		break;
	case PF_DXT5:
		TextureFormat = nvtt::Format_DXT5;
		break;
	default:
		return;
	}

	nvtt::InputOptions		nvttIOptions;
	nvttIOptions.setTextureLayout( nvtt::TextureType_2D, Width, Height );
	nvttIOptions.setFormat( nvtt::InputFormat_BGRA_8UB );
	nvttIOptions.setNormalMap( false );

	nvttIOptions.setMipmapGeneration( false );
	nvttIOptions.setMipmapFilter( nvtt::MipmapFilter_Box );
	nvttIOptions.setConvertToNormalMap( false ); // ?这个方法！！！

	nvttIOptions.setMipmapData( SrcData, Width, Height );

	CompressDXTNoMipmap     nvttOutputHandler( &DestData );
	CompressDXTError		nvttOutputError;
	nvtt::OutputOptions		nvttOOptions;
	nvttOOptions.setOutputHeader( true );
	nvttOOptions.setOutputHandler( &nvttOutputHandler );
	nvttOOptions.setErrorHandler( &nvttOutputError );

	nvtt::CompressionOptions nvttOptions;
	nvttOptions.setFormat( TextureFormat );

	nvtt::Compressor		nvttCompressor;

	if ( !nvttCompressor.process( nvttIOptions, nvttOptions, nvttOOptions ) )
	{
		warnf( TEXT("Texture compressor failing") );
	}
#endif
}
