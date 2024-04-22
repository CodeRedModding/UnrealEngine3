/**
 * FaceFXStudioSkeletalComponent.cpp: AnimSet viewer main
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 **/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	FFaceFXStudioSkelSceneProxy
-----------------------------------------------------------------------------*/

/**
* A skeletal mesh component scene proxy.
*/
class FFaceFXStudioSkelSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	/** 
	* Constructor. 
	* @param	Component - skeletal mesh primitive being added
	*/
	FFaceFXStudioSkelSceneProxy( const UFaceFXStudioSkelComponent* InComponent ) 
		: FSkeletalMeshSceneProxy(InComponent) {}

	virtual ~FFaceFXStudioSkelSceneProxy() {}

	// FPrimitiveSceneProxy interface.
	/** Ensure its always in the foreground DPG. */
	virtual FPrimitiveViewRelevance GetViewRelevance( const FSceneView* View )
	{
		FPrimitiveViewRelevance Result = FSkeletalMeshSceneProxy::GetViewRelevance(View);
		Result.SetDPG(SDPG_Foreground, TRUE);
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return sizeof(*this) + GetAllocatedSize(); }
	DWORD GetAllocatedSize( void ) const { return FSkeletalMeshSceneProxy::GetAllocatedSize(); }
};

/*-----------------------------------------------------------------------------
	UFaceFXStudioSkelComponent
-----------------------------------------------------------------------------*/

/** Create the scene proxy needed for rendering a FaceFX Studio skeletal mesh */
FPrimitiveSceneProxy* UFaceFXStudioSkelComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Result = NULL;

	// Only create a scene proxy for rendering if properly initialized.
	if( SkeletalMesh && 
		SkeletalMesh->LODModels.IsValidIndex(PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject )
	{
		Result = ::new FFaceFXStudioSkelSceneProxy(this);
	}

	return Result;
}

/**
 * This will implement the FaceFXSudioSkelComponent in all cases so we are able to link correctly
 * when we #define WITH_FACEFX 0.
 *
 **/
IMPLEMENT_CLASS(UFaceFXStudioSkelComponent);
