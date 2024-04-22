/*=============================================================================
	NvApexRender.cpp : Implements the APEX rendering system.
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
#include "ScenePrivate.h"
#include "EngineMeshClasses.h"
#include "UnParticleHelper.h"

// even if apex is disabled, we still need the vertex factories
#if !WITH_APEX
#include "NvApexRenderClasses.h"
#endif

#if WITH_NOVODEX

#include "NvApexManager.h"

#endif

#if WITH_APEX

#include "UnNovodexSupport.h"
#include "NvApexRender.h"
#include "NvApexCommands.h"

#include "NvApexRenderClasses.h"

enum EnqueType
{
	ET_CREATE_VERTEX_BUFFER,
	ET_CREATE_INDEX_BUFFER,
	ET_CREATE_INSTANCE_BUFFER,
	ET_CREATE_RENDER_RESOURCE,
	ET_WRITE_VERTEX_BUFFER,
	ET_WRITE_INDEX_BUFFER,
	ET_WRITE_INSTANCE_BUFFER,
	ET_LAST
};

/**
* This class is used to enque rendering requests from the game thread that must be processed by the rendering thread
*/
class FEnqueData
{
public:
	FEnqueData(void)
	{
		mSemaphore = FALSE; // This semaphore indicates that the rendering thread has completed processing of this request.
	}

	virtual ~FEnqueData(void)
	{
		// TODO: check if we can clear the semaphore before deleting in FApexRenderResourceManager::FlushFEnqueDataGameThread
		// instead of removing this check
		//check(!mSemaphore); // Check that the semaphore has always been cleared by the rendering thread before this class can be destructed.
	}

	virtual UBOOL IsUnused(void) // By default this data item is currently unused
	{
		return TRUE;
	}

	EnqueType mType; // Defines the type of request.

	/**
	* A pure virtual method to indicate that the parent buffer for this data request has been destructed.
	*/
	virtual UBOOL NotifyBufferGone(void *buffer) = 0;

	/***
	* Returns true if the rendering thread is finished processing this data item.
	*/
	UBOOL  ProcessGameThread(void)  // return true of the data item should be killed.
	{
		return IsSemaphoreUsed() ? FALSE : TRUE;
	}

	/***
	* Defines the virtual method that performs the specific request in the rendering thread.
	*/
	virtual void  ProcessRenderingThread(void) = 0; // return true of the data item should be killed.

	/***
	* Returns the current state of the rendering semaphore.
	*/
	UBOOL IsSemaphoreUsed(void) const
    {
    	return mSemaphore;
    };

	/***
	* Sets the rendering thread semaphore to true.  Asserts that it was previously false.
	*/
	void  SetSemaphore(void)
	{
		check(!mSemaphore);
		mSemaphore = TRUE;
	}

	/***
	* Clears the rendering thread semaphore.  Asserts that it was previously set to true.
	*/
	void ClearSemaphore(void)
	{
		check(mSemaphore);
		mSemaphore = FALSE;
	}

private:
	UBOOL	mSemaphore; // The rendering thread semaphore

};

/***
* Holds a create vertex buffer request
*/
class EnqueCreateVertexBuffer : public FEnqueData
{
public:
	EnqueCreateVertexBuffer(FApexRenderVertexBuffer *buffer,const physx::NxUserRenderVertexBufferDesc &desc)
	{
		mType   = ET_CREATE_VERTEX_BUFFER;
		mBuffer = buffer;
		mDesc   = desc;
	}

	virtual ~EnqueCreateVertexBuffer()
	{
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer->Init(mDesc);
			mBuffer = NULL;
		}
		ClearSemaphore();
	}

	FApexRenderVertexBuffer 		*mBuffer;
	physx::NxUserRenderVertexBufferDesc 	 mDesc;
};

/****
* Holds a create index buffer request
*/
class EnqueCreateIndexBuffer : public FEnqueData
{
public:
	EnqueCreateIndexBuffer(FApexRenderIndexBuffer *buffer,const NxUserRenderIndexBufferDesc &desc)
	{
		mType   = ET_CREATE_INDEX_BUFFER;
		mBuffer = buffer;
		mDesc   = desc;
	}

	virtual ~EnqueCreateIndexBuffer()
	{
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer->Init(mDesc);
			mBuffer = NULL;
		}
		ClearSemaphore();
	}

	FApexRenderIndexBuffer 		*mBuffer;
	NxUserRenderIndexBufferDesc 	 mDesc;

};

/****
* Holds a create instance buffer request
*/
class EnqueCreateInstanceBuffer : public FEnqueData
{
public:
	EnqueCreateInstanceBuffer(FApexRenderInstanceBuffer *buffer,const NxUserRenderInstanceBufferDesc &desc)
	{
		mType   = ET_CREATE_INSTANCE_BUFFER;
		mBuffer = buffer;
		mDesc   = desc;
	}

	virtual ~EnqueCreateInstanceBuffer()
	{
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer = NULL;
		}
		ClearSemaphore();
	}

	FApexRenderInstanceBuffer 		*mBuffer;
	NxUserRenderInstanceBufferDesc 	 mDesc;

};

/****
* Holds a create render resource request.
*/
class EnqueCreateRenderResource : public FEnqueData
{
public:
#define MAX_NUM_VERTEX_BUFFERS 16 // can't imagine ever having more than 16!
	EnqueCreateRenderResource(FApexRenderResource *buffer,const physx::NxUserRenderResourceDesc &desc)
	{
		mType   = ET_CREATE_RENDER_RESOURCE;
		mBuffer = buffer;
		PX_ASSERT( desc.numVertexBuffers < MAX_NUM_VERTEX_BUFFERS );
		mDesc   = desc;
		for (physx::PxU32 i=0; i<desc.numVertexBuffers; i++)
		{
			mBuffers[i] = desc.vertexBuffers[i];
		}
		mDesc.vertexBuffers = mBuffers;
	}

	virtual ~EnqueCreateRenderResource()
	{
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer->Init(mDesc);
			mBuffer = NULL;
		}
		ClearSemaphore();
	}

	FApexRenderResource 		*mBuffer;
	physx::NxUserRenderResourceDesc 	 mDesc;
	physx::NxUserRenderVertexBuffer	*mBuffers[MAX_NUM_VERTEX_BUFFERS];

};


UBOOL	GReverseCulling=TRUE;

/****
* Holds a write to vertex buffer request
*/
class EnqueWriteVertexBuffer : public FEnqueData
{
public:
	EnqueWriteVertexBuffer(FApexRenderVertexBuffer *buffer,const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices)
	{
		mNormalSign = GApexNormalSign;
		mType   = ET_WRITE_VERTEX_BUFFER;
		mBuffer = buffer;

		mFirstVertex = firstVertex;
		mNumVertices = numVertices;

		mData = NULL;
		UpdateWithData(data);

		mFlushCount = 0;
	}

	virtual ~EnqueWriteVertexBuffer()
	{
		appFree(mData);
	}

	virtual UBOOL IsUnused(void)
	{
		mFlushCount++;
		return mFlushCount > 1 ? TRUE : FALSE;
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer->writeBuffer(mData);
		}
		ClearSemaphore();
	}

	/***
	* Returns true if this new write buffer request exactly matches the previous one.
	*/
	UBOOL VertexBufferMatch(FApexRenderVertexBuffer *buffer, const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices)
	{
		UBOOL ret = FALSE;
		return ret;

		check( !IsSemaphoreUsed() );

		if ( buffer == mBuffer && firstVertex == mFirstVertex && numVertices == mNumVertices )
		{
			mFlushCount = 0;
			UpdateWithData(data);
			ret = TRUE;
		}
		return ret;
	}

private:

	/*
	process the vb data and update our internal copy (via utility methods on the vertex buffer class)
	*/
	void UpdateWithData(const NxApexRenderVertexBufferData &data)
	{
		check(mBuffer);

		if(!mData)
		{
			// alloc a buffer to hold all semantics
			mData = (BYTE*)appMalloc(mBuffer->GetBufferSize());
		}

		// copy it over
		for(physx::PxU32 i=0; i<NxRenderVertexSemantic::NUM_SEMANTICS; i++)
		{
			NxRenderVertexSemantic::Enum semantic = (NxRenderVertexSemantic::Enum)i;
			const NxApexRenderSemanticData &semanticData = data.getSemanticData(semantic);

			if ( semantic == NxRenderVertexSemantic::NORMAL )
				GApexNormalSign = mNormalSign;

			mBuffer->writeBufferSemantic(mData,semantic,semanticData.data,semanticData.stride,semanticData.format,mFirstVertex,mNumVertices);
			
			GApexNormalSign = 1;
		}
	}


	FApexRenderVertexBuffer *mBuffer;
	BYTE *							mData;	// contains a full copy of the VB updates with the data from all the semantics.
	physx::PxU32					mBufferSize;
	
	physx::PxU32					mFirstVertex;
	physx::PxU32					mNumVertices;

	physx::PxU32					mFlushCount;
	physx::PxF32					mNormalSign;
};

/****
* Handles a write index buffer request.
*/
class EnqueWriteIndexBuffer : public FEnqueData
{
public:
	EnqueWriteIndexBuffer(FApexRenderIndexBuffer *buffer,
							const void *SrcData,
							physx::PxU32 SrcStride,
							physx::PxU32 FirstDestElement,
							physx::PxU32 NumElements)
	{
		mType   = ET_WRITE_VERTEX_BUFFER;
		mBuffer = buffer;
		mSrcData = appMalloc(SrcStride*NumElements);
		mSrcStride = SrcStride;
		memcpy(mSrcData,SrcData,SrcStride*NumElements);
		mFirstDestElement = FirstDestElement;
		mNumElements = NumElements;
		mFlushCount = 0;
	}

	virtual ~EnqueWriteIndexBuffer()
	{
		if ( mSrcData )
		{
			appFree(mSrcData);
		}
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer  )
		{
			mBuffer->writeBuffer(mSrcData,mSrcStride,mFirstDestElement,mNumElements);
		}
		ClearSemaphore();
	}

	FApexRenderIndexBuffer			*mBuffer;
	void 							*mSrcData;
	physx::PxU32 						 	mSrcStride;
	physx::PxU32 						 	mFirstDestElement;
	physx::PxU32 						 	mNumElements;
	physx::PxU32							mFlushCount;
};

/****
* Handles a write instance buffer request.
*/
class EnqueWriteInstanceBuffer : public FEnqueData
{
public:
	EnqueWriteInstanceBuffer(FApexRenderInstanceBuffer *buffer,
							physx::NxRenderInstanceSemantic::Enum Semantic,
							const void *SrcData,
							physx::PxU32 SrcStride,
							physx::PxU32 FirstDestElement,
							physx::PxU32 NumElements)
	{
		mType   = ET_WRITE_VERTEX_BUFFER;
		mBuffer = buffer;
		mSemantic = Semantic;

		mSrcData = appMalloc(SrcStride*NumElements);
		mSrcStride = SrcStride;
		memcpy(mSrcData,SrcData,SrcStride*NumElements);
		mFirstDestElement = FirstDestElement;
		mNumElements = NumElements;
		mFlushCount = 0;
	}

	virtual ~EnqueWriteInstanceBuffer()
	{
		appFree(mSrcData);
	}

	virtual UBOOL IsUnused(void)
	{
		mFlushCount++;
		return mFlushCount > 1 ? TRUE : FALSE;
	}

	virtual UBOOL NotifyBufferGone(void *buffer)
	{
		UBOOL ret = FALSE;
		if ( buffer == mBuffer )
		{
			ClearSemaphore();
			ret = TRUE;
		}
		return ret;
	}

	virtual void  ProcessRenderingThread(void)
	{
		if ( mBuffer )
		{
			mBuffer->writeBuffer(mSemantic,mSrcData,mSrcStride,mFirstDestElement,mNumElements);
		}
		ClearSemaphore();
	}

	UBOOL InstanceBufferMatch(FApexRenderInstanceBuffer *buffer,
							physx::NxRenderInstanceSemantic::Enum Semantic,
							const void *SrcData,
							physx::PxU32 SrcStride,
							physx::PxU32 FirstDestElement,
							physx::PxU32 NumElements)
	{
		UBOOL ret = FALSE;

		check( !IsSemaphoreUsed() );

		if ( buffer == mBuffer &&
			 Semantic == mSemantic &&
			 SrcStride == mSrcStride &&
			 FirstDestElement == mFirstDestElement &&
			 NumElements == mNumElements )
		{
			mFlushCount = 0;
			memcpy(mSrcData,SrcData,SrcStride*NumElements);
			ret = TRUE;
		}
		return ret;
	}


	FApexRenderInstanceBuffer			*mBuffer;
	physx::NxRenderInstanceSemantic::Enum 	mSemantic;
	void 							*mSrcData;
	physx::PxU32 						 	mSrcStride;
	physx::PxU32 						 	mFirstDestElement;
	physx::PxU32 						 	mNumElements;
	physx::PxU32							mFlushCount;
};



// Adds a create vertex buffer request to the queue
void FApexRenderResourceManager::Enque(FApexRenderVertexBuffer *buffer,const physx::NxUserRenderVertexBufferDesc &desc)
{
	check(IsInGameThread());

	EnqueCreateVertexBuffer *e = new EnqueCreateVertexBuffer(buffer,desc);
	PostFEnqueDataGameThread(e);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		FEnqueData,
		EnqueCreateVertexBuffer *, e, e,
		{
			GApexRender->PostFEnqueDataRenderingThread(e);
		}
	);

}

void FApexRenderResourceManager::Enque(FApexRenderVertexBuffer *buffer,const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices)
{
	check(IsInGameThread());

	// See if this write buffer request matches any buffer in our 'reuse' queue.
	EnqueWriteVertexBuffer *e = NULL;
	for (TFEnqueDataArray::iterator i=EnqueDataReuse.begin(); i!=EnqueDataReuse.end(); ++i)
	{
		FEnqueData *ed = (*i);
		if (ed && ed->mType == ET_WRITE_VERTEX_BUFFER )
		{
			EnqueWriteVertexBuffer *found = static_cast< EnqueWriteVertexBuffer *>(ed);
			if ( found->VertexBufferMatch(buffer,data,firstVertex,numVertices) )
			{
				(*i) = NULL;
				e = found;
				break;
			}
		}
	}

	if ( e == NULL )
	{
		e = new EnqueWriteVertexBuffer(buffer,data,firstVertex,numVertices);
	}

	PostFEnqueDataGameThread(e);

	// Enque this post request on the rendering thread
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
		FEnqueData,
		EnqueWriteVertexBuffer *, e, e,
	{
		GApexRender->PostFEnqueDataRenderingThread(e);
	}
	);
}

// Add a create index buffer request to the queue
void FApexRenderResourceManager::Enque(FApexRenderIndexBuffer *buffer,const NxUserRenderIndexBufferDesc &desc)
{
	check(IsInGameThread());

	EnqueCreateIndexBuffer *e = new EnqueCreateIndexBuffer(buffer,desc);
	PostFEnqueDataGameThread(e);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		FEnqueData,
		EnqueCreateIndexBuffer *, e, e,
		{
			GApexRender->PostFEnqueDataRenderingThread(e);
		}
	);

}

// Add a write index buffer request to the queue
void FApexRenderResourceManager::Enque(FApexRenderIndexBuffer *buffer,const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements)
{
	check(IsInGameThread());
	EnqueWriteIndexBuffer *e = new EnqueWriteIndexBuffer(buffer,SrcData,SrcStride,FirstDestElement,NumElements);
	PostFEnqueDataGameThread(e);
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		FEnqueData,
		EnqueWriteIndexBuffer *, e, e,
		{
			GApexRender->PostFEnqueDataRenderingThread(e);
		}
	);
}

// Add a create instance buffer request to the queue
void FApexRenderResourceManager::Enque(FApexRenderInstanceBuffer *buffer,const NxUserRenderInstanceBufferDesc &desc)
{
	check(IsInGameThread());

	EnqueCreateInstanceBuffer *e = new EnqueCreateInstanceBuffer(buffer,desc);
	PostFEnqueDataGameThread(e);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		FEnqueData,
		EnqueCreateInstanceBuffer *, e, e,
		{
			GApexRender->PostFEnqueDataRenderingThread(e);
		}
	);

}

// Add a write instance buffer request to the queue
void FApexRenderResourceManager::Enque(FApexRenderInstanceBuffer *buffer,physx::NxRenderInstanceSemantic::Enum Semantic,const void *SrcData,physx::PxU32 SrcStride,physx::PxU32 FirstDestElement,physx::PxU32 NumElements,physx::PxU32 *indexRemapping)
{
	check(IsInGameThread());
	check(indexRemapping==NULL);

	EnqueWriteInstanceBuffer *e = NULL;
	for (TFEnqueDataArray::iterator i=EnqueDataReuse.begin(); i!=EnqueDataReuse.end(); ++i)
	{
		FEnqueData *ed = (*i);
		if (ed && ed->mType == ET_WRITE_INSTANCE_BUFFER )
		{
			EnqueWriteInstanceBuffer *found = static_cast< EnqueWriteInstanceBuffer *>(ed);
			if ( found->InstanceBufferMatch(buffer,Semantic,SrcData,SrcStride,FirstDestElement,NumElements) )
			{
				(*i) = NULL;
				e = found;
				break;
			}
		}
	}

	if ( e == NULL )
	{
    	e = new EnqueWriteInstanceBuffer(buffer,Semantic,SrcData,SrcStride,FirstDestElement,NumElements);
	}

	PostFEnqueDataGameThread(e);
   	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
   	(
   		FEnqueData,
   		EnqueWriteInstanceBuffer *, e, e,
   		{
   			GApexRender->PostFEnqueDataRenderingThread(e);
   		}
   	);
}

// Add a create render resource request to the queue
void FApexRenderResourceManager::Enque(FApexRenderResource *buffer,const physx::NxUserRenderResourceDesc &desc)
{
	check(IsInGameThread());

	EnqueCreateRenderResource *e = new EnqueCreateRenderResource(buffer,desc);
	PostFEnqueDataGameThread(e);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(
		FEnqueData,
		EnqueCreateRenderResource *, e, e,
		{
			GApexRender->PostFEnqueDataRenderingThread(e);
		}
	);

}

using namespace physx::apex;

FIApexRender *GApexRender = NULL;
physx::PxF32	GApexNormalSign=1;

/* FApexDynamicRenderer */

extern bool gOpaqueRender;

void FApexDynamicRenderer::RenderStaticMeshInstanced(FPrimitiveDrawInterface &PDI,UStaticMesh *StaticMesh,UINT NumInstances,const FMatrix *transforms, const FSceneView* View, UINT DPGIndex)
{

//	debugf(NAME_DevPhysics, TEXT("RenderStaticMeshInstanced(%d)"), NumInstances );

	const FStaticMeshRenderData&       LODModel = StaticMesh->LODModels(0);
	const FStaticMeshElement&          Element  = LODModel.Elements(0);

	FMeshEmitterMaterialInstanceResource	AutoMIRes;
	FMeshEmitterMaterialInstanceResource*	MIRes = & AutoMIRes;

	MIRes->Param_MeshEmitterVertexColor = FLinearColor(1.0f, 1.0f, 1.0f);
	if (Element.Material)
	{
		MIRes->Parent = Element.Material->GetRenderProxy(bSelected);
	}
	else
	{
		MIRes->Parent = GEngine->DefaultMaterial->GetRenderProxy(bSelected);
	}

	UBOOL bWireframe = ((View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials));

	FMeshBatch MeshElement;
	FMeshBatchElement& BatchElement = MeshElement.Elements(0);
	MeshElement.VertexFactory = &LODModel.VertexFactory;
	MeshElement.DynamicVertexData = NULL;
	MeshElement.LCI = NULL;

	BatchElement.FirstIndex     = Element.FirstIndex;
	BatchElement.MinVertexIndex = Element.MinVertexIndex;
	BatchElement.MaxVertexIndex = Element.MaxVertexIndex;
	MeshElement.UseDynamicData = FALSE;
	MeshElement.ReverseCulling = FALSE;
	MeshElement.CastShadow     = FALSE;
	MeshElement.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;
	MeshElement.bWireframe     = bWireframe;
	MeshElement.bUsePreVertexShaderCulling = FALSE;
	MeshElement.PlatformMeshData = NULL;

	if (bWireframe && LODModel.WireframeIndexBuffer.IsInitialized())
	{
		BatchElement.IndexBuffer    = &LODModel.WireframeIndexBuffer;
		MeshElement.MaterialRenderProxy = MIRes; // TODO:JWR Proxy->GetDeselectedWireframeMatInst();
		MeshElement.Type           = PT_LineList;
		BatchElement.NumPrimitives  = LODModel.WireframeIndexBuffer.Indices.Num() / 2;
	}
	else
	{
		BatchElement.IndexBuffer    = &LODModel.IndexBuffer;
		MeshElement.MaterialRenderProxy = MIRes;
		MeshElement.Type           = PT_TriangleList;
		BatchElement.NumPrimitives  = LODModel.IndexBuffer.Indices.Num() / 3;
	}

	for (UINT i =0; i<NumInstances; i++)
	{
		BatchElement.LocalToWorld = transforms[i];
		BatchElement.WorldToLocal = BatchElement.LocalToWorld.Inverse();
		PDI.DrawMesh(MeshElement);
	}
}
void FApexDynamicRenderer::renderResource(const NxApexRenderContext &Context)
{
	if ( Context.renderResource)
	{
		FApexRenderResource &Mesh = *static_cast<FApexRenderResource*>(Context.renderResource);
		if ( Mesh.GetOpaqueMesh() )
		{
			// render using a UE3 StaticMesh object!
			UStaticMesh *staticMesh = (UStaticMesh *)Mesh.GetOpaqueMesh();
			FApexRenderInstanceBuffer *instanceBuffer = Mesh.GetInstanceBuffer();
			if ( instanceBuffer && Mesh.GetNumInstances() )
			{
				RenderStaticMeshInstanced(PDI,staticMesh,Mesh.GetNumInstances(),&instanceBuffer->GetTransform(Mesh.GetFirstInstance()),View,DPGIndex);
			}
		}
		else
		{
			FMeshBatch MeshElement;
			FMeshBatchElement& BatchElement = MeshElement.Elements(0);
			if(Mesh.loadMeshElement(MeshElement, Context, bSelected))
			{
				MeshElement.DepthPriorityGroup	= DPGIndex;

				MeshElement.CastShadow = bCastShadow;
				if(bWireframe)
				{
					MeshElement.bWireframe = TRUE;
				}
				FApexRenderInstanceBuffer *InstanceBuffer = Mesh.GetInstanceBuffer();
				if ( InstanceBuffer )
				{
					for (INT i=0; i < InstanceBuffer->GetNumTransforms(); i++)
					{
						BatchElement.LocalToWorld = InstanceBuffer->GetTransform(i);
						PDI.DrawMesh(MeshElement);
					}
				}
				else
				{
					PDI.DrawMesh(MeshElement);
				}
			}
		}
	}
}



/* FApexRenderResourceManager */

NxUserRenderVertexBuffer *FApexRenderResourceManager::createVertexBuffer(const physx::NxUserRenderVertexBufferDesc &Desc)
{
	check(GIsRHIInitialized);
	FApexRenderVertexBuffer *VB = NULL;
#if !ENABLE_NULL_VB
	check(Desc.isValid()); // Commented out for now because sometimes APEX asks for a zero sized VB.
#endif
	if(Desc.isValid())
	{
		VB = new FApexRenderVertexBuffer(Desc);
	}
	PX_ASSERT(VB);
	return VB;
}

NxUserRenderIndexBuffer *FApexRenderResourceManager::createIndexBuffer(const NxUserRenderIndexBufferDesc &Desc)
{
	check(GIsRHIInitialized);
	FApexRenderIndexBuffer *IB = NULL;
	check(Desc.isValid());
	if(Desc.isValid())
	{
		IB = new FApexRenderIndexBuffer(Desc);
	}
	return IB;
}

NxUserRenderBoneBuffer *FApexRenderResourceManager::createBoneBuffer(const NxUserRenderBoneBufferDesc &Desc)
{
	check(GIsRHIInitialized);
	FApexRenderBoneBuffer *BB = NULL;
	check(Desc.isValid());
	if(Desc.isValid())
	{
		BB = new FApexRenderBoneBuffer(Desc);
	}
	return BB;
}

NxUserRenderInstanceBuffer *FApexRenderResourceManager::createInstanceBuffer(const NxUserRenderInstanceBufferDesc &Desc)
{
	check(GIsRHIInitialized);
	FApexRenderInstanceBuffer *IB = NULL;
	check(Desc.isValid());
	if(Desc.isValid())
	{
		IB = new FApexRenderInstanceBuffer(Desc);
	}
	return IB;
}

NxUserRenderSpriteBuffer *FApexRenderResourceManager::createSpriteBuffer( const NxUserRenderSpriteBufferDesc &Desc )
{
	// Unimplemented
	return NULL;
}

NxUserRenderResource *FApexRenderResourceManager::createResource(const physx::NxUserRenderResourceDesc &Desc)
{
	check(GIsRHIInitialized);
	NxUserRenderResource *RenderResource = NULL;
	check(Desc.isValid());
	if(Desc.isValid())
	{
		RenderResource = new FApexRenderResource(Desc);
	}
	return RenderResource;
}

void FApexRenderResourceManager::releaseVertexBuffer(NxUserRenderVertexBuffer &InBuffer)
{
	if(IsInRenderingThread())
	{
		delete static_cast<FApexRenderVertexBuffer*>(&InBuffer);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			ReleaseVertexBuffer,
			NxUserRenderVertexBuffer*, Buffer, &InBuffer,
			{
				delete static_cast<FApexRenderVertexBuffer*>(Buffer);
			}
		);
	}
}

void FApexRenderResourceManager::releaseIndexBuffer(NxUserRenderIndexBuffer &InBuffer)
{
	if(IsInRenderingThread())
	{
		delete static_cast<FApexRenderIndexBuffer*>(&InBuffer);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			ReleaseIndexBuffer,
			NxUserRenderIndexBuffer*, Buffer, &InBuffer,
			{
				delete static_cast<FApexRenderIndexBuffer*>(Buffer);
			}
		);
	}
}

void FApexRenderResourceManager::releaseBoneBuffer(NxUserRenderBoneBuffer &InBuffer)
{
	if(IsInRenderingThread())
	{
		delete static_cast<FApexRenderBoneBuffer*>(&InBuffer);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			ReleaseBoneBuffer,
			NxUserRenderBoneBuffer*, Buffer, &InBuffer,
			{
				delete static_cast<FApexRenderBoneBuffer*>(Buffer);
			}
		);
	}
}

void FApexRenderResourceManager::releaseInstanceBuffer(NxUserRenderInstanceBuffer &InBuffer)
{
	if(IsInRenderingThread())
	{
		delete static_cast<FApexRenderInstanceBuffer*>(&InBuffer);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			ReleaseInstanceBuffer,
			NxUserRenderInstanceBuffer*, Buffer, &InBuffer,
			{
				delete static_cast<FApexRenderInstanceBuffer*>(Buffer);
			}
		);
	}
}

void FApexRenderResourceManager::releaseSpriteBuffer( NxUserRenderSpriteBuffer &InBuffer )
{
	// Unimplemented
}

void FApexRenderResourceManager::releaseResource(NxUserRenderResource &RenderResource)
{
	if(IsInRenderingThread())
	{
		delete &RenderResource;
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			ReleaseResource,
			NxUserRenderResource*, Resource, &RenderResource,
			{
				delete Resource;
			}
		);
	}
}

physx::PxU32 FApexRenderResourceManager::getMaxBonesForMaterial(void *Material)
{
	return APEX_MAX_BONES;
}


/* FApexDynamicResources */

FApexDynamicResources::FApexDynamicResources(INT NumBones)
{
#ifndef __CELLOS_LV2__
    NumBones;
#endif
	// Unimplemented
}


/*	UApexStaticComponent */

FPrimitiveSceneProxy *UApexStaticComponent::CreateSceneProxy()
{
	return new FApexStaticSceneProxy(*this);
}


/* FApexBaseSceneProxy */

FApexBaseSceneProxy::FApexBaseSceneProxy(const UApexComponentBase &Component):
FPrimitiveSceneProxy(&Component, Component.Asset->GetFName()),
Owner(Component.GetOwner()),
TotalScale3D(1.0f,1.0f,1.0f),
bCastShadow(Component.CastShadow),
bShouldCollide(Component.ShouldCollide()),
bBlockZeroExtent(Component.BlockZeroExtent),
bBlockNonZeroExtent(Component.BlockNonZeroExtent),
bBlockRigidBody(Component.BlockRigidBody),
bForceStaticDecal(TRUE),	// Not using Component.bForceStaticDecals
MaterialViewRelevance(Component.GetMaterialViewRelevance()),
WireframeColor(FColor(255,128,64,255))	// Not using Component.WireframeColor
{
	bMovable            = TRUE;
	bApexUpdateRequired = FALSE;
}

/* FPrimitiveSceneProxy interface. */
void FApexBaseSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
}

/**
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
* @param	DPGIndex - current depth priority
* @param	Flags - optional set of flags from EDrawDynamicElementFlags
*/
void FApexBaseSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface *PDI, const FSceneView *View, UINT DPGIndex, DWORD Flags)
{
	// Bool that defines how we should draw the collision for this mesh.
	const UBOOL bIsCollisionView      = IsCollisionView(View);
	const UBOOL bDrawCollision        = bIsCollisionView;	// Not using && ShouldDrawCollision(View)
	const UBOOL bDrawSimple           = FALSE;	// Not using ShouldDrawSimpleCollision(View, StaticMesh)
	const UBOOL bDrawComplexCollision = (bDrawCollision && !bDrawSimple);
	const UBOOL bDrawSimpleCollision  = (bDrawCollision && bDrawSimple);
	const UBOOL bDrawMesh             = bIsCollisionView ? bDrawComplexCollision : IsRichView(View) || HasViewDependentDPG() || IsMovable();

	check(PDI && View && View->Family && PrimitiveSceneInfo);
	if(PDI && View && View->Family && PrimitiveSceneInfo)
	{
		EShowFlags ShowFlags = View->Family->ShowFlags;
		if(DPGIndex == SDPG_Foreground && (ShowFlags & SHOW_Bounds) && (ShowFlags & SHOW_StaticMeshes) && (GIsGame || !Owner || bSelected))
		{
			// Draw the static mesh's bounding box and sphere.
			DrawWireBox(PDI, PrimitiveSceneInfo->Bounds.GetBox(), FColor(72,72,255), SDPG_Foreground);
			DrawCircle( PDI, PrimitiveSceneInfo->Bounds.Origin, FVector(1,0,0), FVector(0,1,0), FColor(255,255,0), PrimitiveSceneInfo->Bounds.SphereRadius, 32, SDPG_Foreground);
			DrawCircle( PDI, PrimitiveSceneInfo->Bounds.Origin, FVector(1,0,0), FVector(0,0,1), FColor(255,255,0), PrimitiveSceneInfo->Bounds.SphereRadius, 32, SDPG_Foreground);
			DrawCircle( PDI, PrimitiveSceneInfo->Bounds.Origin, FVector(0,1,0), FVector(0,0,1), FColor(255,255,0), PrimitiveSceneInfo->Bounds.SphereRadius, 32, SDPG_Foreground);
		}
	}
}

void FApexBaseSceneProxy::OnTransformChanged()
{
	// Update the cached scaling.
	TotalScale3D.X = FVector(LocalToWorld.TransformNormal(FVector(1,0,0))).Size();
	TotalScale3D.Y = FVector(LocalToWorld.TransformNormal(FVector(0,1,0))).Size();
	TotalScale3D.Z = FVector(LocalToWorld.TransformNormal(FVector(0,0,1))).Size();
}

FPrimitiveViewRelevance FApexBaseSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;

	// Signal that we need to call NxApexRenderable::updateRenderResources()
	bApexUpdateRequired = TRUE;
	bRewriteBuffersRequired = GApexRender->GetRequireRewriteBuffers();

#if 1 // Force UE3 to render APEX actors as Dynamic.

	EShowFlags ShowFlags = (View && View->Family) ? View->Family->ShowFlags : 0;
	if(ShowFlags & SHOW_StaticMeshes)
	{
	#if ENABLE_STATIC_RENDER
		Result.bStaticRelevance  = TRUE;
	#else
		Result.bStaticRelevance  = FALSE;
	#endif
		Result.bDynamicRelevance = TRUE;
		Result.SetDPG(GetDepthPriorityGroup(View), TRUE);

		MaterialViewRelevance.SetPrimitiveViewRelevance(Result);

		EShowFlags ShowFlags = View->Family->ShowFlags;
		if(ShowFlags & SHOW_Bounds)
		{
			Result.SetDPG(SDPG_Foreground, TRUE);
		}

		if(IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
	}

#else

	if(View->Family->ShowFlags & SHOW_StaticMeshes)
	{
		if(IsShown(View))
		{
#if !FINAL_RELEASE
			if(IsCollisionView(View))
			{
				Result.bDynamicRelevance = TRUE;
				Result.bForceDirectionalLightsDynamic = TRUE;
			}
			else
#endif
			if(
#if !FINAL_RELEASE
				IsRichView(View) ||
#endif
				HasViewDependentDPG() ||
				IsMovable()	)
			{
				Result.bDynamicRelevance = TRUE;
			}
			else
			{
				Result.bStaticRelevance = TRUE;
			}
			Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
		}

#if !FINAL_RELEASE
		if(View->Family->ShowFlags & (SHOW_Bounds|SHOW_Collision))
		{
			Result.bDynamicRelevance = TRUE;
		}

		// only add to foreground DPG for debug rendering
		if(View->Family->ShowFlags & SHOW_Bounds)
		{
			Result.SetDPG(SDPG_Foreground,TRUE);
		}
#endif
		if(IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}

		MaterialViewRelevance.SetPrimitiveViewRelevance(Result);

		Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
		Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);
	}

#endif
	return Result;
}


/**
 *	Determines the relevance of this primitive's elements to the given light.
 *	@param	LightSceneInfo			The light to determine relevance for
 *	@param	bDynamic (output)		The light is dynamic for this primitive
 *	@param	bRelevant (output)		The light is relevant for this primitive
 *	@param	bLightMapped (output)	The light is light mapped for this primitive
 */
void FApexBaseSceneProxy::GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
{
	bDynamic = TRUE;
	bRelevant = TRUE;
	bLightMapped = FALSE;
}


/* FApexDynamicSceneProxy */

FApexDynamicSceneProxy::FApexDynamicSceneProxy(const UApexDynamicComponent &Component):
FApexBaseSceneProxy(Component),
ComponentDynamicResources(Component.ComponentDynamicResources)
{
}

void FApexDynamicSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
}


/* FApexStaticSceneProxy */

/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
FApexStaticSceneProxy::FApexStaticSceneProxy(const UApexStaticComponent &Component) :
FApexBaseSceneProxy(Component),
ApexComponent(Component)
{
	check( IsInGameThread() );
	ApexRenderable = ApexComponent.GetApexRenderable();
}

// FPrimitiveSceneProxy interface.
void FApexStaticSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface *PDI)
{
	check(PDI);
	if(PDI && ApexRenderable && GApexCommands->IsShowApex() )
	{
	#if ENABLE_STATIC_RENDER // this gets called from game thread... DrawDynamicElements gets called from rendering thread though.
		FApexStaticRenderer ApexRenderer(*PDI, bSelected, bCastShadow);
		ApexRenderable->dispatchRenderResources(ApexRenderer, 1.0f);
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			DrawApexRenderable,
			NxApexRenderable*, ApexRenderable, ApexRenderable,
			{
				ApexRenderable->updateRenderResources();
			}
		);
	#endif
	}
	FApexBaseSceneProxy::DrawStaticElements(PDI);
}

/**
 * Draw the scene proxy as a dynamic element
 *
 * @param	PDI - draw interface to render to
 * @param	View - current view
 * @param	DPGIndex - current depth priority
 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
 */
void FApexStaticSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface *PDI, const FSceneView *View, UINT DPGIndex, DWORD Flags)
{
	check(PDI);
	if( PDI && ApexRenderable && DPGIndex != SDPG_Foreground && GApexCommands->IsShowApex() )
	{
	#if !ENABLE_STATIC_RENDER
		UBOOL bWireframe = (View && View->Family && (View->Family->ShowFlags & SHOW_Wireframe)) ? TRUE : FALSE;
		FApexDynamicRenderer ApexRenderer(*PDI, bSelected, bCastShadow, bWireframe,View,DPGIndex);
		if(bApexUpdateRequired)
		{
			void* ApexUserData = NULL;
			ApexRenderable->lockRenderResources();
			ApexRenderable->updateRenderResources( bRewriteBuffersRequired, NULL );
			ApexRenderable->unlockRenderResources();
			bApexUpdateRequired = FALSE;
			bRewriteBuffersRequired = FALSE;
		}
		ApexRenderable->dispatchRenderResources(ApexRenderer);
	#endif
	}
	FApexBaseSceneProxy::DrawDynamicElements(PDI, View, DPGIndex, Flags);
}

NxUserRenderer	* FApexRenderResourceManager::CreateDynamicRenderer(void *InPDI, UBOOL Selected, UBOOL CastShadow, UBOOL WireFrame, const FSceneView *View, UINT DPGIndex)
{
	NxUserRenderer *Ret = NULL;
	if ( InPDI )
	{
		FPrimitiveDrawInterface *PDI = (FPrimitiveDrawInterface *)InPDI;
		FApexDynamicRenderer *Renderer = new FApexDynamicRenderer(*PDI,Selected,CastShadow,WireFrame,View,DPGIndex);
		Ret = static_cast< NxUserRenderer *>(Renderer);
	}
	return Ret;
}

void FApexRenderResourceManager::ReleaseDynamicRenderer(NxUserRenderer *UserRenderer)
{
	FApexDynamicRenderer *Renderer = static_cast< FApexDynamicRenderer *>(UserRenderer);
	delete Renderer;
}

FIApexRender * CreateApexRender(void)
{
	FApexRenderResourceManager *Manager = new FApexRenderResourceManager;
	return static_cast< FIApexRender *>(Manager);
}

void          ReleaseApexRender(FIApexRender *Renderer)
{
	FApexRenderResourceManager *Manager = static_cast< FApexRenderResourceManager *>(Renderer);
	delete Manager;
}

// Post this rendering request to the rendering thread queue
void FApexRenderResourceManager::PostFEnqueDataRenderingThread(FEnqueData *e)
{
	check(IsInRenderingThread());
	check( e != NULL );
	check( e->IsSemaphoreUsed() );
	e->ProcessRenderingThread();
}

// Post this rendering request to the game thread queue
void FApexRenderResourceManager::PostFEnqueDataGameThread(FEnqueData *e)
{
	check(IsInGameThread());
	check( !e->IsSemaphoreUsed() );
	e->SetSemaphore();
	EnqueDataGameThread.push_back(e);
}

// Notification that a buffer has been released.  This always happens in the rendering thread.
// If this buffer was actively in the rendering thread queue, its semaphore is cleared so that it can
// be properly released by the game thread.
void FApexRenderResourceManager::NotifyEnqueBufferGone(void *buffer)
{
	check(IsInRenderingThread());
	for (TFEnqueDataArray::iterator i=EnqueDataRenderingThread.begin(); i!=EnqueDataRenderingThread.end(); ++i)
	{
		FEnqueData *e = (*i);
		if ( e )
		{
			UBOOL gone = (*i)->NotifyBufferGone(buffer);
			if ( gone )
			{
				(*i) = NULL;
			}
		}
	}
}

// Process all pending rendering requests from inside the rendering thread
void FApexRenderResourceManager::ProcessFEnqueDataRenderingThread(void)
{
	check(IsInRenderingThread());
	if ( !EnqueDataRenderingThread.empty() )
	{
		for (TFEnqueDataArray::iterator i=EnqueDataRenderingThread.begin(); i!=EnqueDataRenderingThread.end(); ++i)
		{
			FEnqueData *e = (*i);
			if ( e )
			{
				check( e->IsSemaphoreUsed() );
				e->ProcessRenderingThread();
			}
		}
		EnqueDataRenderingThread.clear();
	}
}

// Process all pending requests in the game thread.
void FApexRenderResourceManager::ProcessFEnqueDataGameThread(void)
{
	check(IsInGameThread());

	// See if any of the objects in the 'reuse' thread were not re-used by additional requests.
	// If the were not reused, then delete them.
	for (TFEnqueDataArray::iterator i=EnqueDataReuse.begin(); i!=EnqueDataReuse.end(); ++i)
	{
		FEnqueData *e = (*i);
		if ( e && e->IsUnused() )
		{
			(*i) = 0;
			delete e;
			EnqueDataReuseCount = 2;
		}
	}

	// If it has been several frames since the last time we deleted an object from the 'resue' queue, then clean up the list.
	if ( EnqueDataReuseCount )
	{
		EnqueDataReuseCount--;
		if ( EnqueDataReuseCount == 0 )
		{
			TFEnqueDataArray temp;
        	for (TFEnqueDataArray::iterator i=EnqueDataReuse.begin(); i!=EnqueDataReuse.end(); ++i)
        	{
        		FEnqueData *e = (*i);
        		if ( e )
        		{
        			temp.push_back(e);
        		}
        	}
        	EnqueDataReuse = temp;
		}
	}

	// If we have objects in the game thread queue
	if ( !EnqueDataGameThread.empty() )
	{
		UBOOL dead = FALSE;
		UBOOL alive = FALSE;
		for (TFEnqueDataArray::iterator i=EnqueDataGameThread.begin(); i!=EnqueDataGameThread.end(); ++i)
		{
			FEnqueData *e = (*i);
			if ( e )
			{
				UBOOL kill = e->ProcessGameThread();
				if ( kill )
				{
					if ( e->mType == ET_WRITE_VERTEX_BUFFER )
					{
						for (TFEnqueDataArray::iterator j=EnqueDataReuse.begin(); j!=EnqueDataReuse.end(); ++j)
						{
							if ( (*j) == NULL )
							{
								(*j) = e;
								e = NULL;
								break;
							}
						}
						if ( e )
						{
							check( !e->IsSemaphoreUsed() );
							EnqueDataReuse.push_back(e);
						}
					}
					else
					{
						delete e;
					}
					(*i) = NULL;
					dead = TRUE;
				}
				else
				{
					alive = TRUE; // not that at least
				}
			}
			else
			{
				dead = TRUE;
			}
		}
		if ( dead && !alive ) // if all items are dead, and none are alive, we clear the queue
		{
			EnqueDataGameThread.clear();
		}
		else if ( dead && alive ) // if some are dead and some are alive, we need to rebuild the queue with only the alive ones.
		{
			TFEnqueDataArray aliveData;
			for (TFEnqueDataArray::iterator i=EnqueDataGameThread.begin(); i!=EnqueDataGameThread.end(); ++i)
			{
				FEnqueData *e = (*i);
				if ( e )
				{
					aliveData.push_back(e);
				}
			}
			EnqueDataGameThread = aliveData;
		}
	}
}


void FApexRenderResourceManager::FlushFEnqueDataGameThread(void)
{
	check(IsInGameThread());

	if ( !EnqueDataGameThread.empty() )
	{
		for (TFEnqueDataArray::iterator i=EnqueDataGameThread.begin(); i!=EnqueDataGameThread.end(); ++i)
		{
			FEnqueData *e = (*i);
			if ( e )
			{
				delete e;
			}
		}
		EnqueDataGameThread.clear();
	}
	if ( !EnqueDataReuse.empty() )
	{
		for (TFEnqueDataArray::iterator i=EnqueDataReuse.begin(); i!=EnqueDataReuse.end(); ++i)
		{
			FEnqueData *e = (*i);
			if ( e )
			{
				delete e;
			}
		}
		EnqueDataReuse.clear();
	}
}

void FApexRenderResourceManager::FlushFEnqueDataRenderingThread(void)
{
	check(IsInRenderingThread());
	for (TFEnqueDataArray::iterator i=EnqueDataRenderingThread.begin(); i!=EnqueDataRenderingThread.end(); ++i)
	{
		FEnqueData *e = (*i);
		if( e != NULL)
		{
			e->ClearSemaphore();
		}
	}
	EnqueDataRenderingThread.clear();
}

FApexRenderResourceManager::FApexRenderResourceManager(void)
{
	GApexRender = this;
	EnqueDataReuseCount = 0;
	GApexManager->SetRenderResourceManager(this);
}

FApexRenderResourceManager::~FApexRenderResourceManager(void)
{
	FlushFEnqueDataGameThread();
	GApexRender = NULL;
	GApexManager->SetRenderResourceManager(0);
}

#else // #if WITH_APEX


/** NULL implementation for non-APEX build */
class FPrimitiveSceneProxy *UApexStaticComponent::CreateSceneProxy()
{
	return NULL;
}


#endif // #if WITH_APEX
