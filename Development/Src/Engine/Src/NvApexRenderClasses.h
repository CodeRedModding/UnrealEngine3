/*=============================================================================
	NxApexRenderClasses.h : Declares the NxApexRendering classes.
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

#ifndef NV_APEX_RENDER_CLASSES_H
#define NV_APEX_RENDER_CLASSES_H

#include "NvApexManager.h"
#include "Engine.h"
#include "GPUSkinVertexFactory.h"
#include "LocalVertexFactoryShaderParms.h"

#if WITH_APEX

#include "NvApexRender.h"
#include <NxUserRenderResourceManager.h>
#include <NxUserRenderer.h>
#include <NxUserRenderBoneBuffer.h>
#include <NxUserRenderBoneBufferDesc.h>
#include <NxUserRenderIndexBuffer.h>
#include <NxUserRenderIndexBufferDesc.h>
#include <NxUserRenderInstanceBuffer.h>
#include <NxUserRenderInstanceBufferDesc.h>
#include <NxUserRenderResource.h>
#include <NxUserRenderResourceDesc.h>
#include <NxUserRenderSpriteBuffer.h>
#include <NxUserRenderSpriteBufferDesc.h>
#include <NxUserRenderVertexBuffer.h>
#include <NxUserRenderVertexBufferDesc.h>

using namespace physx::apex;

class FEnqueData;

typedef NxArray< FEnqueData * > TFEnqueDataArray;

/**
 *	Implementation of NxUserRenderResourceManager
 *
 *	APEX will use this implementation to create various buffers.
 */
class FApexRenderResourceManager : public NxUserRenderResourceManager, public FIApexRender
{
public:
	FApexRenderResourceManager(void);
	~FApexRenderResourceManager(void);

	/** NxUserRenderResourceManager methods */
	virtual NxUserRenderVertexBuffer*	createVertexBuffer( const physx::NxUserRenderVertexBufferDesc &Desc );
	virtual NxUserRenderIndexBuffer*	createIndexBuffer( const NxUserRenderIndexBufferDesc &Desc );
	virtual NxUserRenderBoneBuffer*		createBoneBuffer( const NxUserRenderBoneBufferDesc &Desc );
	virtual NxUserRenderInstanceBuffer*	createInstanceBuffer( const NxUserRenderInstanceBufferDesc &Desc );
	virtual NxUserRenderResource*		createResource( const physx::NxUserRenderResourceDesc &Desc );
	virtual NxUserRenderSpriteBuffer*   createSpriteBuffer( const NxUserRenderSpriteBufferDesc &Desc );

	virtual void						releaseVertexBuffer( NxUserRenderVertexBuffer &Buffer );
	virtual void						releaseIndexBuffer( NxUserRenderIndexBuffer &Buffer );
	virtual void						releaseBoneBuffer( NxUserRenderBoneBuffer &Buffer );
	virtual void						releaseInstanceBuffer( NxUserRenderInstanceBuffer &Buffer );
	virtual void						releaseResource( NxUserRenderResource &Resource );
	virtual void                        releaseSpriteBuffer( NxUserRenderSpriteBuffer &Buffer );

	/**
	Get the maximum number of bones supported by a given material. Return 0 for infinite.
	For optimal rendering, do not limit the bone count (return 0 from this function).
	*/
	virtual physx::PxU32                       getMaxBonesForMaterial(void *Material);

	/**
	Get the global APEX Render Resource Manager
	*/
	virtual NxUserRenderResourceManager	* GetRenderResourceManager(void)
	{
		return this;
	}
	/**
	Get the global APEX User Renderer
	*/
	virtual NxUserRenderer 				* GetNxUserRenderer(void)
	{
		return 0;
	}
	/**
	Create a dynamic APEX User Renderer
	*/
	virtual NxUserRenderer				* CreateDynamicRenderer(void *PDI, UBOOL Selected, UBOOL CastShadow, UBOOL WireFrame, const FSceneView *View, UINT DPGIndex);

	/**
	Release a dynamic APEX User Renderer
	*/
	virtual void						ReleaseDynamicRenderer(NxUserRenderer *Renderer);

	/***
	* The following methods handle the case where vertex buffers, index buffers, and other rendering resources need to be
	* created, destroyed, and/or written to from the game thread.  Since all of these requests must be processed on the rendering
	* thread; the requests are posted into two queues.  One to be managed by the game thread, and the other to be managed by the
	* rendering thread.
	*/

	/**
	* Posts a rendering request to be processed by the rendering thread
	*/
	virtual void PostFEnqueDataRenderingThread(FEnqueData *e);

	/***
	* Posts a rendering request to be processed by the game thread
	**/
	virtual void PostFEnqueDataGameThread(FEnqueData *e);

	/**
	* A callback from the rendering thread notifying use that a buffer has been destroyed.
	*/
	virtual void NotifyEnqueBufferGone(void *buffer);

	/**
	* Processes pending requests from the game thread
	*/
	virtual void ProcessFEnqueDataGameThread(void);

	/**
	* Processes pending requests from the rendering thread
	*/
	virtual void ProcessFEnqueDataRenderingThread(void);

	/***
	* Flushes all outstanding data items from the game thread
	*/
	virtual void FlushFEnqueDataGameThread(void);

	/***
	* Flushes all outstanding data items from the rendering thread
	*/
	virtual void FlushFEnqueDataRenderingThread(void);

	/***
	* Enque a create vertex buffer request
	*/
	virtual void Enque(FApexRenderVertexBuffer *buffer,const physx::NxUserRenderVertexBufferDesc &desc);

	/**
	* Enque a write vertex buffer request
	*/
	virtual void Enque(FApexRenderVertexBuffer *buffer,const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices);

	/**
	* Enque a create index buffer request
	*/
	virtual void Enque(FApexRenderIndexBuffer *buffer,const NxUserRenderIndexBufferDesc &desc);

	/***
	* Enque a write index buffer request
	*/
	virtual void Enque(FApexRenderIndexBuffer *buffer,const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements);

	/****
	* Enque a create instance buffer request
	*/
	virtual void Enque(FApexRenderInstanceBuffer *buffer,const NxUserRenderInstanceBufferDesc &Desc);

	/***
	* Enque a write instance buffer request
	*/
	virtual void Enque(FApexRenderInstanceBuffer *buffer,physx::NxRenderInstanceSemantic::Enum Semantic, const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements,physx::PxU32 *indexRemapping);

	/***
	* Enque a create render resource request
	*/
	virtual void Enque(FApexRenderResource *buffer,const physx::NxUserRenderResourceDesc &Desc);

private:
	/***
	* Holds the pending rendering requests that are to be processed by the rendering thread
	*/
	TFEnqueDataArray	EnqueDataRenderingThread;

	/***
	* Holds the pending rendering requests to be processed by the game thread.  A semaphore, which is set by the rendering thread
	* lets us know when it is safe to remove the requests from this queue
	*/
	TFEnqueDataArray	EnqueDataGameThread;

	/****
	* Recently released requests are hold in this queue to see if they can be immediately reused by follow up requests.
	* This prevents us from constantly allocating and deallocating render requests when write buffer calls are done every frame
	* to a particular vertex buffer or instance buffer.
	*/
	TFEnqueDataArray	EnqueDataReuse;

	/***
	* This counter is used to indicate the number of frames since an empty slot was made available in the re-use queue.
	* When this counter goes to zero we reorganize the reuse queue and get rid of the empty entries.
	*/
	physx::PxU32           EnqueDataReuseCount;
};


/** Resources used by a UApexComponentBase for rendering. */
class FApexBaseResources : public FDeferredCleanupInterface
{
public:

	/* Index buffer used by this component for rendering. */
	FRawIndexBuffer InstanceIndexBuffer;

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}
};

/** Resources used by a Apex dynamic components for rendering. */
class FApexDynamicResources : public FDeferredCleanupInterface
{
public:

	/* A FGPUSkinVertexFactory for MAX_GPUSKIN_BONES fragments of the skinned component. */
	TArray<FGPUSkinVertexFactory> ChunkVertexFactories;

	FApexDynamicResources(INT NumBones);

	/** FDeferredCleanupInterface */
	virtual void FinishCleanup()
	{
		delete this;
	}
};

/**
 * Base proxy class for apex rendering.
 */
class FApexBaseSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FApexBaseSceneProxy(const UApexComponentBase &Component);

	virtual ~FApexBaseSceneProxy() {}

	/** FPrimitiveSceneProxy interface. */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	virtual void OnTransformChanged();

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const;

	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); } // Not adding + LODs.GetAllocatedSize()
	
	virtual DWORD GetMemoryFootprint( void )       const { return sizeof(*this) + GetAllocatedSize(); }

protected:

	AActor                *Owner;

	FVector                TotalScale3D;

	const BITFIELD         bCastShadow         : 1;
	const BITFIELD         bShouldCollide      : 1;
	const BITFIELD         bBlockZeroExtent    : 1;
	const BITFIELD         bBlockNonZeroExtent : 1;
	const BITFIELD         bBlockRigidBody     : 1;
	const BITFIELD         bForceStaticDecal   : 1;
	
	BITFIELD               bApexUpdateRequired : 1;
	BITFIELD               bRewriteBuffersRequired : 1;

	/** The view relevance for all the static mesh's materials. */
	FMaterialViewRelevance MaterialViewRelevance;

	const FLinearColor     WireframeColor;
};

/**
 * Rendering thread mirror for UApexStaticComponent
 */
class FApexStaticSceneProxy : public FApexBaseSceneProxy 
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FApexStaticSceneProxy(const UApexStaticComponent &Component);

protected:
	// FPrimitiveSceneProxy interface.
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);
	
	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, DWORD Flags);

private:
	const UApexStaticComponent &	ApexComponent;
	physx::apex::NxApexRenderable *	ApexRenderable;
};

/**
 * Rendering thread mirror for UApexDynamicComponent
 */
class FApexDynamicSceneProxy : public FApexBaseSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FApexDynamicSceneProxy(const UApexDynamicComponent &Component);

	/**
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, DWORD Flags);

protected:

	/* The component's resources needed for skinning */
	const FApexDynamicResources* ComponentDynamicResources;
};
#define ENABLE_NULL_VB              0  // Sometimes APEX asks us to create a Vertex Buffer with no Semantic Formats.
#define APEX_MAX_BONES              75 // Maximum number of bones that can be fit into vertex shader constants... same max value must be in shader!
#define ENABLE_STATIC_RENDER        0  // Use DrawStaticElements instead of DrawDynamicElements.
#define ENABLE_DYNAMIC_BUFFERS      1  // Disabled, we need to make sure we don't discard when locking VBs or IBs
class FApexRenderResource;

/**
 *	FApexRenderVertexBuffer - implementation of NxUserRenderVertexBuffer
 *
 *	Defines how vertex buffers are represented
 */
class FApexRenderVertexBuffer : public NxUserRenderVertexBuffer, public FVertexBuffer
{
private:
	static EResourceUsageFlags GetUsageFlag(physx::NxRenderBufferHint::Enum Hint)
	{
		EResourceUsageFlags UsageHint = RUF_Static;
	#if ENABLE_DYNAMIC_BUFFERS
		switch(Hint)
		{
			case physx::NxRenderBufferHint::STATIC:    UsageHint = RUF_Static;   break;
			case physx::NxRenderBufferHint::DYNAMIC:   UsageHint = RUF_Dynamic;  break;
			case physx::NxRenderBufferHint::STREAMING: UsageHint = RUF_WriteOnly; break;
		}
	#endif
		return UsageHint;
	}

	static EVertexElementType getFormatType(physx::NxRenderDataFormat::Enum Format)
	{
		EVertexElementType UType = VET_MAX;
		switch(Format)
		{
		case physx::NxRenderDataFormat::UBYTE1:
		case physx::NxRenderDataFormat::UBYTE2:
		case physx::NxRenderDataFormat::UBYTE3:
		case physx::NxRenderDataFormat::UBYTE4:
		case physx::NxRenderDataFormat::BYTE_SNORM1:
		case physx::NxRenderDataFormat::BYTE_SNORM2:
		case physx::NxRenderDataFormat::BYTE_SNORM3:
		case physx::NxRenderDataFormat::BYTE_SNORM4:
		case physx::NxRenderDataFormat::BYTE_UNORM1:
		case physx::NxRenderDataFormat::BYTE_UNORM2:
		case physx::NxRenderDataFormat::BYTE_UNORM3:
		case physx::NxRenderDataFormat::BYTE_UNORM4:
		case physx::NxRenderDataFormat::R8G8B8A8:
			UType = VET_UByte4;
			break;

		case physx::NxRenderDataFormat::USHORT1:
		case physx::NxRenderDataFormat::USHORT2:
			UType = VET_Short2;
			break;
		case physx::NxRenderDataFormat::USHORT3:
		case physx::NxRenderDataFormat::USHORT4:
			UType = VET_UByte4;
			break;


		case physx::NxRenderDataFormat::HALF1:
		case physx::NxRenderDataFormat::HALF2:
			UType = VET_Half2;
			break;

		case physx::NxRenderDataFormat::FLOAT1: UType = VET_Float1; break;
		case physx::NxRenderDataFormat::FLOAT2: UType = VET_Float2; break;
		case physx::NxRenderDataFormat::FLOAT3: UType = VET_Float3; break;
		case physx::NxRenderDataFormat::FLOAT4: UType = VET_Float4; break;
		}
		check(UType < VET_MAX);
		return UType;
	}

	static UINT GetFormatByteSize(EVertexElementType UType)
	{
		UINT FormatSize = 0;
		switch(UType)
		{
		case VET_Float1:       return sizeof(FLOAT)*1; break;
		case VET_Float2:       return sizeof(FLOAT)*2; break;
		case VET_Float3:       return sizeof(FLOAT)*3; break;
		case VET_Float4:       return sizeof(FLOAT)*4; break;
		case VET_PackedNormal: return sizeof(BYTE)*4;  break;
		case VET_UByte4:       return sizeof(BYTE)*4;  break;
		case VET_Color:        return sizeof(BYTE)*4;  break;
		case VET_Short2:       return sizeof(WORD)*2;  break;
		case VET_Half2:        return sizeof(WORD)*2;  break;
		}
		check(FormatSize > 0);
		return FormatSize;
	}

	static EVertexElementUsage getSemanticUsage(physx::NxRenderVertexSemantic::Enum Semantic, UINT &UsageIndex)
	{
		EVertexElementUsage USemantic = (EVertexElementUsage)(~(UINT)0);
		UsageIndex = 0;
		switch(Semantic)
		{
		case physx::NxRenderVertexSemantic::POSITION:    USemantic = VEU_Position;              break;
		case physx::NxRenderVertexSemantic::NORMAL:      USemantic = VEU_Normal;                break;
		case physx::NxRenderVertexSemantic::TANGENT:     USemantic = VEU_Tangent;               break;
		case physx::NxRenderVertexSemantic::BINORMAL:    USemantic = VEU_Binormal;              break;
		case physx::NxRenderVertexSemantic::COLOR:       USemantic = VEU_Color; UsageIndex = 1; break;
		case physx::NxRenderVertexSemantic::BONE_INDEX:  USemantic = VEU_BlendIndices;          break;
		case physx::NxRenderVertexSemantic::BONE_WEIGHT: USemantic = VEU_BlendWeight;           break;

		case physx::NxRenderVertexSemantic::TEXCOORD0:
		case physx::NxRenderVertexSemantic::TEXCOORD1:
		case physx::NxRenderVertexSemantic::TEXCOORD2:
		case physx::NxRenderVertexSemantic::TEXCOORD3:
			USemantic  = VEU_TextureCoordinate;
			UsageIndex = Semantic - physx::NxRenderVertexSemantic::TEXCOORD0;
			break;
		}
		check(USemantic != ~(UINT)0);
		return USemantic;
	}

public:
	class FApexVertexElement
	{
	public:
		physx::NxRenderDataFormat::Enum Format;

		EVertexElementType         FormatType;
		UINT                       ApexFormatSize;
		EVertexElementUsage        Usage;
		UINT                       UsageIndex;
		UINT                       Offset;


	public:
		FApexVertexElement(void)
		{
			Format         = physx::NxRenderDataFormat::UNSPECIFIED;
			FormatType     = VET_MAX;
			ApexFormatSize = 0;
			Usage          = VEU_Position;
			UsageIndex     = 0;
			Offset         = 0;
		}

		UBOOL IsValid(void) const
		{
			UBOOL OK = TRUE;
			if(Format == physx::NxRenderDataFormat::UNSPECIFIED)
			{
				OK = FALSE;
			}
			if(FormatType == VET_MAX)
			{
				OK = FALSE;
			}
			if(ApexFormatSize == 0)
			{
				OK = FALSE;
			}
			return OK;
		}
	};

public:
	FApexRenderVertexBuffer(const physx::NxUserRenderVertexBufferDesc &Desc)
	{
		MaxVertices  = 0;
		BufferSize   = 0;
		BufferStride = 0;
		BlendIndexFormat = physx::NxRenderDataFormat::UNSPECIFIED;
		bHaveBlendWeight = FALSE;
		BufferUsage = RUF_Static;

		for(UINT i=0; i<physx::NxRenderVertexSemantic::NUM_SEMANTICS; i++)
		{
			physx::NxRenderVertexSemantic::Enum Semantic = (physx::NxRenderVertexSemantic::Enum)i;
			physx::NxRenderDataFormat::Enum   Format   = Desc.buffersRequest[i];

			if(Format != physx::NxRenderDataFormat::UNSPECIFIED)
			{
				UINT                UsageIndex = 0;
				EVertexElementUsage Usage      = getSemanticUsage(Semantic, UsageIndex);
				EVertexElementType  FormatType = getFormatType(Format);

				if( Usage == VEU_BlendIndices )
				{
					BlendIndexFormat = Format;
				}
				else if ( Usage == VEU_BlendWeight )
				{
					bHaveBlendWeight = TRUE;
				}
				else if((Usage == VEU_Normal || Usage == VEU_Tangent || Usage == VEU_Binormal) && (FormatType == VET_Float3 || FormatType == VET_Float4 || FormatType == VET_UByte4))
				{
					FormatType = VET_PackedNormal;
				}
				else if (Usage == VEU_Color)
				{
					FormatType = VET_Color;
				}

				UINT                FormatSize = GetFormatByteSize(FormatType);

				if(FormatSize>0 && Usage!=~(UINT)0 && FormatType<VET_MAX)
				{
					FApexRenderVertexBuffer::FApexVertexElement &AVE = ApexVertexElements[Semantic];

					AVE.Format         = Format;
					AVE.FormatType     = FormatType;
					AVE.ApexFormatSize = physx::NxRenderDataFormat::getFormatDataSize(Format);
					AVE.Usage          = Usage;
					AVE.UsageIndex     = UsageIndex;
					AVE.Offset         = BufferStride;

					BufferStride += FormatSize;
				}
			}
		}

		MaxVertices  = (UINT)Desc.maxVerts;
		BufferSize   = BufferStride * MaxVertices;

	    if ( IsInRenderingThread() ) // If we are in the rendering thread, then we immediately initialize the vertex buffer
	    {
	    	Init(Desc);
	    }
		else
		{
			GApexRender->Enque(this,Desc); // if we are in the game thread, we post this vertex buffer create call on the queue
		}
	}

	/***
	* Initializes the vertex buffer based on this descriptor.
	*/
	void Init(const physx::NxUserRenderVertexBufferDesc &Desc)
	{
		check( IsInRenderingThread());
		check(BufferSize > 0);
		if(BufferSize > 0)
		{
			BufferUsage = GetUsageFlag(Desc.hint);
			InitResource();
		}
	}

	virtual ~FApexRenderVertexBuffer(void)
	{
		check(IsInRenderingThread());
		GApexRender->NotifyEnqueBufferGone(this);
		ReleaseResource();
	}
	const FApexVertexElement *getApexVertexElements(void) const { return ApexVertexElements; }
	UINT                      getBufferStride(void) const       { return BufferStride; }

public:
	/*
		Main hook from Apex.  Updates all semantic data in the buffer.  Supports multithreaded by scheduling an update to
		be performed on the renderthread if necessary.
	*/
	virtual void writeBuffer(const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices)
	{
		check(GIsRHIInitialized);
		if ( IsInRenderingThread() ) // If we are in the rendering thread, then process this write buffer call.
		{
			BYTE *VBData = (BYTE*)RHILockVertexBuffer(VertexBufferRHI, 0, BufferSize, FALSE);
			check(VBData);

			//// Default implementation that maps to old API...
			for(physx::PxU32 i=0; i<NxRenderVertexSemantic::NUM_SEMANTICS; i++)
			{
				NxRenderVertexSemantic::Enum semantic = (NxRenderVertexSemantic::Enum)i;
				const NxApexRenderSemanticData &semanticData = data.getSemanticData(semantic);
				if(semanticData.data)
				{
					writeBufferSemantic(VBData,semantic, semanticData.data, semanticData.stride, semanticData.format, firstVertex, numVertices);
				}
			}

			if(VBData != NULL)
			{
				RHIUnlockVertexBuffer(VertexBufferRHI);
			}
		}
		else
		{
			GApexRender->Enque(this,data, firstVertex, numVertices);		
		}			
	}

	/*
		Update the target RHI Vertex Buffer with the data passed in
	*/
	void writeBuffer(BYTE *SrcVBData)
	{
		check(GIsRHIInitialized);
		check(IsInRenderingThread());
		BYTE *DstVBData = (BYTE*)RHILockVertexBuffer(VertexBufferRHI, 0, BufferSize, FALSE);
		check(DstVBData);

		memcpy(DstVBData,SrcVBData,BufferSize);

		if(DstVBData != NULL)
		{
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}


	}

	UBOOL HasBlendWeight(void) const
	{
		return bHaveBlendWeight;
	}

	/*
		Update a single semantic with data into the BYTE buffer parameter using proper stride.
		Assumed the VBData has been allocated to hold all semantics
	*/
	void writeBufferSemantic(		 BYTE *VBData,
							 physx::NxRenderVertexSemantic::Enum Semantic,
							 const void *SrcData,
							 physx::PxU32 SrcStride,
							 physx::NxRenderDataFormat::Enum SrcFormat,
							 physx::PxU32 FirstDestElement,
							 physx::PxU32 NumElements)
	{
		check(GIsRHIInitialized);
		check(FirstDestElement + NumElements <= MaxVertices);
		const FApexVertexElement &AVE = ApexVertexElements[Semantic];
		if(AVE.IsValid())
		{
			check(VBData);
			if(VBData)
			{
				if(AVE.FormatType == VET_PackedNormal)
				{
					if( Semantic == physx::NxRenderVertexSemantic::NORMAL )
					{
						CopyNormals(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, SrcFormat, NumElements);
					}
					else
					{
						CopyTangents(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, SrcFormat, NumElements);

						if (Semantic == physx::NxRenderVertexSemantic::TANGENT)
						{
							// we need to calculate the determinant of the tangent basis. the sign of the result will go into normal.w.
							// this is because the binormal (bitangent) is essentially ignored by UE3, and recalculated in a shader. the determinant
							//  is needed for correct bitangent recalculation. a further improvement would be to ignore when APEX tells us to pack
							//  binormals into the VB, since they aren't used anyway. todo. 

							// given the order of the NxRenderVertexSemantic enum and the loop in the caller(s), we can be sure that by the time we handle BINORMAL,
							//  we've already copied the normal & tangents. still feels kinda hacky though. swapping the loops would facilitate a more elegant
							//  solution, and might make the whole transaction more cache friendly? 
							const FApexVertexElement &AVE_Norm = ApexVertexElements[physx::NxRenderVertexSemantic::NORMAL];
							const FApexVertexElement &AVE_Tan = ApexVertexElements[physx::NxRenderVertexSemantic::TANGENT];

							for (UINT i = 0; i < NumElements; ++i)
							{
								FPackedNormal& PackedNormal = *(FPackedNormal*)(VBData + AVE_Norm.Offset);
								FPackedNormal& PackedTangent = *(FPackedNormal*)(VBData + AVE_Tan.Offset);
								PackedNormal.Vector.W = PackedTangent.Vector.W;

								VBData = VBData + BufferStride;
							}
						}
					}
				}
				else if(Semantic == physx::NxRenderVertexSemantic::POSITION)
				{
					FVector *vecData = (FVector*)SrcData;
					CopyPositions(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, NumElements);
				}
				else if(Semantic == physx::NxRenderVertexSemantic::TEXCOORD0 || Semantic == physx::NxRenderVertexSemantic::TEXCOORD1
					|| Semantic == physx::NxRenderVertexSemantic::TEXCOORD2 || Semantic == physx::NxRenderVertexSemantic::TEXCOORD3)
				{
					CopyTexels(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, NumElements);
				}
				else if( Semantic == physx::NxRenderVertexSemantic::BONE_INDEX )
				{
    				VBData = VBData + (BufferStride*FirstDestElement) + AVE.Offset;
    				BYTE *VBBegin = VBData;
					// Take care of all of the permutations of copying the bone indices from the
					// source to the dest vertexbuffer.
					switch ( BlendIndexFormat )
					{
					case physx::NxRenderDataFormat::USHORT1:
						for(UINT i=0; i<NumElements; i++)
						{
							const NxU16 *Src = (const NxU16 *)SrcData;
							NxU16 *Dest = (NxU16 *) VBData;
							Dest[0] = Src[0];
							Dest[1] = 0;
							VBData = VBData + BufferStride;
							SrcData = ((BYTE*)SrcData) + SrcStride;
						}

						break;
					case physx::NxRenderDataFormat::USHORT2:
						for(UINT i=0; i<NumElements; i++)
						{
							appMemcpy(VBData, SrcData, AVE.ApexFormatSize);
							VBData = VBData + BufferStride;
							SrcData = ((BYTE*)SrcData) + SrcStride;
						}
						break;
					case physx::NxRenderDataFormat::USHORT3:
						for(UINT i=0; i<NumElements; i++)
						{
							const NxU16 *Src = (const NxU16 *)SrcData;
							NxU8 *Dest = (NxU8 *) VBData;
							Dest[0] = (NxU8)Src[0];
							Dest[1] = (NxU8)Src[1];
							Dest[2] = (NxU8)Src[2];
							Dest[3] = 0;
							VBData = VBData + BufferStride;
							SrcData = ((BYTE*)SrcData) + SrcStride;
						}
						break;
					case physx::NxRenderDataFormat::USHORT4:
						for(UINT i=0; i<NumElements; i++)
						{
							const NxU16 *Src = (const NxU16 *)SrcData;
							NxU8 *Dest = (NxU8 *) VBData;
							Dest[0] = (NxU8)Src[0];
							Dest[1] = (NxU8)Src[1];
							Dest[2] = (NxU8)Src[2];
							Dest[3] = (NxU8)Src[3];
							VBData = VBData + BufferStride;
							SrcData = ((BYTE*)SrcData) + SrcStride;
						}
						break;

					default:
						appErrorf( TEXT("Bad NxRenderVertexFormat") );
					}
				}
				else if(Semantic == physx::NxRenderVertexSemantic::COLOR)
				{
					CopyColor(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, NumElements);
				}
				else
				{
					CopyGeneric(VBData+AVE.Offset, BufferStride, SrcData, SrcStride, AVE.ApexFormatSize, NumElements);
				}
			}
			//RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}
 	}

	UINT GetBufferSize() {return BufferSize;}
	virtual FString GetFriendlyName() const { return TEXT("FApexVertexBuffer"); }

	virtual void InitDynamicRHI() 
	{
		if(!(BufferUsage & RUF_Dynamic) || BufferSize <= 0)
		{
			return;
		}

		VertexBufferRHI = RHICreateVertexBuffer(BufferSize, 0, RUF_Dynamic);
	}

	virtual void ReleaseDynamicRHI() 
	{
		if( (BufferUsage & RUF_Dynamic) && BufferSize > 0)
		{
			VertexBufferRHI.SafeRelease();
		}
	}

	virtual void InitRHI() 
	{
		if(!(BufferUsage & RUF_Static) || BufferSize <= 0)
		{
			return;
		}
		VertexBufferRHI = RHICreateVertexBuffer(BufferSize, 0, RUF_Static);
	}

	virtual void ReleaseRHI() 
	{
		if( (BufferUsage & RUF_Static) && BufferSize > 0 )
		{
			VertexBufferRHI.SafeRelease();
		}
	}
private:

	void CopyGeneric(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, UINT ElementSize, UINT Num)
	{
		while(Num--)
		{
			memcpy(Dest, Src, ElementSize); // use XMemCpyStreaming on 360? Probably not a good idea for small sets of data...
			Dest = ((BYTE*)Dest) + DestStride;
			Src  = ((BYTE*)Src)  + SrcStride;
		}
	}

	void CopyPositions(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, UINT Num)
	{
		while(Num--)
		{
			(*(FVector*)Dest) = (*(FVector*)Src) * P2UScale;
			Dest = ((BYTE*)Dest) + DestStride;
			Src  = ((BYTE*)Src)  + SrcStride;
		}
	}
	void CopyColor(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, UINT Num)
	{
		while(Num--)
		{
			const FColor &SrcColor = *(FColor*)Src;
			FColor &DestColor = *(FColor*)Dest;
			// Swizzle Color Components
			DestColor.R = SrcColor.B;
			DestColor.G = SrcColor.G;
			DestColor.B = SrcColor.R;
			DestColor.A = SrcColor.A;
			Dest = ((BYTE*)Dest) + DestStride;
			Src  = ((BYTE*)Src)  + SrcStride;
		}
	}
	void CopyTexels(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, UINT Num)
	{
		PX_ASSERT( SrcStride == sizeof(physx::PxF32)*2 );
		while(Num--)
		{
			physx::PxF32 *dest = (physx::PxF32 *)Dest;
			const physx::PxF32 *src = (const physx::PxF32 *)Src;
			dest[0] = src[0];
			dest[1] = src[1];
			Dest = ((BYTE*)Dest) + DestStride;
			Src  = ((BYTE*)Src)  + SrcStride;
		}
	}

	void CopyNormals(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, physx::NxRenderDataFormat::Enum SrcFormat, UINT Num)
	{
		switch(SrcFormat)
		{
		case physx::NxRenderDataFormat::FLOAT3:
		case physx::NxRenderDataFormat::FLOAT4:
			while(Num--)
			{
				FVector SrcV = *(FVector*)Src;
				SrcV.Normalize();
				FPackedNormal &PackedNormal = *(FPackedNormal*)Dest;
				PackedNormal.Set(FVector(SrcV[0]*GApexNormalSign, SrcV[1]*GApexNormalSign, SrcV[2]*GApexNormalSign));
				PackedNormal.Vector.W = 255;
				Dest = ((BYTE*)Dest) + DestStride;
				Src  = ((BYTE*)Src)  + SrcStride;
			}
			break;
		case physx::NxRenderDataFormat::BYTE_SNORM3:
		case physx::NxRenderDataFormat::BYTE_SNORM4:
			{
				const FLOAT BYTE_SNORM_Scale = GApexNormalSign/127.0f;
				while(Num--)
				{
					const BYTE* SrcV = (const BYTE*)Src;
					FPackedNormal &PackedNormal = *(FPackedNormal*)Dest;
					PackedNormal.Set(FVector(SrcV[0]*BYTE_SNORM_Scale-GApexNormalSign, SrcV[1]*BYTE_SNORM_Scale-GApexNormalSign, SrcV[2]*BYTE_SNORM_Scale-GApexNormalSign));
					PackedNormal.Vector.W = 255;
					Dest = ((BYTE*)Dest) + DestStride;
					Src  = ((BYTE*)Src)  + SrcStride;
				}
			}
			break;
		default:
			debugf(TEXT("ERROR: Unhandled normal format in APEX render mesh."));
			check(0);
		}
	}

	void CopyTangents(void *RESTRICT Dest, UINT DestStride, const void *RESTRICT Src, UINT SrcStride, physx::NxRenderDataFormat::Enum SrcFormat, UINT Num)
	{
		switch(SrcFormat)
		{
		case physx::NxRenderDataFormat::FLOAT3:
			while(Num--)
			{
				const FVector &SrcV = *(FVector*)Src;
				FPackedNormal &PackedNormal = *(FPackedNormal*)Dest;
				PackedNormal	= FVector4(SrcV[0]*GApexNormalSign, SrcV[1]*GApexNormalSign, SrcV[2]*GApexNormalSign, GApexNormalSign);
				Dest = ((BYTE*)Dest) + DestStride;
				Src  = ((BYTE*)Src)  + SrcStride;
			}
			break;
		case physx::NxRenderDataFormat::FLOAT4:
			while(Num--)
			{
				FVector SrcV = *(FVector*)Src;
				FLOAT*	SrcF = (FLOAT*)Src;
				SrcV.Normalize();
				FPackedNormal &PackedNormal = *(FPackedNormal*)Dest;
				PackedNormal	= FVector4(SrcV[0]*GApexNormalSign, SrcV[1]*GApexNormalSign, SrcV[2]*GApexNormalSign, SrcF[3]*GApexNormalSign);
				Dest = ((BYTE*)Dest) + DestStride;
				Src  = ((BYTE*)Src)  + SrcStride;
			}
			break;
		case physx::NxRenderDataFormat::BYTE_SNORM3:
		case physx::NxRenderDataFormat::BYTE_SNORM4:
			{
				const FLOAT BYTE_SNORM_Scale = GApexNormalSign/127.0f;
				while(Num--)
				{
					const BYTE* SrcV = (const BYTE*)Src;
					FPackedNormal &PackedNormal = *(FPackedNormal*)Dest;
					PackedNormal.Set(FVector(SrcV[0]*BYTE_SNORM_Scale-GApexNormalSign, SrcV[1]*BYTE_SNORM_Scale-GApexNormalSign, SrcV[2]*BYTE_SNORM_Scale-GApexNormalSign));
					Dest = ((BYTE*)Dest) + DestStride;
					Src  = ((BYTE*)Src)  + SrcStride;
				}
			}
			break;
		default:
			debugf(TEXT("ERROR: Unhandled tangent format in APEX render mesh."));
			check(0);
		}
	}

	UBOOL						bHaveBlendWeight;

	UINT						MaxVertices;
	UINT						BufferSize;	
	UINT						BufferStride;

	FApexVertexElement			ApexVertexElements[physx::NxRenderVertexSemantic::NUM_SEMANTICS];

	physx::NxRenderDataFormat::Enum	BlendIndexFormat;
	EResourceUsageFlags BufferUsage;
};


/**
 * FApexRenderIndexBuffer - implementation of NxUserRenderIndexBuffer
 *
 * Defines how index buffers are represented
 */
class FApexRenderIndexBuffer : public NxUserRenderIndexBuffer, public FIndexBuffer
{
private:
	static EResourceUsageFlags GetUsageFlag(physx::NxRenderBufferHint::Enum hint)
	{
		EResourceUsageFlags UsageHint = RUF_Static;
	#if ENABLE_DYNAMIC_BUFFERS
		switch(hint)
		{
			case physx::NxRenderBufferHint::STATIC:    UsageHint = RUF_Static;   break;
			case physx::NxRenderBufferHint::DYNAMIC:   UsageHint = RUF_Dynamic;  break;
			case physx::NxRenderBufferHint::STREAMING: UsageHint = RUF_WriteOnly; break;
		}
	#endif
		return UsageHint;
	}

	static UINT GetFormatStride(physx::NxRenderDataFormat::Enum Format)
	{
		UINT Stride = 0;
		switch(Format)
		{
			case physx::NxRenderDataFormat::USHORT1: Stride = sizeof(WORD); break;
			case physx::NxRenderDataFormat::UINT1:   Stride = sizeof(UINT); break;
		}
		check(Stride > 0);
		return Stride;
	}
	
public:
	FApexRenderIndexBuffer(const NxUserRenderIndexBufferDesc &Desc)
	{
		if ( IsInRenderingThread() ) // If we are in the rendering thread, then create this index buffer immediatly
		{
			Init(Desc);
		}
		else
		{
			GApexRender->Enque(this,Desc); // if we are in the game thread, then queue this create index buffer call
		}
	}

	void Init(const NxUserRenderIndexBufferDesc &Desc)
	{
		check( IsInRenderingThread());
		MaxIndices   = 0;
		BufferSize   = 0;
		BufferStride = 0;

		BufferStride					= GetFormatStride(Desc.format);

		MaxIndices = (UINT)Desc.maxIndices;
		BufferSize = MaxIndices * BufferStride;

		check(BufferSize > 0);
		if(BufferSize > 0)
		{
			BufferUsage = GetUsageFlag(Desc.hint);
			InitResource();
		}
	}

	virtual ~FApexRenderIndexBuffer(void)
	{
		GApexRender->NotifyEnqueBufferGone(this);
		ReleaseResource();
	}

	virtual FString GetFriendlyName() const { return TEXT("FApexIndexBuffer"); }

	virtual void InitDynamicRHI() 
	{
		if(!(BufferUsage & RUF_Dynamic) || BufferSize <= 0)
		{
			return;
		}

		IndexBufferRHI = RHICreateIndexBuffer(BufferStride, BufferSize, 0, RUF_Dynamic);
	}

	virtual void ReleaseDynamicRHI() 
	{
		if( (BufferUsage & RUF_Dynamic) && BufferSize > 0)
		{
			IndexBufferRHI.SafeRelease();
		}
	}

	virtual void InitRHI() 
	{
		if(!(BufferUsage & RUF_Static) || BufferSize <= 0)
		{
			return;
		}
		IndexBufferRHI = RHICreateIndexBuffer(BufferStride, BufferSize, 0, RUF_Static);
	}

	virtual void ReleaseRHI() 
	{
		if( (BufferUsage & RUF_Static) && BufferSize > 0 )
		{
			IndexBufferRHI.SafeRelease();
		}
	}

public:



	virtual void writeBuffer(const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements)
	{
		check(GIsRHIInitialized);
		if ( IsInRenderingThread() ) // if we are in the rendering thread, then process this write buffer call immediately
		{
			check(FirstDestElement + NumElements <= MaxIndices);
    		UINT Offset   = FirstDestElement * BufferStride;
    		UINT LockSize = NumElements      * BufferStride;
    		check(Offset+LockSize<=BufferSize);
    		BYTE *IB = (BYTE*)RHILockIndexBuffer(IndexBufferRHI, Offset, LockSize);
    		if(IB)
    		{
    			for(UINT i=0; i<NumElements; i++)
    			{
    				appMemcpy(IB, SrcData, BufferStride);
    				IB     += BufferStride;
    				SrcData = ((BYTE*)SrcData) + SrcStride;
    			}
    		}
    		RHIUnlockIndexBuffer(IndexBufferRHI);
    	}
    	else
    	{
    		// if we are in the game thread, then queue this write buffer call
    		GApexRender->Enque(this,SrcData,SrcStride,FirstDestElement,NumElements);
    	}
	}

private:
	UINT                MaxIndices;
	UINT                BufferSize;
	UINT                BufferStride;
	EResourceUsageFlags BufferUsage;
};


/**
 * FApexRenderBoneBuffer - implementation of NxUserRenderBoneBuffer
 *
 * Defines how bone buffers are represented
 */
class FApexRenderBoneBuffer : public NxUserRenderBoneBuffer
{
public:
	FApexRenderBoneBuffer(const NxUserRenderBoneBufferDesc &Desc)
	{
		Bones                = 0;
		MaxBones             = (UINT)Desc.maxBones;
		check(MaxBones > 0);
		if(MaxBones > 0)
		{
			if(Desc.buffersRequest[NxRenderBoneSemantic::POSE] == physx::NxRenderDataFormat::FLOAT3x4)
			{
				Bones = (FSkinMatrix3x4 *)appMalloc(sizeof(FSkinMatrix3x4)*MaxBones);
				for(UINT i=0; i<MaxBones; i++)
				{
					Bones[i].SetMatrix(FMatrix::Identity);
				}
			}
		}
	}

	virtual ~FApexRenderBoneBuffer(void)
	{
		if(Bones)
		{
			appFree(Bones);
		}
	}

	const FSkinMatrix3x4 *GetBones(void)    const { return Bones;    }
	UINT                  GetMaxBones(void) const { return MaxBones; }

public:
	virtual void writeBuffer(const NxApexRenderBoneBufferData &data, physx::PxU32 firstBone, physx::PxU32 numBones)
	{
		// Default implementation that maps to old API...
		for(physx::PxU32 i=0; i<NxRenderBoneSemantic::NUM_SEMANTICS; i++)
		{
			NxRenderBoneSemantic::Enum semantic = (NxRenderBoneSemantic::Enum)i;
			const NxApexRenderSemanticData &semanticData = data.getSemanticData(semantic);
			if(semanticData.data)
			{
				writeBuffer(semantic, semanticData.data, semanticData.stride, firstBone, numBones);
			}
		}
	}

	virtual void writeBuffer(NxRenderBoneSemantic::Enum Semantic, const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements)
	{
		check(FirstDestElement + NumElements <= MaxBones);
		if(Semantic == NxRenderBoneSemantic::POSE && Bones)
		{
			FSkinMatrix3x4 *BonesBegin = Bones + FirstDestElement;
			for(UINT i=0; i<NumElements; i++)
			{
				BonesBegin[i].SetMatrixTranspose(N2UTransformApex(*(physx::PxMat34Legacy*)SrcData));
				SrcData = ((BYTE*)SrcData)+SrcStride;
			}
		}
	}

private:
	FSkinMatrix3x4 *Bones;
	UINT            MaxBones;
};


/**
 * FApexRenderInstanceBuffer - implementation of NxUserRenderInstanceBuffer
 *
 * Defines how instance buffers are represented
 */
class FApexRenderInstanceBuffer : public NxUserRenderInstanceBuffer
{
public:
	FApexRenderInstanceBuffer(const NxUserRenderInstanceBufferDesc & /*Desc*/)
	{
	}

	virtual void writeBuffer(const NxApexRenderInstanceBufferData &data, physx::PxU32 firstInstance, physx::PxU32 numInstances)
	{
		// Default implementation that maps to old API...
		for(physx::PxU32 i=0; i<NxRenderInstanceSemantic::NUM_SEMANTICS; i++)
		{
			NxRenderInstanceSemantic::Enum semantic = (NxRenderInstanceSemantic::Enum)i;
			const NxApexRenderSemanticData &semanticData = data.getSemanticData(semantic);
			if(semanticData.data)
			{
				writeBuffer(semantic, semanticData.data, semanticData.stride, firstInstance, numInstances);
			}
		}
	}

	void writeBuffer( physx::NxRenderInstanceSemantic::Enum Semantic, const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements)
	{
		if ( (INT)NumElements > Transforms.Num() )
		{
			INT ocount = Transforms.Num();
			Transforms.Add((INT)NumElements-Transforms.Num());
			for (INT i=ocount; i<Transforms.Num(); i++)
			{
				Transforms(i).SetIdentity();
//				FVector v;
//				v.X = (float)(rand()%100);
//				v.Y = (float)(rand()%100);
//				v.Z = 0;
//				Transforms(i).SetOrigin(v);
			}
		}

#if 0
		for (INT i=0; i<Transforms.Num(); i++)
		{
			FVector v = Transforms(i).GetOrigin();
			v.Z+=0.01f;
			Transforms(i).SetOrigin(v);			
		}
#else
		for(UINT i=0; i<NumElements; i++)
		{
			if(Semantic == physx::NxRenderInstanceSemantic::ROTATION_SCALE)
			{
				N2UTransformApexRotationOnly(*(const physx::PxMat33*)SrcData,Transforms(i));
			}
			else if(Semantic == physx::NxRenderInstanceSemantic::POSITION)
			{
				const physx::PxVec3 *position = (const physx::PxVec3 *)SrcData;
				//Transforms(i).SetOrigin( N2UPositionApex(*position) );
				FVector p(position->x,position->y,position->z);
				Transforms(i).SetOrigin( p );
			}
			SrcData = ((BYTE*)SrcData)+SrcStride;
    	}
#endif
	}

	INT GetNumTransforms(void) const { return Transforms.Num(); };
	const FMatrix & GetTransform(INT i) const { return Transforms(i); };

private:
	TArray< FMatrix >		Transforms;
};


#endif // end of with apex

/**
 * FLocalVertexFactoryApex
 *
 * FLocalVertexFactoryApex used by APEX renderer
 */
class FLocalVertexFactoryApex : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLocalVertexFactoryApex);

public:
	
#if WITH_APEX

	FLocalVertexFactoryApex(const FApexRenderResource &InApexRenderResource) :
		ApexRenderResource(InApexRenderResource)
	{

	}
#endif

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency)
	{
		return ShaderFrequency == SF_Vertex ? new FLocalVertexFactoryShaderParameters() : NULL;
	}

	/* Called during update to reset the information.  Do nothing because caller will assume needs to call parameterized InitRHI */
	void InitRHI()
	{
	}
#if WITH_APEX
	virtual FVertexElement GetAccessStreamComponent(const physx::NxRenderVertexSemantic::Enum Semantic, BYTE Usage, BYTE UsageIndex)
	{
		return AccessStreamComponent(Data.VertexComponents[Semantic], Usage, UsageIndex);
	}
	virtual void InitRHIWithAPEXBuffers(const FApexRenderVertexBuffer *const*VertexBuffers,
										UINT NumVertexBuffers,
										const FApexRenderInstanceBuffer *InstanceBuffer)
	{
		FVertexDeclarationElementList Elements;
		FVertexDeclarationElementList PositionOnlyStreamElements;
		UBOOL bColor1Found = FALSE;
		for(UINT i=0; i<NumVertexBuffers; i++)
		{
			const FApexRenderVertexBuffer                     &ApexVertexBuffer   = *VertexBuffers[i];
			const FApexRenderVertexBuffer::FApexVertexElement *ApexVertexElements = ApexVertexBuffer.getApexVertexElements();
			for(UINT j=0; j<physx::NxRenderVertexSemantic::NUM_SEMANTICS; j++)
			{
				physx::NxRenderVertexSemantic::Enum Semantic = (physx::NxRenderVertexSemantic::Enum)j;
				const FApexRenderVertexBuffer::FApexVertexElement &AVE = ApexVertexElements[Semantic];
				if(AVE.IsValid())
				{
					Data.VertexComponents[Semantic] = FVertexStreamComponent(&ApexVertexBuffer, AVE.Offset, ApexVertexBuffer.getBufferStride(), AVE.FormatType, FALSE);
					Elements.AddItem(GetAccessStreamComponent(Semantic, AVE.Usage, AVE.UsageIndex));
					if ((AVE.Usage == VEU_Color) && (AVE.UsageIndex == 1))
					{
						bColor1Found = TRUE;
					}
				}
			}
		}

		if(!bColor1Found)
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
			Elements.AddItem(AccessStreamComponent(NullColorComponent,VEU_Color,1));
		}

		// Only D3D11 has the issue with strict shader decl to vertex decl
		if(GRHIShaderPlatform == SP_PCD3D_SM5 || GRHIShaderPlatform == SP_PCD3D_SM4)
		{
			// Add in any semantics that we know we need but may not exist in the vertex streams
			AddMissingSemanticElements(Elements);
		}

		InitDeclaration(Elements, Data);
	}
#endif
	void SetNumVertices(UINT NumVertices)
	{
		Data.NumVerticesPerInstance = NumVertices;
	}

	void SetNumInstances(UINT NumInstances)
	{
		Data.NumInstances = NumInstances;
	}
#if WITH_APEX
	const FApexRenderResource &getApexRenderResource(void) const { return ApexRenderResource; }
#endif

public:
	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial *Material, const class FShaderType *ShaderType)
	{
		return Platform != SP_WIIU && (Material->IsUsedWithAPEXMeshes() || Material->IsSpecialEngineMaterial());
	}

private:
	struct DataType : public FVertexFactory::DataType
	{
#if WITH_APEX
		FVertexStreamComponent VertexComponents[physx::NxRenderVertexSemantic::NUM_SEMANTICS];
#endif
	};

private:
#if WITH_APEX
	const FApexRenderResource& ApexRenderResource;
#endif
protected:
	DataType                   Data;
#if WITH_APEX

	virtual void AddMissingSemanticElements(FVertexDeclarationElementList &Elements)
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		EVertexElementUsage RequiredSemantics[] = {VEU_Color};
		UINT RequiredSemanticIndices[] = {1};	// NOTE This corresponds to the RequiredSemantics array
		UINT NumRequiredSemantics = (UINT)(sizeof(RequiredSemantics)/sizeof(EVertexElementUsage));

		// verify the above listed elements don't exist in the stream already, and if not add dummy data
		// NOTE, this is virtual and called down to subclasses
		UtilAddMissingSemanticElements(Elements,RequiredSemantics,RequiredSemanticIndices,NumRequiredSemantics);
	}

	/*
		If the mesh is a missing a required semantic (manually updated on demand based on shader code declarations), set a null buffer on a new stream with a stride of 0.
		This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		It is required only for D3D10/11 where the runtime enforces the bound streams match the shader declaration regardless of if those semantics are actually used.

		TODO rework shaders so that unused semantics are compiled out and then remove this hackery.
	*/
	void UtilAddMissingSemanticElements(FVertexDeclarationElementList &Elements,EVertexElementUsage *RequiredSemantics,UINT *RequiredSemanticIndices, UINT NumRequiredSemantics)
	{
		for(UINT iRequiredElement = 0; iRequiredElement<NumRequiredSemantics; iRequiredElement++)
		{
			UBOOL WasSemanticFound = FALSE;
			for(UINT iElement=0; iElement<Elements.Num(); iElement++)
			{
				if(Elements(iElement).Usage == RequiredSemantics[iRequiredElement] && Elements(iElement).UsageIndex == RequiredSemanticIndices[iRequiredElement])
				{
					WasSemanticFound = TRUE;
					break;
				}
			}

			if(!WasSemanticFound)
			{
				EVertexElementType VertexType = VET_Color;
				if(RequiredSemantics[iRequiredElement] == VEU_Tangent)
				{
					VertexType = VET_PackedNormal;
				}
				// Just stick in the common global null color as it doesn't matter what we actually put in there.
				FVertexStreamComponent NullSemanticComponent(&GNullColorVertexBuffer, 0, 0, VertexType);
				Elements.AddItem(AccessStreamComponent(NullSemanticComponent,RequiredSemantics[iRequiredElement],RequiredSemanticIndices[iRequiredElement]));
			}				

		}
	}


#endif
};


/**
 * FGPUSkinVertexFactoryApexDestructible - used by APEX destructible module which may take advantage of GPU skinning
 */
class FGPUSkinVertexFactoryApexDestructible : public FLocalVertexFactoryApex
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactoryApexDestructible);

public:
#if WITH_APEX
	FGPUSkinVertexFactoryApexDestructible(const FApexRenderResource &ApexRenderResource) :
		FLocalVertexFactoryApex(ApexRenderResource)
	{

	}

	virtual void AddMissingSemanticElements(FVertexDeclarationElementList &Elements)
	{
		// Base class has a list, first enforce that list
		FLocalVertexFactoryApex::AddMissingSemanticElements(Elements);

		// Add our own in.  Sometimes Destructibles don't have tangents or colors....
		EVertexElementUsage RequiredSemantics[] = {VEU_Tangent,VEU_Color};
		UINT RequiredSemanticIndices[] = {0,0};	// NOTE This corresponds to the RequireElements array
		UINT NumRequiredSemantics = (UINT)(sizeof(RequiredSemantics)/sizeof(EVertexElementUsage));

		// verify the above listed elements don't exist in the stream already, and if not add dummy data
		// NOTE, this is virtual and called down to subclasses
		UtilAddMissingSemanticElements(Elements,RequiredSemantics,RequiredSemanticIndices,NumRequiredSemantics);
	}
	virtual FVertexElement GetAccessStreamComponent(const physx::NxRenderVertexSemantic::Enum Semantic, BYTE Usage, BYTE UsageIndex)
	{
		// For GPUSkinVertexFactory the Color Vertex Buffer has UsageIndex 0
		if(Usage == VEU_Color && UsageIndex == 1)
		{
			UsageIndex = 0;
		}
		return AccessStreamComponent(Data.VertexComponents[Semantic], Usage, UsageIndex);
	}
#endif

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	virtual UBOOL IsGPUSkinned(void) const { return TRUE; }

	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial *Material, const class FShaderType *ShaderType)
	{
		return Platform != SP_WIIU && (Material->IsUsedWithAPEXMeshes() || Material->IsSpecialEngineMaterial());
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("GPUSKIN_FACTORY"),TEXT("1"));
	}
};

/**
 * FGPUSkinVertexFactoryApexClothing - used by APEX clothing module which may take advantage of GPU skinning
 */
class FGPUSkinVertexFactoryApexClothing : public FLocalVertexFactoryApex
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactoryApexClothing);

public:
#if WITH_APEX
	FGPUSkinVertexFactoryApexClothing(const FApexRenderResource &ApexRenderResource) :
		FLocalVertexFactoryApex(ApexRenderResource)
	{

	}
	virtual FVertexElement GetAccessStreamComponent(const physx::NxRenderVertexSemantic::Enum Semantic, BYTE Usage, BYTE UsageIndex)
	{
		// For GPUSkinVertexFactory the Color Vertex Buffer has UsageIndex 0
		if(Usage == VEU_Color && UsageIndex == 1)
		{
			UsageIndex = 0;
		}
		return AccessStreamComponent(Data.VertexComponents[Semantic], Usage, UsageIndex);
	}
#endif

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	virtual UBOOL IsGPUSkinned(void) const { return TRUE; }

	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial *Material, const class FShaderType *ShaderType)
	{
		return Platform != SP_WIIU && (Material->IsUsedWithAPEXMeshes() || Material->IsSpecialEngineMaterial());
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("GPUSKIN_FACTORY"),TEXT("1"));
	}
};


/**
 * FApexLightCache
 *
 * Implementation of FLightCacheInterface for APEX
 */
class FApexLightCache : public FLightCacheInterface
{
public:
	virtual FLightInteraction GetInteraction(const class FLightSceneInfo *LightSceneInfo) const
	{
		return FLightInteraction::Uncached();
	}

	virtual FLightMapInteraction GetLightMapInteraction(void) const
	{
		return FLightMapInteraction::None();
	}
};


/**
 * FApexRenderResource - implementation of NxUserRenderResource
 *
 * Represents APEX render data
 */
#if WITH_APEX
class FApexRenderResource : public NxUserRenderResource
{
private:
	UINT GetNumPrimitives(void)
	{
		check( NumIndices%3 == 0 );
		UINT NumPrimitives = NumIndices / 3; // This assumes triangles!
		return NumPrimitives;
	}
#if WITH_APEX
	static EPrimitiveType ConvertPrimitiveType(NxRenderPrimitiveType::Enum Primitive)
	{
		EPrimitiveType PrimitiveType = PT_TriangleList; // This assume triangles!
		return PrimitiveType;
	}
#endif

public:
	FApexRenderResource(const physx::NxUserRenderResourceDesc &Desc)
	{
		VertexBuffers    = NULL;
		NumVertexBuffers = NULL;
		FirstVertex      = 0;
		NumVertices      = 0;

		IndexBuffer      = NULL;
		FirstIndex       = 0;
		NumIndices       = 0;

		BoneBuffer       = NULL;
		FirstBone        = 0;
		NumBones         = 0;

		InstanceBuffer   = NULL;
		FirstInstance    = 0;
		NumInstances     = 1;

		PrimitiveType    = ConvertPrimitiveType(Desc.primitives);
		CullMode         = Desc.cullMode;
		mReverseCulling	 = GReverseCulling;

		Material         = NULL;

		VertexFactory    = NULL;

		OpaqueMesh		 = (UStaticMesh *)Desc.opaqueMesh;
		OverrideMats	 = NULL;

		if ( IsInRenderingThread() ) // If we are in the rendering thread, then immediately create this render resource
		{
			Init(Desc);
		}
		else
		{
			// If we are in the game thread, then post
			GApexRender->Enque(this,Desc);
		}
	}

	void Init(const physx::NxUserRenderResourceDesc &Desc)
	{
		check( IsInRenderingThread());

		UBOOL bUseClothing = FALSE;
//		UINT NumVBs = (UINT)Desc.vertexBuffers.size();
		UINT NumVBs = (UINT)Desc.numVertexBuffers;
		if(NumVBs > 0)
		{
			VertexBuffers = (FApexRenderVertexBuffer**)appMalloc(sizeof(FApexRenderVertexBuffer*)*NumVBs);
			for(UINT i=0; i<NumVBs; i++)
			{
				VertexBuffers[NumVertexBuffers] = static_cast<FApexRenderVertexBuffer*>(Desc.vertexBuffers[i]);
			#if !ENABLE_NULL_VB
				check(VertexBuffers[NumVertexBuffers]);
			#endif
				if(VertexBuffers[NumVertexBuffers])
				{
					FApexRenderVertexBuffer *vb = VertexBuffers[NumVertexBuffers];
					if ( vb->HasBlendWeight() )
					{
						bUseClothing = TRUE;
					}
					NumVertexBuffers++;
				}
			}
		}

		IndexBuffer = static_cast<FApexRenderIndexBuffer*>(Desc.indexBuffer);
		BoneBuffer  = static_cast<FApexRenderBoneBuffer*>(Desc.boneBuffer);
		InstanceBuffer = static_cast<FApexRenderInstanceBuffer*>(Desc.instanceBuffer);

		if(BoneBuffer)
		{
			if ( bUseClothing )
			{
				VertexFactory = new FGPUSkinVertexFactoryApexClothing(*this);
			}
			else
			{
				VertexFactory = new FGPUSkinVertexFactoryApexDestructible(*this);
			}
		}
		else
		{
			VertexFactory = new FLocalVertexFactoryApex(*this);
		}

		setVertexBufferRange(Desc.firstVertex, Desc.numVerts);
		setIndexBufferRange(Desc.firstIndex, Desc.numIndices);
		setBoneBufferRange(Desc.firstBone, Desc.numBones);
		setInstanceBufferRange(Desc.firstInstance, Desc.numInstances ? Desc.numInstances : 1);
		if ( Desc.userRenderData != NULL )
		{
			TArray<UMaterialInterface*>* NewOverrideMats = static_cast<TArray<UMaterialInterface*>*>(Desc.userRenderData);
			OverrideMats = new TArray<UMaterialInterface*>(*NewOverrideMats);
		}

		setMaterial(Desc.material);

		if(VertexFactory)
		{
			VertexFactory->InitRHIWithAPEXBuffers(VertexBuffers, NumVertexBuffers, InstanceBuffer);
			VertexFactory->InitResource();
		}
	}

	virtual ~FApexRenderResource(void)
	{
		GApexRender->NotifyEnqueBufferGone(this);
		if(VertexFactory)
		{
			VertexFactory->ReleaseResource();
			delete VertexFactory;
		}
		if(VertexBuffers)
		{
			appFree(VertexBuffers);
		}

		if ( OverrideMats != NULL )
		{
			delete OverrideMats;
			OverrideMats = NULL;
		}
	}

	UBOOL loadMeshElement(FMeshBatch &MeshElement, const NxApexRenderContext &Context, UBOOL bSelected)
	{
		UBOOL OK = FALSE;

		if ( !VertexBuffers ) return FALSE;

		VertexFactory->UpdateRHI();
		VertexFactory->InitRHIWithAPEXBuffers(VertexBuffers, NumVertexBuffers, InstanceBuffer);

		FMeshBatchElement& BatchElement = MeshElement.Elements(0);
		BatchElement.IndexBuffer                = IndexBuffer;
		BatchElement.FirstIndex                 = FirstIndex;
		BatchElement.NumPrimitives              = GetNumPrimitives();

		MeshElement.VertexFactory              = VertexFactory;
		MeshElement.DynamicVertexData          = NULL;
		MeshElement.MaterialRenderProxy        = Material ? Material->GetRenderProxy(bSelected) : GEngine->DefaultMaterial->GetRenderProxy(bSelected);
		MeshElement.LCI                        = &LightCache;

		BatchElement.MinVertexIndex             = FirstVertex;
		BatchElement.MaxVertexIndex             = NumVertices ? FirstVertex+NumVertices-1 : 0;
		MeshElement.UseDynamicData             = FALSE;

		const FApexRenderResource *RenderResource = static_cast<const  FApexRenderResource *>(Context.renderResource);

		MeshElement.ReverseCulling             = RenderResource->GetReverseCulling();

		MeshElement.CastShadow                 = FALSE;
		MeshElement.Type                       = PrimitiveType;
		MeshElement.DepthPriorityGroup         = SDPG_World;
		MeshElement.bUsePreVertexShaderCulling = TRUE;

		if(BoneBuffer || InstanceBuffer)
		{
			BatchElement.LocalToWorld           = FMatrix::Identity;
			BatchElement.WorldToLocal           = FMatrix::Identity;
		}
		else
		{
			BatchElement.LocalToWorld           = N2UTransformApex(Context.local2world);
			BatchElement.WorldToLocal           = N2UTransformApex(Context.world2local);
		}

		if(BatchElement.IndexBuffer && BatchElement.NumPrimitives > 0 && MeshElement.VertexFactory && MeshElement.MaterialRenderProxy)
		{
			OK = TRUE;
		}

		return OK;
	}
	UINT                         GetFirstBoneIndex(void) const { return FirstBone;  }
	UINT                         GetNumBones(void)       const { return NumBones;   }

public:
	virtual void setVertexBufferRange(physx::PxU32 InFirstVertex, physx::PxU32 InNumVerts)
	{
		FirstVertex = (UINT)InFirstVertex;
		NumVertices = (UINT)InNumVerts;
		if(VertexFactory)
		{
			VertexFactory->SetNumVertices(NumVertices);
		}

	}

	virtual void setIndexBufferRange(physx::PxU32 InFirstIndex, physx::PxU32 InNumIndices)
	{
		FirstIndex = (UINT)InFirstIndex;
		NumIndices = (UINT)InNumIndices;
	}

	virtual void setBoneBufferRange(physx::PxU32 InFirstBone, physx::PxU32 InNumBones)
	{
		FirstBone = (UINT)InFirstBone;
		NumBones  = (UINT)InNumBones;
	}

	virtual void setInstanceBufferRange(physx::PxU32 InFirstInstance, physx::PxU32 InNumInstances)
	{
		FirstInstance = (UINT)InFirstInstance;
		NumInstances  = (UINT)InNumInstances;
		if(VertexFactory)
		{
			VertexFactory->SetNumInstances(NumInstances);
		}
	}

	virtual void setSpriteBufferRange(physx::PxU32 InFirstSprite, physx::PxU32 InNumSprites )
	{
	}

	virtual void setMaterial(void *InMaterial)
	{
		if ( OverrideMats != NULL )
		{
			INT MatIdx = (INT)(size_t)InMaterial;
			// Index offset by 1 so we can differentiate between no material and material index 0
			MatIdx -= 1;
			if ( (MatIdx >= 0) && (MatIdx < OverrideMats->Num()) )
			{
				Material = OverrideMats->GetData()[MatIdx];
			}
			else
			{
				Material = NULL;
			}
		}
		else
		{
			Material = (UMaterialInterface*)InMaterial;
		}
	}

	virtual physx::PxU32 getNbVertexBuffers(void) const
	{
		return NumVertexBuffers;
	}

	virtual NxUserRenderVertexBuffer *getVertexBuffer(physx::PxU32 Index) const
	{
		check(Index < NumVertexBuffers);
		return VertexBuffers[Index];
	}

	virtual NxUserRenderIndexBuffer *getIndexBuffer(void) const
	{
		return IndexBuffer;
	}

	virtual NxUserRenderBoneBuffer *getBoneBuffer(void)	const
	{
		return BoneBuffer;
	}

	virtual NxUserRenderInstanceBuffer *getInstanceBuffer(void)	const
	{
		return InstanceBuffer;
	}

	virtual NxUserRenderSpriteBuffer *getSpriteBuffer(void)	const
	{
		return NULL;
	}

	UBOOL GetReverseCulling(void) const { return mReverseCulling; };

	FApexRenderInstanceBuffer * GetInstanceBuffer(void) { return InstanceBuffer; };

	UStaticMesh * GetOpaqueMesh(void) const { return OpaqueMesh; };

	UINT GetNumInstances(void) const { return NumInstances; };
	UINT GetFirstInstance(void) const { return FirstInstance; };

private:
	UBOOL						mReverseCulling;
	FApexRenderVertexBuffer  **VertexBuffers;
	UINT                       NumVertexBuffers;
	UINT                       FirstVertex;
	UINT                       NumVertices;
	FApexRenderIndexBuffer    *IndexBuffer;
	UINT                       FirstIndex;
	UINT                       NumIndices;
	FApexRenderBoneBuffer     *BoneBuffer;
	UINT                       FirstBone;
	UINT                       NumBones;
	FApexRenderInstanceBuffer *InstanceBuffer;
	UINT                       FirstInstance;
	UINT                       NumInstances;

	EPrimitiveType             PrimitiveType;

	UMaterialInterface        *Material;
	FLocalVertexFactoryApex   *VertexFactory;
	NxRenderCullMode::Enum	   CullMode;
	FApexLightCache            LightCache;
	UStaticMesh					*OpaqueMesh;
	TArray<UMaterialInterface*>	*OverrideMats;
};
#endif


IMPLEMENT_VERTEX_FACTORY_TYPE(FLocalVertexFactoryApex, "LocalVertexFactory", TRUE, TRUE, TRUE, TRUE, TRUE, VER_DEPRECATED_EDITOR_POSITION, 0);

/**
 * FGPUSkinVertexFactoryShaderParametersApexDestructible
 *
 * Implementation of FGPUSkinVertexFactoryShaderParametersApexDestructible modules which can use GPU skinning
 */
class FGPUSkinVertexFactoryShaderParametersApexDestructible : public FLocalVertexFactoryShaderParameters
{
public:
	FGPUSkinVertexFactoryShaderParametersApexDestructible(void)
	{
	}

public:
	virtual void Bind(const FShaderParameterMap &ParameterMap)
	{
		FLocalVertexFactoryShaderParameters::Bind(ParameterMap);

		BoneMatricesParameter.Bind(ParameterMap,TEXT("BoneMatrices"));
		MeshOriginParameter.Bind(ParameterMap,TEXT("MeshOrigin"),TRUE);
		MeshExtensionParameter.Bind(ParameterMap,TEXT("MeshExtension"),TRUE);
		UsePerBoneMotionBlurParameter.Bind(ParameterMap,TEXT("bUsePerBoneMotionBlur"),TRUE);
	}

	virtual void Serialize(FArchive& Ar)
	{
		FLocalVertexFactoryShaderParameters::Serialize(Ar);

		Ar << BoneMatricesParameter;
		Ar << MeshOriginParameter;
		Ar << MeshExtensionParameter;
		Ar << UsePerBoneMotionBlurParameter;
	}

	virtual void Set(FShader *VertexShader, const FVertexFactory *VertexFactory, const FSceneView &View) const
	{
		FLocalVertexFactoryShaderParameters::Set(VertexShader, VertexFactory, View);

#if WITH_APEX
		const FLocalVertexFactoryApex &ApexVertexFactory  = *static_cast<const FLocalVertexFactoryApex*>(VertexFactory);
		const FApexRenderResource     &ApexRenderResource = ApexVertexFactory.getApexRenderResource();
		const FApexRenderBoneBuffer *BoneBuffer     = static_cast<const FApexRenderBoneBuffer*>(ApexRenderResource.getBoneBuffer());
		UINT                         FirstBoneIndex = ApexRenderResource.GetFirstBoneIndex();
		UINT                         NumBones       = ApexRenderResource.GetNumBones();
		UINT                         MaxBones       = BoneBuffer ? BoneBuffer->GetMaxBones() : 0;
		check(BoneBuffer && FirstBoneIndex+NumBones <= MaxBones && NumBones <= APEX_MAX_BONES);
		if(BoneBuffer && FirstBoneIndex+NumBones <= MaxBones && NumBones <= APEX_MAX_BONES)
		{
			const FSkinMatrix3x4 *Bones = BoneBuffer->GetBones();
			if(Bones)
			{
				Bones += FirstBoneIndex;
			}
			if(NumBones > MaxBones)
			{
				NumBones = MaxBones; // Clamp to the maximum number of bones...
			}

			SetVertexShaderValues<FSkinMatrix3x4>(VertexShader->GetVertexShader(), BoneMatricesParameter, Bones, NumBones);
		}
#endif
		SetVertexShaderValue(VertexShader->GetVertexShader(), MeshExtensionParameter, FVector(1,1,1) );
		SetVertexShaderValue(VertexShader->GetVertexShader(), MeshOriginParameter, FVector(0,0,0) );
	}

private:
	FShaderParameter BoneMatricesParameter;
	FShaderParameter MeshOriginParameter;
	FShaderParameter MeshExtensionParameter;
	UINT             MaxBones;
	FShaderParameter UsePerBoneMotionBlurParameter;
};

FVertexFactoryShaderParameters* FGPUSkinVertexFactoryApexDestructible::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinVertexFactoryShaderParametersApexDestructible() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactoryApexDestructible, "ApexGpuSkinVertexFactory_1Bone", TRUE, FALSE, TRUE, FALSE, TRUE, VER_MOTIONBLURSKINNING, 0);

/**
 * FGPUSkinVertexFactoryShaderParametersApexClothing
 *
 * Implementation of FGPUSkinVertexFactoryShaderParametersApexClothing modules which can use GPU skinning
 */
class FGPUSkinVertexFactoryShaderParametersApexClothing : public FLocalVertexFactoryShaderParameters
{
public:
	FGPUSkinVertexFactoryShaderParametersApexClothing(void)
	{
	}

public:
	virtual void Bind(const FShaderParameterMap &ParameterMap)
	{
		FLocalVertexFactoryShaderParameters::Bind(ParameterMap);
		BoneMatricesParameter.Bind(ParameterMap,TEXT("BoneMatrices"));
		MeshOriginParameter.Bind(ParameterMap,TEXT("MeshOrigin"),TRUE);
		MeshExtensionParameter.Bind(ParameterMap,TEXT("MeshExtension"),TRUE);
		ApexDummyParameter.Bind(ParameterMap,TEXT("ApexDummy"),FALSE);
	}

	virtual void Serialize(FArchive& Ar)
	{
		FLocalVertexFactoryShaderParameters::Serialize(Ar);
		Ar << BoneMatricesParameter;
		Ar << ApexDummyParameter;
		Ar << MeshOriginParameter;
		Ar << MeshExtensionParameter;
	}

	virtual void Set(FShader *VertexShader, const FVertexFactory *VertexFactory, const FSceneView &View) const
	{
		FLocalVertexFactoryShaderParameters::Set(VertexShader, VertexFactory, View);

#if WITH_APEX
		const FLocalVertexFactoryApex &ApexVertexFactory  = *static_cast<const FLocalVertexFactoryApex*>(VertexFactory);
		const FApexRenderResource     &ApexRenderResource = ApexVertexFactory.getApexRenderResource();

		const FApexRenderBoneBuffer *BoneBuffer     = static_cast<const FApexRenderBoneBuffer*>(ApexRenderResource.getBoneBuffer());
		UINT                         FirstBoneIndex = ApexRenderResource.GetFirstBoneIndex();
		UINT                         NumBones       = ApexRenderResource.GetNumBones();
		UINT                         MaxBones       = BoneBuffer ? BoneBuffer->GetMaxBones() : 0;
		check(BoneBuffer && FirstBoneIndex+NumBones <= MaxBones && NumBones <= APEX_MAX_BONES);
		if(BoneBuffer && FirstBoneIndex+NumBones <= MaxBones && NumBones <= APEX_MAX_BONES)
		{
			const FSkinMatrix3x4 *Bones = BoneBuffer->GetBones();
			if(Bones)
			{
				Bones += FirstBoneIndex;
			}
			if(NumBones > MaxBones)
			{
				NumBones = MaxBones; // Clamp to the maximum number of bones...
			}

			SetVertexShaderValues<FSkinMatrix3x4>(VertexShader->GetVertexShader(), BoneMatricesParameter, Bones, NumBones);
		}
#endif
		SetVertexShaderValue(VertexShader->GetVertexShader(), ApexDummyParameter, 1.0f);
		SetVertexShaderValue(VertexShader->GetVertexShader(), MeshExtensionParameter, FVector(1,1,1) );
		SetVertexShaderValue(VertexShader->GetVertexShader(), MeshOriginParameter, FVector(0,0,0) );

	}

private:
	FShaderParameter BoneMatricesParameter;
	UINT             MaxBones;
	FShaderParameter MeshOriginParameter;
	FShaderParameter MeshExtensionParameter;
	FShaderParameter ApexDummyParameter;
};

FVertexFactoryShaderParameters* FGPUSkinVertexFactoryApexClothing::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinVertexFactoryShaderParametersApexClothing() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactoryApexClothing, "ApexGpuSkinVertexFactory", TRUE, FALSE, TRUE, FALSE, TRUE, VER_REMOVE_MAXBONEINFLUENCE, 0);

/**
 * FApexStaticRenderer - implementation of NxUserRenderer
 *
 *	Used by APEX to pass static render buffers to UE3
 */
#if WITH_APEX
class FApexStaticRenderer : public NxUserRenderer
{
public:
	FApexStaticRenderer(FStaticPrimitiveDrawInterface &InPDI, UBOOL bInSelected, UBOOL bInCastShadow) :
	PDI(InPDI),
	bSelected(bInSelected),
	bCastShadow(bInCastShadow)
	{
	}

	virtual void renderResource(const NxApexRenderContext &Context)
	{
		if(Context.renderResource)
		{
			FApexRenderResource &Mesh = *static_cast<FApexRenderResource*>(Context.renderResource);
			FMeshBatch MeshElement;
			if(Mesh.loadMeshElement(MeshElement, Context, bSelected))
			{
				MeshElement.CastShadow = bCastShadow;
				PDI.DrawMesh(MeshElement, 0, FLT_MAX);
			}
		}
	}
	
private:
	FStaticPrimitiveDrawInterface &PDI;
	UBOOL                          bSelected;
	UBOOL                          bCastShadow;
		
};
#endif

#endif	// #ifdef NV_APEX_RENDER_CLASSES_H
