//! @file SubstanceAirTexture2D.cpp
//! @brief Implementation of the USubstanceAirTexture2D class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirResource.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"

#if WITH_EDITOR
#include "SubstanceAirEdHelpers.h"
#endif

FString USubstanceAirTexture2D::GetDesc()
{
	return FString::Printf( TEXT("%dx%d[%s]"), SizeX, SizeY, GPixelFormats[Format].Name);
}


void USubstanceAirTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << OutputGuid;
	Ar << ParentInstance;
}


void USubstanceAirTexture2D::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

	if (OutputCopy)
	{
		// nullify the pointer to this texture's address
		// so that others see it has been destroyed
		*OutputCopy->Texture.get() = NULL;

		delete OutputCopy;
		OutputCopy = 0;

		// disable the output in the parent instance
		if(ParentInstance)
		{
			SubstanceAir::List<output_inst_t>::TIterator 
				ItOut(ParentInstance->Instance->Outputs.itfront());

			for(; ItOut ; ++ItOut)
			{
				if ((*ItOut).OutputGuid == OutputGuid)
				{
					(*ItOut).bIsEnabled = FALSE;
					break;
				}
			}
		}
	}
}


UBOOL USubstanceAirTexture2D::CanEditChange(const UProperty* InProperty) const
{
	UBOOL bIsEditable = Super::CanEditChange(InProperty);

	if (bIsEditable && InProperty != NULL)
	{
		bIsEditable = FALSE;

		if (InProperty->GetFName() == TEXT("AddressX") ||
			InProperty->GetFName() == TEXT("AddressY") ||
			InProperty->GetFName() == TEXT("UnpackMin") ||
			InProperty->GetFName() == TEXT("UnpackMax") ||
			InProperty->GetFName() == TEXT("Filter") ||
			InProperty->GetFName() == TEXT("LODBias") ||
			InProperty->GetFName() == TEXT("LODGroup"))
		{
			bIsEditable = TRUE;
		}
	}

	return bIsEditable;
}


void USubstanceAirTexture2D::PostLoad()
{
	// load the existing mips
	RequestedMips = ResidentMips = Mips.Num();

	if (NULL == ParentInstance)
	{
		appDebugMessagef(TEXT("Error, no parent instance found for this SubstanceAirTexture (%s). You need to delete the texture."),*GetName());
		debugf(TEXT("Substance: error, no parent instance found for this SubstanceAirTexture (%s). You need to delete the texture."),*GetFullName());
		Super::PostLoad();
		return;
	}

	// find the output using the UID serialized
	ParentInstance->ConditionalPostLoad(); // make sure the parent instance is loaded

	output_inst_t* Output = NULL;
	SubstanceAir::List<output_inst_t>::TIterator 
		OutIt(ParentInstance->Instance->Outputs.itfront());

	for ( ; OutIt ; ++OutIt)
	{
		if ((*OutIt).OutputGuid == OutputGuid)
		{
			Output = &(*OutIt);
			break;
		}
	}

	// integrity check, used to detect instance / desc mismatch
	if (!Output->GetOutputDesc())
	{
		appDebugMessagef(TEXT("Error, no matching output description found for this SubstanceAirTexture (%s). You need to delete the texture and its parent instance."),*GetName());
		debugf(TEXT("Substance: error, no matching output description found for this SubstanceAirTexture (%s). You need to delete the texture and its parent instance."),*GetFullName());
		Super::PostLoad();
		return;
	}

	if (!Output)
	{
		// the opposite situation is possible, no texture but an OutputInstance,
		// in this case the OutputInstance is disabled, but when the texture is
		// alive the Output must exist.
		appDebugMessagef(TEXT("Error, no matching output found for this SubstanceAirTexture (%s)"),*GetName());
		debugf(TEXT("Substance: error, no matching output found for this SubstanceAirTexture (%s)"),*GetFullName());
		Super::PostLoad();
		return;
	}
	else
	{
		// link the corresponding output to this texture 
		// this is already done when duplicate textures
		if (*Output->Texture == NULL)
		{
			// build the copy of the Output Instance
			*Output->Texture = this;
			OutputCopy = new output_inst_t(*Output);
		}

		// enable this output
		Output->bIsEnabled = TRUE;

		// flag this texture out of date so that it gets updated
		if (GUseSeekFreeLoading)
		{
			Output->flagAsDirty();
			SubstanceAir::Helpers::PushDelayedRender(ParentInstance->Instance);
		}
		else
		{
			Output->bIsDirty = FALSE;
			OutputCopy->bIsDirty = FALSE;
		}
	}

	Super::PostLoad();
}


void USubstanceAirTexture2D::PostDuplicate()
{
#if WITH_EDITOR
	// after duplication, we need to recreate a parent instance
	// look for the original object, using the GUID
	USubstanceAirTexture2D* RefTexture = NULL;

	for (TObjectIterator<USubstanceAirTexture2D> It; It; ++It)
	{
		if ((*It)->OutputGuid == OutputGuid && *It != this)
		{
			RefTexture = *It;
			break;
		}
	}

	check(RefTexture);

	USubstanceAirGraphInstance* RefInstance = RefTexture->ParentInstance;

	// then create the new instance
	SubstanceAir::List<graph_inst_t*> NewInstances;
	UBOOL bCreateOutputs = FALSE; 
	UBOOL bUseDefaultPackage = TRUE;

	SubstanceAir::Helpers::DuplicateGraphInstance(
		RefInstance->Instance,
		NewInstances, 
		GetOuter(), 
		bUseDefaultPackage, 
		bCreateOutputs);
	check(NewInstances.Num() == 1);

	// now we need to bind ourselves to this new instance and its output
	ParentInstance  = NewInstances(0)->ParentInstance;

	this->OutputGuid = appCreateGuid(); // create a new unique GUID (current one is duplicated from ref)
	SubstanceAir::List<output_inst_t>::TIterator It(ParentInstance->Instance->Outputs.itfront());

	for (;It;++It)
	{
		if ((*It).Uid == RefTexture->OutputCopy->Uid)
		{
			(*It).OutputGuid = this->OutputGuid;
			*(*It).Texture.get() = this;
			this->OutputCopy =
				new SubstanceAir::FOutputInstance(*It);
			break;
		}
	}

	ParentInstance->GetOuter()->MarkPackageDirty(TRUE);
#endif
}


FTextureResource* USubstanceAirTexture2D::CreateResource()
{
#if WITH_SUBSTANCE_AIR
	FTexture2DResource* Texture2DResource = 
		new FSubstanceAirTexture2DResource( this, ResidentMips, OutputCopy );
	return Texture2DResource;
#else // WITH_SUBSTANCE_AIR
	return NULL;
#endif 
}


void USubstanceAirTexture2D::LighterInit(UINT InSizeX, 
										 UINT InSizeY,
										 EPixelFormat InFormat)
{
	// Check that the dimensions are powers of two and evenly divisible by the format block size.
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));

	SizeX = InSizeX;
	SizeY = InSizeY;
	OriginalSizeX = InSizeX;
	OriginalSizeY = InSizeY;
	Format = InFormat;
}


void USubstanceAirTexture2D::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData);

	if (GUseSubstanceInstallTimeCache)
	{
		// textures have already been processed at this stage early exit
		return;
	}

	if (OutputCopy && OutputCopy->GetParentGraphInstance() && 
		FALSE == OutputCopy->GetParentGraphInstance()->bIsBaked)
	{
		// Get the number of mips to keep from the engine
		// configuration file, in the SubstanceAir section
		INT MipsToKeep = SBS_DFT_COOKED_MIPS_NB;
		GConfig->GetInt(TEXT("SubstanceAir"), TEXT("MipCountAfterCooking"), MipsToKeep, GEngineIni);

		// and remove all mips above
		if (Mips.Num() > MipsToKeep)
		{
			Mips.Remove(0, Mips.Num() - MipsToKeep);
		}
	}
}


UBOOL USubstanceAirTexture2D::HasSourceArt() const
{
	// source art is the sbsar file, but this function is used to
	// access the PNG copy of the standard texture 2D, so always FALSE

	return FALSE;
}


IMPLEMENT_CLASS( USubstanceAirTexture2D )
