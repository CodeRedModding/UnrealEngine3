//! @file SubstanceAirHelpers.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirCallbacks.h"
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirHelpers.h"

#include "framework/renderer.h"

#if WITH_EDITOR
#include "SubstanceAirEdHelpers.h"
#endif // WITH_EDITOR

#include <UnJPEG.h>
#include <atc_api.h>

extern atc_api::Reader* GSubstanceAirTextureReader;

namespace SubstanceAir
{

std::auto_ptr<SubstanceAir::RenderCallbacks> gCallbacks(new SubstanceAir::RenderCallbacks());
std::auto_ptr<SubstanceAir::Renderer> GSubstanceRenderer(NULL);

UBOOL bInstancesPushedDuringLoading = FALSE;
int_t RefreshContentBrowserCount = 0;

SubstanceAir::List<graph_inst_t*> LoadingQueue; // queue used by PushDelayedRender & PerformDelayedRender
SubstanceAir::List<graph_inst_t*> PriorityLoadingQueue; // queue used by PushDelayedRender & PerformDelayedRender

typedef pair_t< img_input_inst_t*, graph_inst_t* >::Type GraphImageInputPair_t;
TArray<GraphImageInputPair_t> DelayedImageInputs;

TMap< USubstanceAirGraphInstance*, TMap< UObject*, INT > > ActiveImageInputs;

namespace Helpers
{


void RenderPush(graph_inst_t* Instance)
{
	if (FALSE == Instance->bIsBaked && FALSE == Instance->bHasPendingImageInputRendering)
	{
		GSubstanceRenderer->push(Instance);
	}
}


void RenderAsync(SubstanceAir::List<graph_inst_t*>& Instances)
{
	SubstanceAir::Helpers::Tick();

	GSubstanceRenderer->push(Instances);
	
	GSubstanceRenderer->run(
		SubstanceAir::Renderer::Run_Asynchronous|
		SubstanceAir::Renderer::Run_Replace|
		SubstanceAir::Renderer::Run_First|
		SubstanceAir::Renderer::Run_PreserveRun);

	SubstanceAir::Helpers::Tick();
}


void RenderAsync(graph_inst_t* Instance)
{
	SubstanceAir::Helpers::Tick();

	GSubstanceRenderer->push(Instance);

	GSubstanceRenderer->run(
		SubstanceAir::Renderer::Run_Asynchronous|
		SubstanceAir::Renderer::Run_Replace|
		SubstanceAir::Renderer::Run_First|
		SubstanceAir::Renderer::Run_PreserveRun);

	SubstanceAir::Helpers::Tick();
}


void RenderSync(SubstanceAir::List<graph_inst_t*>& Instances)
{
	GSubstanceRenderer->setCallbacks(NULL);

	GSubstanceRenderer->push(Instances);
	GSubstanceRenderer->run(SubstanceAir::Renderer::Run_First);        // This computed first

	GSubstanceRenderer->setCallbacks(gCallbacks.get());
}


void RenderSync(graph_inst_t* Instance)
{
	GSubstanceRenderer->setCallbacks(NULL);

	GSubstanceRenderer->push(Instance);
	GSubstanceRenderer->run(SubstanceAir::Renderer::Run_First);        // This computed first

	GSubstanceRenderer->setCallbacks(gCallbacks.get());
}


void PushDelayedRender(graph_inst_t* Instance)
{
	if (FALSE == Instance->bIsBaked)
	{
		if (FALSE == Instance->bHasPendingImageInputRendering)
		{
			PriorityLoadingQueue.AddUniqueItem(Instance);
		}
		else // instances waiting for image inputs are not filed in the priority queue
		{
			LoadingQueue.AddUniqueItem(Instance);
		}
		bInstancesPushedDuringLoading = TRUE;
	}
}


void PushDelayedImageInput(img_input_inst_t* ImgInput,graph_inst_t* Instance)
{
	DelayedImageInputs.AddUniqueItem(std::make_pair(ImgInput, Instance));
}


void SetDelayedImageInput()
{
	TArray<GraphImageInputPair_t>::TIterator ItInp(DelayedImageInputs);

	for(;ItInp;++ItInp)
	{
		img_input_inst_t* ImgInput = (*ItInp).first;
		graph_inst_t* Instance = (*ItInp).second;

		ImgInput->SetImageInput(ImgInput->ImageSource, Instance, FALSE);

		if (Cast<USubstanceAirTexture2D>(ImgInput->ImageSource))
		{
			SubstanceAir::Helpers::RegisterOutputAsImageInput(
				Instance->ParentInstance,
				Cast<USubstanceAirTexture2D>(ImgInput->ImageSource));
		}
	}

	DelayedImageInputs.Empty();
}


void PerformDelayedRender()
{
	if (PriorityLoadingQueue.Num())
	{
		RenderSync(PriorityLoadingQueue);
		SubstanceAir::Helpers::UpdateTextures(PriorityLoadingQueue);
		PriorityLoadingQueue.Empty();
	}

	SetDelayedImageInput();

	if (LoadingQueue.Num())
	{
		RenderAsync(LoadingQueue);
		LoadingQueue.Empty();
	}
}


void FlagRefreshContentBrowser(int OutputsNeedingRefresh)
{
	RefreshContentBrowserCount += OutputsNeedingRefresh;
}


void UpdateTexture(output_inst_t::Result & Res, output_inst_t* Output)
{
	USubstanceAirTexture2D* Texture = *(Output->Texture.get());

	if (NULL == Texture)
	{
		return;
	}

	// grab the Result computed in the Substance Thread
	const SubstanceTexture& ResultText = Res->getTexture();
	const SIZE_T Mipstart = (SIZE_T) ResultText.buffer;
	SIZE_T MipOffset = 0;

	// prepare mip map data
	FTexture2DMipMap* MipMap = 0;
	INT MipSizeX = ResultText.level0Width;
	INT MipSizeY = ResultText.level0Height;

	// create as much mip as necessary
	if (Texture->Mips.Num() != ResultText.mipmapCount ||
		MipSizeX != Texture->SizeX ||
		MipSizeY != Texture->SizeY)
	{
		Texture->Mips.Empty();

		// initialize the Texture's size
		Texture->LighterInit(MipSizeX,MipSizeY,(EPixelFormat)Texture->Format);

		for (int_t IdxMip=0 ; IdxMip < ResultText.mipmapCount ; ++IdxMip)
		{
			MipMap = new(Texture->Mips) FTexture2DMipMap;
			MipMap->SizeX = MipSizeX;
			MipMap->SizeY = MipSizeY;

			// compute the next mip size
			MipSizeX = Max(MipSizeX>>1, 1);
			MipSizeY = Max(MipSizeY>>1, 1);

			// not smaller than the "block size"
			MipSizeX = Max((INT)GPixelFormats[Texture->Format].BlockSizeX,MipSizeX);
			MipSizeY = Max((INT)GPixelFormats[Texture->Format].BlockSizeY,MipSizeY);
		}
	}

	// fill up the texture's mip
	for (int_t IdxMip=0 ; IdxMip < ResultText.mipmapCount ; ++IdxMip)
	{
		MipMap = &Texture->Mips(IdxMip);

		// get the size of the mip's content
		const SIZE_T ImageSize = CalculateImageBytes(
			MipMap->SizeX,
			MipMap->SizeY,
			0,
			(EPixelFormat)Texture->Format);
		check(0 != ImageSize);

		// copy the data
		MipMap->Data = FTextureMipBulkData();
		MipMap->Data.Lock(LOCK_READ_WRITE);
		void* TheMipDataPtr = MipMap->Data.Realloc(ImageSize);
		appMemcpy(TheMipDataPtr, (void*)(Mipstart+MipOffset), ImageSize);
		MipOffset += ImageSize;
		MipMap->Data.ClearBulkDataFlags( BULKDATA_SingleUse );
		MipMap->Data.Unlock();
	}

	// trigger the upload in video memory
	Texture->RequestedMips = Texture->ResidentMips = Texture->Mips.Num();
	Texture->UpdateResource();
	Texture->MarkPackageDirty();
	Texture->OutputCopy->bIsDirty = FALSE;

#if WITH_EDITOR == 1
	if (GIsEditor)
	{	
		GCallbackEvent->Send(CALLBACK_TextureModified, Texture);
		GCallbackEvent->Send(CALLBACK_ObjectPropertyChanged, Texture);
	}
#endif
}


void Tick()
{
	if (GUseSubstanceInstallTimeCache)
	{
		return;
	}

	if (bInstancesPushedDuringLoading)
	{
		bInstancesPushedDuringLoading = FALSE;
		PerformDelayedRender();
	}

	SubstanceAir::List<output_inst_t*> Outputs = 
		RenderCallbacks::getComputedOutputs();

	SubstanceAir::List<output_inst_t*>::TIterator ItOut(Outputs.itfront());

	UBOOL bUpdatedOutput = FALSE;

	for (;ItOut;++ItOut)
	{
		// Grab Result (auto pointer on RenderResult)
		output_inst_t::Result Result = ((*ItOut)->grabResult());

		if (Result.get())
		{
			UpdateTexture(Result, *ItOut);
			bUpdatedOutput = TRUE;
		}
	}

	// Ask for a refresh
	if (bUpdatedOutput && RefreshContentBrowserCount)
	{
#if WITH_EDITOR == 1
		DWORD RefreshFlags = CBR_UpdateAssetList|CBR_UpdatePackageList;
		FCallbackEventParameters Parms( NULL, CALLBACK_RefreshContentBrowser, RefreshFlags );
		GCallbackEvent->Send(Parms);
#endif // WITH_EDITOR
		--RefreshContentBrowserCount;
	}

#if WITH_EDITOR == 1
	PerformDelayedDeletion();
#endif
}


void CancelPendingActions()
{
	if(GSubstanceRenderer.get())
	{
		LoadingQueue.Empty();
		PriorityLoadingQueue.Empty();
		GSubstanceRenderer->cancelAll();
	}
}


output_desc_t* GetOutputDesc(output_inst_t* Output)
{
	output_desc_t* Desc = NULL;

	SubstanceAir::List<output_desc_t>::TIterator OutIt(
		Output->ParentInstance->Instance->Desc->OutputDescs.itfront());

	for ( ; OutIt ; ++OutIt)
	{
		if ((*OutIt).Uid == Output->Uid)
		{
			Desc = &(*OutIt);
			break;
		}
	}

	return Desc;
}


void CreateTextures(graph_inst_t* GraphInstance)
{
	UBOOL bCreateAllOutputs = FALSE;
	GConfig->GetBool(TEXT("SubstanceAir"), TEXT("bCreateAllOutputs"), bCreateAllOutputs, GEngineIni);

	for(UINT Idx=0 ; Idx<GraphInstance->Outputs.size() ; ++Idx)
	{
		output_inst_t* OutputInstance = &GraphInstance->Outputs(Idx);
		USubstanceAirTexture2D** ptr = OutputInstance->Texture.get();

		if (ptr && *ptr == NULL)
		{
			if (bCreateAllOutputs)
			{
				CreateTexture2D(OutputInstance);
			}
			else
			{
				// find the description 
				output_desc_t* Desc = GetOutputDesc(OutputInstance);

				// create Output if it does match to a slot of the default material
				switch(Desc->Channel)
				{
				case CHAN_Diffuse:
				case CHAN_Emissive:
				case CHAN_Normal:
				case CHAN_Specular:
				case CHAN_SpecularColor:
				case CHAN_SpecularLevel:
				case CHAN_Mask:
				case CHAN_Opacity:
				case CHAN_AnisotropyAngle:
					CreateTexture2D(OutputInstance);
					break;
				default:
					// else, do nothing
					break;
				}
			}
		}
		else
		{
			check(0);
			debugf(TEXT("Substance: Texture already exists"));		
		}
	}
}


UBOOL UpdateTextures(SubstanceAir::List<FGraphInstance*>& Instances)
{
	UBOOL GotSomething = FALSE;

	SubstanceAir::List<FGraphInstance*>::TIterator ItInst(Instances.itfront());
	
	for (;ItInst;++ItInst)
	{
		// Iterate on all Outputs
		SubstanceAir::List<output_inst_t>::TIterator ItOut((*ItInst)->Outputs.itfront());

		for (;ItOut;++ItOut)
		{
			// Grab Result (auto pointer on RenderResult)
			output_inst_t::Result Result = ((*ItOut).grabResult());

			if (Result.get())
			{
				UpdateTexture(Result, &*ItOut);
				GotSomething = TRUE;
			}
		}
	}

	return GotSomething;
}


void CreateTexture2D(output_inst_t* OutputInstance, FString Name)
{
	check(OutputInstance);
	check(OutputInstance->ParentInstance);

	USubstanceAirGraphInstance* OuterInst = OutputInstance->ParentInstance;
	UObject* Outer = OuterInst->GetOuter();

#if WITH_EDITOR == 1
	if (0 == Name.Len())
	{
		Name = GetSuitableName(OutputInstance, Outer);
	}
#endif

	USubstanceAirTexture2D* Texture = 
		ConstructObject< USubstanceAirTexture2D >( 
			USubstanceAirTexture2D::StaticClass(),
			Outer,
			*Name,
			RF_Public|RF_Standalone);

	Texture->ParentInstance = OuterInst;
	Texture->OutputGuid = OutputInstance->OutputGuid;

	Texture->AddressX		= TA_Wrap;
	Texture->AddressY		= TA_Wrap;
	Texture->CompressionFullDynamicRange = FALSE;
	Texture->Filter			= TF_Linear;
	Texture->LODGroup		= TEXTUREGROUP_World;
	Texture->LODBias		= -1;	// to be sure the top level is used
	Texture->NeverStream	= FALSE;
	Texture->SRGB = TRUE;

	if (OuterInst->Instance && OuterInst->Instance->Desc)
	{
		// find the description of the Output
		output_desc_t* Desc = GetOutputDesc(OutputInstance);

		// If normal map, use those special parameters
		if (Desc && Desc->Channel == CHAN_Normal)
		{
			FLOAT NormalMapUnpackMin[4] = { -1, -1, -1, +0 };
			FLOAT NormalMapUnpackMax[4] = { +1, +1, +1, +1 };
			appMemcpy(
				Texture->UnpackMin,
				NormalMapUnpackMin,
				sizeof(NormalMapUnpackMin));
			appMemcpy(
				Texture->UnpackMax,
				NormalMapUnpackMax,
				sizeof(NormalMapUnpackMax));

			Texture->SRGB = FALSE;
		}
	}

	INT Format = 
		SubstanceToUe3Format(
			(SubstancePixelFormat)OutputInstance->Format);

	// unsupported format
	if (PF_Unknown == Format)
	{
		debugf((TEXT("Substance: error, unsupported format generated")));
		Texture->ClearFlags(RF_Standalone);
		return;
	}

	if (PF_G8 == Format ||
		PF_G16 == Format)
	{
		Texture->SRGB = FALSE;
	}

	Texture->Format = Format;
	Texture->ResidentMips = 0;
	Texture->RequestedMips = 0;
	Texture->SizeX = 0;
	Texture->SizeY = 0;

	OutputInstance->bIsEnabled = TRUE;
	OutputInstance->flagAsDirty();
	*OutputInstance->Texture.get() = Texture;

	// make a copy of the Output instance in the texture
	(*OutputInstance->Texture.get())->OutputCopy =
		new FOutputInstance(*OutputInstance);
}


SubstancePixelFormat ValidateFormat(const SubstancePixelFormat Format)
{
	INT NewFormat = Format;

	// do not want 16 bit texture
	{
		NewFormat &= ~Substance_PF_16b;
	}

	//! @note: SRGB is not implemented in Substance Air for the moment
	{
		NewFormat &= ~Substance_PF_sRGB;
	}

	switch(NewFormat)
	{
	// we force the Output in DXT when raw
	case Substance_PF_RGBA:
		NewFormat = Substance_PF_DXT5;
		break;

	// we force the Output in DXT when raw
	case Substance_PF_RGBx:
	case Substance_PF_RGB:

	case Substance_PF_DXT1:
		NewFormat = Substance_PF_DXT1;
		break;

	// we support the grayscale format
	case Substance_PF_L:
		NewFormat = Substance_PF_L;
		break;

	case Substance_PF_DXT2:
	case Substance_PF_DXT3:
		NewFormat = Substance_PF_DXT3;
		break;

	// the rest should be DXT5
	case Substance_PF_DXT4:
	case Substance_PF_DXTn:
	case Substance_PF_DXT5:
	default:
		NewFormat = Substance_PF_DXT5;
		break;
	}

	return (SubstancePixelFormat)NewFormat;
}


bool_t IsSupported(EPixelFormat Fmt)
{
	switch (Fmt)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_A8R8G8B8:
		case PF_G8:
			return TRUE;
		default:
			return FALSE;
	}
}


SubstancePixelFormat Ue3FormatToSubstance(EPixelFormat Fmt)
{
	switch (Fmt)
	{
	case PF_DXT1: return Substance_PF_DXT1;
	case PF_DXT3: return Substance_PF_DXT3;
	case PF_DXT5: return Substance_PF_DXT5;
	case PF_A8R8G8B8: return Substance_PF_RGBA;
	case PF_G8: return Substance_PF_L;
	case PF_G16: return SubstancePixelFormat(Substance_PF_L|Substance_PF_16b);
	case PF_A16B16G16R16:  return SubstancePixelFormat(Substance_PF_RGBA|Substance_PF_16b);
	default:
		return Substance_PF_RAW;
	}
}


EPixelFormat SubstanceToUe3Format(const SubstancePixelFormat Format)
{
	SubstancePixelFormat InFmt = SubstancePixelFormat(Format &~ Substance_PF_sRGB);

	EPixelFormat OutFormat = PF_Unknown;

	//! @see SubstanceXmlHelper.cpp, ValidateFormat(...) function
	switch ((unsigned int)InFmt)
	{
	case Substance_PF_L|Substance_PF_16b:
	case Substance_PF_L:
		OutFormat = PF_G8;
		break;
	case Substance_PF_DXT1:
		OutFormat = PF_DXT1;
		break;
	case Substance_PF_DXT3:
		OutFormat = PF_DXT3;
		break;
	case Substance_PF_DXT5:
		OutFormat = PF_DXT5;
		break;

	// all other formats are replaced by 
	// one of the previous ones (DXT5 mostly)
	default: 
		OutFormat = PF_Unknown;
		break;
	}

	return OutFormat;
}


bool_t AreInputValuesEqual(
	std::tr1::shared_ptr<input_inst_t> &A, std::tr1::shared_ptr<input_inst_t> &B)
{
	input_inst_t* InstanceA = A.get();
	input_inst_t* InstanceB = B.get();

	if (!InstanceA || !InstanceB)
	{
		check(0);
		debugf(TEXT("Invalid arguments specified"));
		return FALSE;
	}
	
	// don't bother comparing values of inputs that don't relate to the same Input
	if ((InstanceA->Uid != InstanceB->Uid) || 
		(InstanceA->Type != InstanceB->Type)) // if the UIDs match, the type
	{											// should also match, but better
		return FALSE;							// safe than sorry...
	}

	switch((SubstanceInputType)InstanceA->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float_t>* InputA = 
				(FNumericalInputInstance<float_t>*)InstanceA;
				
			FNumericalInputInstance<float_t>* InputB = 
				(FNumericalInputInstance<float_t>*)InstanceB;

			return appIsNearlyEqual(InputA->Value, InputB->Value,(FLOAT)DELTA);
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* InputA = 
				(FNumericalInputInstance<vec2float_t>*)InstanceA;
				
			FNumericalInputInstance<vec2float_t>* InputB = 
				(FNumericalInputInstance<vec2float_t>*)InstanceB;
			
			return 
				appIsNearlyEqual(InputA->Value.X, InputB->Value.X,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(FLOAT)DELTA);
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* InputA = 
				(FNumericalInputInstance<vec3float_t>*)InstanceA;
				
			FNumericalInputInstance<vec3float_t>* InputB = 
				(FNumericalInputInstance<vec3float_t>*)InstanceB;
				
			return
				appIsNearlyEqual(InputA->Value.X, InputB->Value.X,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.Z, InputB->Value.Z,(FLOAT)DELTA);
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* InputA = 
				(FNumericalInputInstance<vec4float_t>*)InstanceA;
				
			FNumericalInputInstance<vec4float_t>* InputB = 
				(FNumericalInputInstance<vec4float_t>*)InstanceB;
				
			return
				appIsNearlyEqual(InputA->Value.X, InputB->Value.X,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.Z, InputB->Value.Z,(FLOAT)DELTA) && 
				appIsNearlyEqual(InputA->Value.W, InputB->Value.W,(FLOAT)DELTA);
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int_t>* InputA = 
				(FNumericalInputInstance<int_t>*)InstanceA;
				
			FNumericalInputInstance<int_t>* InputB = 
				(FNumericalInputInstance<int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* InputA = 
				(FNumericalInputInstance<vec2int_t>*)InstanceA;
				
			FNumericalInputInstance<vec2int_t>* InputB = 
				(FNumericalInputInstance<vec2int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;

		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* InputA = 
				(FNumericalInputInstance<vec3int_t>*)InstanceA;
				
			FNumericalInputInstance<vec3int_t>* InputB = 
				(FNumericalInputInstance<vec3int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* InputA = 
				(FNumericalInputInstance<vec4int_t>*)InstanceA;
				
			FNumericalInputInstance<vec4int_t>* InputB = 
				(FNumericalInputInstance<vec4int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Image:
		{
			FImageInputInstance* ImgInputA =
				(FImageInputInstance*)InstanceA;
			
			FImageInputInstance* ImgInputB =
				(FImageInputInstance*)InstanceB;

			return (ImgInputA->ImageSource == ImgInputB->ImageSource) ? TRUE : FALSE;
		}
		break;
	default:
		break;
	}
	debugf((TEXT("Substance: Input with an invalid type specified.")));
	check(0);
	return FALSE;
}


void SetupSubstanceAir()
{
	GSubstanceRenderer.reset(new SubstanceAir::Renderer());
	GSubstanceRenderer->setCallbacks(gCallbacks.get());
}


void TearDownSubstanceAir()
{
	GSubstanceRenderer.reset(NULL);
}


void Disable(output_inst_t* Output)
{
	Output->bIsEnabled = FALSE;

	if (*Output->Texture.get())
	{
		Clear(Output->Texture);
	}
}


void Clear(std::tr1::shared_ptr<USubstanceAirTexture2D*>& OtherTexture)
{
	check(*(OtherTexture).get() != NULL);
	check(OtherTexture.unique() == false);

#if WITH_EDITOR == 1
	// clearing a USubstanceAirTexture2D outside of the garbage collection
	// happens when disabling from the graph instance editor, need to explicitly 
	// delete the texture, otherwise, texture will be garbage collected
	if (FALSE == GIsGarbageCollecting)
	{
		SubstanceAir::Helpers::RegisterForDeletion(*OtherTexture);
	}
	else
#endif
	{
		(*OtherTexture)->SetFlags(RF_PendingKill);
		(*OtherTexture)->ClearFlags(RF_Standalone);
	}

	delete (*OtherTexture)->OutputCopy;
	(*OtherTexture)->OutputCopy = NULL;
	*(OtherTexture).get() = NULL;
}


graph_desc_t* FindParentGraph(
	SubstanceAir::List<graph_desc_t*>& Graphs,
	graph_inst_t* Instance)
{
	SubstanceAir::List<graph_desc_t*>::TIterator GraphIt(Graphs.itfront());
	
	for ( ; GraphIt ; ++GraphIt)
	{
		SubstanceAir::List<guid_t>::TConstIterator 
			UidIt((*GraphIt)->InstanceUids.itfrontconst());

		for (; UidIt ; ++UidIt )
		{
			if (*UidIt == Instance->InstanceGuid)
			{
				return (*GraphIt);
			}
		}
	}

	return NULL;
}


graph_desc_t* FindParentGraph(
	SubstanceAir::List<graph_desc_t*>& Graphs,
	const FString& ParentUrl)
{
	SubstanceAir::List<graph_desc_t*>::TIterator GraphIt(Graphs.itfront());

	for ( ; GraphIt ; ++GraphIt)
	{
		if ((*GraphIt)->PackageUrl == ParentUrl)
		{
			return (*GraphIt);
		}
	}

	return NULL;
}


BYTE* DecompressJpeg(
	const BYTE* Buffer, const INT Length,
	INT* Width, INT* Height, INT NumComp)
{
	FDecoderJPEG Decoder(Buffer,Length, NumComp);
	BYTE* UncompressedImage = Decoder.Decode();

	*Width = Decoder.GetWidth();
	*Height = Decoder.GetHeight();

	return UncompressedImage;
}


//! @brief Compress a jpeg buffer in RAW RGBA
void CompressJpeg(
	const BYTE* InBuffer, const INT InLength, 
	INT W, INT H, INT NumComp,
	BYTE** OutBuffer, INT& OutLength)
{
	static int countJpeg = 0;

	FEncoderJPEG Encoder(InBuffer, InLength, W, H, NumComp);
	*OutBuffer = Encoder.Encode();
	OutLength = Encoder.GetEncodedSize();
}


void Join_RGBA_8bpp(
	INT Width, INT Height,
	BYTE* DecompressedImageRGBA, const INT TextureDataSizeRGBA,
	BYTE* DecompressedImageA, const INT TextureDataSizeA)
{
	check(DecompressedImageRGBA);
	
	if (DecompressedImageA)
	{
		BYTE* ImagePtrRGBA = DecompressedImageRGBA;
		BYTE* ImagePtrA = DecompressedImageA;

		for(INT Y = 0; Y < Height; Y++)
		{					
			for(INT X = 0;X < Width;X++)
			{	
				ImagePtrRGBA += 3;
				*ImagePtrRGBA++ = *ImagePtrA++;
			}
		}
	}
}


void PrepareFileImageInput_GetBGRA(USubstanceAirImageInput* Input, BYTE** Outptr, INT& Width, INT& Height)
{
	BYTE* UncompressedImageRGBA = NULL;
	int SizeUncompressedImageRGBA = 0;

	BYTE* UncompressedImageA = NULL;
	int SizeUncompressedImageA = 0;

	// decompressed both parts, color and alpha
	if (Input->CompressedImageRGB.GetBulkDataSize())
	{
		BYTE* CompressedImageRGB =
			(BYTE *)Input->CompressedImageRGB.Lock(LOCK_READ_ONLY);

		INT SizeCompressedImageRGB = Input->CompressedImageRGB.GetBulkDataSize();

		UncompressedImageRGBA =
			SubstanceAir::Helpers::DecompressJpeg(
				CompressedImageRGB,
				SizeCompressedImageRGB,
				&Width,
				&Height);

		Input->CompressedImageRGB.Unlock();
	}

	if (Input->CompressedImageA.GetBulkDataSize())
	{
		BYTE* CompressedImageA =
			(BYTE *)Input->CompressedImageA.Lock(LOCK_READ_ONLY);

		INT SizeCompressedImageA = Input->CompressedImageA.GetBulkDataSize();

		UncompressedImageA =
			SubstanceAir::Helpers::DecompressJpeg(
			CompressedImageA,
			SizeCompressedImageA,
			&Width,
			&Height,
			1);

		Input->CompressedImageA.Unlock();
	}

	// recompose the two parts in one single image buffer
	if (UncompressedImageA && UncompressedImageRGBA)
	{
		INT OutptrSize = Width * Height * 4;
		*Outptr = UncompressedImageRGBA;

		Join_RGBA_8bpp(
			Width, Height,
			UncompressedImageRGBA, SizeUncompressedImageRGBA, 
			UncompressedImageA, SizeUncompressedImageA);

		appFree(UncompressedImageA);
		UncompressedImageA = NULL;
	}
	else if (UncompressedImageRGBA)
	{
		INT OutptrSize = Width * Height * 4;
		*Outptr = UncompressedImageRGBA;
	}
	else if (UncompressedImageA)
	{
		appFree(UncompressedImageA);
		UncompressedImageA = NULL;
		check(0);
	}
}


std::tr1::shared_ptr<ImageInput> PrepareFileImageInput(USubstanceAirImageInput* Input)
{
	if (NULL == Input)
	{
		return std::tr1::shared_ptr<ImageInput>();
	}

	INT Width;
	INT Height;
	BYTE* UncompressedDataPtr = NULL;

	PrepareFileImageInput_GetBGRA(Input, &UncompressedDataPtr, Width, Height);

	if (UncompressedDataPtr)
	{
		SubstanceTexture texture = {
			UncompressedDataPtr,
			Width,
			Height,
			Substance_PF_RGBA,
			Substance_ChanOrder_BGRA,
			1};

			std::tr1::shared_ptr<ImageInput> res = ImageInput::create(texture);
			appFree(UncompressedDataPtr);

			return res;
	}
	else
	{
		return std::tr1::shared_ptr<ImageInput>();
	}
}


std::tr1::shared_ptr<ImageInput> PrepareSbsImageInput(USubstanceAirTexture2D* Input)
{
	if (NULL == Input)
	{
		return std::tr1::shared_ptr<ImageInput>();
	}

	if (!Input->OutputCopy || !Input->OutputCopy->ParentInstance || !Input->OutputCopy->ParentInstance->Parent)
	{
		return std::tr1::shared_ptr<ImageInput>();
	}

	void* MipData = NULL;
	INT MipSizeX = 0;
	INT MipSizeY = 0;
	FTexture2DMipMap* Mip = &Input->Mips(0);

	if ((GUseSeekFreeLoading || GIsCooking) && !GUseSubstanceInstallTimeCache)
	{
		SubstanceAir::List<graph_inst_t*> Graphs;
		graph_inst_t* GraphInst = Input->OutputCopy->GetParentGraphInstance();

		if (NULL == GraphInst)
		{
			debugf(TEXT("Error preparing image input for %s"), *(Input->GetFullName()));
			return std::tr1::shared_ptr<ImageInput>();
		}

		Mip = &Input->Mips(0);
		INT MipSize = Mip->Data.GetBulkDataSize();
		MipData = appMalloc(MipSize);
		check(MipData);

		appMemcpy(MipData, Mip->Data.Lock(LOCK_READ_ONLY), MipSize);
		Mip->Data.Unlock();

		MipSizeX = Mip->SizeX;
		MipSizeY = Mip->SizeY;
	}
	else if (GUseSubstanceInstallTimeCache && !GIsCooking)
	{
		FString TextureName;
		Input->GetFullName().ToLower().Split(TEXT(" "), NULL, &TextureName);

		MipData = appMalloc(Mip->Data.GetBulkDataSize());
		check(MipData);

		GSubstanceAirTextureReader->getMipMap(
			TCHAR_TO_UTF8(*TextureName),
			MipSizeX, 
			MipSizeY, 
			(char*)MipData, 
			Mip->Data.GetBulkDataSize());
	}
	else
	{
		INT MipSize = Mip->Data.GetBulkDataSize();
		MipData = appMalloc(MipSize);
		appMemcpy(MipData, Mip->Data.Lock(LOCK_READ_ONLY), MipSize);
		Mip->Data.Unlock();
		MipSizeX = Mip->SizeX;
		MipSizeY = Mip->SizeY;
	}

	if (NULL == MipData)
	{
		return std::tr1::shared_ptr<ImageInput>();
	}

	SubstanceTexture Texture = {
		MipData,
		MipSizeX,
		MipSizeY,
		Helpers::Ue3FormatToSubstance((EPixelFormat)Input->Format),
		Substance_ChanOrder_NC,
		1};

	std::tr1::shared_ptr<ImageInput> NewImageInput = ImageInput::create(Texture);

	appFree(MipData);

	return NewImageInput;
}


void LinkImageInput(USubstanceAirImageInput* BmpImageInput, FImageInputInstance* ImgInputInst) 
{
	BmpImageInput->Inputs.AddUniqueItem(ImgInputInst);

	// look for eventual inputs to unplug from the image Input
	TArrayNoInit<SubstanceAir::FImageInputInstance*>::TIterator 
		ItInput(BmpImageInput->Inputs);

	TArray<SubstanceAir::FImageInputInstance*> ObsoleteInputs;

	for (;ItInput;++ItInput)
	{
		if ((*ItInput)->ImageSource != BmpImageInput)
		{
			ObsoleteInputs.AddItem(*ItInput);
		}
	}

	TArray<SubstanceAir::FImageInputInstance*>::TIterator 
		ItObsolete(ObsoleteInputs);

	for (;ItObsolete;++ItObsolete)
	{
		BmpImageInput->Inputs.RemoveItem(*ItObsolete);
	}
}


UBOOL ImageInputLoop(USubstanceAirTexture2D* SbsImageInput, FGraphInstance* ImgInputInstParent)
{
	if (SbsImageInput->ParentInstance->Instance == ImgInputInstParent)
	{
		return TRUE;
	}

	return FALSE;
}


std::tr1::shared_ptr<ImageInput> PrepareImageInput(
	UObject* Image, 
	FImageInputInstance* ImgInputInst,
	FGraphInstance* ImgInputInstParent)
{
	USubstanceAirImageInput* BmpImageInput = Cast<USubstanceAirImageInput>(Image);

	if (BmpImageInput)
	{
		// link the image Input with the Input
		LinkImageInput(BmpImageInput, ImgInputInst);
		return PrepareFileImageInput(BmpImageInput);
	}

	USubstanceAirTexture2D* SbsImageInput = Cast<USubstanceAirTexture2D>(Image);

	if (SbsImageInput)
	{
		if (NULL == SbsImageInput->OutputCopy ||
			SbsImageInput->OutputCopy->isDirty())
		{
			return std::tr1::shared_ptr<ImageInput>();
		}

		if (ImageInputLoop(SbsImageInput, ImgInputInstParent))
		{
			return std::tr1::shared_ptr<ImageInput>();
		}

		return PrepareSbsImageInput(SbsImageInput);
	}

	return std::tr1::shared_ptr<ImageInput>();
}


TArray< INT > GetValueInt(const std::tr1::shared_ptr<input_inst_t>& Input)
{
	TArray< INT > Value;

	switch(Input->Desc->Type)
	{
	case Substance_IType_Integer:
		{
			SubstanceAir::FNumericalInputInstance<int_t>* TypedInst =
				(SubstanceAir::FNumericalInputInstance<int_t>*)&(*Input);
			Value.AddItem(TypedInst->Value);
		}
		break;
	case Substance_IType_Integer2:
		{
			SubstanceAir::FNumericalInputInstance<vec2int_t>* TypedInst =
				(SubstanceAir::FNumericalInputInstance<vec2int_t>*)&(*Input);

			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
		}
		break;

	case Substance_IType_Integer3:
		{
			SubstanceAir::FNumericalInputInstance<vec3int_t>* TypedInst = 
				(SubstanceAir::FNumericalInputInstance<vec3int_t>*)&(*Input);

			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
			Value.AddItem(TypedInst->Value.Z);
		}
		break;

	case Substance_IType_Integer4:
		{
			SubstanceAir::FNumericalInputInstance<vec4int_t>* TypedInst = 
				(SubstanceAir::FNumericalInputInstance<vec4int_t>*)&(*Input);
			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
			Value.AddItem(TypedInst->Value.Z);
			Value.AddItem(TypedInst->Value.W);
		}
		break;
	}

	return Value;
}


TArray< FLOAT > GetValueFloat(const std::tr1::shared_ptr<input_inst_t>& Input)
{
	TArray< FLOAT > Value;

	switch(Input->Desc->Type)
	{
	case Substance_IType_Float:
		{
			SubstanceAir::FNumericalInputInstance<float_t>* TypedInst =
				(SubstanceAir::FNumericalInputInstance<float_t>*)&(*Input);
			Value.AddItem(TypedInst->Value);
		}
		break;
	case Substance_IType_Float2:
		{
			SubstanceAir::FNumericalInputInstance<vec2float_t>* TypedInst =
				(SubstanceAir::FNumericalInputInstance<vec2float_t>*)&(Input);

			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
		}
		break;

	case Substance_IType_Float3:
		{
			SubstanceAir::FNumericalInputInstance<vec3float_t>* TypedInst = 
				(SubstanceAir::FNumericalInputInstance<vec3float_t>*)&(*Input);

			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
			Value.AddItem(TypedInst->Value.Z);
		}
		break;

	case Substance_IType_Float4:
		{
			SubstanceAir::FNumericalInputInstance<vec4float_t>* TypedInst = 
				(SubstanceAir::FNumericalInputInstance<vec4float_t>*)&(*Input);
			Value.AddItem(TypedInst->Value.X);
			Value.AddItem(TypedInst->Value.Y);
			Value.AddItem(TypedInst->Value.Z);
			Value.AddItem(TypedInst->Value.W);
		}
		break;
	}

	return Value;
}


FString GetValueString(const std::tr1::shared_ptr<input_inst_t>& InInput)
{
	FString ValueStr;

	switch((SubstanceInputType)InInput->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float_t>* Input =
				(FNumericalInputInstance<float_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%f"), 
				Input->Value);
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Input =
				(FNumericalInputInstance<vec2float_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%f,%f"),
				Input->Value.X,
				Input->Value.Y);
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Input =
				(FNumericalInputInstance<vec3float_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%f,%f,%f"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z);
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Input =
				(FNumericalInputInstance<vec4float_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%f,%f,%f,%f"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z,
				Input->Value.W);
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int_t>* Input =
				(FNumericalInputInstance<int_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%d"), 
				(INT)Input->Value);
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Input =
				(FNumericalInputInstance<vec2int_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%d,%d"),
				Input->Value.X,
				Input->Value.Y);
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Input =
				(FNumericalInputInstance<vec3int_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%d,%d,%d"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z);
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Input =
				(FNumericalInputInstance<vec4int_t>*)InInput.get();

			ValueStr = FString::Printf(
				TEXT("%d,%d,%d,%d"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z,
				Input->Value.W);
		}
		break;
	case Substance_IType_Image:
		{
			FImageInputInstance* Input =
				(FImageInputInstance*)InInput.get();

			if (Input->ImageSource)
			{
				Input->ImageSource->GetFullName().Split(TEXT(" "), NULL, &ValueStr);
			}
			else
			{
				ValueStr = TEXT("NULL");
			}			
		}
		break;
	default:
		break;
	}

	return ValueStr;
}




void Cleanup( USubstanceAirGraphInstance* InstanceContainer )
{
	if (InstanceContainer->Instance != NULL)
	{
		TArray<GraphImageInputPair_t>::TIterator ItInp(DelayedImageInputs);

		for(;ItInp;++ItInp)
		{
			img_input_inst_t* ImgInput = (*ItInp).first;
			graph_inst_t* Instance = (*ItInp).second;

			if (Instance == InstanceContainer->Instance)
			{
				DelayedImageInputs.Remove(ItInp.GetIndex());
				ItInp.Reset();
			}
		}

		LoadingQueue.RemoveItem(InstanceContainer->Instance);
		PriorityLoadingQueue.RemoveItem(InstanceContainer->Instance);

		SubstanceAir::List<output_inst_t>::TIterator 
			ItOut(InstanceContainer->Instance->Outputs.itfront());

		for(; ItOut ; ++ItOut)
		{
			SubstanceAir::Helpers::Disable(&(*ItOut));
		}
		
		// the desc may have already forced
		// the UnSubscribtion for its instances
		if(InstanceContainer->Instance->Desc)
		{
			InstanceContainer->Instance->Desc->InstanceUids.RemoveItem(
				InstanceContainer->Instance->InstanceGuid);

			InstanceContainer->Instance->Desc->UnSubscribe(
				InstanceContainer->Instance);
		}

		delete InstanceContainer->Instance;
		InstanceContainer->Instance = 0;
		InstanceContainer->Parent = 0;
	}
}

UBOOL IsSupportedImageInput( UObject* CandidateImageInput)
{
	if (NULL == CandidateImageInput)
	{
		return FALSE;
	}

	if (CandidateImageInput->IsPendingKill())
	{
		return FALSE;
	}

	if (Cast<USubstanceAirImageInput>(CandidateImageInput) || 
		Cast<USubstanceAirTexture2D>(CandidateImageInput))
	{
		return TRUE;
	}

	return FALSE;
}


template<typename T> void resetInputNumInstance(input_inst_t* Input)
{
	FNumericalInputInstance<T>* TypedInput = (FNumericalInputInstance<T>*)Input;

	FNumericalInputDesc<T>* InputDesc= 
		(FNumericalInputDesc<T>*)Input->Desc;

	TypedInput->Value = InputDesc->DefaultValue;
}


void ResetToDefault(graph_inst_t* Instance)
{
	SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInput(Instance->Inputs.itfront());

	for (;ItInput;++ItInput)
	{
		switch((SubstanceInputType)ItInput->get()->Desc->Type)
		{
		case Substance_IType_Float:
			{
				resetInputNumInstance<float_t>(ItInput->get());
			}
			break;
		case Substance_IType_Float2:
			{
				resetInputNumInstance<vec2float_t>(ItInput->get());
			}
			break;
		case Substance_IType_Float3:
			{
				resetInputNumInstance<vec3float_t>(ItInput->get());
			}
			break;
		case Substance_IType_Float4:
			{
				resetInputNumInstance<vec4float_t>(ItInput->get());
			}
			break;
		case Substance_IType_Integer:
			{
				resetInputNumInstance<int_t>(ItInput->get());
			}
			break;
		case Substance_IType_Integer2:
			{
				resetInputNumInstance<vec2int_t>(ItInput->get());
			}
			break;
		case Substance_IType_Integer3:
			{
				resetInputNumInstance<vec3int_t>(ItInput->get());
			}
			break;
		case Substance_IType_Integer4:
			{
				resetInputNumInstance<vec4int_t>(ItInput->get());
			}
			break;
		case  Substance_IType_Image:
			{
				FImageInputInstance* TypedInput = (FImageInputInstance*)ItInput->get();

				FImageInputDesc* InputDesc= 
					(FImageInputDesc*)ItInput->get()->Desc;

				TypedInput->SetImageInput(NULL, Instance);
			}
			break;
		default:
			break;
		}
		
	}

	// force outputs to dirty so they can be updated
	SubstanceAir::List<output_inst_t>::TIterator 
		ItOut(Instance->Outputs.itfront());

	for (;ItOut;++ItOut)
	{
		ItOut->flagAsDirty();
	}
}


void RegisterOutputAsImageInput(USubstanceAirGraphInstance* G, USubstanceAirTexture2D* T)
{
	if (NULL == ActiveImageInputs.Find(G))
	{
		TMap<UObject*, INT> TexturesUsage;
		TexturesUsage.Set(T, 0);
		ActiveImageInputs.Set(G, TexturesUsage);
	}

	TMap<UObject*, INT>* TexturesUsage = ActiveImageInputs.Find(G);

	if (TexturesUsage->Find(T))
	{
		++(*TexturesUsage->Find(T));
	}
	else
	{
		TexturesUsage->Set(T, 1);
	}

	check(*TexturesUsage->Find(T) > 0);
}


void UnregisterOutputAsImageInput(USubstanceAirGraphInstance* Graph)
{
	ActiveImageInputs.RemoveKey(Graph);
}


void UnregisterOutputAsImageInput(USubstanceAirTexture2D* Texture, UBOOL bUnregisterAll, UBOOL bUpdateGraphInstance)
{
	TMap< USubstanceAirGraphInstance*, TMap< UObject*, INT > >::TIterator
		ItMap(ActiveImageInputs);

	for(;ItMap;++ItMap)
	{
		if (ItMap.Value().Find(Texture))
		{
			TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(ItMap.Key()->Instance->Inputs.itfront());
			for (; ItInInst ; ++ItInInst)
			{
				if (ItInInst->get()->Desc && ItInInst->get()->Desc->IsImage())
				{
					img_input_inst_t* ImgInput = (img_input_inst_t*)(ItInInst->get());

					ImgInput->SetImageInput(
						ImgInput->ImageSource,
						ItMap.Key()->Instance,
						FALSE);

					INT ModifiedOuputs = 0;

					for (UINT Idx=0 ; Idx<ImgInput->Desc->AlteredOutputUids.size() ; ++Idx)
					{
						output_inst_t* OutputModified = ItMap.Key()->Instance->GetOutput(ImgInput->Desc->AlteredOutputUids(Idx));

						if (OutputModified && OutputModified->bIsEnabled)
						{
							OutputModified->flagAsDirty();
							++ModifiedOuputs;
						}
					}

					if (ModifiedOuputs)
					{
						ItMap.Key()->MarkPackageDirty();
					}

					SubstanceAir::Helpers::PushDelayedRender(ItMap.Key()->Instance);
				}
			}
		}
	}
}


} // namespace Helpers
} // namespace SubstanceAir
