/*=============================================================================
	NvApexCommands.cpp : Manages serializing, deserializing, and management of APEX assets.
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
#include "EngineMeshClasses.h"
#include "UnNovodexSupport.h"

#if WITH_NOVODEX

#include "NvApexManager.h"

#if WITH_APEX

#include "NxParamUtils.h"
#pragma warning(disable:4996)

#include "NvApexCommands.h"
#include "NvApexScene.h"
#include "NvApexResourceCallback.h"

#include <NxApexSDK.h>
#include <NxUserRenderResourceManager.h>
#include <NxResourceCallback.h>
#include <NxModuleDestructible.h>
#include <NxModuleClothing.h>
#include <NxRendermeshAsset.h>
#include <NxDestructibleAsset.h>
#include <NxClothingAsset.h>
#if WITH_APEX_PARTICLES
#include <NxApexEmitterAsset.h>
#include <NxGroundEmitterAsset.h>
#include <NxImpactEmitterAsset.h>
#include <NxBasicIosAsset.h>
#include <NxBasicIosActor.h>
#include <NxIofxAsset.h>
#endif
#include <NxActorDesc.h>
#include <NxStream.h>
#include <NxModuleDestructible.h>
#include <NxDestructibleAsset.h>
#include <NxDestructiblePreview.h>
#include <NxDestructibleActor.h>
#include <NxModuleClothing.h>
#include <NxClothingAsset.h>
#include <NxClothingPreview.h>
#include <NxClothingActor.h>
#include <NxClothingAssetAuthoring.h>
#include <NxParameterized.h>
#include <NxDebugRenderable.h>
#include <NxScene.h>

#include "NvApexScene.h"
#include "NvApexRender.h"
#include "NvApexScene.h"
#include "NxSerializer.h"


using namespace physx::apex;


FIApexCommands *GApexCommands=0;




namespace NI_APEX_SUPPORT
{

NxResourceProvider *GNRP=0;


//*********** end of temporary named resource provider.

static const char *lastDot(const char *str)
{
  const char *ret = NULL;
  while  ( str && *str )
  {
    str = strchr(str,'.');
    if ( str )
    {
      ret = str;
      str++;
    }
  }
  return ret;
}

class ApexAsset;

typedef NxArray< FIApexActor * > TApexActorVector;
typedef NxArray< physx::PxMat44 > TPxMat44Vector;
typedef NxArray< ApexAsset * > TApexAssetVector;
typedef NxArray< FIApexScene *> TApexSceneVector;
typedef NxArray< UApexAsset * > TUApexAssetVector;


physx::apex::NxApexAssetAuthoring* ApexAuthoringFromAsset(physx::apex::NxApexAsset* Asset, physx::apex::NxApexSDK* ApexSDK);

class ApexAsset : public FIApexAsset, 
					public NxParameterized::NamedReferenceInterface
#if WITH_APEX_PARTICLES
					,public ApexEditNotify
#endif
{
public:
#if WITH_APEX_PARTICLES
	ApexAsset(const char *fname,ApexAssetType type,UApexAsset *ue3Object,UApexGenericAsset *parent);
#endif
	ApexAsset(const char *fname,const void *mem,physx::PxU32 dlen,const void *params,const char *originalName,UApexAsset *ue3Object);
	virtual ~ApexAsset(void);

#if WITH_APEX_PARTICLES
	virtual UBOOL  OnPostPropertyEdit(NxParameterized::Interface *iface,NxParameterized::Handle &handle)
	{
		if ( MApexAsset )
		{

			mObject->Modify();
			mObject->MarkPackageDirty();

			NotifyAssetGoingAway();
			if ( mEdited )
			{
				NxParameterized::Interface *ifc = MApexAsset->releaseAndReturnNxParameterizedInterface();
				PX_ASSERT( ifc == iface ); // must be the same NxParameterized!
			}
			else
			{
				GApexManager->GetApexSDK()->releaseAsset(*MApexAsset);
			}
			MApexAsset = GApexManager->GetApexSDK()->createAsset(iface,MName);
			mEdited = TRUE;
			NotifyAssetImport();
		}
		return TRUE;
	}

	virtual UBOOL  NotifyEditorClosed(NxParameterized::Interface *iface) 
	{
		mApexEditInterface = NULL;
		return !mEdited; // if it was edited, then don't kill the parameterized interface
	}

	virtual void NotifyApexEditMode(class ApexEditInterface *iface) 
	{
		mApexEditInterface = iface;
		iface->SetApexEditNotify(mObject,this);
	}


#endif
#if WITH_APEX_PARTICLES
	void CreateDefaultApexAsset(ApexAssetType type,UApexAsset *obj,UApexGenericAsset *parent);
#endif

	UBOOL ReleaseNxApexAsset(NxApexAsset *apexAsset);

	virtual const char * namedReferenceCallback(const char *className,const char *namedReference,NxParameterized::Handle & /*handle*/) 
	{
		const char *ret = NULL;
		ret = GApexCommands->namedReferenceCallback(className,namedReference);
		return ret;
	}

	class ObjectDependencies : public NxParameterized::NamedReferenceInterface
	{
	public:
		ObjectDependencies(UApexAsset *object)
		{
			mObject = object;
			mFirst = TRUE;
		}

		virtual const char * namedReferenceCallback(const char *className,const char *namedReference,NxParameterized::Handle & /*handle*/) 
		{
			const char *ret = NULL;

			if ( mFirst )
			{
				mFirst = FALSE;
				mObject->ResetNamedReferences();
			}
#if 1
			FIApexAsset *asset = GApexCommands->GetApexAsset(namedReference);
			UApexAsset *obj = asset ? asset->GetUE3Object() : NULL;
#else
			UApexAsset *obj = FindObject<UApexAsset>(0, ANSI_TO_TCHAR(namedReference), 0);
#endif
			if ( obj )
			{
				mObject->AddNamedReference(obj);
			}

			return ret;
		}
	private:
		UApexAsset	*mObject;
		UBOOL			mFirst;
	};

	void CreateObjectDependencies(void)
	{
		// fixup named references here..
		if ( MApexAsset )
		{
			const NxParameterized::Interface *_iface = MApexAsset->getAssetNxParameterized();
			if ( _iface )
			{
				NxApexSDK *apexSDK = GApexManager->GetApexSDK();
				ObjectDependencies od(mObject);
				NxParameterized::getNamedReferences(*(NxParameterized::Interface *)_iface,od,true);
			}
		}
	}

	void FixupApexAsset(void)
	{
		// fixup named references here..
		if ( MApexAsset )
		{
			const NxParameterized::Interface *_iface = MApexAsset->getAssetNxParameterized();
			if ( _iface )
			{
				NxApexSDK *apexSDK = GApexManager->GetApexSDK();
				physx::PxU32 changeCount = NxParameterized::getNamedReferences(*(NxParameterized::Interface *)_iface,*this,true);
				if ( changeCount )
				{
					NotifyAssetGoingAway();
					NxParameterized::Interface *iface = MApexAsset->releaseAndReturnNxParameterizedInterface();
					MApexAsset = apexSDK->createAsset(iface,MName);
					NotifyAssetImport();
#if 0
					physx::PxFileBuf *b = apexSDK->createMemoryWriteStream();
					NxParameterized::Serializer *ser = apexSDK->createSerializer(NxParameterized::Serializer::NST_XML);
					ser->serialize(*b,&iface,1);
					static int remapCount=0;
					static int noChangeCount=0;
					char scratch[512];
					if ( changeCount )
						physx::string::sprintf_s(scratch,512,"remap%02d.apx", ++remapCount );
					else
						physx::string::sprintf_s(scratch,512,"no_change%02d.apx", ++noChangeCount );
					FILE *fph = fopen(scratch,"wb");
					if ( fph )
					{
						physx::PxU32 dlen;
						const void *data = apexSDK->getMemoryWriteBuffer(*b,dlen);
						fwrite(data,dlen,1,fph);
						fclose(fph);
					}
					apexSDK->releaseMemoryWriteStream(*b);
					ser->release();
#endif
				}
			}
		}
	}

	UApexAsset * GetUE3Object(void) const 
	{
		return mObject;
	}

	void SetUE3Object(UApexAsset *obj)
	{
		mObject = obj;
	}

	UBOOL LoadFromMemory(const void *mem,physx::PxU32 dlen,const char *AssetName,const char *originalName,UApexAsset *obj);
	UBOOL LoadFromParams(const void *Params,const char *AssetName,const char *originalName,UApexAsset *obj);

	virtual UBOOL IsReady(void) const  // returns true if the asset is ready to be used.
	{
		UBOOL ret = FALSE;
		if ( MApexAsset && MForceLoad )
		{
			ret = TRUE;
		}
		return ret;
	}

	/**
	 * @return	The ApexAssetType.
	**/
  	virtual ApexAssetType GetType(void) const
  	{
    	return MType;
  	}

  	UBOOL SameName(const char *fname) const
  	{
		return strcmp(MName,fname) == 0;
  	}

  	UBOOL IsValid(void) const
  	{
    	return MType != AAT_LAST;
  	}

	void NotifyAssetGoingAway(void);

	/**
  	 * @return	null if it fails, else a pointer to the NxApexAsset. 
  	**/
  	virtual NxApexAsset * GetNxApexAsset(void) const { return MApexAsset; };
  	
	/**
	 * Causes any actors using this asset to be created due to property changes.
  	**/
	virtual void RefreshActors(void)
	{
		NotifyAssetGoingAway();
		NotifyAssetImport();
	}

	/***
	* Causes the named resource provider to resolve any asset dependencies.
	***/
	void ForceLoad(void)
	{
		if ( MApexAsset && !MForceLoad )
		{
			// Note: By not checking the return value of forceLoadAssets we are assuming it 
			// was able to load everything in the first try. According to the APEX documentation
			// forceLoadAssets should be called periodically until it returns 0. This is probably
			// fine in the editor, but shouldn't be expected to work if called in the game during
			// async loading.
			if ( GIsEditor && !GIsGame )
			{
				MApexAsset->forceLoadAssets();
			}
			MForceLoad = TRUE;
		}
	}

  	/**
	 * Remaps the material names to the submeshes
  	 * @param	SubMeshCount -	Number of sub meshes. 
  	 * @param	Names -			The sub mesh names. 
  	**/
  	virtual	void				  ApplyMaterialRemap(physx::PxU32 subMeshCount,const char **names)
  	{
  		if ( MApexAsset )
  		{
  			NotifyAssetGoingAway();
			physx::PxU32 index = 0;
			NxParameterized::Interface *iface = MApexAsset->releaseAndReturnNxParameterizedInterface();
			MApexAsset = NULL;
			PX_ASSERT(iface);
			if ( iface )
			{
				physx::PxU32 count;
				const NxParameterized::ParamResult *results = NxParameterized::getParamList(*iface,NULL,"materialNames",count,true,false,GApexManager->GetApexSDK()->getParameterizedTraits());
				if ( results )
				{
					for (physx::PxU32 i=0; i<count; i++)
					{
						const NxParameterized::ParamResult &r = results[i];
						if ( r.mDataType == NxParameterized::TYPE_ARRAY )
						{
							for (physx::PxI32 j=0; j<r.mArraySize; j++)
							{
								if ( index < subMeshCount )
								{
									NxParameterized::Handle handle(r.mHandle);
									handle.set(j);
									handle.setParamString(names[index]);
									handle.popIndex();
								}
								index++;
							}
						}
					}
					NxParameterized::releaseParamList(count,results,GApexManager->GetApexSDK()->getParameterizedTraits());
				}
				MApexAsset = GApexManager->GetApexSDK()->createAsset(iface,MName);
			}
			NotifyAssetImport();
		}
	}

	/**
   * Notifies the actor that a fresh asset has been imported and the actor can be re-created.
	**/
    void NotifyAssetImport(void);

  	const char * GetName(void) const { return MName; };

	virtual const char * GetOriginalApexName(void) 
	{
		return MOriginalApexName;
	}

	void setOriginalName(const char *name)
	{
		PX_ASSERT(name);
		if ( MOriginalApexName )
		{
			appFree(MOriginalApexName);
		}
		physx::PxU32 slen = (physx::PxU32)strlen(name)+1;
		MOriginalApexName = (char *)appMalloc(slen);
		memcpy(MOriginalApexName,name,slen);
	}

	/**
	 * Gets the default parameterized interface
	 * @return	null if it fails, else a pointer to the ::NxParameterized::Interface.
	**/
	virtual ::NxParameterized::Interface * GetDefaultApexActorDesc(void)
  	{
		::NxParameterized::Interface *ret = NULL;

		if ( MApexAsset )
		{
			ret = MApexAsset->getDefaultActorDesc();
		}

	  	return ret;
  	}

	UBOOL FindActor(FIApexActor *actor) const
	{
		UBOOL ret = FALSE;
		for (TApexActorVector::const_iterator i=MActors.begin(); i!=MActors.end(); ++i)
		{
			if ( actor == static_cast<FIApexActor *>(*i) )
			{
				ret = TRUE;
				break;
			}
		}
		return ret;
	}

  /**
	* Increases the reference count
  	* @param [in,out]	Actor - If non-null, it will put the actor on a list for this asset. 
  	*
  	* @return	The reference count 
  **/
  virtual physx::PxI32 IncRefCount(FIApexActor *actor)
  {
  	  if ( actor )
  	  {
  	  	UBOOL found = FindActor( actor );
  	  	PX_ASSERT( !found );
  	  	if ( !found )
  	  	{
    		for (TApexActorVector::iterator i=MActors.begin(); i!=MActors.end(); ++i)
    		{
    			if ( (*i) == NULL )
    			{
    				(*i) = actor;
    				found = TRUE;
    				break;
    			}
    		}
    		if ( !found )
    		{
  	  			MActors.push_back(actor);
  	  		}
		}
	  }
	  MRefCount++;
	  return MRefCount;
  }

  /**
	* Decrements the reference count
  	* @param [in,out]	Actor - If non-null, it will put the actor on a list for this asset. 
  	*
  	* @return	The reference count 
  **/
  virtual physx::PxI32 DecRefCount(FIApexActor *actor)
  {
  	  if ( actor )
  	  {
  	  	UBOOL found = FALSE;
		for (TApexActorVector::iterator i=MActors.begin(); i!=MActors.end(); ++i)
		{
			if ( actor == (*i) )
			{
				found = TRUE;
				(*i) = NULL;
				break;
			}
		}
		PX_ASSERT( found );
	  }
	  MRefCount--;
	  physx::PxI32 ret = MRefCount;
	  PX_ASSERT( MRefCount >= 0 );
	  if ( MRefCount == 0 )
	  {
		  delete this;
	  }
	  return ret;
  }

  /**
	* Gets the asset name
  	* @return	null if it fails, else the asset name. 
  **/
  virtual const char * GetAssetName(void) const
  {
	  return MName;
  }

  /**
  * Renames the asset
  * @param	Fname	the new name. 
  **/
  virtual void Rename(const char * /*fname*/)
  {

  }


  /**
  * Gets the submesh count
  * @return	The submesh count.
  **/
  virtual physx::PxU32 GetNumMaterials(void)
  {
	  physx::PxU32 ret = 0;

	  if ( MApexAsset )
	  {
		  const NxParameterized::Interface *iface = MApexAsset->getAssetNxParameterized();
		  if ( iface )
		  {
			  physx::PxU32 count;
			  const NxParameterized::ParamResult *results = NxParameterized::getParamList(*iface,NULL,"materialNames",count,true,false,GApexManager->GetApexSDK()->getParameterizedTraits());
			  if ( results )
			  {
				  for (physx::PxU32 i=0; i<count; i++)
				  {
					  const NxParameterized::ParamResult &r = results[i];
					  if ( r.mDataType == NxParameterized::TYPE_ARRAY )
					  {
						ret+=r.mArraySize;
					  }
				  }
				  NxParameterized::releaseParamList(count,results,GApexManager->GetApexSDK()->getParameterizedTraits());
			  }
		  }
	  }



	  return ret;
  }

  /**
  * Returns the material name
  * @param	Index - The material index.
  *
  * @return	null if it fails, else the material name. 
  **/
  virtual const char * GetMaterialName(physx::PxU32 findIndex)
  {
	  const char *ret = NULL;

	  physx::PxU32 index = 0;

	  if ( MApexAsset )
	  {
		  const NxParameterized::Interface *iface = MApexAsset->getAssetNxParameterized();
		  if ( iface )
		  {
			  physx::PxU32 count;
			  const NxParameterized::ParamResult *results = NxParameterized::getParamList(*iface,NULL,"materialNames",count,true,false,GApexManager->GetApexSDK()->getParameterizedTraits());
			  if ( results )
			  {
				  for (physx::PxU32 i=0; i<count && ret==NULL; i++)
				  {
					  const NxParameterized::ParamResult &r = results[i];
					  if ( r.mDataType != NxParameterized::TYPE_ARRAY )
					  {
						  for (physx::PxI32 j=0; j<r.mArraySize; j++)
						  {
							  if ( index == findIndex )
							  {
								  NxParameterized::Handle handle(r.mHandle);
								  handle.set(j);
								  handle.getParamString(ret);
								  handle.popIndex();
								  break;
							  }
							  index++;
						  }
					  }
				  }
				  NxParameterized::releaseParamList(count,results,GApexManager->GetApexSDK()->getParameterizedTraits());
			  }
		  }
	  }

	  return ret;
  }

  	/**
	 * Serialize the asset so the game engine can save it out and restore it later.
  	 * @param [in,out]	Length - the length. 
  	 *
  	 * @return	null if it fails, else. 
  	**/
  	virtual const void * Serialize(physx::PxU32 &len) // serialize the asset so the game engine can save it out and restore it later.
  	{
  		void *ret = NULL;
  		len = 0;

		physx::PxFileBuf *buffer = GApexManager->GetApexSDK()->createMemoryWriteStream();
		const NxParameterized::Interface *obj = MApexAsset ? MApexAsset->getAssetNxParameterized() : NULL;
		PX_ASSERT(obj);
		PX_ASSERT(buffer);

		if ( obj && buffer )
		{
   			NxParameterized::Serializer *ser = GApexManager->GetApexSDK()->createSerializer(NxParameterized::Serializer::NST_BINARY);
   			PX_ASSERT(ser);
   			if ( ser )
   			{
   				ser->serialize(*buffer,&obj,1);
   				ser->release();
   			}

			const void *data = GApexManager->GetApexSDK()->getMemoryWriteBuffer(*buffer,len);
			PX_ASSERT(data);
			if ( data )
			{
				ret = appMalloc(len);
				memcpy(ret,data,len);
			}
		}

		if ( buffer )
		{
			GApexManager->GetApexSDK()->releaseMemoryWriteStream(*buffer);
		}
  		return ret;
	}

  	/**
	 * Releases the serialized memory
  	 * @param	Mem - a pointer to the memory.
  	**/
	void ReleaseSerializeMemory(const void *mem)
	{
		appFree((void *)mem);
	}

	/**
	 * Gets the bind transformation for the asset
	 * @return	The bind transformation.
	**/
	virtual const physx::PxMat44& GetBindTransformation(void)
	{
		return MTransformation;
	}

	void flipUV(::NxParameterized::Interface *ApexRenderMeshAssetAuthoring,bool flipU,bool flipV)
	{
		if ( !flipU && !flipV ) return;

		::NxParameterized::Handle handle(*ApexRenderMeshAssetAuthoring);
		::NxParameterized::Interface *iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,"submeshes",handle);
		if ( iface )
		{
			int NumSubmeshes=0;
			if ( handle.getArraySize(NumSubmeshes,0) == ::NxParameterized::ERROR_NONE )
			{
				// iterate each submesh
				for (physx::PxI32 i = 0; i < NumSubmeshes; i++)
				{
					char scratch[MAX_SPRINTF];

					appSprintfANSI( scratch, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats", i );
					iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,scratch,handle);
					physx::PxI32 BufArraySize = 0;
					::NxParameterized::ErrorType BufArrayErrorType = handle.getArraySize(BufArraySize, 0);
					PX_ASSERT(BufArrayErrorType ==::NxParameterized::ERROR_NONE);

					// iterate each vertex buffer of this submesh
					for (physx::PxI32 j = 0; j < BufArraySize; j++)
					{
						appSprintfANSI( scratch, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats[%d].semantic", i, j );
						iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring, scratch, handle);

						if (iface)
						{
							physx::PxI32 bufSemantic;
							::NxParameterized::ErrorType BufSemanticErrorType = handle.getParamI32(bufSemantic);
							PX_ASSERT(BufSemanticErrorType ==::NxParameterized::ERROR_NONE);

							// this vertex buffer is a texCoords one
							if ( bufSemantic <= physx::NxRenderVertexSemantic::TEXCOORD3
								&& bufSemantic >= physx::NxRenderVertexSemantic::TEXCOORD0 )
							{
								// retrieve the data handle, It is not an F32x2 array it is an struct array
								appSprintfANSI( scratch, "submeshes[%d].vertexBuffer.buffers[%d].data", i, j );
								iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring, scratch, handle);
								physx::PxI32 DataElementSize = 0;

								if (handle.getArraySize(DataElementSize, 0) ==::NxParameterized::ERROR_NONE)
								{
									physx::PxF32 maxTexelU = -1e9;
									physx::PxF32 maxTexelV = -1e9;

									for (physx::PxI32 k = 0; k < DataElementSize; k++)
									{
										handle.set(k); // push handle to select array element k
	                                    
										physx::PxF32 texel[2];
										handle.set(0); // read element 0
										handle.getParamF32(texel[0]);
										handle.popIndex();
										handle.set(1); // read element 1
										handle.getParamF32(texel[1]);
										handle.popIndex();

										maxTexelU = Max(maxTexelU, texel[0] - 0.0001f);
										maxTexelV = Max(maxTexelV, texel[1] - 0.0001f);
										handle.popIndex(); // pop handle back to array
									}

									maxTexelU = ::floor(maxTexelU) + 1.0f;
									maxTexelV = ::floor(maxTexelV) + 1.0f;

									for (physx::PxI32 k = 0; k < DataElementSize; k++)
									{
										handle.set(k); // push handle to select array element k

										physx::PxF32 texel;
										handle.set(1); // read element 1
										handle.getParamF32(texel);
										handle.popIndex();

										if (flipU)
										{
											handle.set(0); // read element 0
											handle.getParamF32(texel);
											handle.setParamF32(maxTexelU - texel);
											handle.popIndex();
										}

										if (flipV)
										{
											handle.set(1); // read element 1
											handle.getParamF32(texel);
											handle.setParamF32(maxTexelV - texel);
											handle.popIndex();
										}

										handle.popIndex(); // pop handle back to array
									}
								}
								else
								{
									PX_ASSERT(0);
								}
							}
						}
					}
				}
			}
		}
	}


#if !WITH_APEX_SHIPPING

	/**
	 * Applies a transformation to the asset
	 * @param	Transformation - The transformation.
	 * @param	Scale -	The scale.
	 * @param	bApplyToGraphics - TRUE to apply to graphics.
	 * @param	bApplyToPhysics	- TRUE to apply to physics.
	 * @param	bClockWise - TRUE to clock wise.
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails.
	**/
	virtual UBOOL ApplyTransformation(const physx::PxMat44 &transformation,physx::PxF32 scale,UBOOL applyToGraphics,UBOOL applyToPhysics,UBOOL /*clockWise*/)
  	{
  		UBOOL ret = FALSE;

		if ( MType == AAT_DESTRUCTIBLE && MApexAsset )
		{
			NotifyAssetGoingAway();

			NxApexSDK *apexSDK = GApexManager->GetApexSDK();
			NxApexAsset *apexAsset = MApexAsset;
			NxDestructibleAsset *ApexDestructibleAsset = static_cast< NxDestructibleAsset *>(apexAsset);
			NxModuleDestructible *dmodule = GApexManager->GetModuleDestructible();

			// create an NxDestructibleAssetAuthoring instance of the asset...
			NxDestructibleAssetAuthoring *ApexDestructibleAssetAuthoring = static_cast<NxDestructibleAssetAuthoring*>(ApexAuthoringFromAsset(MApexAsset, apexSDK));

			PX_ASSERT(ApexDestructibleAssetAuthoring);

			if( ensure(ApexDestructibleAssetAuthoring) )
			{
				//Avoid using NxDestructibleAssetAuthoring::getRenderMeshAssetAuthoring function since it will return you a 
				//copy instead of a reference of the render mesh asset authoring
				NxParameterized::Interface* _ApexDestructibleInterface = ApexDestructibleAssetAuthoring->getNxParameterized();
				
				if ( ensure(_ApexDestructibleInterface) )
				{
					NxParameterized::Interface *iface = NULL;
					bool flipU = false;
					bool flipV = true;

					NxParameterized::getParamRef( *_ApexDestructibleInterface, "renderMeshAsset", iface );
					PX_ASSERT( iface );
#if 0
					NxParameterized::Handle handle(iface);
					NxParameterized::Interface *uVOriginInterface = NxParameterized::findParam(*iface,"textureUVOrigin",handle);
					if ( uVOriginInterface )
					{
						physx::PxU32 textureUVOrigin;
						handle.getParamU32(textureUVOrigin);
						textureUVOrigin = NxTextureUVOrigin::ORIGIN_BOTTOM_LEFT;
						switch ( textureUVOrigin )
						{
							case NxTextureUVOrigin::ORIGIN_TOP_LEFT:
								flipU = false;
								flipV = false;
								break;
							case NxTextureUVOrigin::ORIGIN_TOP_RIGHT:	
								flipU = true;
								flipV = false;
								break;
							case NxTextureUVOrigin::ORIGIN_BOTTOM_LEFT:
								flipU = false;
								flipV = true;
								break;
							case NxTextureUVOrigin::ORIGIN_BOTTOM_RIGHT:
								flipU = false;
								flipV = false;
								break;
						}
						handle.setParamU32(NxTextureUVOrigin::ORIGIN_TOP_LEFT);
					}
#endif
					if( ensure(iface) )
					{
						flipUV(iface,flipU,flipV);
					}
					if( iface == NULL )
					{
						debugf( NAME_DevPhysics, TEXT("Unable to find renderMeshAsset in the APEX destructible asset") );
						return FALSE;
					}
				}

				ApexDestructibleAssetAuthoring->applyTransformation(transformation,scale);

				// cache off asset name...
				char ApexAssetName[1024] = {0};
				appStrncpyANSI( ApexAssetName, ApexDestructibleAsset->getName(), sizeof(ApexAssetName) );

				// destroy old asset...
				ApexDestructibleAsset->release();
				ApexDestructibleAsset = NULL;

				// create new asset from the authoring...
				ApexDestructibleAsset = (NxDestructibleAsset*)apexSDK->createAsset( *ApexDestructibleAssetAuthoring, ApexAssetName );
				PX_ASSERT(ApexDestructibleAsset);

				// release authoring...
				ApexDestructibleAssetAuthoring->release();

				MApexAsset = ApexDestructibleAsset;
				NotifyAssetImport();

			}
		}
		else if ( MType == AAT_CLOTHING && MApexAsset )
		{
  			NotifyAssetGoingAway();

   			NxApexSDK *apexSDK = GApexManager->GetApexSDK();
   			NxApexAsset *apexAsset = MApexAsset;
   			NxClothingAsset *ApexClothingAsset = static_cast< NxClothingAsset *>(apexAsset);
   			NxModuleClothing *dmodule = GApexManager->GetModuleClothing();

   			// create an NxClothingAssetAuthoring instance of the asset...
			NxClothingAssetAuthoring *ApexClothingAssetAuthoring = static_cast<NxClothingAssetAuthoring*>(ApexAuthoringFromAsset(MApexAsset, apexSDK));

   			PX_ASSERT(ApexClothingAssetAuthoring);

   			if(ApexClothingAssetAuthoring)
   			{
				ApexClothingAssetAuthoring->applyTransformation(transformation,scale,applyToGraphics ? true : false,applyToPhysics ? true : false);
				MTransformation = transformation;
				TPxMat44Vector newBindPoses;
				for (physx::PxU32 i=0; i<ApexClothingAsset->getNumUsedBones(); i++)
				{
					physx::PxMat44 mat;
					ApexClothingAssetAuthoring->getBoneBindPose(i,mat);
					physx::PxMat44 pmat;
					pmat = mat * transformation;
					newBindPoses.push_back(pmat);
				}

				ApexClothingAssetAuthoring->updateBindPoses(&newBindPoses[0], newBindPoses.size(), TRUE, TRUE);

    			// destroy and create a new asset based off the authoring version...
    			if(ApexClothingAssetAuthoring)
    			{

					for (physx::PxU32 i=0; i<256; i++)
					{
						NxParameterized::Interface *ApexRenderMeshAssetAuthoring = ApexClothingAssetAuthoring->getRenderMeshAssetAuthoring(i);
						if ( ApexRenderMeshAssetAuthoring )
						{
							NxParameterized::Handle handle(ApexRenderMeshAssetAuthoring);
							bool flipU = false;
							bool flipV = false;
							NxParameterized::Interface *iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,"textureUVOrigin",handle);
							if ( iface )
							{
								physx::PxU32 textureUVOrigin;
								handle.getParamU32(textureUVOrigin);
								textureUVOrigin = NxTextureUVOrigin::ORIGIN_BOTTOM_LEFT;
								switch ( textureUVOrigin )
								{
									case NxTextureUVOrigin::ORIGIN_TOP_LEFT:
										flipU = false;
										flipV = false;
										break;
									case NxTextureUVOrigin::ORIGIN_TOP_RIGHT:	
										flipU = true;
										flipV = false;
										break;
									case NxTextureUVOrigin::ORIGIN_BOTTOM_LEFT:
										flipU = false;
										flipV = true;
										break;
									case NxTextureUVOrigin::ORIGIN_BOTTOM_RIGHT:
										flipU = false;
										flipV = false;
										break;
								}
								handle.setParamU32(NxTextureUVOrigin::ORIGIN_TOP_LEFT);
							}
							flipUV(ApexRenderMeshAssetAuthoring,flipU,flipV);
#if 0  // experimental code to flip the winding order of the mesh
							{
								char scratch[512];
								physx::string::sprintf_s(scratch,512,"submeshes[%d].indexBuffer",i);
								iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,scratch,handle);
								PxI32 size=0;
								if ( iface->getArraySize(handle,size,0) == NxParameterized::ERROR_NONE )
								{
									physx::PxU32 tcount = size/3;
									for (physx::PxU32 i=0; i<tcount; i++)
									{
										physx::PxU32 indices[3];
										NxParameterized::ErrorType type = handle.getParamU32Array(indices,3,(i*3));
										PX_ASSERT( type == NxParameterized::ERROR_NONE );
										physx::PxU32 t = indices[0];
										indices[0] = indices[2];
										indices[2] = t;
										iface->setParamU32Array(handle,indices,3,(i*3));
									}
								}
								else
								{
									PX_ASSERT(0);
								}
							}
#endif
#if 0 // experimental code to inverse the normals of the mesh
							{
								char scratch[512];
								physx::string::sprintf_s(scratch,512,"submeshes[%d].vertexBuffer.normals",i);
								iface = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,scratch,handle);
								PxI32 size=0;
								if ( iface->getArraySize(handle,size,0) == NxParameterized::ERROR_NONE )
								{
									physx::PxU32 nc = size;
									for (physx::PxU32 i=0; i<nc; i++)
									{
										physx::PxVec3 normal;
										NxParameterized::ErrorType type = iface->getParamVec3Array(handle,&normal,1,i);
										PX_ASSERT( type == NxParameterized::ERROR_NONE );
										normal*=-1; // flip normal sign
										iface->setParamVec3Array(handle,&normal,1,i);
									}
								}
								else
								{
									PX_ASSERT(0);
								}
							}
#endif
						}
						else
						{
							break;
						}


					}

    				// cache off asset name...
    				char ApexAssetName[1024] = {0};
					appStrncpyANSI( ApexAssetName, ApexClothingAsset->getName(), sizeof(ApexAssetName) );

    				// destroy old asset...
    				ApexClothingAsset->release();
    				ApexClothingAsset = NULL;

    				// create new asset from the authoring...
    				ApexClothingAsset = (NxClothingAsset*)apexSDK->createAsset( *ApexClothingAssetAuthoring, ApexAssetName );
    				PX_ASSERT(ApexClothingAsset);

    				// release authoring...
    				ApexClothingAssetAuthoring->release();
    			}

    			MApexAsset = ApexClothingAsset;

   				NotifyAssetImport();
   			}
		}

		return ret;
	}


	/**
	 * Flips a clothing asset's index buffer triangle winding (used for 0.9 assets that were never flipped)
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL MirrorLegacyClothingNormalsAndTris()
	{
		UBOOL ret = FALSE;
		if ( MType == AAT_CLOTHING && MApexAsset )
		{
  			NotifyAssetGoingAway();

   			NxApexSDK *apexSDK = GApexManager->GetApexSDK();
   			NxApexAsset *apexAsset = MApexAsset;
   			NxClothingAsset *ApexClothingAsset = static_cast< NxClothingAsset *>(apexAsset);
   			NxModuleClothing *dmodule = GApexManager->GetModuleClothing();

			NxClothingAssetAuthoring *ApexClothingAssetAuthoring = static_cast<NxClothingAssetAuthoring*>(ApexAuthoringFromAsset(MApexAsset, apexSDK));

   			PX_ASSERT(ApexClothingAssetAuthoring);

			// destroy and create a new asset based off the authoring version...
			if(ApexClothingAssetAuthoring)
			{

				for (physx::PxU32 i=0; i<256; i++)
				{
					NxParameterized::Interface *ApexRenderMeshAssetAuthoring = ApexClothingAssetAuthoring->getRenderMeshAssetAuthoring(i);
					if ( ApexRenderMeshAssetAuthoring )
					{
						NxParameterized::Handle handle(ApexRenderMeshAssetAuthoring);
						NxParameterized::Interface *iface = 0;
						// reverse triangle winding
						{
							char ibName[MAX_SPRINTF];
							appSprintfANSI( ibName, "submeshes[%d].indexBuffer", i );
							iface = NxParameterized::findParam( *ApexRenderMeshAssetAuthoring, ibName, handle );
							PX_ASSERT( iface );
							physx::PxI32 size=0;
							if ( handle.getArraySize(size,0) == NxParameterized::ERROR_NONE )
							{
								physx::PxU32 tcount = size/3;
								for (physx::PxU32 i=0; i<tcount; i++)
								{
									physx::PxU32 indices[3];
									NxParameterized::ErrorType type = handle.getParamU32Array( indices, 3, (i*3) );
									PX_ASSERT( type == NxParameterized::ERROR_NONE );
									physx::PxU32 t = indices[1];
									indices[1] = indices[2];
									indices[2] = t;
									handle.setParamU32Array( indices, 3, (i*3) );

									ret = TRUE;
								}
							}
							else
							{
								PX_ASSERT(0);
							}
						}

						// reverse normals
						{
							char vbName[MAX_SPRINTF];

							physx::PxI32 numFormats = 0;
							appSprintfANSI( vbName, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats", i );
							iface = NxParameterized::findParam( *ApexRenderMeshAssetAuthoring, vbName, handle );
							handle.getArraySize(numFormats);

							for ( physx::PxI32 j=0; j<numFormats; j++ )
							{
								// look at each buffer semantic in the VertexFormatParameters
								physx::PxI32 semantic = physx::NxRenderVertexSemantic::NUM_SEMANTICS;

								appSprintfANSI( vbName, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats[%d].semantic", i, j );
								iface = NxParameterized::findParam( *ApexRenderMeshAssetAuthoring, vbName, handle );
								handle.getParamI32(semantic);

								if ( physx::NxRenderVertexSemantic::NORMAL == semantic )
								{
									// verify this semantic is FLOAT3
									physx::PxU32 format = physx::NxRenderDataFormat::UNSPECIFIED;
									appSprintfANSI( vbName, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats[%d].format", i, j);
									iface = NxParameterized::findParam( *ApexRenderMeshAssetAuthoring, vbName, handle );
									handle.getParamU32(format);

									PX_ASSERT( format == physx::NxRenderDataFormat::FLOAT3 );
									if ( format != physx::NxRenderDataFormat::FLOAT3 )
									{
										break;
									}

									// get a handle to the proper data buffer
									appSprintfANSI( vbName, "submeshes[%d].vertexBuffer.buffers[%d].data", i, j );
									iface = NxParameterized::findParam( *ApexRenderMeshAssetAuthoring, vbName, handle );

									physx::PxI32 size=0;
									if ( handle.getArraySize(size) == NxParameterized::ERROR_NONE )
									{
										// This transform is a complete inversion, not just along y-axis.
										// The reason for this is that the old transform that was erronously applied was a 3x3 diagonal matrix (-1, 1, -1)
										// The real transform that should have happened is a 3x3 diagonal matrix (1, -1, 1).
										// To undo the old and do the right transform, the combined transform is the 3x3 diagonal matrix (-1, -1, -1)
										physx::PxMat33 mirrorTransform = -physx::PxMat33::createIdentity();

										for (physx::PxI32 k=0; k<size; k++)
										{
											physx::PxVec3 normal;

											handle.set(k);
											//PX_ASSERT( NxParameterized::DataType::TYPE_VEC3 == handle.parameterDefinition()->type() );

											NxParameterized::ErrorType e = handle.getParamVec3( normal );
											PX_ASSERT( e == NxParameterized::ERROR_NONE );

											// apply the transformation
											normal = mirrorTransform.transform( normal );
											handle.setParamVec3( normal );

											handle.popIndex();
											ret = TRUE;
										}
									}
									else
									{
										PX_ASSERT(0);
									}
								}
							}
						}

					}
					else
					{
						break;
					}
				}

				// cache off asset name...
				char ApexAssetName[1024] = {0};
				appStrncpyANSI( ApexAssetName, ApexClothingAsset->getName(), sizeof(ApexAssetName) );

				// destroy old asset...
				ApexClothingAsset->release();
				ApexClothingAsset = NULL;

				// create new asset from the authoring...
				ApexClothingAsset = (NxClothingAsset*)apexSDK->createAsset( *ApexClothingAssetAuthoring, ApexAssetName );
				PX_ASSERT(ApexClothingAsset);

				// release authoring...
				ApexClothingAssetAuthoring->release();
			}

			MApexAsset = ApexClothingAsset;

			NotifyAssetImport();
		
		}

		return ret;
	}

#endif

	virtual UBOOL GetDesc(char *dest,physx::PxU32 slen)
	{
		UBOOL ret = FALSE;
		if ( MApexAsset )
		{
			const char *type = "UNKNOWN";

			switch ( GetType() )
			{
			case AAT_DESTRUCTIBLE:
				type = "Destructible";
				break;
			case AAT_CLOTHING:
				type = "Clothing";
				break;
			case AAT_APEX_EMITTER:		// asset used to construct emitters.
				type = "Shape Emitter";
				break;
			case AAT_RENDER_MESH:
				type = "Render Mesh";
				break;
			case AAT_GENERIC: // just generic data, we don't resolve it to a specific type.
				PX_ALWAYS_ASSERT();
				break;
			case AAT_GROUND_EMITTER:
				type = "Ground Emitter";
				break;
			case AAT_IMPACT_EMITTER:
				type = "Impact Emitter";
				break;
			case AAT_BASIC_IOS:
				type = "Basic IOS";
				break;
			case AAT_FLUID_IOS:
				type = "Fluid IOS";
				break;
			case AAT_IOFX:
				type = "IOFX";
				break;
			default:
				PX_ALWAYS_ASSERT();
				break;

			}

			appStrncpyANSI( dest, type, slen );

			ret = TRUE;
		}
		else
		{
			appStrncpyANSI( dest, "NO ASSET", slen );
		}
		return ret;
	}

	// only keep the root file name; so skip path information up to all 'slashes'
	// also, truncate any postfix '.apx' '.apb', etc.
	void normalizeName(const char *source,char *_dest,physx::PxU32 len)
	{
		char *dest = _dest;
		const char *begin = source;
		const char *stop = &source[len];
		while ( *source && source < stop )
		{
			if ( *source == '\\' )
			{
				begin = source+1;
			}
			else if ( *source == '/' )
			{
				begin = source+1;
			}
			source++;
		}
		while ( *begin && begin < stop && *begin != '.' )
		{
			*dest++ = *begin++;
		}
		*dest = 0;

		// Convert _dest to lower case, no appStrlwrANSI exists
		for (ANSICHAR* Lower = _dest; *Lower != '\0'; ++Lower)
		{
			*Lower = tolower(*Lower);
		}
	}

	BOOL NameMatch(const char *typeName,const char *fname,BOOL &exactMatch)
	{
		BOOL ret = FALSE;

		if ( MApexAsset )
		{
			const char *objName = MApexAsset->getObjTypeName();

			if ( typeName == NULL )
			{
				typeName = "";
				objName = "";
			}
			if ( strcmp(objName,typeName) == 0 )
			{
				if ( strcmp(fname,MName) == 0 ) // if the base names match, it's already cool!
				{
					exactMatch = TRUE;
					ret = TRUE;
				}
				else
				{
					char scratch1[512];
					normalizeName(fname,scratch1,512);
					char scratch2[512];
					normalizeName(MOriginalApexName,scratch2,512);
					if ( strcmp(scratch1,scratch2) == 0 )
					{
						ret = TRUE;
					}
				}
			}
		}

		return ret;
	}

  ApexAssetType			MType;
  char *				MName;
  char *				MOriginalApexName;
  physx::PxI32			MRefCount;
  NxApexAsset		   *MApexAsset;
  TApexActorVector		MActors;  // the actors associated with
  physx::PxMat44		MTransformation;
  UBOOL					MForceLoad;
  UApexAsset			*mObject;
#if WITH_APEX_PARTICLES
	UBOOL				mEdited; // true if this asset was edited with the APEX properties editor.
	ApexEditInterface *mApexEditInterface;
#endif
};



//**********************************************************************

#define APEX_DEBUG_ADD(x) GNRP->setResourceU32("AI_APEX_DEBUG",#x,NxApexParam::VISUALIZE_##x,TRUE)

enum ApexCommands
{
    ASC_NONE,
	ASC_APEXVIS,
	ASC_APEX_SHOW,
	ASC_APEXDESTRUCTIBLELOD,
	ASC_APEXDESTRUCTIBLEMAXDYNAMICCHUNKISLAND,
	ASC_APEXDESTRUCTIBLEMAXCHUNK,
	ASC_APEX_LOD_RESOURCE_BUDGET,
	ASC_APEXCLOTHINGREC
};

#define ASC_ADD(x) GNRP->setResourceU32("AI_COMMANDS",#x,ASC_##x,TRUE)

#define MODULE_COUNT 11
static const char *moduleNames[MODULE_COUNT] =
{
	"Clothing",
	"Destructible",
	"Emitter",
	"Iofx",
	"Particles",
	"Vegetation",
	"Wind",
	"Explosion",
	"FieldBoundary",
	"BasicIos",
	"FluidIos"
};

class MyApexCommands : public FIApexCommands
{
public:
  MyApexCommands(void)
  {
	  mLastBuffer = NULL;
	  mLastName = NULL;

  	GNRP = GApexManager->GetApexSDK()->getNamedResourceProvider();
	MApexRender = CreateApexRender();
	MResourceCallback = CreateResourceCallback();
	MShowApex = TRUE;

	GApexManager->SetResourceCallback(MResourceCallback);
	GApexManager->SetRenderResourceManager(MApexRender->GetRenderResourceManager() );

	MOnExit = FALSE;
    GApexCommands = this;
	MApexSDK = GApexManager->GetApexSDK();
	MVisualizeApex  = FALSE;
	MClothingRecording = FALSE;
    MFreshCommand = FALSE;

	mDebugVisNamesInitialized = FALSE;

	ASC_ADD(APEXVIS);
	ASC_ADD(APEX_SHOW);
	ASC_ADD(APEXDESTRUCTIBLELOD);
	ASC_ADD(APEXDESTRUCTIBLEMAXCHUNK);
	ASC_ADD(APEX_LOD_RESOURCE_BUDGET);
	ASC_ADD(APEXCLOTHINGREC);
  }

  ~MyApexCommands(void)
  {
	  appFree(mLastBuffer);
	  appFree(mLastName);
	MOnExit = TRUE;
	physx::PxU32 count;
	void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
	for (physx::PxU32 i=0; i<count; i++)
	{
		ApexAsset *aa = (ApexAsset *)assets[i];
		delete aa;
	}
	GNRP->releaseAllResourcesInNamespace("AI_APEX_ASSET");

	ReleaseApexRender(MApexRender);

	{
		physx::PxU32 count;
		void **strings = GNRP->findAllResources("AI_PERSISTENT_STRING",count);
		for (physx::PxU32 i=0; i<count; i++)
		{
			appFree(strings[i]);
		}
	}

	GNRP->releaseAllResourcesInNamespace("AI_APEX_DEBUG");
	GNRP->releaseAllResourcesInNamespace("AI_COMMANDS");
	GNRP->releaseAllResourcesInNamespace("AI_PERSISTENT_STRING");

	ReleaseResourceCallback(MResourceCallback);
	GApexManager->SetResourceCallback(0);

  }

  virtual NxResourceCallback      * getResourceCallback(void)
  {
    return MResourceCallback;
  }

  virtual NxUserRenderResourceManager * getRenderResourceManager(void)
  {
  	return MApexRender->GetRenderResourceManager();
  }

  const char *skipSpaces(const char *str)
  {
    while ( *str && *str == 32 ) str++;
    if ( *str == 0 )
    {
        str = NULL;
    }
    return str;
  }

  const char * GetCommand(const char *cmd,char *command)
  {
    char *dest = command;
    char *edest = &command[510];
    while ( *cmd && *cmd != 32 )
    {
      *dest++ = *cmd++;
      if ( dest == edest )
      {
          break;
      }
    }
    *dest = NULL;
    cmd = skipSpaces(cmd);

	// Convert command to upper case, no appStruprANSI exists
	for (ANSICHAR* Upper = command; *Upper != '\0'; ++Upper)
	{
		*Upper = toupper(*Upper);
	}

	return cmd;
  }

  virtual UBOOL isNumeric(const char *cmd)
  {
    UBOOL ret = FALSE;
    if ( cmd )
    {
      if ( *cmd == '-' || (*cmd >= '0' && *cmd <= '9') || *cmd == '.' )
      {
        ret = TRUE;
      }
    }
    return ret;
  }

  physx::PxF32 GetDebugValue(const char *cmd,physx::PxF32 v)
  {
	  if ( isNumeric(cmd) )
	  {
		  v = (physx::PxF32)atof(cmd);
	  }
	  else if ( strcmp(cmd,"TRUE") == 0 )
	  {
		  v = 1;
	  }
	  else if ( strcmp(cmd,"FALSE") == 0 )
	  {
		  v = 0;
	  }
	  return v;
  }

  bool IsNameToIgnore(const char* name) const
  {
	  return strcmp(name,"VISUALIZATION_ENABLE") == 0 ||
		  strcmp(name,"VISUALIZATION_SCALE") == 0  ||
		  strcmp(name,"VISUALIZE_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_EMITTER_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_DESTRUCTIBLE_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_IOFX_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_GROUND_EMITTER_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_IMPACT_EMITTER_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_BASIC_IOS_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_FLUID_IOS_ACTOR") == 0  ||
		  strcmp(name,"VISUALIZE_CLOTHING_ACTOR") == 0 ||
		  strcmp(name,"moduleName") == 0;
  }

  /**
  * Process commands
  * @param	Format - Describes the format to use. 
  *
  * @return	TRUE if it succeeds, FALSE if it fails. 
  **/
  virtual UBOOL ProcessCommand(const char* Format, FOutputDevice* Ar)
  {
	UBOOL ret = TRUE;

	const char *cmd = Format;

    char command[512];
    cmd = GetCommand(cmd,command);
    ApexCommands asc = (ApexCommands)GNRP->findResourceU32("AI_COMMANDS",command);
    switch ( asc )
    {
		case ASC_APEX_LOD_RESOURCE_BUDGET:
			if ( cmd )
			{
				physx::PxF32 LOD = 0.0f;
				if ( 0 == appStricmp(ANSI_TO_TCHAR(cmd), TEXT("default")) )
				{
					LOD = GSystemSettings.ApexLODResourceBudget;
				}
				else
				{
					LOD = atof( cmd );
				}
				if ( GWorld && GWorld->RBPhysScene && GWorld->RBPhysScene->ApexScene )
				{
					physx::apex::NxApexScene *Scene = GWorld->RBPhysScene->ApexScene->GetApexScene();
					if ( Scene )
					{
						Scene->setLODResourceBudget( LOD );
						Ar->Logf(NAME_DevPhysics, TEXT("SetApexLODResourceBudget(%f)"), (FLOAT) LOD );
					}
				}
			}
			else if ( GWorld && GWorld->RBPhysScene && GWorld->RBPhysScene->ApexScene )
			{
				physx::apex::NxApexScene *scene = GWorld->RBPhysScene->ApexScene->GetApexScene();
				if ( scene )
				{
					physx::PxF32 LOD = scene->getLODResourceConsumed();
					Ar->Logf(NAME_DevPhysics, TEXT("ApexLODResourceConsumed(%f)"), (FLOAT) LOD );
				}
			}
			break;
		case ASC_APEXDESTRUCTIBLELOD:
			if ( cmd )
			{
				// Set new destructible LOD (must be between 0 and 1)
				physx::apex::NxModuleDestructible *DestructibleModule = GApexManager->GetModuleDestructible();
				if ( DestructibleModule  )
				{
					FLOAT NewLOD = 0.0f;
					if ( 0 == appStricmp(ANSI_TO_TCHAR(cmd), TEXT("default")) )
					{
						NewLOD = GSystemSettings.ApexDestructionMaxChunkSeparationLOD;
					}
					else
					{
						NewLOD = Clamp<FLOAT>( appAtof(ANSI_TO_TCHAR(cmd)), 0.0f, 1.0f );
					}

					DestructibleModule->setMaxChunkSeparationLOD( NewLOD );

					Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleLod(%f)"), NewLOD );
				}
			}
			else
			{
				// Show usage
				Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleLod [newLOD], new LOD between 0 and 1") );
			}
			break;
		case ASC_APEXDESTRUCTIBLEMAXDYNAMICCHUNKISLAND:
			if ( cmd )
			{
				// Set new destructible max chunk setting
				physx::apex::NxModuleDestructible *DestructibleModule = GApexManager->GetModuleDestructible();
				if ( DestructibleModule  )
				{
					UINT MaxChunkCount = 0;
					if ( 0 == appStricmp(ANSI_TO_TCHAR(cmd), TEXT("default")) )
					{
						MaxChunkCount = GSystemSettings.ApexDestructionMaxChunkIslandCount;
					}
					else
					{
						MaxChunkCount = appAtoi(ANSI_TO_TCHAR(cmd));
					}

					DestructibleModule->setMaxDynamicChunkIslandCount( MaxChunkCount );

					Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleMaxDynamicChunkIsland(%d)"), MaxChunkCount );
				}
			}
			else
			{
				// Show usage
				Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleMaxDynamicChunkIsland [newMaxChunkCount]") );
			}
			break;
		case ASC_APEXDESTRUCTIBLEMAXCHUNK:
			if ( cmd )
			{
				// Set new destructible max chunk setting
				physx::apex::NxModuleDestructible *DestructibleModule = GApexManager->GetModuleDestructible();
				if ( DestructibleModule  )
				{
					UINT MaxChunkCount = 0;
					if ( 0 == appStricmp(ANSI_TO_TCHAR(cmd), TEXT("default")) )
					{
						MaxChunkCount = GSystemSettings.ApexDestructionMaxShapeCount;
					}
					else
					{
						MaxChunkCount = appAtoi(ANSI_TO_TCHAR(cmd));
					}

					DestructibleModule->setMaxChunkCount( MaxChunkCount );

					Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleMaxChunk(%d)"), MaxChunkCount );
				}
			}
			else
			{
				// Show usage
				Ar->Logf( NAME_DevPhysics, TEXT("ApexDestructibleMaxChunk [newMaxChunkCount]") );
			}
			break;
    	case ASC_APEX_SHOW:
    		MShowApex = MShowApex ? FALSE : TRUE;
			Ar->Logf(NAME_DevPhysics, TEXT("ShowApex=%s"), MShowApex ? TEXT("TRUE") : TEXT("FALSE") );
			break;
		case ASC_APEXVIS:
			if ( cmd != NULL && mApexScenes.size() )
			{
				cmd = GetCommand(cmd,command);
				for (UINT sno=0; sno<mApexScenes.size(); sno++)
				{
					FIApexScene *fscene = mApexScenes[sno];
					if ( fscene == NULL ) continue;
					NxApexScene *apexScene = fscene->GetApexScene();
					if ( apexScene == NULL ) continue;

					bool visualizeThis = false;
					bool visualizeAny = false;
					FLOAT visScale = 0.0f;
					NxParameterized::Interface *sceneDebugRenderParams=NULL;
					sceneDebugRenderParams = apexScene->getDebugRenderParams();
					NxParameterized::getParamF32(*sceneDebugRenderParams,"VISUALIZATION_SCALE",visScale);
					// set default visualization scale
					if(visScale == 0.0f)
					{
						visScale = U2PScale;
						NxParameterized::setParamF32( *sceneDebugRenderParams,"VISUALIZATION_SCALE",visScale );
					}
					UBOOL debugOffApex  = strcmp(command,"APEX_CLEAR_ALL") == 0;
					for (UINT m=0; m<=MODULE_COUNT; m++)
					{
						NxParameterized::Interface *debugRenderParams=NULL;

						const char *moduleName = "ApexSDK";

						if ( m == 0 )
						{
							debugRenderParams = apexScene->getDebugRenderParams();
						}
						else
						{
							debugRenderParams = apexScene->getModuleDebugRenderParams(moduleNames[m-1]);
							moduleName = moduleNames[m-1];
						}

						if ( debugRenderParams )
						{
							UINT count = debugRenderParams->numParameters();
							for (UINT i=0; i<count; i++)
							{
								const NxParameterized::Definition* child = debugRenderParams->parameterDefinition(i);
								const char* name = child->name();

								if (IsNameToIgnore(name))
								{
									continue;
								}
								if ( debugOffApex )
								{
									switch ( child->type() )
									{
									case NxParameterized::TYPE_BOOL:
										NxParameterized::setParamBool(*debugRenderParams, name, false);
										break;
									case NxParameterized::TYPE_F32:
										NxParameterized::setParamF32(*debugRenderParams, name, 0.0f);
										break;
									case NxParameterized::TYPE_ARRAY:
										// do nothing with array types.
										break;
									case NxParameterized::TYPE_REF:
										// do nothing
										break;
									case NxParameterized::TYPE_STRUCT:
										// do nothing
										break;
									default:
										PX_ALWAYS_ASSERT();
										break;
									}
								}
								else
								{
									if ( appStricmp(name, command) == 0 )
									{
										FLOAT vis = 0;
										switch ( child->type() )
										{
										case NxParameterized::TYPE_BOOL:
											{
												bool b;
												NxParameterized::getParamBool(*debugRenderParams, name, b);
												vis = b ? 1.0f : 0.0f;
											}
											break;
										case NxParameterized::TYPE_F32:
											NxParameterized::getParamF32(*debugRenderParams, name, vis);
											break;
										default:
											PX_ALWAYS_ASSERT();
											break;
										}
										vis = vis > 0 ? 0.0f : 1.0f;
										if ( cmd )
										{
											if ( appStricmp(cmd, "true") == 0 )
											{
												vis = 1.0f;
											}
											else if ( appStricmp(cmd, "false") == 0 )
											{
												vis = 0.0f;
											}
											else
											{
												vis = atof(cmd);
											}
										}
										visualizeThis = vis > 0.0f;

										switch ( child->type() )
										{
										case NxParameterized::TYPE_BOOL:
											NxParameterized::setParamBool(*debugRenderParams, name, vis > 0.0f);
											break;
										case NxParameterized::TYPE_F32:
											NxParameterized::setParamF32(*debugRenderParams, name, vis);
											break;
										case NxParameterized::TYPE_REF:
											// do nothing
											break;
										default:
											PX_ALWAYS_ASSERT();
											break;
										}
									}
									else
									{
										switch ( child->type() )
										{
										case NxParameterized::TYPE_BOOL:
											{
												bool b = false;
												NxParameterized::getParamBool(*debugRenderParams, name, b);
												visualizeAny |= b;
											}
											break;
										case NxParameterized::TYPE_F32:
											{
												float vis = 0.0f;
												NxParameterized::getParamF32(*debugRenderParams, name, vis);
												visualizeAny |= vis > 0.0f;
											}
											break;
										}
									}
								}
							}
						}
					}
					visualizeAny |= visualizeThis;
					NxParameterized::Interface *debugRenderParams = apexScene->getDebugRenderParams();
					NxParameterized::setParamBool( *debugRenderParams, "VISUALIZATION_ENABLE", visualizeAny );
					Ar->Logf(NAME_DevPhysics, TEXT("Apex Debug (Scene %d): apexvis %s %s"), sno, ANSI_TO_TCHAR(command), visualizeThis ? TEXT("TRUE") : TEXT("FALSE") );
					MVisualizeApex = visualizeAny ? TRUE : FALSE;
				}
				ret = TRUE;
			}
			break;
		case ASC_APEXCLOTHINGREC:
		{
			MClothingRecording = !MClothingRecording;
			ret = TRUE;
			break;
		}
		case ASC_NONE:
			ret = FALSE;
			break;
		}
		return ret;
	}


  	/**
	 * Returns the Apex Asset
  	 * @param	AssetName - Name of the asset. 
  	 *
  	 * @return	null if it fails, else a pointer to the APEX asset. 
  	**/
    virtual FIApexAsset * GetApexAsset(const char *fname)
    {
        FIApexAsset *ret = NULL;

        if ( fname )
        {
			ApexAsset *found = (ApexAsset *)GNRP->findResource("AI_APEX_ASSET",fname);
			if ( found )
          	{
		    	found->MRefCount++;
            	ret = static_cast< FIApexAsset *>(found);
            }
#if 0
			else
			{
				physx::PxU32 count = 0;
				void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
				for (physx::PxU32 i=0; i<count; i++)
				{
					found = (ApexAsset *)assets[i];
					BOOL exactMatch;
					if ( found->NameMatch(NULL,fname,exactMatch) )
					{
						ret = static_cast< FIApexAsset *>(found);
						found->MRefCount++;
						break;
					}
				}
			}
#endif
        }
        return ret;
    }

	/**
	* Returns the Apex Manager
	* @return	null if it fails, else returns a pointer to the FIApexManager.
	**/
	virtual FIApexManager * GetApexManager(void) const
	{
		return GApexManager;
	}

	/**
	 * Notifies the Apex Asset that ithas been removed
	 * @param [in,out]	Asset - If non-null, the asset. 
	**/
	void NotifyApexAssetGone(FIApexAsset *_aa)
	{
		ApexAsset *aa = static_cast< ApexAsset *>(_aa);
		for (TApexAssetVector::iterator i=MPendingForceLoad.begin(); i!=MPendingForceLoad.end(); ++i)
		{
			if ( (*i) == aa )
			{
				(*i) = NULL;
				break;
			}
		}
		if ( !MOnExit )
		{
			physx::PxU32 refCount = GNRP->releaseResource("AI_APEX_ASSET", aa->GetName() );
#ifndef __CELLOS_LV2__
			refCount;
#endif
			PX_ASSERT(refCount == 0);
		}
	}

	/**
	 * Returns the NxUserRenderer
	 * @return	null if it fails, a pointer to the NxUserRenderer
	**/
	virtual NxUserRenderer*	GetNxUserRenderer(void)
	{
		return MApexRender->GetNxUserRenderer();
	}

	/**
	 * Returns the Apex Render
	 * @return	null if it fails, else returns a pointer to the FIApexRender.
	**/
	virtual FIApexRender * GetApexRender(void)
	{
		return MApexRender;
	}

  	/**
	 * Returns the Apex Asset from memory
  	 * @param	AssetName - Name of the asset. 
  	 * @param	Data - The data. 
  	 * @param	DataLength - Length of the data. 
  	 *
  	 * @return	null if it fails, else returns the FIApexAsset from memory. 
  	**/
  	virtual FIApexAsset* GetApexAssetFromMemory(const char *fname,const void *data,physx::PxU32 dlen,const char *originalName,UApexAsset *obj)
  	{
  		FIApexAsset *ret = NULL;

		ApexAsset *aa = (ApexAsset *)GNRP->findResource("AI_APEX_ASSET",fname);
		if ( aa )
		{
			aa->NotifyAssetGoingAway();
		}
		aa = new ApexAsset(fname,data,dlen,NULL,originalName,obj);
		ret = static_cast< FIApexAsset *>(aa);
		GNRP->setResource("AI_APEX_ASSET",fname,aa,TRUE);
		return ret;
  	}

  	/**
	 * Returns the Apex Asset from NxParameterized::Interface
  	 * @param	AssetName - Name of the asset.
  	 * @param	Params - NxParameterized::Interface.
  	 *
  	 * @return	null if it fails, else returns the FIApexAsset from NxParameterized::Interface.
  	**/
  	virtual FIApexAsset * GetApexAssetFromParams(const char *fname,const void *params,const char *originalName,UApexAsset *obj)
	{
		FIApexAsset *ret = NULL;

		ApexAsset *aa = (ApexAsset *)GNRP->findResource("AI_APEX_ASSET",fname);
		if ( aa )
		{
        	aa->LoadFromParams(fname,fname,originalName,obj);
        	ret = static_cast< FIApexAsset *>(aa);
		}
		else
		{
        	ApexAsset *aa = new ApexAsset(fname,NULL,0,params,originalName,obj);
        	ret = static_cast< FIApexAsset *>(aa);
        	GNRP->setResource("AI_APEX_ASSET",fname,aa,TRUE);
        }
        return ret;
	}

	/**
	 * Sets the resource callback
	 * @param [in,out]	Callback - If non-null, a pointer to the callback function. 
	**/
	virtual void SetNxResourceCallback(NxResourceCallback *callback)
	{
		MResourceCallback = callback;
	}

	/**
	 * Releases the NxApexAsset
	 * @param [in,out]	Asset	If non-null, the asset. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL ReleaseNxApexAsset(NxApexAsset *asset) // this apex asset instance is being requested to be released.
	{
		UBOOL ret = FALSE;

		physx::PxU32 count = 0;
		void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
		for (physx::PxU32 i=0; i<count; i++)
		{
			ApexAsset *aa = (ApexAsset *)assets[i];
			ret = aa->ReleaseNxApexAsset(asset);
			if ( ret )
			{
				break;
			}
		}

		return ret;
	}

	/**
	 * Returns the persistent name
	 * @param	String - The string.
	 *
	 * @return	null if it fails, else the persistent string.
	**/
	virtual const char * GetPersistentString(const char *str)
	{
		if ( str == 0 )
		{
		    str = "";
		}
		const char *ret = (const char *)GNRP->findResource("AI_PERSISTENT_STRING",str);
		if ( ret == 0 )
		{
			physx::PxU32 len = (physx::PxU32)strlen(str);
			char *tmp = (char *)appMalloc(len+1);
			memcpy(tmp,str,len+1);
			GNRP->setResource("AI_PERSISTENT_STRING",tmp,tmp,TRUE);
			ret = tmp;
		}
		return ret;
	}


	/**
	 * Check to see if Apex is being shown
	 * @return	TRUE if show apex, FALSE if not.
	**/
	virtual UBOOL IsShowApex(void) const { return MShowApex; };

	/**
	 * Check to see if a command is an APEX command
	 * @param	Command	The command.
	 *
	 * @return	TRUE if apex command, FALSE if not.
	**/
	virtual UBOOL IsApexCommand(const char *cmd)
	{
		UBOOL ret = FALSE;

    	char command[512];
    	cmd = GetCommand(cmd,command);
    	ApexCommands asc = (ApexCommands)GNRP->findResourceU32("AI_COMMANDS",command);
    	if ( asc !=	ASC_NONE )
    	{
    		ret = TRUE;
    	}
    	return ret;
	}

	virtual UBOOL IsVisualizeApex(void) const
	{
		return MVisualizeApex;
	}

	virtual UBOOL IsClothingRecording()
	{
		return MClothingRecording;
	}

	void NotifyAssetGoingAway(ApexAsset *asset)
	{
		for (TApexAssetVector::iterator i=MPendingForceLoad.begin(); i!=MPendingForceLoad.end(); ++i)
		{
			if ( (*i) == asset )
			{
				(*i) = NULL;
				break;
			}
		}
	}

	void NotifyAssetImport(ApexAsset *asset)
	{
		for (TApexAssetVector::iterator i=MPendingForceLoad.begin(); i!=MPendingForceLoad.end(); ++i)
		{
			if ( (*i) == asset )
			{
				asset = NULL;
				break;
			}
		}
		if ( asset )
		{
			MPendingForceLoad.push_back(asset);
		}
	}

	/**
	* Pump the ApexCommands system which will detect if any pending assets which have been recently loaded
	* need to have a 'force load' performed on them.  This is done because it must happen in the game thread.
	*/
	virtual void Pump(void)
	{
  	    GApexRender->ProcessFEnqueDataGameThread(); // clean up pending render resource buffers
		for (TUApexAssetVector::iterator i=mUpdateMaterials.begin(); i!=mUpdateMaterials.end(); ++i)
		{
			UApexAsset *ua = (*i);
			if ( ua )
			{
				ua->UpdateMaterials();
			}
		}
		mUpdateMaterials.clear();

#if !WITH_APEX_SHIPPING
		if ( !MPendingForceLoad.empty() && GIsEditor )
		{
			physx::PxU32 count;
			void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
			for (physx::PxU32 i=0; i<count; i++)
			{
				ApexAsset *aa = (ApexAsset *)assets[i];
				aa->FixupApexAsset();
			}
			for (physx::PxU32 i=0; i<count; i++)
			{
				ApexAsset *aa = (ApexAsset *)assets[i];
				aa->CreateObjectDependencies();
			}
		}
#endif
		if ( !MPendingForceLoad.empty() )
		{
			for (TApexAssetVector::iterator i=MPendingForceLoad.begin(); i!=MPendingForceLoad.end(); ++i)
			{
				if ( (*i) )
				{
					(*i)->ForceLoad();
				}
			}
			MPendingForceLoad.clear();
		}
	}

	virtual void AddUpdateMaterials(UApexAsset *Asset) 
	{
		UBOOL found = FALSE;
		for (TUApexAssetVector::iterator i=mUpdateMaterials.begin(); i!=mUpdateMaterials.end(); ++i)
		{
			UApexAsset *ua = (*i);
			if ( ua == Asset )
			{
				found = TRUE;
				break;
			}
		}
		if ( !found )
		{
			mUpdateMaterials.push_back(Asset);
		}

	}

	/***
	Notification that this clothing asset is about to be destroyed and should be removed from the pending material resolution queue.
	*/
	virtual void NotifyDestroy(UApexAsset *Asset) 
	{
		if ( GIsEditor )
		{
			physx::PxU32 count;
			void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
			for (physx::PxU32 i=0; i<count; i++)
			{
				ApexAsset *aa = (ApexAsset *)assets[i];
				UApexAsset *obj = aa->GetUE3Object();
				if ( obj != Asset )
				{
					if ( obj )
					{
						obj->RemoveNamedReference(Asset);
					}
				}
				else
				{
					aa->SetUE3Object(NULL);
				}
			}
		}

		for (TUApexAssetVector::iterator i=mUpdateMaterials.begin(); i!=mUpdateMaterials.end(); ++i)
		{
			UApexAsset *ua = (*i);
			if ( ua == Asset )
			{
				(*i) = NULL;
				break;
			}
		}
	}
	bool isAsciiText(const unsigned char *str) const
	{
		bool ret = true;
		while ( *str )
		{
			if ( *str < 32 || *str > 126 )
			{
				ret = false;
				break;
			}
			str++;
		}
		return ret;
	}

#if !WITH_APEX_SHIPPING
	virtual const char * GetApexAssetBuffer(physx::PxU32 objNumber,const void *Buffer,physx::PxU32 dlen,const void *&buffer,physx::PxU32 &bufferLen) 
	{
		const char *ret = NULL;
		buffer = NULL;
		bufferLen = 0;

		appFree(mLastBuffer);
		appFree(mLastName);
		mLastBuffer = NULL;
		mLastName = NULL;


		physx::PxU32 numObj=0;
		ApexAssetFormat fmt = GetApexAssetFormat(Buffer,dlen,numObj);
		if ( objNumber < numObj )
		{
			NxParameterized::Interface *iface = NULL;
			NxParameterized::Serializer *ser = NULL;
			switch ( fmt )
			{
				case AAF_DESTRUCTIBLE_LEGACY:
				case AAF_CLOTHING_LEGACY:
				case AAF_CLOTHING_MATERIAL_LEGACY:
				case AAF_RENDER_MESH_LEGACY:
					PX_ALWAYS_ASSERT();
					break;
				case AAF_DESTRUCTIBLE_XML:
				case AAF_CLOTHING_XML:
				case AAF_XML:
					{
						ser = GApexManager->GetApexSDK()->createSerializer(NxParameterized::Serializer::NST_XML);
					}
					break;
				case AAF_DESTRUCTIBLE_BINARY:
				case AAF_CLOTHING_BINARY:
				case AAF_BINARY:
					{
						ser = GApexManager->GetApexSDK()->createSerializer(NxParameterized::Serializer::NST_BINARY);
					}
					break;
			}
			if ( ser )
			{
				NxParameterized::Serializer::DeserializedData desData;
				physx::PxFileBuf *rbuff = GApexManager->GetApexSDK()->createMemoryReadStream(Buffer,dlen);
				NxParameterized::Serializer::ErrorType err = ser->deserialize(*rbuff,desData);
				GApexManager->GetApexSDK()->releaseMemoryReadStream(*rbuff);
 				if ( err == NxParameterized::Serializer::ERROR_NONE )
				{
					ser->release();
					ser = NULL;
					PX_ASSERT( objNumber < desData.size() );
					if ( objNumber < desData.size() )
					{
						iface = desData[objNumber];
						physx::PxFileBuf *fbuffer = GApexManager->GetApexSDK()->createMemoryWriteStream();
						NxParameterized::Serializer *ser = GApexManager->GetApexSDK()->createSerializer(NxParameterized::Serializer::NST_BINARY);
						PX_ASSERT(ser);
						const char *objName = iface->name();
						static physx::PxU32 oc = 0;
						if ( objName == NULL )
						{
							objName = GApexManager->GetCurrentImportFileName();
							iface->setName(objName);
						}
						if ( ser )
						{
							const NxParameterized::Interface *save = iface;
							ser->serialize(*fbuffer,&save,1);	
							ser->release();									
						}

						const void *data = GApexManager->GetApexSDK()->getMemoryWriteBuffer(*fbuffer,bufferLen);
						if ( data )
						{
							buffer = appMalloc(bufferLen);
							memcpy((void *)buffer,data,bufferLen);
							mLastBuffer = (void *)buffer;
							physx::PxU32 slen = strlen(objName);
							mLastName = (char *)appMalloc(slen+1);
							memcpy(mLastName,objName,slen+1);
							char *scan = mLastName;
							char *lastDot = NULL;
							while ( *scan )
							{
								if ( *scan == '.' )
								{
									lastDot = scan;
									*scan = '_';
								}
								scan++;
							}
							if ( lastDot )
							{
								*lastDot = 0;
							}
							ret = mLastName;
						}

						if ( fbuffer )
						{
							GApexManager->GetApexSDK()->releaseMemoryWriteStream(*fbuffer);
						}

						for (physx::PxU32 i=0; i<numObj; i++)
						{
							NxParameterized::Interface *iface = desData[i];
							iface->destroy();
						}
					}
				}
				if ( ser )
				{
					ser->release();
				}
			}

		}

		return ret;
	}
#endif

	virtual ApexAssetFormat GetApexAssetFormat(const void *Buffer,physx::PxU32 dlen,physx::PxU32 &numObj)
	{
		ApexAssetFormat ret = AAF_LAST;

		numObj = 1;
		const physx::PxU32 *scan = (const physx::PxU32 *)Buffer;
		physx::PxU32 headerVersion = scan[0];
		physx::PxU32 stringLen     = scan[1];
		// tests legacy version...
		if (headerVersion >= 10 && headerVersion <= 48 && stringLen < 256 )
		{
			scan+=2;
			const char *str = (const char *)scan;
			char scratch[256];
			memcpy(scratch,str,stringLen);
			scratch[stringLen] = 0;
			physx::PxU32 slen = (physx::PxU32 )strlen(scratch);
			if ( slen == stringLen && isAsciiText((const unsigned char *)scratch) )
			{
				if ( strcmp(scratch,NX_CLOTHING_AUTHORING_TYPE_NAME) == 0 )
				{
					ret = AAF_CLOTHING_LEGACY;
				}
				else if ( strcmp(scratch,NX_DESTRUCTIBLE_AUTHORING_TYPE_NAME) == 0 )
				{
					ret = AAF_DESTRUCTIBLE_LEGACY;
				}
				else if ( strcmp(scratch,NX_RENDER_MESH_AUTHORING_TYPE_NAME) == 0 )
				{
					ret = AAF_RENDER_MESH_LEGACY;
				}
				else
				{
					PX_ALWAYS_ASSERT(); // some unknown type!?
				}
			}

		}
		if ( ret == AAF_LAST )
		{
			::NxParameterized::Serializer::SerializeType stype = GApexManager->GetApexSDK()->getSerializeType(Buffer,dlen);
			if ( stype != ::NxParameterized::Serializer::NST_LAST )
			{
				::NxParameterized::Serializer *ser = GApexManager->GetApexSDK()->createSerializer(stype);
				PX_ASSERT(ser);
				NxParameterized::Serializer::ErrorType err = ser->peekNumObjectsInplace(Buffer,dlen,numObj);
				if ( err == NxParameterized::Serializer::ERROR_NONE )
				{
					if ( numObj == 1 )
					{
						// ok.. we need to decipher what type it is...
						if ( stype == ::NxParameterized::Serializer::NST_XML )
						{
							char scratch[1024];
							const char *source = (const char *)Buffer;
							physx::PxU32 clen = dlen < 1023 ? dlen : 1023;
							appStrncpyANSI( scratch, (const char *)Buffer, clen );
							scratch[1023] = 0;
							// now look for the ascii identifier for a clothing asset.
							const char *cc = strstr(scratch,"\"ClothingAssetParameters\"");
							if ( cc )
							{
								ret = AAF_CLOTHING_XML;
							}
							else
							{
								const char *cc = strstr(scratch,"\"DestructibleAssetParameters\"");
								if ( cc )
								{
									ret = AAF_DESTRUCTIBLE_XML;
								}
								else 
								{
									ret = AAF_XML;
								}
							}
						}
						else
						{
							// need to find out the object type!  Unfortunately, with a binary object, the only way we can do so is by deserializing it.
							physx::PxFileBuf *ReadBuffer = GApexManager->GetApexSDK()->createMemoryReadStream(Buffer,dlen);
							::NxParameterized::Serializer::DeserializedData desData;
							ser->deserialize(*ReadBuffer,desData);
							if ( desData.size() )
							{
								NxParameterized::Interface *iface = desData[0];
								const char *className = iface->className();
								if ( strcmp(className,"DestructibleAssetParameters") == 0 )
								{
									ret = AAF_DESTRUCTIBLE_BINARY;
								}
								else if ( strcmp(className,"ClothingAssetParameters") == 0 )
								{
									ret = AAF_CLOTHING_BINARY;
								}
								else
								{
									ret = AAF_BINARY;
								}
								iface->destroy(); // release the interface
							}
							GApexManager->GetApexSDK()->releaseMemoryReadStream(*ReadBuffer);
						}
					}
					else
					{
						// if there is more than one object, then we always treat it as a generic for now...
						ret = (stype == ::NxParameterized::Serializer::NST_BINARY) ? AAF_BINARY : AAF_XML;
					}
					ser->release(); // release the serializer
				}
			}
		}

		if ( ret == AAF_LAST )
		{
			numObj = 0;
		}

		return ret;
	}

	virtual const char * namedReferenceCallback(const char *className,const char *namedReference)  
	{
		const char *ret = NULL;
		physx::PxU32 count;
		void **assets = GNRP->findAllResources("AI_APEX_ASSET",count);
		BOOL exactMatch=FALSE;

		for (physx::PxU32 i=0; i<count; i++)
		{
			ApexAsset *aa = (ApexAsset *)assets[i];
			if ( aa->NameMatch(className,namedReference,exactMatch) )
			{
				if ( !exactMatch )
				{
					ret = aa->GetAssetName();
				}
				break;
			}
		}
		return ret;
	}
#if WITH_APEX_PARTICLES
	virtual FIApexAsset * CreateDefaultApexAsset(const char *assetName,ApexAssetType type,UApexAsset *obj,UApexGenericAsset *parent) 
	{
		FIApexAsset *asset = GetApexAsset(assetName);
		if ( asset )
		{
			ApexAsset *aa = static_cast< ApexAsset *>(asset);
			aa->CreateDefaultApexAsset(type,obj,parent);
		}
		else
		{
			ApexAsset *aa = new ApexAsset(assetName,type,obj,parent);
			asset = static_cast< FIApexAsset *>(aa);
			GNRP->setResource("AI_APEX_ASSET",assetName,aa,TRUE);
		}
		return asset;
	}
#endif

	virtual void AddApexScene(FIApexScene *scene) 
	{
		UBOOL added = FALSE;
		for (physx::PxU32 i=0; i<mApexScenes.size(); i++)
		{
			if ( mApexScenes[i] == NULL )
			{
				mApexScenes[i] = scene;
				added = TRUE;
				break;
			}
			PX_ASSERT( mApexScenes[i] != scene );
		}
		if ( !added )
		{
			mApexScenes.push_back(scene);
		}

		InitDebugVisualizationNames();
	}

	virtual void RemoveApexScene(FIApexScene *scene) 
	{
		for (physx::PxU32 i=0; i<mApexScenes.size(); i++)
		{
			if ( mApexScenes[i] == scene )
			{
				mApexScenes[i] = NULL;
			}
		}
	}

	void InitDebugVisualizationNames()
	{
		if (mDebugVisNamesInitialized)
			return;

		if (mApexScenes.size() == 0)
			return;

		FIApexScene* fscene = mApexScenes[0];
		if ( fscene == NULL )
			return;

		NxApexScene* apexScene = fscene->GetApexScene();
		if ( apexScene == NULL)
			return;

		mDebugVisNamesInitialized = TRUE;

		for (UINT i = 0; i < MODULE_COUNT; ++i)
		{
			// module params
			mDebugParamIndex.Set(moduleNames[i], i); 
			NxParameterized::Interface *debugRenderParams = apexScene->getModuleDebugRenderParams(moduleNames[i]);
			if (debugRenderParams != NULL)
			{
				UINT count;
				const NxParameterized::ParamResult* results = NxParameterized::getParamList(*debugRenderParams, NULL, NULL, count, false, false, MApexSDK->getParameterizedTraits());
				for (UINT j = 0; j < count; ++j)
				{
					const char* name = results[j].mName;

					if (!IsNameToIgnore(name))
						mDebugParams[i].AddItem(name);
				}
			}
		}


		mDebugParamIndex.Set("", MODULE_COUNT); // apex params
		NxParameterized::Interface *debugRenderParams = apexScene->getDebugRenderParams();
		if (debugRenderParams != NULL)
		{
			UINT count;
			const NxParameterized::ParamResult* results = NxParameterized::getParamList(*debugRenderParams, NULL, NULL, count, false, false, MApexSDK->getParameterizedTraits());
			for (UINT i = 0; i < count; ++i)
			{
				const char* name = results[i].mName;

				if (!IsNameToIgnore(name))
					mDebugParams[MODULE_COUNT].AddItem(name);
			}
		}
	}

	virtual UINT GetNumDebugVisualizationNames(const char* moduleName)
	{
		if (!mDebugVisNamesInitialized)
			return 0;

		if (moduleName == 0)
			moduleName = "";

		return mDebugParams[mDebugParamIndex.FindRef(moduleName)].Num();
	}

	virtual const char* GetDebugVisualizationName(const char* moduleName, UINT i)
	{
		if (!mDebugVisNamesInitialized)
			return 0;

		if (moduleName == 0)
			moduleName = "";

		return mDebugParams[mDebugParamIndex.FindRef(moduleName)](i);
	}

	virtual void GetDebugVisualizationNamePretty(FString& prettyName, const char* moduleName, UINT i)
	{
		getPrettyName(prettyName, GetDebugVisualizationName(moduleName, i));
	}


	void getPrettyName(FString& output, const char* uglyName)
	{
		if (strncmp(uglyName, "VISUALIZE_CLOTHING_", 19) == 0)
		{
			uglyName += 19;
		}

		bool capital = true;
		bool wasLower = false;

		while (*uglyName != 0)
		{
			if (*uglyName == '_')
			{
				capital = true;
				output.AppendChar(' ');
			}
			else if (wasLower && isupper(*uglyName))
			{
				output.AppendChar(' ');
				output.AppendChar(*uglyName);
			}
			else
			{
				char letter = capital ? *uglyName : tolower(*uglyName);
				output.AppendChar(letter);
				capital = false;
			}

			wasLower = islower(*uglyName) != 0;
			uglyName++;
		}
	}

private:
	void						*mLastBuffer;
	char						*mLastName;
	UBOOL					   MShowApex;
	UBOOL					   MOnExit;
  	UBOOL 					   MVisualizeApex;
	UBOOL 					   MClothingRecording;
    UBOOL                      MFreshCommand;
    NxApexSDK                 *MApexSDK;
	FIApexRender              *MApexRender;
	NxResourceCallback		  *MResourceCallback;
	TApexAssetVector		   MPendingForceLoad;
	TUApexAssetVector  mUpdateMaterials;
	TApexSceneVector	mApexScenes;
	UBOOL						mDebugVisNamesInitialized;
	TMap<FString, UINT>			mDebugParamIndex;
	TArray<const char*>			mDebugParams[MODULE_COUNT+1];
};

/**
 * Implementation of FIApexAsset
**/
ApexAsset::ApexAsset(const char *fname,const void *data,physx::PxU32 dlen,const void* params, const char *originalName,UApexAsset *obj)
{
#if WITH_APEX_PARTICLES
	mEdited = FALSE;
	mApexEditInterface = NULL;
#endif
	mObject = obj;
	MForceLoad = FALSE;
	physx::PxU32 slen = (physx::PxU32)strlen(fname);
	MOriginalApexName = NULL;
	MName = (char *)appMalloc(slen+1);
	memcpy(MName,fname,slen+1);
  	MRefCount = 0;
  	MType = AAT_LAST;
  	MApexAsset = NULL;
	MTransformation = physx::PxMat44::createIdentity();

  	if ( data )
  	{
  		LoadFromMemory(data,dlen,fname,originalName,obj);
  	}
	else if ( params )
	{
		LoadFromParams(params,fname,originalName,obj);
	}
	else
	{
		UObject* Object = obj;
		if ( UObject::IsReferenced( Object, GARBAGE_COLLECTION_KEEPFLAGS ) )
		{
			warnf(NAME_Warning, TEXT("ApexAsset (%s) is created without data. Object can not be deleted because it is referenced by other objects:"));

			// We cannot safely delete this object. Print out a list of objects referencing this one
			// that prevent us from being able to delete it.
			FStringOutputDevice Ar;
			obj->OutputReferencers(Ar, FALSE);
			warnf( TEXT("%s"), *Ar );		
		}
		else
		{
			warnf(NAME_Warning, TEXT("ApexAsset (%s) is created without data. Marking object for deletion."), *FString(fname));

			// Mark its package as dirty as we're going to delete the obj.
			obj->MarkPackageDirty();
			// Remove standalone flag so garbage collection can delete the object.
			obj->ClearFlags( RF_Standalone );
		}
	}
}

#if WITH_APEX_PARTICLES
ApexAsset::ApexAsset(const char *fname,ApexAssetType type,UApexAsset *obj,UApexGenericAsset *parent)
{
	mEdited = FALSE;
	mApexEditInterface = NULL;
	mObject = obj;
	MForceLoad = FALSE;
	physx::PxU32 slen = (physx::PxU32)strlen(fname);
	MOriginalApexName = NULL;
	MName = (char *)appMalloc(slen+1);
	memcpy(MName,fname,slen+1);
	setOriginalName(MName);
	MRefCount = 0;
	MType = AAT_LAST;
	MApexAsset = NULL;
	MTransformation = physx::PxMat44::createIdentity();
	CreateDefaultApexAsset(type,obj,parent);
}

void ApexAsset::CreateDefaultApexAsset(ApexAssetType type,UApexAsset *obj,UApexGenericAsset *parent)
{
	//...
	PX_ASSERT( obj == mObject );
	NotifyAssetGoingAway();
	MType = AAT_LAST;
  	NxApexSDK *apexSDK = GApexManager->GetApexSDK();
	if ( MApexAsset )
	{
		apexSDK->releaseAsset(*MApexAsset);
		MApexAsset = NULL;
	}

	const char *assetName = NULL;
	switch ( type )
	{
  		case AAT_DESTRUCTIBLE:	// used to construct destructible actors
  			assetName =	NX_DESTRUCTIBLE_AUTHORING_TYPE_NAME; 
  			break;
  		case AAT_CLOTHING:		// asset used to construct clothing actors
  			assetName = NX_CLOTHING_AUTHORING_TYPE_NAME;
  			break;
  		case AAT_APEX_EMITTER:		// asset used to construct emitters.
  			assetName = NX_APEX_EMITTER_AUTHORING_TYPE_NAME;
  			break;
		case AAT_GROUND_EMITTER:
			assetName = NX_GROUND_EMITTER_AUTHORING_TYPE_NAME;
			break;
		case AAT_IMPACT_EMITTER:
			assetName = NX_IMPACT_EMITTER_AUTHORING_TYPE_NAME;
			break;
		case AAT_FLUID_IOS:
			assetName = NX_FLUID_IOS_AUTHORING_TYPE_NAME;
			break;
  		case AAT_BASIC_IOS:
  			assetName = NX_BASIC_IOS_AUTHORING_TYPE_NAME;
  			break;
  		case AAT_IOFX:
  			assetName = NX_IOFX_AUTHORING_TYPE_NAME;
  			break;
  		case AAT_RENDER_MESH:
  			assetName = NX_RENDER_MESH_AUTHORING_TYPE_NAME;
  			break;
  		default:
  			PX_ALWAYS_ASSERT();
  			break;
	}
	if ( assetName )
	{
		NxApexAssetAuthoring *authoring = apexSDK->createAssetAuthoring(assetName);
		PX_ASSERT(authoring);
		if ( authoring )
		{
			NxParameterized::Interface *iface = authoring->getNxParameterized();
			iface->setName(MName);
			switch ( type )
			{
				case AAT_BASIC_IOS:
					{
						NxParameterized::setParamF32(*iface,"restDensity",1);
						NxParameterized::setParamF32(*iface,"particleRadius",0.1f);
						NxParameterized::setParamF32(*iface,"maxInjectedParticleCount",0.1f);
					}
					break;
				case AAT_FLUID_IOS:
					{
						NxParameterized::setParamString(*iface,"collisionGroupName","PhysXFluidCollisionGroup");
					}
					break;
				case AAT_APEX_EMITTER:
					{
						UBOOL ok = false;
						NxParameterized::Handle handle(*iface);
						if ( NxParameterized::findParam(*iface,"iosAssetName",handle) )
						{
							NxParameterized::Interface *paramRef;
#if 1
							if ( handle.initParamRef("NxFluidIosAsset") == NxParameterized::ERROR_NONE )
							{
								if ( handle.getParamRef(paramRef) == NxParameterized::ERROR_NONE )
								{
									paramRef->setName("DefaultFluidIOS");
									ok = TRUE;
								}
							}
#else
							if ( handle.initParamRef("NxBasicIosAsset") == NxParameterized::ERROR_NONE )
							{
								if ( handle.getParamRef(paramRef) == NxParameterized::ERROR_NONE )
								{
									paramRef->setName("DefaultBasicIOS");
									ok = TRUE;
								}
							}
#endif
						}
						if ( ok && NxParameterized::findParam(*iface,"iofxAssetName",handle) )
						{
							NxParameterized::Interface *paramRef;
							if ( handle.initParamRef("IOFX") == NxParameterized::ERROR_NONE )
							{
								if ( handle.getParamRef(paramRef) == NxParameterized::ERROR_NONE )
								{
									if ( parent && parent->MApexAsset )
									{
										paramRef->setName( parent->MApexAsset->GetAssetName() );
									}
									else
									{
										paramRef->setName("DefaultIOFX");
									}
									ok = TRUE;
								}
							}
						}
						if ( ok )
						{
							NxParameterized::setParamF32(*iface,"rateRange.min",10);
							NxParameterized::setParamF32(*iface,"rateRange.max",1000);
							NxParameterized::setParamVec3(*iface,"velocityRange.min",physx::PxVec3(-3.0f,0.0f,-3.0f));
							NxParameterized::setParamVec3(*iface,"velocityRange.max",physx::PxVec3(3.0f,6.0f,3.0f));
							NxParameterized::setParamF32(*iface,"lifetimeRange.min",2);
							NxParameterized::setParamF32(*iface,"lifetimeRange.max",5);
							NxParameterized::setParamU32(*iface,"maxSamples",100);
							NxParameterized::setParamF32(*iface,"lodParamDesc.maxDistance",10000);
							ok = FALSE;
							NxParameterized::Handle handle(*iface);
							if ( NxParameterized::findParam(*iface,"geometryType",handle) )
							{
								NxParameterized::Interface *paramRef;
								if ( handle.initParamRef("EmitterGeomSphereParams") == NxParameterized::ERROR_NONE )
								{
									if ( handle.getParamRef(paramRef) == NxParameterized::ERROR_NONE )
									{
										NxParameterized::setParamF32(*iface,"geometryType.radius",2);
										ok = TRUE;
									}
								}

							}
						}
						PX_ASSERT(ok);
					}
					break;
				case AAT_IOFX:
					{
						UBOOL ok = FALSE;
						NxParameterized::Handle handle(*iface);
						if ( NxParameterized::findParam(*iface,"renderMeshList",handle) )
						{
							handle.resizeArray(1);
							NxParameterized::setParamU32(*iface,"renderMeshList[0].weight",1);
							if ( NxParameterized::findParam(*iface,"renderMeshList[0].meshAssetName",handle))
							{
								NxParameterized::Interface *paramRef;
								if ( handle.initParamRef("ApexOpaqueMesh") == NxParameterized::ERROR_NONE )
								{
									if ( handle.getParamRef(paramRef) == NxParameterized::ERROR_NONE )
									{
										paramRef->setName("EngineMeshes.ParticleCube");
										ok = TRUE;
									}
								}
							}
						}
						if ( ok )
						{
							ok = FALSE;
							if ( NxParameterized::findParam(*iface,"spawnModifierList",handle) )
							{
								handle.resizeArray(2);
								if ( NxParameterized::findParam(*iface,"spawnModifierList[0]",handle))
								{
									if ( handle.initParamRef("SimpleScaleModifierParams") == NxParameterized::ERROR_NONE )
									{
										//physx::PxVec3 scale(0.025f,0.025f,0.025f);
										physx::PxVec3 scale(1,1,1);
										NxParameterized::setParamVec3(*iface,"spawnModifierList[0].scaleFactor",scale);
										ok = TRUE;
									}
								}
								if ( ok && NxParameterized::findParam(*iface,"spawnModifierList[1]",handle))
								{
									ok = FALSE;
									if ( handle.initParamRef("RotationModifierParams") == NxParameterized::ERROR_NONE )
									{
										NxParameterized::setParamEnum(*iface,"spawnModifierList[1].rollType","SPHERICAL");
										NxParameterized::setParamF32(*iface,"spawnModifierList[1].maxRotationRatePerSec",0);
										NxParameterized::setParamF32(*iface,"spawnModifierList[1].maxSettleRatePerSec",1);
										NxParameterized::setParamF32(*iface,"spawnModifierList[1].inAirRotationMultiplier",1);
										NxParameterized::setParamF32(*iface,"spawnModifierList[1].collisionRotationMultiplier",1);

										ok = TRUE;
									}
								}
							}
						}
						if ( ok )
						{
							ok = FALSE;
							if ( NxParameterized::findParam(*iface,"continuousModifierList",handle) )
							{
								handle.resizeArray(4);
								if ( NxParameterized::findParam(*iface,"continuousModifierList[0]",handle))
								{
									if ( handle.initParamRef("RotationModifierParams") == NxParameterized::ERROR_NONE )
									{
										NxParameterized::setParamEnum(*iface,"continuousModifierList[0].rollType","SPHERICAL");
										NxParameterized::setParamF32(*iface,"continuousModifierList[0].maxRotationRatePerSec",300);
										NxParameterized::setParamF32(*iface,"continuousModifierList[0].maxSettleRatePerSec",100);
										NxParameterized::setParamF32(*iface,"continuousModifierList[0].inAirRotationMultiplier",1);
										NxParameterized::setParamF32(*iface,"continuousModifierList[0].collisionRotationMultiplier",80);
										ok = TRUE;
									}
								}
								if ( ok && NxParameterized::findParam(*iface,"continuousModifierList[1]",handle))
								{
									ok = FALSE;
									if ( handle.initParamRef("ScaleVsLifeModifierParams") == NxParameterized::ERROR_NONE )
									{
										NxParameterized::setParamEnum(*iface,"continuousModifierList[1].scaleAxis","xAxis");
										if ( NxParameterized::findParam(*iface,"continuousModifierList[1].controlPoints",handle))
										{
											handle.resizeArray(3);
											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[0].x",0);
											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[0].y",0);

											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[1].x",0.1f);
											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[1].y",1);

											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[2].x",1);
											NxParameterized::setParamF32(*iface,"continuousModifierList[1].controlPoints[2].y",1);
											ok = TRUE;
										}
									}
									if ( ok && NxParameterized::findParam(*iface,"continuousModifierList[2]",handle))
									{
										ok = FALSE;
										if ( handle.initParamRef("ScaleVsLifeModifierParams") == NxParameterized::ERROR_NONE )
										{
											NxParameterized::setParamEnum(*iface,"continuousModifierList[2].scaleAxis","yAxis");
											if ( NxParameterized::findParam(*iface,"continuousModifierList[2].controlPoints",handle))
											{
												handle.resizeArray(3);
												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[0].x",0);
												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[0].y",0);

												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[1].x",0.1f);
												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[1].y",1);

												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[2].x",1);
												NxParameterized::setParamF32(*iface,"continuousModifierList[2].controlPoints[2].y",1);
												ok = TRUE;
											}
										}
									}
									if ( ok && NxParameterized::findParam(*iface,"continuousModifierList[3]",handle))
									{
										ok = FALSE;
										if ( handle.initParamRef("ScaleVsLifeModifierParams") == NxParameterized::ERROR_NONE )
										{
											NxParameterized::setParamEnum(*iface,"continuousModifierList[3].scaleAxis","zAxis");
											if ( NxParameterized::findParam(*iface,"continuousModifierList[3].controlPoints",handle))
											{
												handle.resizeArray(3);
												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[0].x",0);
												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[0].y",0);

												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[1].x",0.1f);
												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[1].y",1);

												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[2].x",1);
												NxParameterized::setParamF32(*iface,"continuousModifierList[3].controlPoints[2].y",1);
												ok = TRUE;
											}
										}
									}
								}
							}
						}
						PX_ASSERT(ok);
					}
					break;
			}
			MApexAsset = apexSDK->createAsset(*authoring,assetName);
			apexSDK->releaseAssetAuthoring(*authoring);
		}
	}
	if ( MApexAsset )
	{
		MType = type;
		NotifyAssetImport();
	}
}
#endif

/**
 * Destructor
**/
ApexAsset::~ApexAsset(void)
{
#if WITH_APEX_PARTICLES
	if ( mApexEditInterface )
	{
		mApexEditInterface->NotifyObjectDestroy(mObject);
	}
#endif
	GApexCommands->NotifyApexAssetGone(this);
	if ( MApexAsset )
	{
		NxApexSDK *sdk = GApexManager->GetApexSDK();
		sdk->releaseAsset(*MApexAsset);
	}
	appFree(MName);
	appFree(MOriginalApexName);
}

/**
* Notifies the asset that it is being removed
**/
void ApexAsset::NotifyAssetGoingAway(void)
{
	MForceLoad = FALSE; // clear the 'force load' semaphore
	MyApexCommands *mc = static_cast< MyApexCommands *>(GApexCommands);
	mc->NotifyAssetGoingAway(this);
	{
    	for (TApexActorVector::iterator i=MActors.begin(); i!=MActors.end(); ++i)
        {
          FIApexActor *actor = (*i);
          if ( actor )
          {
            actor->NotifyAssetGone();
          }
        }

	}
	// if there is any apex actors that are still pending deferred deletion, process the deletion 
	if(FApexCleanUp::PendingObjects)
	{
		FlushRenderingCommands();
	}
}

// Set to 1 to write out ACA, PDA, ACML, etc files
#define DUMP_APEX_FILES 0

/**
* Loads the asset from memory
* @param	Mem	- a pointer to the memory location.
* @param	Length - 	the length of the asset.
*
* @return	TRUE if it succeeds, FALSE if it fails.
**/
UBOOL ApexAsset::LoadFromMemory(const void *data,physx::PxU32 dlen,const char *assetName,const char *originalName,UApexAsset *obj)
{
#if ALLOW_DEBUG_FILES && DUMP_APEX_FILES
	{
		// dump file
		FString OutputFileName = appGameLogDir() + FString( assetName );
		FArchive* OutputFile = GFileManager->CreateDebugFileWriter( *OutputFileName );
		if( OutputFile != NULL )
		{
			OutputFile->Serialize( const_cast<void*>(data), dlen );
			delete OutputFile;
		}
	}
#endif

	PX_ASSERT(obj == mObject );

	if ( originalName == NULL || originalName[0] == '\0' )
	{
		originalName = assetName;
	}
#if !WITH_APEX_PARTICLES
	originalName = assetName;
#endif
	setOriginalName(originalName);

	physx::PxU32 numObj;
	ApexAssetFormat fmt = GApexCommands->GetApexAssetFormat(data,dlen,numObj);
	if ( fmt == AAF_LAST )
		return FALSE;
	PX_ASSERT(numObj ==  1 ); // we should never have more than one apex asset embedded!
	if ( numObj != 1 )
		return FALSE;

	NotifyAssetGoingAway();
	MType = AAT_LAST;
	FIApexCommands *ic = GApexCommands;
  	PX_ASSERT(ic);
  	FIApexManager *pm = ic->GetApexManager();
  	PX_ASSERT(pm);
  	NxApexSDK *apexSDK = pm->GetApexSDK();
	if ( MApexAsset )
	{
		apexSDK->releaseAsset(*MApexAsset);
		MApexAsset = NULL;
	}
	physx::PxFileBuf *mrb = GApexManager->GetApexSDK()->createMemoryReadStream(data,dlen);
	PX_ASSERT(mrb);
	switch ( fmt )
	{
		case AAF_DESTRUCTIBLE_LEGACY:
		case AAF_CLOTHING_LEGACY:
		case AAF_CLOTHING_MATERIAL_LEGACY:
			warnf(NAME_Warning, TEXT("Legacy APEX Assets are no longer supported by APEX 1.1"));

			//check(0);
			// SetNxApexAsset(apexSDK->createAsset(*mrb,assetName));

			break;
		case AAF_DESTRUCTIBLE_BINARY:
		case AAF_CLOTHING_BINARY:
		case AAF_BINARY:
		case AAF_CLOTHING_XML:
		case AAF_DESTRUCTIBLE_XML:
		case AAF_XML:
			if ( mrb )
			{
				NxParameterized::Serializer::SerializeType type = NxParameterized::Serializer::NST_BINARY;
				if ( fmt == AAF_CLOTHING_XML ||	fmt == AAF_DESTRUCTIBLE_XML || fmt ==AAF_XML )
				{
					type = NxParameterized::Serializer::NST_XML;
				}
				NxParameterized::Serializer *ser = GApexManager->GetApexSDK()->createSerializer(type);
				PX_ASSERT(ser);
				if ( ser )
				{
					::NxParameterized::Serializer::DeserializedData desData;
					bool bIsUpdated = false;
					ser->deserialize(*mrb,desData,bIsUpdated);

					PX_ASSERT( desData.size());
					PX_ASSERT( desData.size()==1);
					if ( desData.size() )
					{
						::NxParameterized::Interface *iface = desData[0];
						const char *name = iface->name();
						if ( name )
						{
							setOriginalName(name);
						}
						MApexAsset = apexSDK->createAsset(iface,assetName);
						PX_ASSERT(MApexAsset);
						bIsUpdated |= MApexAsset->isDirty();
					}
					else
					{
						PX_ALWAYS_ASSERT();
					}
					ser->release();

					if (bIsUpdated && !GIsCooking)
					{
						warnf(NAME_DevPhysics, TEXT("ApexAsset %s (%s) has been updated and needs to be resaved. Use the ResavePackages commandlet or resave in editor."), *obj->GetPathName(), *FString(assetName));
						
						// Uncomment the following line if you want the asset's package to be 
						// automatically marked dirty when the APEX asset is out of date.
						//obj->MarkPackageDirty();
					}
				}
			}
			break;
	}

	GApexManager->GetApexSDK()->releaseMemoryReadStream(*mrb);

	if ( MApexAsset )
	{
		const char *typeName = MApexAsset->getObjTypeName();
		if ( strcmp(typeName,NX_DESTRUCTIBLE_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_DESTRUCTIBLE;
		}
		else if ( strcmp(typeName,NX_CLOTHING_AUTHORING_TYPE_NAME) == 0)
		{
			MType = AAT_CLOTHING;
		}
		else if ( strcmp(typeName,NX_RENDER_MESH_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_RENDER_MESH;
		}
#if WITH_APEX_PARTICLES
		else if ( strcmp(typeName,NX_BASIC_IOS_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_BASIC_IOS;
		}
		else if ( strcmp(typeName,NX_APEX_EMITTER_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_APEX_EMITTER;
		}
		else if ( strcmp(typeName,NX_IOFX_AUTHORING_TYPE_NAME ) == 0 )
		{
			MType = AAT_IOFX;
		}
#endif
		else
		{
			PX_ALWAYS_ASSERT(); // unexpected asset type...
			MType = AAT_GENERIC;
		}
	}

	UBOOL ret = IsValid();

	if ( ret )
	{
		// Note that this call will queue up a force load command, which will cause APEX to call the 
		// NRP getResource callback for any unknown resources such as materials. If this is done during 
		// async package loading the call to getResource will return NULL because the package is still 
		// loading. To prevent that from happening, call setResource before the call to forceLoadAssets 
		// has a chance to fire off. Then you won't get any getResource callback.
		NotifyAssetImport();
	}

	return ret;
}

/**
* Loads the asset from memory
* @param	Mem	- a pointer to the memory location.
* @param	Length - 	the length of the asset.
*
* @return	TRUE if it succeeds, FALSE if it fails.
**/
UBOOL ApexAsset::LoadFromParams(const void *params,const char *assetName,const char *originalName,UApexAsset *obj)
{
	PX_ASSERT(obj == mObject );

	if ( originalName == NULL || originalName[0] == '\0' )
	{
		originalName = assetName;
	}
#if !WITH_APEX_PARTICLES
	originalName = assetName;
#endif
	setOriginalName(originalName);

	NotifyAssetGoingAway();
	MType = AAT_LAST;
	FIApexCommands *ic = GApexCommands;
  	PX_ASSERT(ic);
  	FIApexManager *pm = ic->GetApexManager();
  	PX_ASSERT(pm);
  	NxApexSDK *apexSDK = pm->GetApexSDK();
	if ( MApexAsset )
	{
		apexSDK->releaseAsset(*MApexAsset);
		MApexAsset = NULL;
	}
	
	::NxParameterized::Interface *iface = (::NxParameterized::Interface*)params;
	const char *name = iface->name();
	if ( name )
	{
		setOriginalName(name);
	}
	MApexAsset = apexSDK->createAsset(iface,assetName);
	PX_ASSERT(MApexAsset);

	if ( MApexAsset )
	{
		const char *typeName = MApexAsset->getObjTypeName();
		if ( strcmp(typeName,NX_DESTRUCTIBLE_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_DESTRUCTIBLE;
		}
		else if ( strcmp(typeName,NX_CLOTHING_AUTHORING_TYPE_NAME) == 0)
		{
			MType = AAT_CLOTHING;
		}
		else if ( strcmp(typeName,NX_RENDER_MESH_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_RENDER_MESH;
		}
#if WITH_APEX_PARTICLES
		else if ( strcmp(typeName,NX_BASIC_IOS_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_BASIC_IOS;
		}
		else if ( strcmp(typeName,NX_APEX_EMITTER_AUTHORING_TYPE_NAME) == 0 )
		{
			MType = AAT_APEX_EMITTER;
		}
		else if ( strcmp(typeName,NX_IOFX_AUTHORING_TYPE_NAME ) == 0 )
		{
			MType = AAT_IOFX;
		}
#endif
		else
		{
			PX_ALWAYS_ASSERT(); // unexpected asset type...
			MType = AAT_GENERIC;
		}
	}

	UBOOL ret = IsValid();

	if ( ret )
	{
		// Note that this call will queue up a force load command, which will cause APEX to call the 
		// NRP getResource callback for any unknown resources such as materials. If this is done during 
		// async package loading the call to getResource will return NULL because the package is still 
		// loading. To prevent that from happening, call setResource before the call to forceLoadAssets 
		// has a chance to fire off. Then you won't get any getResource callback.
		NotifyAssetImport();
	}

	return ret;
}

UBOOL ApexAsset::ReleaseNxApexAsset(NxApexAsset *apexAsset)
{
	UBOOL ret = FALSE;
	if ( apexAsset == MApexAsset )
	{
		NotifyAssetGoingAway();
		NxApexSDK *sdk = GApexManager->GetApexSDK();
		sdk->releaseAsset(*MApexAsset);
		MApexAsset = NULL;
		ret = TRUE;
	}
	return ret;
}

/**
* Notifies the actor that a fresh asset has been imported and the actor can be re-created.
**/
void ApexAsset::NotifyAssetImport(void)
{
	MyApexCommands *mc = static_cast< MyApexCommands *>(GApexCommands);
	mc->NotifyAssetImport(this);
	for (TApexActorVector::iterator i=MActors.begin(); i!=MActors.end(); ++i)
	{
		FIApexActor *actor = (*i);
		if ( actor )
		{
			actor->NotifyAssetImport();
		}
	}
}

physx::apex::NxApexAssetAuthoring* ApexAuthoringFromAsset(physx::apex::NxApexAsset* Asset, physx::apex::NxApexSDK* ApexSDK)
{
	const ::NxParameterized::Interface* OldInterface = Asset->getAssetNxParameterized();

	// make the copy
	::NxParameterized::Interface* NewInterface = ApexSDK->getParameterizedTraits()->createNxParameterized(OldInterface->className());
	NewInterface->copy(*OldInterface);

	return ApexSDK->createAssetAuthoring(NewInterface, "AuthoringFromAsset");
}


}; // end of namespace

using namespace NI_APEX_SUPPORT;



FIApexCommands * CreateApexCommands(void)
{
    FIApexCommands *ret = NULL;

    MyApexCommands *ap = new MyApexCommands;
    ret = static_cast< FIApexCommands *>(ap);

    return ret;
}

void ReleaseApexCommands(FIApexCommands *ic)
{
	MyApexCommands *m = static_cast< MyApexCommands *>(ic);
	delete m;
}


void InitializeApex(void)
{
	if ( GApexManager == 0 )
	{
		GApexManager = CreateApexManager( GNovodexSDK, GNovodexCooking );
		CreateApexCommands();
		if ( GWorld && GWorld->RBPhysScene )
		{
			NxScene *scene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
			if ( scene )
			{
				FIApexScene *apexScene = CreateApexScene(scene, GApexManager, true );
				GWorld->RBPhysScene->SetApexScene(apexScene);
			}
		}
	}
}

#endif

#endif
