/*============================================================================
	Karma Integration Support
    
    - MeMemory/MeMessage glue
    - Debug line drawing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 ===========================================================================*/

#include "EnginePrivate.h"
#include "UnPhysicalMaterial.h"

/* *********************************************************************** */
/* *********************************************************************** */
/* *********************** MODELTOHULLS  ********************************* */
/* *********************************************************************** */
/* *********************************************************************** */


/** Returns FALSE if ModelToHulls operation should halt because of vertex count overflow. */
static UBOOL AddConvexPrim(FKAggregateGeom* outGeom, TArray<FPlane> &planes, UModel* inModel)
{
	// Add Hull.
	const INT ex = outGeom->ConvexElems.AddZeroed();
	FKConvexElem* c = &outGeom->ConvexElems(ex);

	// Because of precision, we use the original model verts as 'snap to' verts.
	TArray<FVector> SnapVerts;
	for(INT k=0; k<inModel->Verts.Num(); k++)
	{
		// Find vertex vector. Bit of  hack - sometimes FVerts are uninitialised.
		const INT pointIx = inModel->Verts(k).pVertex;
		if(pointIx < 0 || pointIx >= inModel->Points.Num())
		{
			continue;
		}

		SnapVerts.AddItem(inModel->Points(pointIx));
	}

	// Create a hull from a set of planes
	UBOOL bSuccess = c->HullFromPlanes(planes, SnapVerts);

	// If it failed for some reason, remove from the array
	if(!bSuccess || !c->ElemBox.IsValid)
	{
		outGeom->ConvexElems.Remove(ex);
	}

	// Return if we succeeded or not
	return bSuccess;
}

// Worker function for traversing collision mode/blocking volumes BSP.
// At each node, we record, the plane at this node, and carry on traversing.
// We are interested in 'inside' ie solid leafs.
/** Returns FALSE if ModelToHulls operation should halt because of vertex count overflow. */
static UBOOL ModelToHullsWorker(FKAggregateGeom* outGeom,
								UModel* inModel, 
								INT nodeIx, 
								UBOOL bOutside, 
								TArray<FPlane> &planes)
{
	FBspNode* node = &inModel->Nodes(nodeIx);
	if(node)
	{
		// BACK
		if(node->iBack != INDEX_NONE) // If there is a child, recurse into it.
		{
			planes.AddItem(node->Plane);
			if ( !ModelToHullsWorker(outGeom, inModel, node->iBack, node->ChildOutside(0, bOutside), planes) )
			{
				return FALSE;
			}
			planes.Remove(planes.Num()-1);
		}
		else if(!node->ChildOutside(0, bOutside)) // If its a leaf, and solid (inside)
		{
			planes.AddItem(node->Plane);
			if ( !AddConvexPrim(outGeom, planes, inModel) )
			{
				return FALSE;
			}
			planes.Remove(planes.Num()-1);
		}

		// FRONT
		if(node->iFront != INDEX_NONE)
		{
			planes.AddItem(node->Plane.Flip());
			if ( !ModelToHullsWorker(outGeom, inModel, node->iFront, node->ChildOutside(1, bOutside), planes) )
			{
				return FALSE;
			}
			planes.Remove(planes.Num()-1);
		}
		else if(!node->ChildOutside(1, bOutside))
		{
			planes.AddItem(node->Plane.Flip());
			if ( !AddConvexPrim(outGeom, planes, inModel) )
			{
				return FALSE;
			}
			planes.Remove(planes.Num()-1);
		}
	}

	return TRUE;
}

// Converts a UModel to a set of convex hulls for.  If flag deleteContainedHull is set any convex elements already in
// outGeom will be destroyed.  WARNING: the input model can have no single polygon or
// set of coplanar polygons which merge to more than FPoly::MAX_VERTICES vertices.
// Creates it around the model origin, and applies the Unreal->Physics scaling.
UBOOL KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, UBOOL deleteContainedHulls/*=TRUE*/ )
{
	UBOOL bSuccess = TRUE;

	if ( deleteContainedHulls )
	{
		outGeom->ConvexElems.Empty();
	}

	const INT NumHullsAtStart = outGeom->ConvexElems.Num();
	
	if( inModel )
	{
		TArray<FPlane>	planes;
		bSuccess = ModelToHullsWorker(outGeom, inModel, 0, inModel->RootOutside, planes);
		if ( !bSuccess )
		{
			// ModelToHulls failed.  Clear out anything that may have been created.
			outGeom->ConvexElems.Remove( NumHullsAtStart, outGeom->ConvexElems.Num() - NumHullsAtStart );
		}
	}

	return bSuccess;
}

/** Set the status of a particular channel in the structure. */
void FRBCollisionChannelContainer::SetChannel(ERBCollisionChannel Channel, UBOOL bNewState)
{
	INT ChannelShift = (INT)Channel;

#if !__INTEL_BYTE_ORDER__
	DWORD ChannelBit = (1 << (31 - ChannelShift));
#else
	DWORD ChannelBit = (1 << ChannelShift);
#endif

	if(bNewState)
	{
		Bitfield = Bitfield | ChannelBit;
	}
	else
	{
		Bitfield = Bitfield & ~ChannelBit;
	}
}

/** This constructor will zero out the struct */
FRBCollisionChannelContainer::FRBCollisionChannelContainer(INT)
{
	appMemzero(this, sizeof(FRBCollisionChannelContainer));
}

/**
 * This is a helper function which will set the PhysicalMaterial based on
 * whether or not we have a PhysMaterial set from a Skeletal Mesh or if we
 * are getting it from the Environment.
 *
 **/
UPhysicalMaterial* DetermineCorrectPhysicalMaterial( const FCheckResult& HitData )
{
	check(GEngine->DefaultPhysMaterial);

	// check to see if this has a Physical Material Override.  If it does then use that
	if( ( HitData.Component != NULL ) && ( HitData.Component->PhysMaterialOverride != NULL ) )
	{
		return HitData.Component->PhysMaterialOverride;
	}
	// if the physical material is already set then we know we hit a skeletal mesh and should return that PhysMaterial
	else if( HitData.PhysMaterial != NULL )
	{
		return HitData.PhysMaterial;
	}
	// else we need to look at the Material and use that for our PhysMaterial.
	// The GetPhysicalMaterial is virtual and will return the correct Material for all
	// Material Types
	// The PhysMaterial may still be null
	else if( HitData.Material != NULL )
	{
		return HitData.Material->GetPhysicalMaterial();
	}
	else if( Cast<UMeshComponent>(HitData.Component) != NULL )
	{
		UMeshComponent* MeshComp = Cast<UMeshComponent>(HitData.Component);

		// look at the materials until we find one that has a valid PhysMaterial.  And then use that.
		// NOTE: this will only check LOD 0 due to how StaticMesh's GetMaterial(INT) works
		for( INT MatIdx = 0; MatIdx < MeshComp->GetNumElements(); ++MatIdx )
		{
			if( MeshComp->GetMaterial(MatIdx) != NULL && MeshComp->GetMaterial(MatIdx)->GetPhysicalMaterial() )
			{
				return MeshComp->GetMaterial(MatIdx)->GetPhysicalMaterial();
			}
		}
	}

	return GEngine->DefaultPhysMaterial;
}
