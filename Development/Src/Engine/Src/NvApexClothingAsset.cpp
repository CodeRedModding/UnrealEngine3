/*=============================================================================
	NvApexClothingAsset.cpp : Manages APEX clothign assets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// This code contains NVIDIA Confidential Information and is disclosed
// under the Mutual Non-Disclosure Agreement.
//
// Notice
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright 2009-2010 NVIDIA Corporation. All rights reserved.
// Copyright 2002-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright 2001-2006 NovodeX. All rights reserved.

#include "EnginePrivate.h"

#if WITH_NOVODEX

#include "NvApexManager.h"

#endif

#include "EngineMeshClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "NvApexScene.h"
#include "UnNovodexSupport.h"
#include "NvApexCommands.h"
#include "UnNovodexSupport.h"

#if WITH_APEX

#include <NxApexSDK.h>
#include <NxClothingAsset.h>
#include <NxUserRenderResourceManager.h>
#include <NxParameterized.h>
#include "NvApexCommands.h"
#include "NvApexGenericAsset.h"
using namespace physx::apex;
#endif

void SetApexMatIndices(UApexAsset* Asset);
void SetUniqueAssetMaterialNames(UApexAsset* Asset, FIApexAsset* ApexAssetWrapper);

/*
*	UApexClothingAsset
*
*/
IMPLEMENT_CLASS(UApexClothingAsset);

void UApexClothingAsset::PostLoad()
{
#if WITH_APEX
	NxClothingAsset* ClothingAsset = static_cast<NxClothingAsset*>(MApexAsset->GetNxApexAsset());
	check(ClothingAsset);
	// initializes the lod material mapping
	if(LodMaterialInfo.Num() == 0)
	{
		UINT GraphicalLODNum = ClothingAsset->getNumGraphicalLodLevels();
		LodMaterialInfo.Empty(GraphicalLODNum);
		for(UINT LodLevel = 0; LodLevel < GraphicalLODNum; ++LodLevel)
		{
			const NxRenderMeshAsset* RenderMeshAsset = ClothingAsset->getRenderMeshAsset(LodLevel);
			UINT SubmeshCount = RenderMeshAsset->getSubmeshCount();
			FClothingLodInfo ClothingLodInfo;
			ClothingLodInfo.LODMaterialMap.Empty(SubmeshCount);
			for(UINT j = 0; j <SubmeshCount; ++j)
			{
				ClothingLodInfo.LODMaterialMap.AddItem(j);
			}
			LodMaterialInfo.AddItem(ClothingLodInfo);
		}
	}
	if ( GApexManager )
	{
		NxApexSDK *apexSDK = GApexManager->GetApexSDK();
		NxResourceProvider* ApexResourceProvider = apexSDK->getNamedResourceProvider();
		if( ApexResourceProvider )
		{
			for(INT LodLevel = 0; LodLevel < LodMaterialInfo.Num(); ++LodLevel)
			{
				const NxRenderMeshAsset* RenderMeshAsset = ClothingAsset->getRenderMeshAsset(LodLevel);
				for(INT SubmeshID = 0; SubmeshID < LodMaterialInfo(LodLevel).LODMaterialMap.Num(); ++SubmeshID)
				{
					UMaterialInterface* Material = Materials(LodMaterialInfo(LodLevel).LODMaterialMap(SubmeshID));
					if(Material == NULL || !Material->CheckMaterialUsage(MATUSAGE_APEXMesh))
					{
						Material = GEngine->DefaultMaterial;
					}
					ApexResourceProvider->setResource( "ApexMaterials", RenderMeshAsset->getMaterialName(SubmeshID), Material);
				}
			}
		}
	}
#endif

#if WITH_APEX && !WITH_APEX_SHIPPING
	if( GIsEditor && !GIsGame )
	{
		// old assets are deprecated
		check(ApexClothingLibrary_DEPRECATED == NULL);
		if ( MApexAsset != NULL)
		{
			// Give materials unique names inside of APEX asset so we can override materials in the actor
			if ( !bHasUniqueAssetMaterialNames )
			{
				SetUniqueAssetMaterialNames( this, MApexAsset );
				bHasUniqueAssetMaterialNames = TRUE;
			}
		}
	}
#endif
	Super::PostLoad();
}

void UApexClothingAsset::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

#if WITH_APEX
	InitializeApex();
	DWORD bAssetValid = MApexAsset != NULL ? 1 : 0;
#else
	DWORD bAssetValid = 1;
#endif
	Ar.SerializeInt( bAssetValid, 1 );

	if( bAssetValid >= 1  )
	{
		if( Ar.IsLoading() )
		{
			TArray<BYTE> NameBuffer;
			UINT NameBufferSize;
			Ar << NameBufferSize;
			NameBuffer.Add( NameBufferSize );
			Ar.Serialize( NameBuffer.GetData(), NameBufferSize );

			TArray<BYTE> Buffer;
			UINT Size;
			Ar << Size;
			Buffer.Add( Size );
			Ar.Serialize( Buffer.GetData(), Size );
#if WITH_APEX
			if ( MApexAsset != NULL )
			{
				MApexAsset->DecRefCount(0);
				MApexAsset = NULL;
			}
			char scratch[1024];
			GetApexAssetName(this,scratch,1024,".aca");
     		MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, (const void *)Buffer.GetData(), (NxU32)Size,  TCHAR_TO_ANSI(*OriginalApexName), this );
			if ( MApexAsset )
			{
				MApexAsset->IncRefCount(0);
				assert( MApexAsset->GetType() == AAT_CLOTHING );
			}
#else
#endif
		}
		else if ( Ar.IsSaving() )
		{
			const char *name = "NO_APEX";
#if WITH_APEX
			name = MApexAsset->GetAssetName();
#endif
			if( name )
			{
				DWORD nameBufferSize = strlen( name )+1;
				Ar << nameBufferSize;
				Ar.Serialize( (void*)name, nameBufferSize );
			}
			else
			{
				DWORD nullNameBufferSize  = 2;
				Ar << nullNameBufferSize;
				Ar.Serialize( (void *)"", nullNameBufferSize );
			}
#if WITH_APEX
			assert( MApexAsset->GetType() == AAT_CLOTHING );
			NxU32 dlen=0;
			const void * data = MApexAsset->Serialize(dlen);
			Ar << dlen;
			if ( data )
			{
				Ar.Serialize( (void *)data, dlen );
				MApexAsset->ReleaseSerializeMemory(data);
			}
#else
			DWORD size=0;
			Ar << size;
#endif
		}
	}
}

UBOOL UApexClothingAsset::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	UBOOL ret = Super::Rename(InName,NewOuter,Flags);
	if ( MApexAsset )
	{
#if WITH_APEX
		MApexAsset->Rename(TCHAR_TO_ANSI(InName));
#endif
	}
	return ret;
}

TArray<FString> UApexClothingAsset::GetGenericBrowserInfo()
{
	TArray<FString> Info;
#if WITH_APEX
	if( MApexAsset )
	{
		FString nameString( MApexAsset->GetAssetName() );
		Info.AddItem( FString::Printf( TEXT("%s"), *nameString ) );
		Info.AddItem(TEXT("APEX CLOTHING ASSET") );
	}
#endif
	return Info;

}

UBOOL UApexClothingAsset::Import( const BYTE* Buffer, INT BufferSize, const FString& Name,UBOOL convertToUE3Coordinates)
{
	UBOOL ret = FALSE;
#if WITH_APEX && !FINAL_RELEASE && !WITH_APEX_SHIPPING
	InitializeApex();
	if ( GApexCommands )
	{
		if ( MApexAsset )
		{
			MApexAsset->DecRefCount(0);
			MApexAsset = NULL;
		}
		char scratch[1024];
		GetApexAssetName(this,scratch,1024,".aca");

		MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, (const void *)Buffer, (NxU32)BufferSize, GApexManager->GetCurrentImportFileName(), this );

		if ( MApexAsset )
		{
			AnsiToString( MApexAsset->GetOriginalApexName(),OriginalApexName );
			assert( MApexAsset->GetType() == AAT_CLOTHING );
			MApexAsset->IncRefCount(0);
			if ( convertToUE3Coordinates )
			{
				physx::PxMat44 mat = physx::PxMat44::createIdentity();
				physx::PxF32 *temp = (physx::PxF32 *)mat.front();
				temp[1*4+1] = -1;
				MApexAsset->ApplyTransformation(mat,U2PScale,TRUE,TRUE,FALSE);
			}

			// Set the correct number of materials
			Materials.AddZeroed(MApexAsset->GetNumMaterials());

			// Give materials unique names inside of APEX asset so we can override materials in the actor
			SetUniqueAssetMaterialNames( this, MApexAsset );
			bHasUniqueAssetMaterialNames = TRUE;

			GApexCommands->AddUpdateMaterials(this);
			NxClothingAsset* ClothingAsset = static_cast<NxClothingAsset*>(MApexAsset->GetNxApexAsset());
			check(ClothingAsset);
			// initializes the lod material mapping
			UINT GraphicalLODNum = ClothingAsset->getNumGraphicalLodLevels();
			LodMaterialInfo.Empty(GraphicalLODNum);
			for(UINT LodLevel = 0; LodLevel < GraphicalLODNum; ++LodLevel)
			{
				const NxRenderMeshAsset* RenderMeshAsset = ClothingAsset->getRenderMeshAsset(LodLevel);
				UINT SubmeshCount = RenderMeshAsset->getSubmeshCount();
				FClothingLodInfo ClothingLodInfo;
				ClothingLodInfo.LODMaterialMap.Empty(SubmeshCount);
				for(UINT j = 0; j <SubmeshCount; ++j)
				{
					ClothingLodInfo.LODMaterialMap.AddItem(j);
				}
				LodMaterialInfo.AddItem(ClothingLodInfo);
			}
			physx::apex::NxClothingAsset* asset = static_cast<physx::apex::NxClothingAsset*>(MApexAsset->GetNxApexAsset());
			if ( asset )
			{
				if (asset->getClothSolverMode() == physx::apex::ClothSolverMode::v3)
				{
					bUseLocalSpaceSimulation = true;
				}
			}

			ret = TRUE;
		}
	}
#endif
	return ret;
}

UBOOL UApexClothingAsset::Export(const FName& Name, UBOOL isKeepUE3Coords)
{
	UBOOL	ret	= TRUE;

#if WITH_APEX && !FINAL_RELEASE && !WITH_APEX_SHIPPING
	ret	= FALSE;

	const NxParameterized::Interface*		params			= NULL;
	NxParameterized::Interface*				paramsDestroy	= NULL;
	physx::general_PxIOStream2::PxFileBuf*	writeStream		= NULL;
	NxParameterized::Serializer*			serializer		= NULL; 
	FIApexAsset*							apexAsset		= NULL;
			
	do
	{
		paramsDestroy	= (NxParameterized::Interface*)GetAssetNxParameterized();
		params			= (const NxParameterized::Interface*)paramsDestroy;
		if ( !isKeepUE3Coords )
		{
			apexAsset = GApexCommands->GetApexAssetFromParams("APEXExporterAsset.tmp", params, NULL, NULL );
			if ( !apexAsset )
				break;

			apexAsset->IncRefCount(0);

			physx::PxMat44	mat		= physx::PxMat44::createIdentity();
			physx::PxF32*	temp	= (physx::PxF32 *)mat.front();
			physx::PxF32	scale	= apexAsset->GetType() == AAT_CLOTHING ? P2UScale : 1.0f;
			temp[1*4+1] = -1;
			apexAsset->ApplyTransformation(mat,scale,TRUE,TRUE,FALSE);	
			// paramsDestroy will be released at ApplyTransformation(), reset it.
			paramsDestroy			= NULL;

			params					= apexAsset->GetNxApexAsset()->getAssetNxParameterized();
		}
		
		writeStream	= GApexManager->GetApexSDK()->createMemoryWriteStream();
		if (!writeStream)
			break;

		serializer	= physx::NxGetApexSDK()->createSerializer(NxParameterized::Serializer::NST_XML);

		NxParameterized::Serializer::ErrorType serError = serializer->serialize(*writeStream, (const ::NxParameterized::Interface **)&params, 1);

		FArchive* OutputFile = GFileManager->CreateFileWriter(*UExporter::CurrentFilename, FILEWRITE_AllowRead);
		if (!OutputFile)
			break;

		physx::PxU32 bufLen = 0;
		const void* buf = GApexManager->GetApexSDK()->getMemoryWriteBuffer(*writeStream, bufLen);
		OutputFile->Serialize( (void*)buf, bufLen );
		delete OutputFile;
		ret	= TRUE;
	} while(0);
	
	if (serializer)
		serializer->release();
	if (writeStream)
		writeStream->release();
	if (apexAsset)
		apexAsset->DecRefCount(0);
	if ( paramsDestroy)
		paramsDestroy->destroy();
#endif

	return ret;
}

#if WITH_APEX
static const char * getNameClothing(const char *src)
{
	if (src == NULL ) src = "";
	NxU32 len = (NxU32) strlen(src);
	char *temp = (char *) appMalloc(len+1);
	memcpy(temp,src,len+1);
	return temp;
}
#endif

void UApexClothingAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITORONLY_DATA
#if WITH_APEX
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("Materials")))
	{
		INT Mcount = Materials.Num();
		if ( Mcount )
		{
			for(INT MatIdx=0; MatIdx<Mcount; ++MatIdx)
			{
				UMaterialInterface *Material = Materials(MatIdx);
				InitMaterialForApex(Material);
			}
		}
	}
	else if( PropertyThatChanged ) // for any other property, cause the actor to be re-created.
	{
		MApexAsset->RefreshActors();
	}
#endif
#endif // WITH_EDITORONLY_DATA
}

/** Fix up unique material names in the APEX asset after rename */
void UApexClothingAsset::PostRename()
{
	Super::PostRename();
#if WITH_EDITOR && WITH_APEX
	OnApexAssetLost();
	SetUniqueAssetMaterialNames( this, MApexAsset );
	bHasUniqueAssetMaterialNames = TRUE;
#endif
}

/** Fix up unique material names in the APEX asset after duplication */
void UApexClothingAsset::PostDuplicate()
{
	Super::PostDuplicate();
#if WITH_EDITOR && WITH_APEX
	SetUniqueAssetMaterialNames( this, MApexAsset );
	bHasUniqueAssetMaterialNames = TRUE;
#endif
}

void UApexClothingAsset::BeginDestroy(void)
{
	Super::BeginDestroy();
#if WITH_APEX
	if ( GApexCommands )
	{
		GApexCommands->NotifyDestroy(this);
	}
	if ( MApexAsset )
	{
		MApexAsset->DecRefCount(0);
		MApexAsset = NULL;
	}
#endif
}

void UApexClothingAsset::UpdateMaterials(void)
{
#if WITH_EDITORONLY_DATA
#if WITH_APEX
	Materials.Empty();
    if ( MApexAsset )
    {
        NxU32 NumSubmeshes = MApexAsset->GetNumMaterials();
		Materials.Empty(NumSubmeshes);
		for(UINT i=0; i<NumSubmeshes; i++)
		{
			const char         *MaterialNameA = MApexAsset->GetMaterialName(i);
			LoadMaterialForApex(MaterialNameA, Materials);
		}
	}

#endif
#endif // WITH_EDITORONLY_DATA
}

void * UApexClothingAsset::GetNxParameterized(void)
{
	void *ret = NULL;
#if WITH_APEX
	if ( MApexAsset )
	{
		::NxParameterized::Interface *params = MApexAsset->GetDefaultApexActorDesc();
		if ( params )
		{
			::NxParameterized::Handle handle(*params);
#if PS3
			// force it on for now. this should eventually be the default in apex
			NxParameterized::setParamBool(*params,"useHardwareCloth",TRUE);
#else
			UBOOL bDisablePhysXHardware = !IsPhysXHardwarePresent();
			NxParameterized::setParamBool(*params,"useHardwareCloth",bDisablePhysXHardware ? FALSE : bUseHardwareCloth);
#endif
			NxParameterized::setParamBool(*params,"useInternalBoneOrder",TRUE);
			NxParameterized::setParamBool(*params,"multiplyGlobalPoseIntoBones", FALSE);
			NxParameterized::setParamBool(*params,"updateStateWithGlobalMatrices",TRUE);
			NxParameterized::setParamBool(*params,"fallbackSkinning",bFallbackSkinning);
			NxParameterized::setParamBool(*params,"slowStart",bSlowStart);
			NxParameterized::setParamBool(*params,"flags.RecomputeNormals",bRecomputeNormals);
			NxParameterized::setParamBool(*params,"allowAdaptiveTargetFrequency",bAllowAdaptiveTargetFrequency);
			NxParameterized::setParamU32(*params,"uvChannelForTangentUpdate",UVChannelForTangentUpdate);
			NxParameterized::setParamF32(*params,"maxDistanceBlendTime",MaxDistanceBlendTime);
			NxParameterized::setParamF32(*params,"lodWeights.maxDistance",LodWeightsMaxDistance*U2PScale);
			NxParameterized::setParamF32(*params,"lodWeights.distanceWeight",LodWeightsDistanceWeight);
			NxParameterized::setParamF32(*params,"lodWeights.bias",LodWeightsBias);
			NxParameterized::setParamF32(*params,"lodWeights.benefitsBias",LodWeightsBenefitsBias);
			NxParameterized::setParamBool(*params,"localSpaceSim",bUseLocalSpaceSimulation);
		}
		ret = params;
	}
#endif
	return ret;
}

void * UApexClothingAsset::GetAssetNxParameterized(void)
{
	void * ret = NULL;
#if WITH_APEX
	if ( MApexAsset )
	{
		physx::apex::NxApexAsset *asset = MApexAsset->GetNxApexAsset();
		if ( asset )
		{
			const NxParameterized::Interface *iface = asset->getAssetNxParameterized();
			if ( iface )
			{
				NxParameterized::Interface *cp = GApexManager->GetApexSDK()->getParameterizedTraits()->createNxParameterized(iface->className());
				if ( cp )
				{
					cp->copy(*iface);
					ret = cp;
				}
			}
		}
	}
#endif
	return ret;
}
