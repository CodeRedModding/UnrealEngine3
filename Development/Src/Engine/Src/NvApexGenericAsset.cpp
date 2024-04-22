/*=============================================================================
	NxApexGenericAsset.cpp : Manages APEX generic assets.
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
#include "NxApexSDK.h"
#include "NxUserRenderResourceManager.h"
#include "NxParameterized.h"
#include "NvApexCommands.h"
#include "NvApexGenericAsset.h"
#include "NxApexAsset.h"
#endif


/*
*	UApexGenericAsset
*
*/
IMPLEMENT_CLASS(UApexGenericAsset);

void UApexGenericAsset::Serialize(FArchive& Ar)
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
			UINT Size;
			Ar << Size;
			Buffer.Add( Size );
			Ar.Serialize( Buffer.GetData(), Size );
#if WITH_APEX
			if ( MApexAsset )
			{
				MApexAsset->DecRefCount(0);
				MApexAsset = NULL;
			}
			char scratch[1024];
			GetApexAssetName(this,scratch,1024,".acml");
     		MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, (const void *)Buffer.GetData(), (NxU32)Size, TCHAR_TO_ANSI(*OriginalApexName), this);
			if ( MApexAsset )
			{
				MApexAsset->IncRefCount(0);
			}
			GApexCommands->AddUpdateMaterials(this);
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

UBOOL UApexGenericAsset::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
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

TArray<FString> UApexGenericAsset::GetGenericBrowserInfo()
{
	TArray<FString> Info;
#if WITH_APEX
	if( MApexAsset )
	{
		switch ( MApexAsset->GetType() )
		{
			case AAT_DESTRUCTIBLE:
			case AAT_CLOTHING:
				PX_ALWAYS_ASSERT();
				break;
			case AAT_APEX_EMITTER:		// asset used to construct emitters.
				Info.AddItem( FString::Printf( TEXT("Shape Emitter")));
				break;
			case AAT_RENDER_MESH:
				Info.AddItem( FString::Printf( TEXT("Render Mesh")));
				break;
			case AAT_GENERIC: // just generic data, we don't resolve it to a specific type.
				PX_ALWAYS_ASSERT();
				break;
			case AAT_GROUND_EMITTER:
				Info.AddItem( FString::Printf( TEXT("Ground Emitter")));
				break;
			case AAT_IMPACT_EMITTER:
				Info.AddItem( FString::Printf( TEXT("Impact Emitter")));
				break;
			case AAT_BASIC_IOS:
				Info.AddItem( FString::Printf( TEXT("Basic IOS")));
				break;
			case AAT_FLUID_IOS:
				Info.AddItem( FString::Printf( TEXT("Fluid IOS")));
				break;
			case AAT_IOFX:
				Info.AddItem( FString::Printf( TEXT("IOFX")));
				break;
			default:
				PX_ALWAYS_ASSERT();
				break;

		}
	}
#endif
	return Info;

}

UBOOL UApexGenericAsset::Import( const BYTE* Buffer, INT BufferSize, const FString& Name)
{
	UBOOL ret = FALSE;
#if WITH_APEX && !FINAL_RELEASE
	InitializeApex();
	if ( GApexCommands )
	{
		if ( MApexAsset )
		{
			MApexAsset->DecRefCount(0);
			MApexAsset = 0;
		}
		char scratch[1024];
		GetApexAssetName(this,scratch,1024,".acml");
		MApexAsset = GApexCommands->GetApexAssetFromMemory(scratch, (const void *)Buffer, (NxU32)BufferSize, GApexManager->GetCurrentImportFileName(), this );
		if ( MApexAsset )
		{
			AnsiToString( MApexAsset->GetOriginalApexName(),OriginalApexName );
			MApexAsset->IncRefCount(0);
			GApexCommands->AddUpdateMaterials(this);
			ret = TRUE;
		}
	}
#endif
	return ret;
}

#if WITH_APEX
static const char * getNameClothingGeneric(const char *src)
{
	if (src == NULL ) src = "";
	NxU32 len = (NxU32) strlen(src);
	char *temp = (char *) appMalloc(len+1);
	memcpy(temp,src,len+1);
	return temp;
}
#endif


void UApexGenericAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITORONLY_DATA
#if WITH_APEX
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("Materials")))
	{
		if( MApexAsset )
		{
			NxU32 mcount = (NxU32)Materials.Num();
			if ( mcount )
			{
				const char **names = new const char *[mcount];
				for(NxU32 i=0; i<mcount; i++)
				{
					UMaterialInterface *Material = Materials(i);
					InitMaterialForApex(Material);
					const FString MaterialPath = Material ? Material->GetPathName() : TEXT("EngineMaterials.DefaultMaterial");
					names[i] = getNameClothingGeneric(TCHAR_TO_ANSI(*MaterialPath));
				}
				MApexAsset->ApplyMaterialRemap(mcount,names);
				for (NxU32 i=0; i<mcount; i++)
				{
					appFree( (void *)names[i] );
				}
				delete []names;
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

void UApexGenericAsset::BeginDestroy(void)
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
		MApexAsset = 0;
	}
#endif
}

void * UApexGenericAsset::GetNxParameterized(void)
{
	void *ret = NULL;
#if WITH_APEX
	if ( MApexAsset )
	{
		::NxParameterized::Interface *params = MApexAsset->GetDefaultApexActorDesc();
		ret = params;
	}
#endif
	return ret;
}

static FString GetFstring(TCHAR *str)
{
	FString ret = str;
	return ret;
}


#if WITH_APEX
void GetApexAssetName(UApexAsset *asset,char *dest,physx::PxU32 slen,const char *extension)
{
	FString Name = asset->GetPathName();

	appStrncpyANSI( dest, TCHAR_TO_ANSI(*Name), slen );
	appStrcatANSI( dest, slen, extension );
}


void AnsiToString(const char *str,FString &fstring)
{
	fstring = GetFstring(ANSI_TO_TCHAR(str));
}
#endif

void UApexGenericAsset::CreateDefaultAssetType(INT t,UApexGenericAsset *parent)
{
#if WITH_APEX_PARTICLES
	InitializeApex();
	if ( GApexCommands )
	{
		if ( MApexAsset )
		{
			MApexAsset->DecRefCount(0);
			MApexAsset = 0;
		}
		char scratch[1024];
		GetApexAssetName(this,scratch,1024,".acml");
		MApexAsset = GApexCommands->CreateDefaultApexAsset(scratch,(ApexAssetType)t,this,parent);
		if ( MApexAsset )
		{
			AnsiToString( MApexAsset->GetOriginalApexName(),OriginalApexName );
			MApexAsset->IncRefCount(0);
		}
	}
#endif
}

FString UApexGenericAsset::GetDesc()
{
	FString ret;
#if WITH_APEX
	if ( MApexAsset )
	{
		char scratch[512];
		MApexAsset->GetDesc(scratch,512);
		ret = GetFstring(ANSI_TO_TCHAR(scratch));
	}
#endif
	return ret;
}

void * UApexGenericAsset::GetAssetNxParameterized(void)
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

/** Re-assigns the APEX material resources by name with the current array of UE3 materials */
void UApexGenericAsset::UpdateMaterials(void)
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

void UApexAsset::ResetNamedReferences(void)
{
#if WITH_EDITORONLY_DATA
	NamedReferences.Empty();
#endif // WITH_EDITORONLY_DATA
}

void UApexAsset::AddNamedReference(class UApexAsset *obj)
{
#if WITH_EDITORONLY_DATA
	UBOOL found = false;
	for (INT i=0; i<NamedReferences.Num(); i++)
	{
		if ( NamedReferences(i) == obj )
		{
			found = TRUE;
			break;
		}
	}
	if ( !found )
	{
		for (INT i=0; i<NamedReferences.Num(); i++)
		{
			if ( NamedReferences(i) == NULL )
			{
				NamedReferences(i) = obj;
				found = TRUE;
				break;
			}
		}
		if ( !found )
		{
			NamedReferences.AddItem(obj);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UApexAsset::RemoveNamedReference(class UApexAsset *obj)
{
#if WITH_EDITORONLY_DATA
	for (INT i=0; i<NamedReferences.Num(); i++)
	{
		if ( NamedReferences(i) == obj )
		{
			NamedReferences(i) = NULL;
			break;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UApexGenericAsset::NotifyApexEditMode(class ApexEditInterface *iface)
{
#if WITH_APEX_PARTICLES
	if ( MApexAsset )
	{
		MApexAsset->NotifyApexEditMode(iface);
	}
#endif
}

void UApexClothingAsset::NotifyApexEditMode(class ApexEditInterface *iface)
{
#if WITH_APEX_PARTICLES
	if ( MApexAsset )
	{
		MApexAsset->NotifyApexEditMode(iface);
	}
#endif
}

void UApexDestructibleAsset::NotifyApexEditMode(class ApexEditInterface *iface)
{
#if WITH_APEX_PARTICLES
	if ( MApexAsset )
	{
		MApexAsset->NotifyApexEditMode(iface);
	}
#endif
}
