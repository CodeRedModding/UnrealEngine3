/*=============================================================================
	NvApexRender.h : Header file for NxApexRender.cpp
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
#ifndef NV_APEX_RENDER_H
#define NV_APEX_RENDER_H

#include "NvApexManager.h"

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

// Forward declared classes in physx::apex namespace
namespace physx
{
	namespace apex
	{
		class NxUserRenderResourceManager;
		class NxUserRenderer;
	};
};


class FEnqueData;
class FApexRenderVertexBuffer;
class FApexRenderIndexBuffer;
class FApexRenderInstanceBuffer;
class FApexRenderResource;

/**
	FIApexRender - interface to APEX rendering utilities
*/

class FIApexRender
{
public:
	/**
	Get the global APEX Render Resource Manager
	*/
	virtual physx::apex::NxUserRenderResourceManager *	GetRenderResourceManager(void) = 0;

	/**
	Get the global APEX User Renderer
	*/
	virtual physx::apex::NxUserRenderer 			 *	GetNxUserRenderer(void) = 0;

	/**
	Create a dynamic APEX User Renderer
	*/
	virtual physx::apex::NxUserRenderer				 *	CreateDynamicRenderer(void *PDI, UBOOL Selected, UBOOL CastShadow, UBOOL WireFrame,const FSceneView *View, UINT DPGIndex) = 0;

	/**
	Release a dynamic APEX User Renderer
	*/
	virtual void										ReleaseDynamicRenderer(physx::apex::NxUserRenderer *Renderer) = 0;

	// All of these methods must be called from the rendering thread!
	virtual void PostFEnqueDataRenderingThread(FEnqueData *e) = 0;
	virtual void PostFEnqueDataGameThread(FEnqueData *e) = 0;

	virtual void NotifyEnqueBufferGone(void *buffer) = 0;

	virtual void ProcessFEnqueDataGameThread(void) = 0;
	virtual void ProcessFEnqueDataRenderingThread(void) = 0;

	virtual void FlushFEnqueDataGameThread(void) = 0;
	virtual void FlushFEnqueDataRenderingThread(void) = 0;

	virtual void Enque(FApexRenderVertexBuffer *buffer,const physx::NxUserRenderVertexBufferDesc &desc) = 0;
	virtual void Enque(FApexRenderVertexBuffer *buffer,const NxApexRenderVertexBufferData &data, physx::PxU32 firstVertex, physx::PxU32 numVertices)=0;
	virtual void Enque(FApexRenderIndexBuffer *buffer,const NxUserRenderIndexBufferDesc &desc) = 0;
	virtual void Enque(FApexRenderIndexBuffer *buffer,const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements) = 0;
	virtual void Enque(FApexRenderInstanceBuffer *buffer,const NxUserRenderInstanceBufferDesc &Desc) = 0;
	virtual void Enque(FApexRenderInstanceBuffer *buffer,physx::NxRenderInstanceSemantic::Enum Semantic, const void *SrcData, physx::PxU32 SrcStride, physx::PxU32 FirstDestElement, physx::PxU32 NumElements,physx::PxU32 *indexRemapping) = 0;
	virtual void Enque(FApexRenderResource *buffer,const physx::NxUserRenderResourceDesc &Desc) = 0;
	void SetRequireRewriteBuffers(UBOOL bRequireRewrite){ bRequireRewriteBuffers = bRequireRewrite; }
	UBOOL GetRequireRewriteBuffers() const { return bRequireRewriteBuffers; }
protected:
	UBOOL bRequireRewriteBuffers;

};

/**
 *	Implementation of NxUserRenderer
 *
 *	Used by APEX to pass dynamic render buffers to UE3
 */
class FApexDynamicRenderer : public NxUserRenderer
{
public:
	FApexDynamicRenderer(FPrimitiveDrawInterface &InPDI, UBOOL bInSelected, UBOOL bInCastShadow, UBOOL bInWireframe,const FSceneView *_View,UINT _DPGIndex) :
	PDI(InPDI),
	bSelected(bInSelected),
	bCastShadow(bInCastShadow),
	bWireframe(bInWireframe),
	View(_View),
	DPGIndex(_DPGIndex)
	{
	}

	virtual void renderResource(const NxApexRenderContext &Context);

	void RenderStaticMeshInstanced(FPrimitiveDrawInterface &PDI,UStaticMesh *Mesh,UINT NumInstances,const FMatrix *transforms, const FSceneView* View, UINT DPGIndex);
private:


	FPrimitiveDrawInterface &PDI;
	UBOOL                    bSelected;
	UBOOL                    bCastShadow;
	UBOOL                    bWireframe;
	const FSceneView		*View;
	UINT					DPGIndex;
};
/**
Create an FIApexRender interface class
*/
FIApexRender* CreateApexRender(void);

/**
Release an FIApexRender interface class
*/
void          ReleaseApexRender(FIApexRender *ir);

/**
Declare a convenience global variable for the Apex Render interface.
*/
extern FIApexRender *GApexRender;
// Designates whether normals are positive or negative.  Positive for destruction assets; negative for clothing assets
extern UBOOL	GReverseCulling;
extern physx::PxF32	GApexNormalSign;


#endif // #if WITH_APEX

#endif // #ifndef NV_APEX_RENDER_H
