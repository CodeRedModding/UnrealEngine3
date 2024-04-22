/*=============================================================================
	NvApexDestructibleAsset.cpp : Handles APEX destructible assets.
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
#include "NvApexManager.h"
#include "UnNovodexSupport.h"



#if WITH_APEX

#include <NxApexSDK.h>
#include <NxUserRenderResourceManager.h>
#include <NxParameterized.h>
#include "NvApexCommands.h"
#include <NxModuleDestructible.h>
#include <NxDestructibleAsset.h>
#include <NxDestructiblePreview.h>
#include <NxDestructibleActor.h>

#include <foundation/PxBounds3.h>
#include "NvApexCommands.h"
#include "NvApexGenericAsset.h"

using namespace physx::apex;

#endif

/*
	This block of code is a treatment for old assets that have garbage tangent channels.  A while back, there was a bug in PhysXLab which caused garbage tangent
	channels to be generated.  Upon deserialization, these values trigger a warning for every data element in the tangent channel buffers.  If you set
	FIXUP_OLD_DESTRUCTIBLE_ASSETS to 1, it checks the "health" of every destructible mesh's render mesh actor's tangent channel.  If any infitite values,
	NaNs, or denormalized values are found, it zeros out the channel.  You will still get the warnings before it does this.  However, if you then resave the
	package which caused this warning, it will now contain cleansed tangent channels, and there will be no warnings the next time you load the package.
*/
#define FIXUP_OLD_DESTRUCTIBLE_ASSETS	0

#if FIXUP_OLD_DESTRUCTIBLE_ASSETS
inline UBOOL PxVec3IsDenormalized(const physx::PxVec3& V)
{
	if (V.isZero())
	{
		return FALSE;
	}

	if (V.isFinite() && physx::PxAbs(V[0]) >= FLT_MIN && physx::PxAbs(V[1]) >= FLT_MIN && physx::PxAbs(V[2]) >= FLT_MIN)
	{
		return FALSE;
	}

	return TRUE;
}

static void EnsureHealthyTangentChannels(const NxRenderMeshAsset* RenderMeshAsset)
{
	for (physx::PxU32 SubmeshIndex = 0; SubmeshIndex < RenderMeshAsset->getSubmeshCount(); ++SubmeshIndex)
	{
		const physx::NxRenderSubmesh& Submesh = RenderMeshAsset->getSubmesh(SubmeshIndex);
		const physx::NxVertexBuffer& VertexBuffer = Submesh.getVertexBuffer();
		const physx::NxVertexFormat& VertexFormat = VertexBuffer.getFormat();
		physx::NxRenderDataFormat::Enum Format;
		const void* TangentChannel = VertexBuffer.getBufferAndFormat(Format, VertexFormat.getBufferIndexFromID(VertexFormat.getSemanticID(physx::NxRenderVertexSemantic::TANGENT)));
		if (TangentChannel != NULL && Format == physx::NxRenderDataFormat::FLOAT3)	// Only handling FLOAT3 tangent channels - these are probably the only ones we need to check
		{
			UBOOL bChannelHealthy = TRUE;	// Until proven otherwise
			physx::PxVec3* Tangents = (physx::PxVec3*)TangentChannel;
			for (physx::PxU32 VertexIndex = 0; VertexIndex < VertexBuffer.getVertexCount(); ++VertexIndex)
			{
				physx::PxVec3& Tangent = Tangents[VertexIndex];
				// Check that the tangent is not inf, nan, or denormalized
				if (PxVec3IsDenormalized(Tangent))
				{
					bChannelHealthy = FALSE;
					break;
				}
			}
			if (!bChannelHealthy)
			{
				// For now, "healthy" will have to mean all zero
				appMemzero(Tangents, VertexBuffer.getVertexCount()*sizeof(physx::PxVec3));
			}
		}
	}
}
#endif // FIXUP_OLD_DESTRUCTIBLE_ASSETS

/*
 *	UApexDestructibleAsset
 *
 */
IMPLEMENT_CLASS(UApexDestructibleAsset);

void UApexDestructibleAsset::PostLoad()
{
#if WITH_APEX
	if ( GApexManager )
	{
		NxApexSDK *apexSDK = GApexManager->GetApexSDK();
		NxResourceProvider* ApexResourceProvider = apexSDK->getNamedResourceProvider();
		if( ApexResourceProvider )
		{
			for(INT i=0; i<Materials.Num(); i++)
			{
				if( Materials(i) == NULL || !Materials(i)->CheckMaterialUsage(MATUSAGE_APEXMesh) )
				{
					// In APEX we map the material name to the default material since the requested material is unusable
					Materials(i) = GEngine->DefaultMaterial;
				}
				NxDestructibleAsset* DestructibleAsset = static_cast<NxDestructibleAsset*>(MApexAsset->GetNxApexAsset());
				check(DestructibleAsset);
				const NxRenderMeshAsset* RenderMeshAsset = DestructibleAsset->getRenderMeshAsset();
				ApexResourceProvider->setResource( "ApexMaterials", RenderMeshAsset->getMaterialName(i), Materials(i));
			}
		}
	}

	FixupAsset();
#endif
	Super::PostLoad();
}

void UApexDestructibleAsset::Serialize(FArchive& Ar)
{

	Super::Serialize( Ar );

#if WITH_APEX
	InitializeApex();
	DWORD bAssetValid = MApexAsset ? 1 : 0;
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
            if ( bAssetValid >= 1 )
			{
			}

			TArray<BYTE> Buffer;
			DWORD Size;
			Ar << Size;
			Buffer.Add( Size );
			Ar.Serialize( Buffer.GetData(), Size );
#if WITH_APEX
			char scratch[1024];
			GetApexAssetName(this,scratch,1024,".pda");
     		MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, (const void *)Buffer.GetData(), (NxU32)Size,  TCHAR_TO_ANSI(*OriginalApexName), this );
			if ( MApexAsset )
			{
				MApexAsset->IncRefCount(0);
				assert( MApexAsset->GetType() == AAT_DESTRUCTIBLE );
#if FIXUP_OLD_DESTRUCTIBLE_ASSETS
				NxDestructibleAsset* DestructibleAsset = static_cast<NxDestructibleAsset*>(MApexAsset->GetNxApexAsset());
				check(DestructibleAsset);
				EnsureHealthyTangentChannels(DestructibleAsset->getRenderMeshAsset());
#endif // FIXUP_OLD_DESTRUCTIBLE_ASSETS
			}
			if (Ar.Ver() < VER_CLEANUP_APEX_DESTRUCTION_VARIABLES)
			{
				DestructibleParameters.DamageParameters.DamageThreshold = DestructibleParameters.DamageThreshold_DEPRECATED;
				DestructibleParameters.DamageParameters.DamageSpread = DestructibleParameters.DamageToRadius_DEPRECATED;
				DestructibleParameters.DamageParameters.ImpactDamage = DestructibleParameters.ForceToDamage_DEPRECATED;
				DestructibleParameters.DamageParameters.ImpactResistance = DestructibleParameters.MaterialStrength_DEPRECATED;
				DestructibleParameters.DebrisParameters.DebrisLifetimeMin = DestructibleParameters.DebrisLifetimeMin_DEPRECATED;
				DestructibleParameters.DebrisParameters.DebrisLifetimeMax = DestructibleParameters.DebrisLifetimeMax_DEPRECATED;
				DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin = DestructibleParameters.DebrisMaxSeparationMin_DEPRECATED;
				DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax = DestructibleParameters.DebrisMaxSeparationMax_DEPRECATED;
				DestructibleParameters.DebrisParameters.ValidBounds = DestructibleParameters.ValidBounds_DEPRECATED;
				DestructibleParameters.AdvancedParameters.DamageCap = DestructibleParameters.DamageCap_DEPRECATED;
				DestructibleParameters.AdvancedParameters.ImpactVelocityThreshold = DestructibleParameters.ImpactVelocityThreshold_DEPRECATED;
				DestructibleParameters.AdvancedParameters.MaxChunkSpeed = DestructibleParameters.MaxChunkSpeed_DEPRECATED;
				DestructibleParameters.AdvancedParameters.MassScaleExponent = DestructibleParameters.MassScaleExponent_DEPRECATED;
				DestructibleParameters.AdvancedParameters.FractureImpulseScale = DestructibleParameters.FractureImpulseScale_DEPRECATED;
				DestructibleParameters.Flags.FORM_EXTENDED_STRUCTURES = DestructibleParameters.bFormExtendedStructures_DEPRECATED;
				for (INT i = 0; i < DestructibleParameters.DepthParameters.Num(); ++i)
				{
					FNxDestructibleDepthParameters&	depthParameter	= DestructibleParameters.DepthParameters(i);
					if (depthParameter.TAKE_IMPACT_DAMAGE_DEPRECATED)
					{
						depthParameter.ImpactDamageOverride = IDO_On;
					}
					else
					{
						depthParameter.ImpactDamageOverride = IDO_Off;
					}
				}
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
			assert( MApexAsset->GetType() == AAT_DESTRUCTIBLE );
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

TArray<FString> UApexDestructibleAsset::GetGenericBrowserInfo()
{
	TArray<FString> Info;
#if WITH_APEX
	NxDestructibleAsset* nDestructibleAsset = NULL;
	if ( MApexAsset )
	{
		NxApexAsset *a_asset = MApexAsset->GetNxApexAsset();
		nDestructibleAsset = static_cast< NxDestructibleAsset *>(a_asset);
	}
	if( nDestructibleAsset != NULL )
	{
		FString nameString( nDestructibleAsset->getName() );
		Info.AddItem( FString::Printf( TEXT("%s"), *nameString ) );
		Info.AddItem( FString::Printf( TEXT("%d Chunks, %d Levels"), nDestructibleAsset->getChunkCount(), nDestructibleAsset->getDepthCount() ) );
	}
#endif
	return Info;
}


#if WITH_APEX

static UBOOL getParamBool(::NxParameterized::Interface *pm,const char *name)
{
	UBOOL ret = FALSE;
	if ( pm )
	{
#if NX_APEX_SDK_RELEASE >= 100
		::NxParameterized::Handle handle(*pm);
#else
		::NxParameterized::Handle handle;
#endif
		if ( pm->getParameterHandle(name,handle) == ::NxParameterized::ERROR_NONE )
		{
			bool bRet;
#if NX_APEX_SDK_RELEASE >= 100
			::NxParameterized::ErrorType vret = handle.getParamBool(bRet);
#else
			::NxParameterized::ErrorType vret = pm->getParamBool(handle,bRet);
#endif
			ret = bRet ? TRUE : FALSE;
			assert( vret == ::NxParameterized::ERROR_NONE );
		}
		else
			assert(0);
	}
	return ret;
}

static void getParamF32(::NxParameterized::Interface *pm,const char *name,FLOAT &value)
{
	if( !pm ) return;
#if NX_APEX_SDK_RELEASE >= 100
	::NxParameterized::Handle handle(*pm);
#else
	::NxParameterized::Handle handle;
#endif
	if ( pm->getParameterHandle(name,handle) == ::NxParameterized::ERROR_NONE )
	{
#if NX_APEX_SDK_RELEASE >= 100
		::NxParameterized::ErrorType ret = handle.getParamF32(value);
#else
		::NxParameterized::ErrorType ret = pm->getParamF32(handle,value);
#endif
		assert( ret == ::NxParameterized::ERROR_NONE  );
	}
	else
		assert(0);
}

static void getParamInt(::NxParameterized::Interface *pm,const char *name,INT &value)
{
	if ( !pm ) return;
#if NX_APEX_SDK_RELEASE >= 100
	::NxParameterized::Handle handle(*pm);
#else
	::NxParameterized::Handle handle;
#endif
	if ( pm->getParameterHandle(name,handle) == ::NxParameterized::ERROR_NONE )
	{
#if NX_APEX_SDK_RELEASE >= 100
		::NxParameterized::ErrorType ret = handle.getParamI32(value);
#else
		::NxParameterized::ErrorType ret = pm->getParamI32(handle,value);
#endif
		if  ( ret != ::NxParameterized::ERROR_NONE  )
		{
			physx::PxU32 uv;
#if NX_APEX_SDK_RELEASE >= 100
			ret = handle.getParamU32(uv);
#else
			ret = pm->getParamU32(handle,uv);
#endif
			assert( ret == ::NxParameterized::ERROR_NONE );
			value = (INT)uv;
		}
	}
	else
		assert(0);
}


static void getParamString(::NxParameterized::Interface *pm,const char *name,FString &value)
{
	if ( !pm ) return;
#if NX_APEX_SDK_RELEASE >= 100
	::NxParameterized::Handle handle(*pm);
#else
	::NxParameterized::Handle handle;
#endif
	if ( pm->getParameterHandle(name,handle) == ::NxParameterized::ERROR_NONE )
	{
		const char *str = NULL;
#if NX_APEX_SDK_RELEASE >= 100
		handle.getParamString(str);
#else
		pm->getParamString(handle,str);
#endif
		if ( str )
		{
			value = str;
		}
	}
	else
		assert(0);
}

static void getParam(::NxParameterized::Interface *pm,const char *name,physx::PxBounds3 &value)
{
	if ( !pm ) return;
#if NX_APEX_SDK_RELEASE >= 100
	::NxParameterized::Handle handle(*pm);
#else
	::NxParameterized::Handle handle;
#endif
	if ( pm->getParameterHandle(name,handle) == ::NxParameterized::ERROR_NONE )
	{
#if NX_APEX_SDK_RELEASE >= 100
		::NxParameterized::ErrorType ret = handle.getParamBounds3(value);
#else
		::NxParameterized::ErrorType ret = pm->getParamBounds3(handle,value);
#endif
		assert( ret == ::NxParameterized::ERROR_NONE );
	}
	else
		assert(0);
}
#endif

#if WITH_APEX
static const char* GetNameDestructible(const char *Src);
static void SetApexNamedMatIndices(const char** Names, UApexAsset* Asset)
{
	// Get APEX's named resource provider
	NxResourceProvider* ApexResourceProvider = NULL;
	if ( GApexManager != NULL )
	{
		NxApexSDK* ApexSDK = GApexManager->GetApexSDK();
		if ( ApexSDK != NULL )
		{
			ApexResourceProvider = ApexSDK->getNamedResourceProvider();
		}
	}
	check( ApexResourceProvider );

	// Map unique names to material indices within APEX, must be done after ApplyMaterialRemap
	for ( INT MatIdx = 0; MatIdx < (INT)Asset->GetNumMaterials(); ++MatIdx )
	{
		UMaterialInterface* Material = Asset->GetMaterial(MatIdx);
		InitMaterialForApex(Material);
		ApexResourceProvider->setResource( APEX_MATERIALS_NAME_SPACE, Names[MatIdx], Material);
	}
}

void SetUniqueAssetMaterialNames(UApexAsset* Asset, FIApexAsset* ApexAssetWrapper)
{
	check( GIsEditor && !GIsGame );

	UBOOL bAssetNeedsUpdate = FALSE;
	INT NumMaterials = Asset->GetNumMaterials();
	FString NamePrefix = Asset->GetFullName();
	NamePrefix += TEXT(".");

	// TODO: Check to see if we really need to update the asset
#if 0
	const ::NxParameterized::Interface* Params = ApexAssetWrapper->GetNxApexAsset()->getAssetNxParameterized();

	for ( INT MatIdx = 0; MatIdx < NumMaterials; ++MatIdx )
	{
		const char* AssetMaterialName;
		char MatName[MAX_SPRINTF];

		// Read the value of materialNames[N] from APEX
		appSprintfANSI( MatName, "renderMeshAsset.materialNames[%d]", MatIdx );

		::NxParameterized::Handle MatNameHandle( *Params );
		::NxParameterized::ErrorType Err = MatNameHandle.getParameter( MatName );
		check( Err == ::NxParameterized::ERROR_NONE );
		if ( Err == ::NxParameterized::ERROR_NONE )
		{
			Err = MatNameHandle.getParamString( AssetMaterialName );
			
			// Check if it's set to the correct unique name
			FString UniqueName = NamePrefix + appItoa( MatIdx );
			if ( appStrcmp(*UniqueName, ANSI_TO_TCHAR(AssetMaterialName)) != 0 )
			{
				bAssetNeedsUpdate = TRUE;
				break;
			}
		}
	}
#else
	bAssetNeedsUpdate = TRUE;
#endif

	if ( bAssetNeedsUpdate )
	{
		// Create a list of ASCII char* names for ApplyMaterialRemap.
		const char **Names = new const char *[NumMaterials];
		for ( INT MatIdx = 0; MatIdx < NumMaterials; ++MatIdx )
		{
			// Create the unique name and convert to ASCII
			FString UniqueName = NamePrefix + appItoa( MatIdx );
			Names[MatIdx] = GetNameDestructible(TCHAR_TO_ANSI(*UniqueName));
		}

		// Write new material names into APEX asset
		ApexAssetWrapper->ApplyMaterialRemap( (physx::PxU32)NumMaterials, Names );

		// Map unique names to material indices within APEX, must be done after ApplyMaterialRemap
		SetApexNamedMatIndices( Names, Asset );

		// Free memory allocated by GetNameDestructible
		for ( INT MatIdx = 0; MatIdx < NumMaterials; ++MatIdx )
		{
			if ( Names[MatIdx] != NULL )
			{
				appFree( (void *)Names[MatIdx] );
			}
		}
		delete [] Names;
	}
}
#endif

UBOOL UApexDestructibleAsset::Import( const BYTE* Buffer, INT BufferSize, const FString& Name, UBOOL convertToUE3Coordinates )
{
#if WITH_APEX && !FINAL_RELEASE && !WITH_APEX_SHIPPING
	InitializeApex();
	OnApexAssetLost();
	if ( MApexAsset )
	{
		MApexAsset->DecRefCount(0);
	}

	char scratch[1024];
	GetApexAssetName(this,scratch,1024,".pda");
	MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, Buffer, BufferSize, GApexManager->GetCurrentImportFileName(), this );

	if ( MApexAsset )
	{
		AnsiToString( MApexAsset->GetOriginalApexName(),OriginalApexName );
		assert( MApexAsset->GetType() == AAT_DESTRUCTIBLE );
		MApexAsset->IncRefCount(0);
		::NxParameterized::Interface *params = MApexAsset->GetDefaultApexActorDesc();
		if ( params )
		{
			getParamString(params,"crumbleEmitterName",CrumbleEmitterName);
			getParamString(params,"dustEmitterName",DustEmitterName);

			getParamF32(params,"destructibleParameters.damageThreshold",			DestructibleParameters.DamageParameters.DamageThreshold);
			getParamF32(params,"destructibleParameters.damageToRadius",				DestructibleParameters.DamageParameters.DamageSpread);
			getParamF32(params,"destructibleParameters.damageCap",					DestructibleParameters.AdvancedParameters.DamageCap);
			getParamF32(params,"destructibleParameters.forceToDamage",				DestructibleParameters.DamageParameters.ImpactDamage);
			DestructibleParameters.DamageParameters.ImpactDamage *= P2UScale;
			getParamF32(params,"destructibleParameters.impactVelocityThreshold",	DestructibleParameters.AdvancedParameters.ImpactVelocityThreshold);
			DestructibleParameters.AdvancedParameters.ImpactVelocityThreshold *= P2UScale;
			getParamF32(params,"destructibleParameters.materialStrength",			DestructibleParameters.DamageParameters.ImpactResistance);
			DestructibleParameters.DamageParameters.ImpactResistance *= P2UScale;
			getParamInt(params,"supportDepth",										DestructibleParameters.SupportDepth);
			getParamInt(params,"destructibleParameters.minimumFractureDepth",		DestructibleParameters.MinimumFractureDepth);
			getParamInt(params,"destructibleParameters.impactDamageDefaultDepth",	DestructibleParameters.DamageParameters.DefaultImpactDamageDepth);
			getParamInt(params,"destructibleParameters.debrisDepth",				DestructibleParameters.DebrisDepth);
			getParamInt(params,"destructibleParameters.essentialDepth",				DestructibleParameters.EssentialDepth);
			getParamF32(params,"destructibleParameters.debrisLifetimeMin",			DestructibleParameters.DebrisParameters.DebrisLifetimeMin);
			getParamF32(params,"destructibleParameters.debrisLifetimeMax",			DestructibleParameters.DebrisParameters.DebrisLifetimeMax);
			getParamF32(params,"destructibleParameters.debrisMaxSeparationMin",		DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin);
			getParamF32(params,"destructibleParameters.debrisMaxSeparationMax",		DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax);

			physx::PxBounds3 b;
			getParam(params,"destructibleParameters.validBounds",b);

			if( b.isEmpty() )
			{
				DestructibleParameters.DebrisParameters.ValidBounds.IsValid = FALSE;
			}
			else
			{
				DestructibleParameters.DebrisParameters.ValidBounds.Min.X = b.minimum.x * P2UScale;
				DestructibleParameters.DebrisParameters.ValidBounds.Min.Y = b.minimum.y * P2UScale;
				DestructibleParameters.DebrisParameters.ValidBounds.Min.Z = b.minimum.z * P2UScale;
														
				DestructibleParameters.DebrisParameters.ValidBounds.Max.X = b.maximum.x * P2UScale;
				DestructibleParameters.DebrisParameters.ValidBounds.Max.Y = b.maximum.y * P2UScale;
				DestructibleParameters.DebrisParameters.ValidBounds.Max.Z = b.maximum.z * P2UScale;

				DestructibleParameters.DebrisParameters.ValidBounds.IsValid = TRUE;
			}

			getParamF32(params,"destructibleParameters.maxChunkSpeed",				DestructibleParameters.AdvancedParameters.MaxChunkSpeed);
			DestructibleParameters.AdvancedParameters.MaxChunkSpeed *= P2UScale;
			

			DestructibleParameters.Flags.ACCUMULATE_DAMAGE = 		getParamBool(params,"destructibleParameters.flags.ACCUMULATE_DAMAGE");
			DestructibleParameters.Flags.ASSET_DEFINED_SUPPORT =   	getParamBool(params,"useAssetDefinedSupport");
			DestructibleParameters.Flags.WORLD_SUPPORT =           	getParamBool(params,"useWorldSupport");
			DestructibleParameters.Flags.DEBRIS_TIMEOUT =          	getParamBool(params,"destructibleParameters.flags.DEBRIS_TIMEOUT");
			DestructibleParameters.Flags.DEBRIS_MAX_SEPARATION =   	getParamBool(params,"destructibleParameters.flags.DEBRIS_MAX_SEPARATION");
			DestructibleParameters.Flags.CRUMBLE_SMALLEST_CHUNKS = 	getParamBool(params,"destructibleParameters.flags.CRUMBLE_SMALLEST_CHUNKS");
			DestructibleParameters.Flags.ACCURATE_RAYCASTS =       	getParamBool(params,"destructibleParameters.flags.ACCURATE_RAYCASTS");
			DestructibleParameters.Flags.USE_VALID_BOUNDS =        	getParamBool(params,"destructibleParameters.flags.USE_VALID_BOUNDS");
			DestructibleParameters.Flags.FORM_EXTENDED_STRUCTURES = getParamBool(params,"formExtendedStructures");


   			getParamF32(params,"destructibleParameters.fractureImpulseScale",DestructibleParameters.AdvancedParameters.FractureImpulseScale);

			// Check for a sane maximum number of depth parameters; some older APEX assets may have more
			// sets of depth parameters than the actual number of fracture levels.
			physx::PxU32 NumFractureLevels = 256;
			{
				NxDestructibleAsset* ApexDestructibleAsset = NULL;
				NxApexAsset* ApexAsset = MApexAsset->GetNxApexAsset();
				ApexDestructibleAsset = static_cast<NxDestructibleAsset*>(ApexAsset);
				if( ApexDestructibleAsset != NULL )
				{
					NumFractureLevels = ApexDestructibleAsset->getDepthCount();
				}
			}

			INT HighestImpactDamageLevel = -1;
			INT ImpactDamageLevelCount = 0;
			for (physx::PxU32 i=0; i<NumFractureLevels; i++)
			{
				char scratch[MAX_SPRINTF];
				FNxDestructibleDepthParameters dp;
				DestructibleParameters.DepthParameters.AddItem(dp);
				appSprintfANSI( scratch, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE", i );
#if NX_APEX_SDK_RELEASE >= 100
				::NxParameterized::Handle handle(*params);
#else
				::NxParameterized::Handle handle;
#endif
				if ( params->getParameterHandle(scratch,handle) != ::NxParameterized::ERROR_NONE ) break;
				const UBOOL bOverrideImpactDamage	= getParamBool(params,scratch);
				UBOOL bOverrideImpactDamageValue	= FALSE;
				if ( bOverrideImpactDamage )
				{
					appSprintfANSI( scratch, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE_VALUE", i );
#if NX_APEX_SDK_RELEASE >= 100
					::NxParameterized::Handle handle(*params);
#else
					::NxParameterized::Handle handle;
#endif
					if ( params->getParameterHandle(scratch,handle) != ::NxParameterized::ERROR_NONE ) break;
					bOverrideImpactDamageValue = getParamBool(params,scratch);
				}

				BYTE&	ImpactDamageOverride	= DestructibleParameters.DepthParameters(i).ImpactDamageOverride;
				if (bOverrideImpactDamageValue)
				{
					HighestImpactDamageLevel = (INT)i;
					++ImpactDamageLevelCount;
					ImpactDamageOverride = IDO_On;
				}
				else
				{
					ImpactDamageOverride = IDO_Off;
				}
			}

			if (ImpactDamageLevelCount > (HighestImpactDamageLevel+1)/2)
			{
				DestructibleParameters.DamageParameters.DefaultImpactDamageDepth = HighestImpactDamageLevel;
			}
			else
			{
				DestructibleParameters.DamageParameters.DefaultImpactDamageDepth = -1;
			}

			for (physx::PxU32 i = 0; i < NumFractureLevels; ++i)
			{
				BYTE&	ImpactDamageOverride	= DestructibleParameters.DepthParameters(i).ImpactDamageOverride;
				if ((INT)i <= HighestImpactDamageLevel)
				{					
					if (ImpactDamageOverride == IDO_On)
					{
						ImpactDamageOverride = IDO_None;	// Use default on
					}
				}
				else
				{
					if (ImpactDamageOverride == IDO_Off)
					{
						ImpactDamageOverride = IDO_None;	// Use default off
					}
				}
			}
		}

		// Allocate one FractureMaterial slot for each fracture level
		NxDestructibleAsset* ApexDestructibleAsset = static_cast<NxDestructibleAsset*>(MApexAsset->GetNxApexAsset());
		if( ApexDestructibleAsset != NULL )
		{
			FractureMaterials.Empty();
			FractureMaterials.AddZeroed( ApexDestructibleAsset->getDepthCount() );
		}
		// Set the correct number of materials
		Materials.AddZeroed(MApexAsset->GetNumMaterials());
		// Give materials unique names inside of APEX asset so we can override materials in the actor
		SetUniqueAssetMaterialNames( this, MApexAsset );
		bHasUniqueAssetMaterialNames = TRUE;
		physx::PxMat44 mat = physx::PxMat44::createIdentity();
		if ( convertToUE3Coordinates )
		{
			physx::PxF32 *temp = (physx::PxF32 *)mat.front();
			temp[1*4+1] = -1;
			MApexAsset->ApplyTransformation(mat,1,TRUE,TRUE,FALSE);
		}

	}
	GApexCommands->AddUpdateMaterials(this);
#endif
	return TRUE;
}

UBOOL UApexDestructibleAsset::Export(const FName& Name, UBOOL isKeepUE3Coords)
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

void UApexDestructibleAsset::UpdateMaterials(void)
{
#if WITH_APEX
	Materials.Empty();
    if ( MApexAsset )
	{
		UINT NumSubmeshes = (UINT)MApexAsset->GetNumMaterials();
		Materials.Empty(NumSubmeshes);
		for(UINT i=0; i<NumSubmeshes; i++)
		{
			const char         *MaterialNameA = MApexAsset->GetMaterialName(i);
			LoadMaterialForApex(MaterialNameA, Materials);
		}
	}
#endif
}

void UApexDestructibleAsset::FixupAsset()
{
#if WITH_APEX
	if( GIsEditor && !GIsGame )
	{
		NxDestructibleAsset* ApexDestructibleAsset = NULL;
		if( MApexAsset )
		{
			NxApexAsset* ApexAsset = MApexAsset->GetNxApexAsset();
			ApexDestructibleAsset = static_cast<NxDestructibleAsset*>(ApexAsset);
		}

		if( ApexDestructibleAsset != NULL )
		{
			INT FractureDepth = static_cast<INT>(ApexDestructibleAsset->getDepthCount());
			// Fix size of FractureMaterials for old assets
			if( FractureDepth != FractureMaterials.Num() )
			{
				FractureMaterials.Empty();
				FractureMaterials.AddZeroed(FractureDepth);
				MarkPackageDirty();
			}

			// Fix size of DepthParameters for old assets
			if( FractureDepth < DestructibleParameters.DepthParameters.Num() )
			{
				INT NumItemsToRemove = DestructibleParameters.DepthParameters.Num() - FractureDepth;
				DestructibleParameters.DepthParameters.Remove(FractureDepth, NumItemsToRemove);
				MarkPackageDirty();
			}

			// Give materials unique names inside of APEX asset so we can override materials in the actor
			if ( !bHasUniqueAssetMaterialNames )
			{
				SetUniqueAssetMaterialNames( this, MApexAsset );
				bHasUniqueAssetMaterialNames = TRUE;
			}
		}
	}
#endif
}

#if WITH_APEX
static const char * GetNameDestructible(const char *Src)
{
	if (Src == NULL )
	{
		Src = "";
	}
	INT Len = appStrlen(Src);
	char *Temp = (char *)appMalloc(Len+1);
	appMemcpy(Temp,Src,Len+1);
	return Temp;
}
#endif

void UApexDestructibleAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
#if WITH_APEX
	if(PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("Materials")))
	{
		OnApexAssetLost();
		INT Mcount = Materials.Num();
		if ( Mcount )
		{
			for(INT MatIdx=0; MatIdx<Mcount; ++MatIdx)
			{
				InitMaterialForApex(Materials(MatIdx));
			}
		}
		if( MApexAsset )
		{
			// Give materials unique names inside of APEX asset so we can override materials in the actor
			SetUniqueAssetMaterialNames( this, MApexAsset );
			bHasUniqueAssetMaterialNames = TRUE;
		}
	}
	if ( PropertyThatChanged )
	{
		// validate that certain property change events are within a valid numeric range.
		if (PropertyThatChanged->GetFName() == FName(TEXT("DamageThreshold")))
		{
			DestructibleParameters.DamageParameters.DamageThreshold = Max( DestructibleParameters.DamageParameters.DamageThreshold, 0.0f );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DamageSpread")))
		{
			DestructibleParameters.DamageParameters.DamageSpread = Max( DestructibleParameters.DamageParameters.DamageSpread, 0.0f );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DamageCap")))
		{
			DestructibleParameters.AdvancedParameters.DamageCap = Max( DestructibleParameters.AdvancedParameters.DamageCap, 0.0f );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("ImpactDamage")))
		{
			DestructibleParameters.DamageParameters.ImpactDamage = Max( DestructibleParameters.DamageParameters.ImpactDamage, 0.0f );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("SupportDepth")))
		{
			DestructibleParameters.SupportDepth = Clamp( DestructibleParameters.SupportDepth, 0, DestructibleParameters.DepthParameters.Num()-1 );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("MinimumFractureDepth")))
		{
			DestructibleParameters.MinimumFractureDepth = Clamp( DestructibleParameters.MinimumFractureDepth, 0, DestructibleParameters.DepthParameters.Num()-1 );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DebrisDepth")))
		{
			DestructibleParameters.DebrisDepth = Clamp( DestructibleParameters.DebrisDepth, -1, DestructibleParameters.DepthParameters.Num()-1 );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DebrisLifetimeMin")))
		{
			DestructibleParameters.DebrisParameters.DebrisLifetimeMin = Clamp( DestructibleParameters.DebrisParameters.DebrisLifetimeMin, 0.0f, DestructibleParameters.DebrisParameters.DebrisLifetimeMax );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DebrisLifetimeMax")))
		{
			DestructibleParameters.DebrisParameters.DebrisLifetimeMax = Max( DestructibleParameters.DebrisParameters.DebrisLifetimeMax, DestructibleParameters.DebrisParameters.DebrisLifetimeMin );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DebrisMaxSeparationMin")))
		{
			DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin = Clamp( DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin, 0.0f, DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax );
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("DebrisMaxSeparationMax")))
		{
			DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax = Max( DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax, DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin );
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("MaxChunkSpeed")))
		{
			DestructibleParameters.AdvancedParameters.MaxChunkSpeed = Max( DestructibleParameters.AdvancedParameters.MaxChunkSpeed, 0.0f );
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("MassScaleExponent")))
		{
			DestructibleParameters.AdvancedParameters.MassScaleExponent = Clamp( DestructibleParameters.AdvancedParameters.MassScaleExponent, 0.0f, 1.0f );
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("MassScale")))
		{
			DestructibleParameters.AdvancedParameters.MassScale = Max( DestructibleParameters.AdvancedParameters.MassScale, 0.000001f );
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("EssentialDepth")))
		{
			DestructibleParameters.EssentialDepth = Clamp( DestructibleParameters.EssentialDepth, 0, 5 );
		}
	}

#endif

}

/** Fix up unique material names in the APEX asset after rename */
void UApexDestructibleAsset::PostRename()
{
	Super::PostRename();
#if WITH_EDITOR && WITH_APEX
	OnApexAssetLost();
	SetUniqueAssetMaterialNames( this, MApexAsset );
	bHasUniqueAssetMaterialNames = TRUE;
#endif
}

/** Fix up unique material names in the APEX asset after duplication */
void UApexDestructibleAsset::PostDuplicate()
{
	Super::PostDuplicate();
#if WITH_EDITOR && WITH_APEX
	SetUniqueAssetMaterialNames( this, MApexAsset );
	bHasUniqueAssetMaterialNames = TRUE;
#endif
}

void UApexDestructibleAsset::BeginDestroy(void)
{
	Super::BeginDestroy();
#if WITH_APEX
	if ( GApexCommands )
	{
		GApexCommands->NotifyDestroy(this);
	}
	// BRG - commenting this check out (along with check in UApexDestructibleAsset::ReleaseDestructiblePreview)
	// seems to allow asset re-import without any side-effects.  This should be examined.
//	check(ApexComponents.Num() == 0);
	OnApexAssetLost();
	if ( MApexAsset )
	{
		if(FApexCleanUp::PendingObjects)
		{
			FlushRenderingCommands();
		}
		MApexAsset->DecRefCount(0);
		MApexAsset = NULL;
	}
#endif
}

void UApexDestructibleAsset::ReleaseDestructibleActor(physx::apex::NxDestructibleActor &ApexDestructibleActor, UApexComponentBase &Component)
{
#if WITH_APEX 
	INT Index = 0;
	UBOOL Found = ApexComponents.FindItem(&Component, Index);
	if(Found)
	{
		ApexComponents.Remove(Index);
		BeginCleanup(new FApexCleanUp(&ApexDestructibleActor, MApexAsset));
	}
#endif
}

physx::apex::NxDestructiblePreview *UApexDestructibleAsset::CreateDestructiblePreview(UApexComponentBase &Component)
{
	physx::apex::NxDestructiblePreview *ApexPreview = NULL;
#if WITH_APEX
	check(MApexAsset);
	if( MApexAsset )
	{
		NxApexAsset *a_asset = MApexAsset->GetNxApexAsset();
		if ( a_asset )
		{
			NxParameterized::Interface *descParams = a_asset->getDefaultAssetPreviewDesc();
			NxParameterized::Handle OverrideSkinnedMaterials(*descParams);
			descParams->getParameterHandle("overrideSkinnedMaterialNames", OverrideSkinnedMaterials);
			OverrideSkinnedMaterials.resizeArray(Materials.Num());
			for(INT i = 0; i < Materials.Num(); i++)
			{
				UMaterialInterface* Material = Materials(i);
				InitMaterialForApex(Materials(i));
				NxParameterized::Handle MaterialHandle(*descParams);
				OverrideSkinnedMaterials.getChildHandle(i, MaterialHandle);
				MaterialHandle.setParamString(TCHAR_TO_ANSI(*Material->GetPathName()));
			}
			ApexPreview = (physx::NxDestructiblePreview *)a_asset->createApexAssetPreview(*descParams);
			check(ApexPreview);
		}
	}
	if(ApexPreview)
	{
		MApexAsset->IncRefCount(0);
		ApexComponents.AddItem(&Component);
	}
#endif
	return ApexPreview;
}	

void UApexDestructibleAsset::ReleaseDestructiblePreview(class physx::apex::NxDestructiblePreview &ApexDestructiblePreview, class UApexComponentBase &Component)
{
#if WITH_APEX 
	INT Index = 0;
	UBOOL Found = ApexComponents.FindItem(&Component, Index);
	// BRG - commenting this check out (along with check in UApexDestructibleAsset::BeginDestroy)
	// seems to allow asset re-import without any side-effects.  This should be examined.
//	check(Found);
	if(Found)
	{
		MDestructibleThumbnailComponent = NULL;
		ApexComponents.Remove(Index);
		BeginCleanup(new FApexCleanUp(&ApexDestructiblePreview, MApexAsset));
	}
#endif
}


void * UApexDestructibleAsset::GetNxParameterized(void)
{
	void *ret = NULL;
#if WITH_APEX
	if ( MApexAsset )
	{
		::NxParameterized::Interface *params = MApexAsset->GetDefaultApexActorDesc();
		if ( params )
		{
			NxParameterized::setParamF32(*params,"destructibleParameters.damageThreshold",				DestructibleParameters.DamageParameters.DamageThreshold);
			NxParameterized::setParamF32(*params,"destructibleParameters.damageToRadius",				DestructibleParameters.DamageParameters.DamageSpread);
			NxParameterized::setParamF32(*params,"destructibleParameters.damageCap",					DestructibleParameters.AdvancedParameters.DamageCap);
			NxParameterized::setParamF32(*params,"destructibleParameters.forceToDamage",				DestructibleParameters.DamageParameters.ImpactDamage*U2PScale);
			NxParameterized::setParamF32(*params,"destructibleParameters.impactVelocityThreshold",		DestructibleParameters.AdvancedParameters.ImpactVelocityThreshold*U2PScale);
			NxParameterized::setParamF32(*params,"destructibleParameters.materialStrength",				DestructibleParameters.DamageParameters.ImpactResistance*U2PScale);
			NxParameterized::setParamU32(*params,"supportDepth",										DestructibleParameters.SupportDepth);
			NxParameterized::setParamU32(*params,"destructibleParameters.minimumFractureDepth",			DestructibleParameters.MinimumFractureDepth);
			NxParameterized::setParamU32(*params,"destructibleParameters.impactDamageDefaultDepth",		DestructibleParameters.DamageParameters.DefaultImpactDamageDepth);
			NxParameterized::setParamI32(*params,"destructibleParameters.debrisDepth",					DestructibleParameters.DebrisDepth);
			NxParameterized::setParamU32(*params,"destructibleParameters.essentialDepth",				DestructibleParameters.EssentialDepth);
			NxParameterized::setParamF32(*params,"destructibleParameters.debrisLifetimeMin",			DestructibleParameters.DebrisParameters.DebrisLifetimeMin);
			NxParameterized::setParamF32(*params,"destructibleParameters.debrisLifetimeMax",			DestructibleParameters.DebrisParameters.DebrisLifetimeMax);
			NxParameterized::setParamF32(*params,"destructibleParameters.debrisMaxSeparationMin",		DestructibleParameters.DebrisParameters.DebrisMaxSeparationMin);
			NxParameterized::setParamF32(*params,"destructibleParameters.debrisMaxSeparationMax",		DestructibleParameters.DebrisParameters.DebrisMaxSeparationMax);

			physx::PxBounds3 b;

			if( DestructibleParameters.DebrisParameters.ValidBounds.IsValid )
			{
				b.minimum.x = DestructibleParameters.DebrisParameters.ValidBounds.Min.X * U2PScale;
				b.minimum.y = DestructibleParameters.DebrisParameters.ValidBounds.Min.Y * U2PScale;
				b.minimum.z = DestructibleParameters.DebrisParameters.ValidBounds.Min.Z * U2PScale;
																	   
				b.maximum.x = DestructibleParameters.DebrisParameters.ValidBounds.Max.X * U2PScale;
				b.maximum.y = DestructibleParameters.DebrisParameters.ValidBounds.Max.Y * U2PScale;
				b.maximum.z = DestructibleParameters.DebrisParameters.ValidBounds.Max.Z * U2PScale;
			}
			else
			{
				b.setEmpty();
			}

			NxParameterized::setParamBounds3(*params,"destructibleParameters.validBounds",b);

			NxParameterized::setParamF32(*params,"destructibleParameters.maxChunkSpeed",					DestructibleParameters.AdvancedParameters.MaxChunkSpeed*U2PScale);

			NxParameterized::setParamBool(*params,"destructibleParameters.flags.ACCUMULATE_DAMAGE",        DestructibleParameters.Flags.ACCUMULATE_DAMAGE);
			NxParameterized::setParamBool(*params,"useAssetDefinedSupport",                                DestructibleParameters.Flags.ASSET_DEFINED_SUPPORT);
			NxParameterized::setParamBool(*params,"useWorldSupport",                                       DestructibleParameters.Flags.WORLD_SUPPORT);
			NxParameterized::setParamBool(*params,"destructibleParameters.flags.DEBRIS_TIMEOUT",           DestructibleParameters.Flags.DEBRIS_TIMEOUT);
			NxParameterized::setParamBool(*params,"destructibleParameters.flags.DEBRIS_MAX_SEPARATION",    DestructibleParameters.Flags.DEBRIS_MAX_SEPARATION);
			NxParameterized::setParamBool(*params,"destructibleParameters.flags.CRUMBLE_SMALLEST_CHUNKS",  DestructibleParameters.Flags.CRUMBLE_SMALLEST_CHUNKS);
			NxParameterized::setParamBool(*params,"destructibleParameters.flags.ACCURATE_RAYCASTS",        DestructibleParameters.Flags.ACCURATE_RAYCASTS);
			NxParameterized::setParamBool(*params,"destructibleParameters.flags.USE_VALID_BOUNDS",         DestructibleParameters.Flags.USE_VALID_BOUNDS);
			NxParameterized::setParamBool(*params,"formExtendedStructures",								   DestructibleParameters.Flags.FORM_EXTENDED_STRUCTURES);

   			NxParameterized::setParamF32(*params,"destructibleParameters.fractureImpulseScale",DestructibleParameters.AdvancedParameters.FractureImpulseScale);

			NxParameterized::setParamU16(*params,"destructibleParameters.dynamicChunkDominanceGroup",	   DestructibleParameters.DynamicChunksDominanceGroup);
			NxGroupsMask DynamicChunksMask = CreateGroupsMask( DestructibleParameters.DynamicChunksChannel,&DestructibleParameters.DynamicChunksCollideWithChannels );
			NxParameterized::setParamBool(*params,"destructibleParameters.dynamicChunksGroupsMask.useGroupsMask", DestructibleParameters.UseDynamicChunksGroupsMask);
			NxParameterized::setParamU32(*params,"destructibleParameters.dynamicChunksGroupsMask.bits0",   DynamicChunksMask.bits0);
			NxParameterized::setParamU32(*params,"destructibleParameters.dynamicChunksGroupsMask.bits1",   DynamicChunksMask.bits1);
			NxParameterized::setParamU32(*params,"destructibleParameters.dynamicChunksGroupsMask.bits2",   DynamicChunksMask.bits2);
			NxParameterized::setParamU32(*params,"destructibleParameters.dynamicChunksGroupsMask.bits3",   DynamicChunksMask.bits3);

			for (INT i=0; i<DestructibleParameters.DepthParameters.Num(); i++)
			{
				char scratch[MAX_SPRINTF];
				appSprintfANSI( scratch, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE", i );
				NxParameterized::setParamBool(*params,scratch,TRUE);
				appSprintfANSI( scratch, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE_VALUE", i );

				const bool bDefaultImpactDamage = (DestructibleParameters.DamageParameters.DefaultImpactDamageDepth >= (INT)i);

				switch (DestructibleParameters.DepthParameters(i).ImpactDamageOverride)
				{
				case IDO_None:
					NxParameterized::setParamBool(*params,scratch,bDefaultImpactDamage);
					break;
				case IDO_On:
					NxParameterized::setParamBool(*params,scratch,TRUE);
					break;
				case IDO_Off:
					NxParameterized::setParamBool(*params,scratch,FALSE);
					break;
				}
			}
		}

		ret = params;
	}
#endif
	return ret;
}

void * UApexDestructibleAsset::GetAssetNxParameterized(void)
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
