//! @file SubstanceAirEdImageInputFactoryClasses.cpp
//! @brief Factory to create Substance Air Image Inputs
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>
#include <UnObjectTools.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirImageInputClasses.h"

#include "SubstanceAirEdFactoryClasses.h"


void Split_RGBA_8bpp(
	INT Width, INT Height,
	BYTE* DecompressedImageRGBA, const INT TextureDataSizeRGBA, 
	BYTE* DecompressedImageRGB, const INT TextureDataSizeRGB,
	BYTE* DecompressedImageA=NULL, const INT TextureDataSizeA=NULL)
{
	BYTE Pixel[4] = {0,0,0,0};

	if (DecompressedImageRGB)
	{
		BYTE* ImagePtrRGBA = DecompressedImageRGBA;
		BYTE* ImagePtrRGB = DecompressedImageRGB;

		for(INT Y = 0; Y < Height; Y++)
		{					
			for(INT X = 0;X < Width;X++)
			{
				// shuffle the image
				Pixel[0] = *ImagePtrRGBA++;
				Pixel[1] = *ImagePtrRGBA++;
				Pixel[2] = *ImagePtrRGBA++;
				ImagePtrRGBA++;

				*ImagePtrRGB++ = Pixel[2];
				*ImagePtrRGB++ = Pixel[1];
				*ImagePtrRGB++ = Pixel[0];
			}
		}
	}

	if (DecompressedImageA)
	{
		BYTE* ImagePtrRGBA = DecompressedImageRGBA;
		BYTE* ImagePtrA = DecompressedImageA;

		for(INT Y = 0; Y < Height; Y++)
		{
			for(INT X = 0;X < Width;X++)
			{	
				ImagePtrRGBA+=3;
				Pixel[0] = *ImagePtrRGBA++;
				*ImagePtrA++ = Pixel[0];
			}
		}
	}
}


void USubstanceAirImageInputFactory::StaticConstructor()
{
	new( GetClass()->HideCategories ) FName( NAME_Object );
}


void USubstanceAirImageInputFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass	= USubstanceAirImageInput::StaticClass();
	Description		= TEXT("Substance Image Input");

	// format of the file to import
	new(Formats)FString(TEXT("jpeg;Substance Image Input"));
	new(Formats)FString(TEXT("jpg;Substance Image Input"));
	new(Formats)FString(TEXT("tga;Substance Image Input"));

	// imports binary data via FactoryCreateBinary
	bText			= FALSE;
	bCreateNew		= FALSE;
	bEditorImport   = 1;
	AutoPriority	= -1;
}


UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromJpeg(
	USubstanceAirImageInput* ImageInput,
	const BYTE*& Buffer,
	const BYTE*	BufferEnd,
	FFeedbackContext* Warn)
{
	check(NULL != ImageInput);
	check(NULL != Buffer);
	check(NULL != BufferEnd);

	INT Width = 0;
	INT Height = 0;
	INT Length = BufferEnd - Buffer;
	void* DecompressedImage = NULL;

	DecompressedImage = 
		SubstanceAir::Helpers::DecompressJpeg(
			Buffer, Length, &Width, &Height /*,3, 4*/);

	if (!DecompressedImage)
	{
		Warn->Logf( NAME_Error, TEXT("Failed to decompress Image."));
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	/*free the decompressed version once the decompression succeeded*/
	appFree(DecompressedImage);

	if (Width < 16 || Height < 16)
	{
		Warn->Logf( NAME_Error, TEXT("Image Input import failed: Image too small, minimum size is 16x16"));
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}
	else if (Width > 2048 || Height > 2048)
	{
		Warn->Logf( NAME_Error, TEXT("Image Input import failed: Image too large, maximum size is 2048x2048."));
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	ImageInput->CompressedImageRGB.Lock(LOCK_READ_WRITE);
	DWORD* DestImageData = (DWORD*) ImageInput->CompressedImageRGB.Realloc(Length);
	appMemcpy(DestImageData, Buffer, Length);
	ImageInput->CompressedImageRGB.Unlock();

	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;
	ImageInput->NumComponents = 3;

	ImageInput->SourceFilePath = 
		GFileManager->ConvertToRelativePath(*CurrentFilename);
	ImageInput->SourceFileTimestamp.Empty();
	FFileManager::FTimeStamp Timestamp;
	if (GFileManager->GetTimestamp(*CurrentFilename, Timestamp))
	{
		FFileManager::FTimeStamp::TimestampToFString(
			Timestamp, 
			ImageInput->SourceFileTimestamp /*out*/);
	}

	ImageInput->CompRGB = 1;
	ImageInput->CompA = 0;

	return ImageInput;
}


UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromTga(
	USubstanceAirImageInput* ImageInput,
	const BYTE*& Buffer,
	const BYTE*	BufferEnd,
	FFeedbackContext* Warn)
{
	const FTGAFileHeader* TGA = (FTGAFileHeader *)Buffer;
	INT Length = BufferEnd - Buffer;
	INT Width = TGA->Width;
	INT Height = TGA->Height;

	// the DecompressTGA_helper works in RGBA8 only
	INT TextureDataSizeRGBA = TGA->Width * TGA->Height * 4;
	DWORD* DecompressedImageRGBA = (DWORD*)appMalloc(TextureDataSizeRGBA);

	INT TextureDataSizeRGB = TGA->Width * TGA->Height * 3;
	BYTE* DecompressedImageRGB = NULL;

	INT TextureDataSizeA = TGA->Width * TGA->Height;
	BYTE* DecompressedImageA = NULL;

	UBOOL res = DecompressTGA_helper(TGA, DecompressedImageRGBA, TextureDataSizeRGBA, Warn);

	if (FALSE == res)
	{
		appFree(DecompressedImageRGBA);
		GWarn->Log( TEXT("-- cannot import: failed to decode TGA image."));
		return NULL;
	}

	if (TGA->BitsPerPixel == 24)
	{
		DecompressedImageRGB = (BYTE*)appMalloc(TextureDataSizeRGB);
		ImageInput->NumComponents = 3;
		ImageInput->CompRGB = 1;
		ImageInput->CompA = 0;
	}
	else if (TGA->BitsPerPixel == 32)
	{
		DecompressedImageRGB = (BYTE*)appMalloc(TextureDataSizeRGB);
		DecompressedImageA = (BYTE*)appMalloc(TextureDataSizeA);
		ImageInput->NumComponents = 4;
		ImageInput->CompRGB = 1;
		ImageInput->CompA = 1;
	}
	else if (TGA->BitsPerPixel == 8)
	{
		DecompressedImageA = (BYTE*)appMalloc(TextureDataSizeA);
		ImageInput->NumComponents = 1;
		ImageInput->CompRGB = 0;
		ImageInput->CompA = 1;
	}

	Split_RGBA_8bpp(
		Width, Height,
		(BYTE*)DecompressedImageRGBA, TextureDataSizeRGBA,
		DecompressedImageRGB, TextureDataSizeRGB,
		DecompressedImageA, TextureDataSizeA);

	appFree(DecompressedImageRGBA);

	BYTE* CompressedImageRGB = NULL;
	int SizeCompressedImageRGB = 0;

	BYTE* CompressedImageA = NULL;
	int SizeCompressedImageA = 0;

	if (DecompressedImageRGB)
	{
		SubstanceAir::Helpers::CompressJpeg(
			(BYTE*)DecompressedImageRGB,
			TextureDataSizeRGB,
			Width,
			Height,
			3,
			&CompressedImageRGB,
			SizeCompressedImageRGB);
		appFree(DecompressedImageRGB);
	}

	if (DecompressedImageA)
	{
		SubstanceAir::Helpers::CompressJpeg(
			(BYTE*)DecompressedImageA,
			TextureDataSizeA,
			Width,
			Height,
			1,
			&CompressedImageA,
			SizeCompressedImageA);
		appFree(DecompressedImageA);


	}

	/*Copy the compressed version in the image input struct*/
	if (!CompressedImageRGB && !CompressedImageA)
	{
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	if (CompressedImageRGB)
	{
		ImageInput->CompressedImageRGB.Lock(LOCK_READ_WRITE);
		DWORD* DestImageData =
			(DWORD*) ImageInput->CompressedImageRGB.Realloc(SizeCompressedImageRGB);

		appMemcpy(DestImageData, CompressedImageRGB, SizeCompressedImageRGB);
		ImageInput->CompressedImageRGB.Unlock();

		/*Release the compressed version*/
		appFree(CompressedImageRGB);
	}

	if (CompressedImageA)
	{
		ImageInput->CompressedImageA.Lock(LOCK_READ_WRITE);
		DWORD* DestImageData =
			(DWORD*) ImageInput->CompressedImageA.Realloc(SizeCompressedImageA);

		appMemcpy(DestImageData, CompressedImageA, SizeCompressedImageA);
		ImageInput->CompressedImageA.Unlock();

		/*Release the compressed version*/
		appFree(CompressedImageA);
	}

	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;

	return ImageInput;
}


UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromTexture(
	USubstanceAirImageInput* ImageInput,
	UTexture2D* ContextTexture,
	const BYTE*& BGRA_Buffer,
	const BYTE*	BGRA_BufferEnd,
	FFeedbackContext* Warn)
{
	const BYTE* DecompressedImage_RGBA = BGRA_Buffer;
	INT Width = ContextTexture->OriginalSizeX;
	INT Height = ContextTexture->OriginalSizeY;
	INT DecompressedImageRGBA_Size = BGRA_BufferEnd - BGRA_Buffer;

	UBOOL bTextureHasAlpha = FALSE;

	for (INT Y = Height - 1 ; Y >= 0 ; --Y)
	{
		const BYTE* Color = &DecompressedImage_RGBA[Y * Width * 4];
		for (INT X = Width ; X > 0; --X)
		{
			// Skip color info
			Color+=3;

			// Get Alpha value then increment the pointer past it for the next pixel
			BYTE Alpha = *Color++;
			if (Alpha != 255)
			{
				// When a texture is imported with no alpha, the alpha bits are set to 255
				// So if the texture has non 255 alpha values, the texture is a valid alpha channel
				bTextureHasAlpha = TRUE;
				break;
			}
		}
		if (bTextureHasAlpha)
		{
			break;
		}
	}

	BYTE* DecompressedImageRGB = NULL;
	INT SizeDecompressedImageRGB = Width * Height * 3;

	BYTE* DecompressedImageA = NULL;
	INT SizeDecompressedImageA = Width * Height;

	if (bTextureHasAlpha)
	{
		DecompressedImageRGB = (BYTE*)appMalloc(SizeDecompressedImageRGB);
		DecompressedImageA = (BYTE*)appMalloc(SizeDecompressedImageA);

		ImageInput->NumComponents = 4;
		ImageInput->CompRGB = 1;
		ImageInput->CompA = 1;

		Split_RGBA_8bpp(
			Width, Height,
			(BYTE*)DecompressedImage_RGBA, DecompressedImageRGBA_Size,
			DecompressedImageRGB, SizeDecompressedImageRGB,
			DecompressedImageA, SizeDecompressedImageA);
	}
	else
	{
		DecompressedImageRGB = (BYTE*)appMalloc(SizeDecompressedImageRGB);
		ImageInput->NumComponents = 3;
		ImageInput->CompRGB = 1;
		ImageInput->CompA = 0;

		Split_RGBA_8bpp(
			Width, Height,
			(BYTE*)DecompressedImage_RGBA, DecompressedImageRGBA_Size,
			DecompressedImageRGB, SizeDecompressedImageRGB,
			NULL, 0);
	}

	BYTE* CompressedImageRGB = NULL;
	int SizeCompressedImageRGB = 0;

	BYTE* CompressedImageA = NULL;
	int SizeCompressedImageA = 0;

	if (DecompressedImageRGB)
	{
		SubstanceAir::Helpers::CompressJpeg(
			(BYTE*)DecompressedImageRGB,
			SizeDecompressedImageRGB,
			Width,
			Height,
			3,
			&CompressedImageRGB,
			SizeCompressedImageRGB);
		appFree(DecompressedImageRGB);
	}

	if (DecompressedImageA)
	{
		SubstanceAir::Helpers::CompressJpeg(
			(BYTE*)DecompressedImageA,
			SizeDecompressedImageA,
			Width,
			Height,
			1,
			&CompressedImageA,
			SizeCompressedImageA);
		appFree(DecompressedImageA);
	}

	/*Copy the compressed version in the image input struct*/
	if (!CompressedImageRGB && !CompressedImageA)
	{
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	if (CompressedImageRGB)
	{
		ImageInput->CompressedImageRGB.Lock(LOCK_READ_WRITE);
		DWORD* DestImageData =
			(DWORD*) ImageInput->CompressedImageRGB.Realloc(SizeCompressedImageRGB);

		appMemcpy(DestImageData, CompressedImageRGB, SizeCompressedImageRGB);
		ImageInput->CompressedImageRGB.Unlock();

		/*Release the compressed version*/
		appFree(CompressedImageRGB);
	}

	if (CompressedImageA)
	{
		ImageInput->CompressedImageA.Lock(LOCK_READ_WRITE);
		DWORD* DestImageData =
			(DWORD*) ImageInput->CompressedImageA.Realloc(SizeCompressedImageA);

		appMemcpy(DestImageData, CompressedImageA, SizeCompressedImageA);
		ImageInput->CompressedImageA.Unlock();

		/*Release the compressed version*/
		appFree(CompressedImageA);
	}

	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;

	return ImageInput;
}


// This function can be called with the following scenarios :
// * the user imported a jpeg file
// * the user imported a tga file
// * the user is transforming a Texture2D in an image input
//
UObject* USubstanceAirImageInputFactory::FactoryCreateBinary(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn)
{
	USubstanceAirImageInput* ImageInput = 
		CastChecked<USubstanceAirImageInput>(
			StaticConstructObject(
				USubstanceAirImageInput::StaticClass(),
				InParent,
				Name,
				RF_Standalone|RF_Public));

	UTexture2D* ContextTexture = Cast<UTexture2D>(Context);

	if (appStricmp( Type, TEXT("tga") ) == 0)
	{
		return FactoryCreateBinaryFromTga(ImageInput, Buffer, BufferEnd, Warn);
	}
	else if (appStricmp( Type, TEXT("jpeg") ) == 0 || 
		     appStricmp( Type, TEXT("jpg") ) == 0)
	{
		return FactoryCreateBinaryFromJpeg(ImageInput, Buffer, BufferEnd, Warn);
	}
	else if (ContextTexture)
	{
		return FactoryCreateBinaryFromTexture(
			ImageInput, ContextTexture, Buffer, BufferEnd, Warn);
	}

	ImageInput->ClearFlags(RF_Standalone);

	return NULL;
}


UReimportSubstanceAirImageInputFactory::UReimportSubstanceAirImageInputFactory():
	pOriginalImage(NULL)
{

}


void UReimportSubstanceAirImageInputFactory::StaticConstructor()
{
	new( GetClass()->HideCategories ) FName(NAME_Object);
}


void UReimportSubstanceAirImageInputFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass	= USubstanceAirImageInput::StaticClass();
	Description		= TEXT("Substance Image input ");

	// imports binary data via FactoryCreateBinary
	bText			= FALSE;
	bCreateNew		= FALSE;
	bEditorImport   = 1;
	AutoPriority	= -1;
}


UBOOL UReimportSubstanceAirImageInputFactory::Reimport( UObject* Obj )
{
	if(!Obj || !Obj->IsA(USubstanceAirImageInput::StaticClass()))
	{
		return FALSE;
	}

	pOriginalImage = Cast<USubstanceAirImageInput>(Obj);

	TArray<SubstanceAir::FImageInputInstance*> OriginalInputs = pOriginalImage->Inputs;

	FString SourceFilePath = 
		pOriginalImage->SourceFilePath;

	if (!(SourceFilePath.Len()))
	{
		return FALSE;
	}

	GWarn->Log( 
		FString::Printf(
		TEXT("Performing atomic reimport of [%s]"),*SourceFilePath) );

	FFileManager::FTimeStamp TS,MyTS;
	if (!GFileManager->GetTimestamp( *SourceFilePath, TS ))
	{
		GWarn->Log( TEXT("-- cannot reimport: source file cannot be found."));

		UFactory* Factory = 
			ConstructObject<UFactory>(
			USubstanceAirImageInputFactory::StaticClass() );

		UBOOL bNewSourceFound = FALSE;
		FString NewFileName;

		if (ObjectTools::FindFileFromFactory(
				Factory,
				LocalizeUnrealEd("Import_SourceFileNotFound"),
				NewFileName))
		{
			SourceFilePath = GFileManager->ConvertToRelativePath(*NewFileName);
			bNewSourceFound = GFileManager->GetTimestamp( *SourceFilePath, TS );
		}

		// If a new source wasn't found or the user canceled out of the dialog,
		// we cannot proceed, but this reimport factory has still technically 
		// "handled" the reimport, so return TRUE instead of FALSE
		if (!bNewSourceFound)
		{
			return TRUE;
		}
	}

	// Pull the timestamp from the user readable string.
	// It would be nice if this was stored directly, and maybe it will be if
	// its decided that UTC dates are too confusing to the users.
	FFileManager::FTimeStamp::FStringToTimestamp(
		pOriginalImage->SourceFileTimestamp, /*out*/ MyTS);

	if (MyTS < TS)
	{
		GWarn->Log(TEXT("-- file on disk exists and is newer.  Attempting import."));

		USubstanceAirImageInput* NewImgInput =
			Cast<USubstanceAirImageInput>(UFactory::StaticImportObject(
				pOriginalImage->GetClass(),
				pOriginalImage->GetOuter(),
				*pOriginalImage->GetName(),
				RF_Public|RF_Standalone,
				*(SourceFilePath),
				NULL,
				this));

		if (NewImgInput)
		{
			NewImgInput->Inputs = OriginalInputs;

			TArrayNoInit<SubstanceAir::FImageInputInstance*>::TIterator 
				itInput(NewImgInput->Inputs);				

			for (;itInput;++itInput)
			{
				(*itInput)->Parent->UpdateInput(
					(*itInput)->Uid,
					NewImgInput);
				SubstanceAir::Helpers::RenderAsync((*itInput)->Parent);
			}

			GWarn->Log(TEXT("-- imported successfully"));
		}
		else
		{
			GWarn->Log(TEXT("-- import failed"));
		}
	}
	else
	{
		GWarn->Log(TEXT("-- Substance Image Input already up to date."));
	}

	return TRUE;
}


IMPLEMENT_CLASS( USubstanceAirImageInputFactory )
IMPLEMENT_CLASS( UReimportSubstanceAirImageInputFactory )
