/*=============================================================================
	ProcBuilding.cpp
	Procedural Building Generation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//#include "UnrealEd.h"

#include "EnginePrivate.h"
#include "EngineProcBuildingClasses.h"
#include "EngineMeshClasses.h"
#include "UnFracturedStaticMesh.h"
#include "UnLinkedObjDrawUtils.h"
#include "EnginePhysicsClasses.h"
#include "EngineUserInterfaceClasses.h"

IMPLEMENT_CLASS(AProcBuilding);
IMPLEMENT_CLASS(AProcBuilding_SimpleLODActor);
IMPLEMENT_CLASS(UProcBuildingRuleset);

IMPLEMENT_CLASS(UPBRuleNodeBase);
IMPLEMENT_CLASS(UPBRuleNodeRepeat);
IMPLEMENT_CLASS(UPBRuleNodeMesh);
IMPLEMENT_CLASS(UPBRuleNodeSplit);
IMPLEMENT_CLASS(UPBRuleNodeQuad);
IMPLEMENT_CLASS(UPBRuleNodeExtractTopBottom);
IMPLEMENT_CLASS(UPBRuleNodeAlternate);
IMPLEMENT_CLASS(UPBRuleNodeOcclusion);
IMPLEMENT_CLASS(UPBRuleNodeRandom);
IMPLEMENT_CLASS(UPBRuleNodeEdgeAngle);
IMPLEMENT_CLASS(UPBRuleNodeLODQuad);
IMPLEMENT_CLASS(UPBRuleNodeCorner);
IMPLEMENT_CLASS(UPBRuleNodeSize);
IMPLEMENT_CLASS(UPBRuleNodeEdgeMesh);
IMPLEMENT_CLASS(UPBRuleNodeVariation);
IMPLEMENT_CLASS(UPBRuleNodeComment);
IMPLEMENT_CLASS(UPBRuleNodeSubRuleset);
IMPLEMENT_CLASS(UPBRuleNodeWindowWall);
IMPLEMENT_CLASS(UPBRuleNodeTransform);
IMPLEMENT_CLASS(UPBRuleNodeCycle);

static const FLOAT Deg2U = 65536 / 360.f;

// whether or not to used instanced staticmeshes
// note that changing this won't affect already serialized buildings, they'll have to be regenerated to switch from one to the other
#define USE_INSTANCING_FOR_NEW_MESHES 1

// Draws lines for occlusion point tests
#define DEBUG_OCCLUSION 0

FLOAT OcclusionTestTime = 0.f;
FLOAT MeshFindTime = 0.f;
FLOAT MeshTime = 0.f;

/** Draws this scope using the World line batcher */
void FPBScope2D::DrawScope(const FColor& DrawColor, const FMatrix& BuildingToWorld, UBOOL bPersistant)
{
#if WITH_EDITORONLY_DATA
	ULineBatchComponent* LineComp = bPersistant ? GWorld->PersistentLineBatcher : GWorld->LineBatcher;

	FMatrix WorldFrame = ScopeFrame * BuildingToWorld;

	FVector Origin = WorldFrame.GetOrigin();

	//LineComp->DrawLine(FVector(0,0,0), Origin, FColor(255,255,255), SDPG_World);

	// Draw rect region
	LineComp->DrawLine(Origin, Origin + WorldFrame.GetAxis(0)*DimX, DrawColor, SDPG_World);
	LineComp->DrawLine(Origin, Origin + WorldFrame.GetAxis(2)*DimZ, DrawColor, SDPG_World);
	LineComp->DrawLine(Origin + WorldFrame.GetAxis(0)*DimX, Origin + WorldFrame.GetAxis(0)*DimX + WorldFrame.GetAxis(2)*DimZ, DrawColor, SDPG_World);
	LineComp->DrawLine(Origin + WorldFrame.GetAxis(2)*DimZ, Origin + WorldFrame.GetAxis(0)*DimX + WorldFrame.GetAxis(2)*DimZ, DrawColor, SDPG_World);
	
	// Draw normal (Y)
	LineComp->DrawLine(Origin, Origin + WorldFrame.GetAxis(1)*50.f, DrawColor, SDPG_World);
#endif // WITH_EDITORONLY_DATA
}

/** Offset the origin of this scope in its local reference frame. */
void FPBScope2D::OffsetLocal(const FVector& LocalOffset)
{
	const FVector WorldOffset = ScopeFrame.TransformNormal(LocalOffset);
	const FVector CurrentOrigin = ScopeFrame.GetOrigin();
	ScopeFrame.SetOrigin(CurrentOrigin + WorldOffset);
}

/** Returns locaiton of the middle point of this scope */
FVector FPBScope2D::GetCenter()
{
	return ScopeFrame.GetOrigin() + (0.5f * DimX * ScopeFrame.GetAxis(0)) + (0.5f * DimZ * ScopeFrame.GetAxis(2));
}

/** Util to wrap In so its < Range and >= 0 */
static INT WrapInRange(INT In, INT Range)
{
	while(In >= Range)
	{
		In -= Range;
	}

	while(In < 0)
	{
		In += Range;
	}

	return In;
}


/** Make sure that the scope's Z axis is the best choice (ie. points the most up in world space). If it isn't, modify so it is */
static void EnsureScopeUpright(FPBScope2D& Scope)
{
	FVector ScopeX = Scope.ScopeFrame.GetAxis(0);
	FVector ScopeY = Scope.ScopeFrame.GetAxis(1);
	FVector ScopeZ = Scope.ScopeFrame.GetAxis(2);
	FVector ScopeOrigin = Scope.ScopeFrame.GetOrigin();

	// Find which axis is best (has largest positive Z component)
	enum EMostUpAxis
	{
		PosZ,
		NegZ,
		PosX,
		NegX
	};

	EMostUpAxis Axis = PosZ;
	FLOAT BestZ = ScopeZ.Z;

	if(-ScopeZ.Z > BestZ)
	{
		BestZ = -ScopeZ.Z;
		Axis = NegZ;
	}

	if(ScopeX.Z > BestZ)
	{
		BestZ = ScopeX.Z;
		Axis = PosX;
	}

	if(-ScopeX.Z > BestZ)
	{
		BestZ = -ScopeX.Z;
		Axis = NegX;
	}

	// Then modify if its not PosZ
	if(Axis == NegZ)
	{
		FVector NewOrigin = ScopeOrigin + (Scope.DimX*ScopeX) + (Scope.DimZ*ScopeZ);
		Scope.ScopeFrame = FMatrix(-ScopeX, ScopeY, -ScopeZ, NewOrigin);
	}
	else if(Axis == PosX)
	{
		FVector NewOrigin = ScopeOrigin + (Scope.DimZ*ScopeZ);
		Scope.ScopeFrame = FMatrix(-ScopeZ, ScopeY, ScopeX, NewOrigin);
		Swap(Scope.DimX, Scope.DimZ);
	}
	else if(Axis == NegX)
	{
		FVector NewOrigin = ScopeOrigin + (Scope.DimX*ScopeX);
		Scope.ScopeFrame = FMatrix(ScopeZ, ScopeY, -ScopeX, NewOrigin);
		Swap(Scope.DimX, Scope.DimZ);
	}
}

/** 
 *	Looks at a poly, and see if we can extract a rectangular scope from it.
 *	Returns TRUE if OutScope is good and should be used along with OutPoly.
 *	OutPolyA/B may contain 0 verts, in which case it should not be used
 */
static UBOOL FPolyToWallScope2D(const FPoly& InPoly, FPBScope2D& OutScope, FPoly& OutPolyA, FPoly& OutPolyB)
{	
	// Default case (where we can't extract a scope)
	OutPolyA = InPoly;

	if(InPoly.Vertices.Num() != 4)
	{
		//debugf( TEXT("Poly has too many verts (%d)"), InPoly.Vertices.Num() );
		return FALSE;
	}

	// first we take find all edge directions and lengths
	FVector EdgeDirs[4];
	FLOAT EdgeLen[4];	
	for(INT i=0; i<4; i++)
	{
		EdgeDirs[i] = InPoly.Vertices(WrapInRange(i+1,4)) - InPoly.Vertices(i);
		EdgeLen[i] = EdgeDirs[i].Size();
		if(EdgeLen[i] < 0.1f)
		{
			return FALSE;
		}
		EdgeDirs[i] /= EdgeLen[i]; // Normalize
	}

	// See if we have two parallel sides
	UBOOL bZeroTwoParallel = appIsNearlyEqual(EdgeDirs[0] | EdgeDirs[2], -1.f, 0.001f);
	UBOOL bOneThreeParallel = appIsNearlyEqual(EdgeDirs[1] | EdgeDirs[3], -1.f, 0.001f);

	// If no opposite edges are parallel, cannot extract rectangle
	if(!bZeroTwoParallel && !bOneThreeParallel)
	{
		return FALSE;
	}

	// Poly normal is always scope Y axis
	FVector ScopeY = InPoly.Normal.SafeNormal();

	// Find parallel dir and 'low' and 'high' pair of verts
	FVector ParallelDir;
	FVector LowPoints[2];
	FVector HighPoints[2]; // Note that HighPoints[0] needs to be along 'parallel edge' from LowPoints[0]

	if(bZeroTwoParallel)
	{
		ParallelDir = EdgeDirs[0].SafeNormal();
		LowPoints[0] = InPoly.Vertices(0);
		LowPoints[1] = InPoly.Vertices(3);
		HighPoints[0] = InPoly.Vertices(1);
		HighPoints[1] = InPoly.Vertices(2);
	}
	else
	{
		ParallelDir = EdgeDirs[1].SafeNormal();
		LowPoints[0] = InPoly.Vertices(1);
		LowPoints[1] = InPoly.Vertices(0);
		HighPoints[0] = InPoly.Vertices(2);
		HighPoints[1] = InPoly.Vertices(3);
	}

	// Taking LowPoints[0] as 'zero' dist along ParallelDir, find how far along that axis all the other points are
	FLOAT Low0Dist = 0.f;
	FLOAT Low1Dist = (LowPoints[1] - LowPoints[0])|ParallelDir;
	FLOAT High0Dist = (HighPoints[0] - LowPoints[0])|ParallelDir;
	FLOAT High1Dist = (HighPoints[1] - LowPoints[0])|ParallelDir;
		
	// Now find region of overlap between parallel edges
	FLOAT MinDist = Max(Low0Dist, Low1Dist);
	FLOAT MaxDist = Min(High0Dist, High1Dist);

	// If there is no 'overlap', then we cannot extract a rect region, so give up now
	if(MinDist > MaxDist)
	{
		return FALSE;
	}

	// Generate direction across poly (between parallel edges)
	FVector SideDir = (ScopeY ^ ParallelDir).SafeNormal();

	// Find how wide we need the poly (distance between parallel edges)
	FLOAT High0SideDist = (HighPoints[0] - LowPoints[0])|SideDir;
	FLOAT High1SideDist = (HighPoints[1] - LowPoints[0])|SideDir;

	// Set up scope, with transform
	FVector ScopeX = SideDir;
	FVector ScopeZ = ParallelDir;
	FLOAT ScopeLen = MaxDist - MinDist;
	FLOAT ScopeWidth = Max(High0SideDist, High1SideDist);

	FVector Origin = LowPoints[0] + (ScopeZ*MinDist);	// Corner of scope is MinDist along ParallelDir
		
	OutScope.ScopeFrame = FMatrix(ScopeX, ScopeY, ScopeZ, Origin);
	OutScope.DimX = ScopeWidth;
	OutScope.DimZ = ScopeLen;

	EnsureScopeUpright(OutScope);

	// We clear OutPolyA, as we don't need to fill the entire hole. We do need to maybe fill gaps though
	OutPolyA.Init();

	// See if there is a gap at the low end between poly and scope
	if(Abs(Low1Dist) > 1.0f)
	{
		OutPolyA.Normal = InPoly.Normal;
		OutPolyA.TextureU = InPoly.TextureU;
		OutPolyA.TextureV = InPoly.TextureV;
		OutPolyA.Base   = InPoly.Base;

		OutPolyA.Vertices.AddItem(LowPoints[1]);
		OutPolyA.Vertices.AddItem(LowPoints[0]);


		// For final vert, we find the highest low vert, and then go across the poly
		if(Low1Dist > Low0Dist)
		{
			OutPolyA.Vertices.AddItem(LowPoints[1] - (SideDir*ScopeWidth));
		}
		else
		{
			OutPolyA.Vertices.AddItem(LowPoints[0] + (SideDir*ScopeWidth));
		}
	}

	// See if there is a gap at the high end, and make a poly there if needed
	if(Abs(High0Dist - High1Dist) > 1.0f)
	{
		OutPolyB.Normal = InPoly.Normal;
		OutPolyB.TextureU = InPoly.TextureU;
		OutPolyB.TextureV = InPoly.TextureV;
		OutPolyB.Base   = InPoly.Base;

		OutPolyB.Vertices.AddItem(HighPoints[0]);
		OutPolyB.Vertices.AddItem(HighPoints[1]);

		// For final vert, we find the lowest high vert, and then go across the poly
		if(High1Dist < High0Dist)
		{
			OutPolyB.Vertices.AddItem(HighPoints[1] - (SideDir*ScopeWidth));
		}
		else
		{
			OutPolyB.Vertices.AddItem(HighPoints[0] + (SideDir*ScopeWidth));
		}
	}

	return TRUE;
}

static void GetWorldSpaceCornersOfScope2D(const FPBScope2D& InScope, const FMatrix& BuildingTM, FVector* OutVecs)
{
	const FLOAT OffSurfaceCheck = 8.f;

	// Offset vectors along X and Z
	// We don't return the very corners, we pull in 
	FVector MinXVec = InScope.ScopeFrame.GetAxis(0) * OffSurfaceCheck;
	FVector MaxXVec = InScope.ScopeFrame.GetAxis(0) * (InScope.DimX - OffSurfaceCheck);
	FVector MinZVec = InScope.ScopeFrame.GetAxis(2) * OffSurfaceCheck;
	FVector MaxZVec = InScope.ScopeFrame.GetAxis(2) * (InScope.DimZ - OffSurfaceCheck);

	// Offset out along Y - so we are occluded if another volume surface is co-planer with this face
	FVector YVec = InScope.ScopeFrame.GetAxis(1) * OffSurfaceCheck;

	// Location of each corner
	OutVecs[0] = InScope.ScopeFrame.GetOrigin() + MinXVec + MinZVec + YVec;
	OutVecs[1] = InScope.ScopeFrame.GetOrigin() + MinXVec + MaxZVec + YVec;
	OutVecs[2] = InScope.ScopeFrame.GetOrigin() + MaxXVec + MinZVec + YVec;
	OutVecs[3] = InScope.ScopeFrame.GetOrigin() + MaxXVec + MaxZVec + YVec;

	// Vectors are currently in Actor space, transform to World space
	for(INT i=0; i<4; i++)
	{
		OutVecs[i] = BuildingTM.TransformFVector(OutVecs[i]);
	}
}

/** Returns 0 if clear, 1 if partial and 2 if blocked */
static INT CheckScopeOcclusionStatus(FPBScope2D& InScope, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding)
{
	DOUBLE StartTime = appSeconds();

	// Find four corners of scope in world space
	FVector CornerLocations[4];
	GetWorldSpaceCornersOfScope2D(InScope, BaseBuilding->LocalToWorld(), CornerLocations);
	// Array to keep track of if corners are 
	UBOOL CornerBlocked[4] = {FALSE, FALSE, FALSE, FALSE};

	// Iterate over all nearby buildings..
	for(INT BuildingIdx=0; BuildingIdx<ScopeBuilding->OverlappingBuildings.Num(); BuildingIdx++)
	{
		// Check it is not the building this scope comes from, and it is valid
		AProcBuilding* TestBuilding = ScopeBuilding->OverlappingBuildings(BuildingIdx);
		if(TestBuilding && (TestBuilding != ScopeBuilding) && TestBuilding->BrushComponent)
		{
			// Grab brush component to test against
			UBrushComponent* BuildingComp = TestBuilding->BrushComponent;
			// And then test each corner
			for(INT CornerIdx=0; CornerIdx<4; CornerIdx++)
			{
				if(!CornerBlocked[CornerIdx])
				{
					FCheckResult Hit(1.f);
					UBOOL bHit = !BuildingComp->PointCheck(Hit, CornerLocations[CornerIdx], FVector(0,0,0), 0);
#if DEBUG_OCCLUSION
					GWorld->PersistentLineBatcher->DrawLine(FVector(0,0,0), CornerLocations[CornerIdx], bHit ? FColor(255,0,0) : FColor(0,0,255), SDPG_World);
#endif					
					if(bHit)
					{
						CornerBlocked[CornerIdx] = TRUE;
					}
				}
			}		
		}
	}

	// Now look at our result
	INT BlockedCount = 0;
	for(INT CornerIdx=0; CornerIdx<4; CornerIdx++)
	{
		if(CornerBlocked[CornerIdx])
		{
			BlockedCount++;
		}
	}

	OcclusionTestTime += (1000.f * (appSeconds() - StartTime));

	// Return status based on how many corners are blocked.

	if(BlockedCount == 0)// Clear 
	{
		return 0;
	}	
	else if(BlockedCount == 4) // Blocked
	{
		return 2;	
	}	
	else // Partial
	{
		return 1;		
	}
}

/**
 * Delay calling the update function by a frame because while importing a building, we are potentially creating
 * actors, which will kill the importer. Also, we may get several PostEdit* calls, no need to update the same 
 * building multiple times for one import operation
 */
static void PostUpdateCallback(AProcBuilding* Building)
{
	// update the base-most building, this makes it so that if, say 5 child buildings were 
	// updated, it won't regenerate the base building 5 times, since the update only updates
	// the base-most building (unless the child has a component, in which case it will need
	// updating, but this will be rare)

	AProcBuilding* BuildingToUpdate = Building->GetBaseMostBuilding();
	// if it's a child building that was once a base building and has a low LOD pointer, update the child instead
	if (Building != BuildingToUpdate && (Building->LowLODPersistentActor || Building->SimpleMeshComp))
	{
		BuildingToUpdate = Building;
	}
	GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("ProcBuildingUpdate %s"), *BuildingToUpdate->GetPathName()));
}


//////////////////////////////////////////////////////////////////////////
// AProcBuilding

/**
 * Before we save, if the LOQ quad material points to another package, we NULL it out
 */
void AProcBuilding::ClearLODQuadMaterial()
{
	// this is only needed if the low LOD mesh is in another level
	if (!LowLODPersistentActor)
	{
		return;
	}

	// look for LOD quad components
	for(INT LODQuadIndex = 0; LODQuadIndex < LODMeshComps.Num(); LODQuadIndex++)
	{
		// this component is going to be LOD quad, whose material will be the low LOD RTT material
		UStaticMeshComponent* Component = LODMeshComps(LODQuadIndex);

		// make sure the material is set up like we think it should be (if it's an old building, the quad pointed directly to the actor's material)
		if (Component->GetMaterial(0) == LowLODPersistentActor->StaticMeshComponent->StaticMesh->LODModels(0).Elements(0).Material)
		{
			// old style
			// clear out the material (that is pointing into the P map)
			Component->SetMaterial(0, NULL);
		}
		else
		{
			UMaterialInstanceConstant* QuadMIC = Cast<UMaterialInstanceConstant>(Component->GetMaterial(0));
			ensure(QuadMIC->Parent == LowLODPersistentActor->StaticMeshComponent->StaticMesh->LODModels(0).Elements(0).Material);
			QuadMIC->SetParent(NULL);
			Component->BeginDeferredReattach();
		}

		// delay the reset of the material until after the save has finished
		GEngine->DeferredCommands.AddUniqueItem(TEXT("FixupProcBuildingLODQuadsAfterSave"));
	}
}

/**
 * After saving (when it was NULLed) or loading (after NULL was loaded), we reset any
 * LOD Quad materials that should be pointing to another level, we reset it to the 
 * RTT mateial on the low LOD mesh
 */
void AProcBuilding::ResetLODQuadMaterial()
{
	// this is only needed if the low LOD mesh is in another level
	if (!LowLODPersistentActor)
	{
		return;
	}

	UBOOL bNeedsReattach = FALSE;
	for(INT LODQuadIndex = 0; LODQuadIndex < LODMeshComps.Num(); LODQuadIndex++)
	{
		// this component is going to be LOD quad
		UStaticMeshComponent* Component = LODMeshComps(LODQuadIndex);

		// reset the material to the low LOD building's RTT material
		if (Component->Materials(0) == NULL)
		{
			// old style, where the material was the LOD building's material, so just point it back to it
			Component->SetMaterial(0, LowLODPersistentActor->StaticMeshComponent->StaticMesh->LODModels(0).Elements(0).Material);
			if (Component->IsAttached())
			{
				bNeedsReattach = TRUE;
			}
		}
		else
		{
			// new style, where we have a MIC whose parent is the LOD building's material
			UMaterialInstanceConstant* QuadMIC = Cast<UMaterialInstanceConstant>(Component->GetMaterial(0));
			if (QuadMIC && QuadMIC->Parent == NULL)
			{
				if (QuadMIC != LowLODPersistentActor->StaticMeshComponent->StaticMesh->LODModels(0).Elements(0).Material)
				{
					QuadMIC->SetParent(LowLODPersistentActor->StaticMeshComponent->StaticMesh->LODModels(0).Elements(0).Material);
					if (Component->IsAttached())
					{
						bNeedsReattach = TRUE;
					}
				}
			}
		}
	}

	// reattach the components after setting the MIC parents to update render thread
	if (bNeedsReattach)
	{
		MarkComponentsAsDirty();
	}
}

/**
 * There are potential pointers to materials in the P map, on LOD Quads, which point to the low detail MIC.
 * We don't want all material pointers to be CPF_CrossLevel (plus its natively serialized), so we just
 * NULL it out before we save, and restore it when you load, since the P map will already be loaded. 
 */
void AProcBuilding::ClearCrossLevelReferences()
{
	// if the low LOD building is in another map, then we need to clean out LOD quad materials
	if (LowLODPersistentActor)
	{
		ClearLODQuadMaterial();
	}
}

/**
 * In PreSave, the LODQuad material pointers are NULLed out, this will fix them up again
 */
void AProcBuilding::FixupProcBuildingLODQuadsAfterSave()
{
	for (FActorIterator It; It; ++It)
	{
		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if (Building && Building->LowLODPersistentActor)
		{
			Building->ResetLODQuadMaterial();
		}
	}
}

/** Returns TRUE if this building would like to set some additional params on MICs applied to it */
UBOOL AProcBuilding::HasBuildingParamsForMIC()
{
	// See if either
	AProcBuilding* BaseBuilding = GetBaseMostBuilding();
	if( (BaseBuilding->BuildingMaterialParams.Num() > 0) ||
		(BuildingMaterialParams.Num() > 0) ||
		(BaseBuilding->ParamSwatchName != NAME_None) || 
		(ParamSwatchName != NAME_None) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** Get the ruleset used for this building volume. Will look at this override, and then base building if none set. */
UProcBuildingRuleset* AProcBuilding::GetRuleset()
{
	UProcBuildingRuleset* ProcRuleset = NULL;
#if WITH_EDITORONLY_DATA
	// here we are going to check to see if this Level has decided to override the ProcBuildingRuleset.
	// Overriding is useful for doing perf testing / load testing / being able to overcome memory issues in other tools until they are fixed
	// withOUT compromising the RuleSet creation process and the application of the rulesets in the level 
	if( ( GWorld != NULL ) && ( GWorld->GetWorldInfo() != NULL ) && ( GWorld->GetWorldInfo()->bUseProcBuildingRulesetOverride == TRUE ) )
	{
		ProcRuleset = GWorld->GetWorldInfo()->ProcBuildingRulesetOverride;
	}
	else
	{
		ProcRuleset = Ruleset;
		if(!ProcRuleset)
		{
			AProcBuilding* BaseBuilding = GetBaseMostBuilding();
			if(BaseBuilding)
			{
				ProcRuleset = BaseBuilding->Ruleset;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return ProcRuleset;
}

/** Update brush color, to indicate whether this is the 'base' building of a group */
void AProcBuilding::UpdateBuildingBrushColor()
{
#if WITH_EDITORONLY_DATA
	FColor NewBrushColor;

	if(AttachedBuildings.Num() == 0)
	{
		// Get default brush color
		AProcBuilding* DefBuilding = CastChecked<AProcBuilding>(GetClass()->GetDefaultActor());
		NewBrushColor = DefBuilding->BrushColor;
	}
	else
	{
		// Make 'base building' color
		// Thought about putting this in the class, but that uses mem...
		NewBrushColor = FColor(170,255,135);
	}

	BrushColor = NewBrushColor;

	// Will reattach component to pick up new brush color
	if(BrushComponent != NULL)
	{
		FComponentReattachContext Reattach(BrushComponent);
	}
#endif // WITH_EDITORONLY_DATA
}

/** Set any building-wide optional MIC params on the supplied MIC. */
void AProcBuilding::SetBuildingMaterialParamsOnMIC(UMaterialInstanceConstant* InMIC)
{
	if(InMIC)
	{
		AProcBuilding* BaseBuilding = GetBaseMostBuilding();

		// Get swatch name from building (or base building if its none on me)
		FName UseSwatchName = (ParamSwatchName != NAME_None) ? ParamSwatchName : BaseBuilding->ParamSwatchName;
		// If we get a name, try and apply params from swatch with that name
		if(UseSwatchName != NAME_None)
		{
			INT UseSwatchIndex = INDEX_NONE;

			// Get the ruleset..
			UProcBuildingRuleset* ThisRuleset = GetRuleset();
			if(ThisRuleset)
			{
				UseSwatchIndex = ThisRuleset->GetSwatchIndexFromName(UseSwatchName);
			}

			// Did we find a swatch we want to apply?
			if(UseSwatchIndex != INDEX_NONE)
			{
				FPBParamSwatch& Swatch = ThisRuleset->ParamSwatches(UseSwatchIndex);

				// we did - apply each parameter
				for(INT ParamIdx=0; ParamIdx<Swatch.Params.Num(); ParamIdx++)
				{
					FPBMaterialParam& ParamInfo = Swatch.Params(ParamIdx);
					if(ParamInfo.ParamName != NAME_None)
					{				
						InMIC->SetVectorParameterValue(ParamInfo.ParamName, ParamInfo.Color);
					}
				}
			}
		}

		// Then apply params from base building
		for(INT ParamIdx=0; ParamIdx<BaseBuilding->BuildingMaterialParams.Num(); ParamIdx++)
		{
			FPBMaterialParam& ParamInfo = BaseBuilding->BuildingMaterialParams(ParamIdx);
			if(ParamInfo.ParamName != NAME_None)
			{				
				InMIC->SetVectorParameterValue(ParamInfo.ParamName, ParamInfo.Color);
			}
		}
		
		// Then apply params from this building
		for(INT ParamIdx=0; ParamIdx<BuildingMaterialParams.Num(); ParamIdx++)
		{
			FPBMaterialParam& ParamInfo = BuildingMaterialParams(ParamIdx);
			if(ParamInfo.ParamName != NAME_None)
			{
				InMIC->SetVectorParameterValue(ParamInfo.ParamName, ParamInfo.Color);
			}
		}
	}
}

/** Components array is not serialized, so we make sure to add them here. */
void AProcBuilding::PostLoad()
{
	// Attach all building components
	for(INT i=0; i<BuildingMeshCompInfos.Num(); i++)
	{
		if(BuildingMeshCompInfos(i).MeshComp)
		{
			Components.AddItem(BuildingMeshCompInfos(i).MeshComp);	
		}
	}
	
	// Attach all fractured components
	for(INT i=0; i<BuildingFracMeshCompInfos.Num(); i++)
	{
		if(BuildingFracMeshCompInfos(i).FracMeshComp)
		{
			Components.AddItem(BuildingFracMeshCompInfos(i).FracMeshComp);	
		}
	}

	// ATtach simple building mesh
	if(SimpleMeshComp)
	{
		Components.AddItem(SimpleMeshComp);
	}

	// let the editor fixup maps with old buildings in them
	if (GetLinker() && GetLinker()->Ver() < VER_NEED_TO_CLEANUP_OLD_BUILDING_TEXTURES)
	{
		GEngine->DeferredCommands.AddUniqueItem(TEXT("CLEANUPOLDBUILDINGTEXTURES"));
	}

	// copy over MinDrawDistance to MassiveLODDistance for old content
	if (GetLinker() && GetLinker()->Ver() < VER_ADDED_CROSSLEVEL_REFERENCES && SimpleMeshComp)
	{
		SimpleMeshComp->MassiveLODDistance = SimpleMeshComp->MinDrawDistance;

		// update the building's version of the MassiveLOD distance from the component's setting
		SimpleMeshMassiveLODDistance = SimpleMeshComp->MassiveLODDistance;
	}

	// if the low LOD building is in another map, then we need to fixup LOD quad materials to point back to where they should be
	if (LowLODPersistentActor)
	{
		ResetLODQuadMaterial();
	}

#if WITH_EDITORONLY_DATA
	// For older content, make sure we are in the base list
	AProcBuilding* NewBaseBuilding = Cast<AProcBuilding>(Base);
	if(NewBaseBuilding)
	{
		NewBaseBuilding->AttachedBuildings.AddItem(this);
	}

	// check for empty AttachedBuildings entries
	for ( INT i=0; i<AttachedBuildings.Num(); i++ )
	{
		if ( (AttachedBuildings(i) == NULL) || (AttachedBuildings(i)->Base != this) || AttachedBuildings(i)->bDeleteMe )
		{
			AttachedBuildings.Remove(i--);
		}
	}
#endif // WITH_EDITORONLY_DATA

	UpdateBuildingBrushColor();

	Super::PostLoad();
}

/**
 * This is called on actors when they are added to the world after streaming
 */
void AProcBuilding::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	ResetLODQuadMaterial();
}

/**
 * This callback is called when a pointer inside this object was set via delayed cross level reference fixup (it had a cross level
 * reference, but the other level wasn't loaded until now, which set the pointer
 */
void AProcBuilding::PostCrossLevelFixup()
{
	// if the low LOD building is in another map, then we need to fixup LOD quad materials to point back to where they should be
	if (LowLODPersistentActor)
	{
		ResetLODQuadMaterial();
	}
}


void AProcBuilding::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsPendingKill())
	{
		// for some properties, we don't need to rebuild the entire building, just push the change down to the component and reattach
		UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
		if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("SimpleMeshMassiveLODDistance"))
		{
			if (LowLODPersistentActor)
			{
				LowLODPersistentActor->StaticMeshComponent->MassiveLODDistance = SimpleMeshMassiveLODDistance;
				LowLODPersistentActor->ReattachComponent(LowLODPersistentActor->StaticMeshComponent);
			}
			else if(SimpleMeshComp)
			{
				SimpleMeshComp->MassiveLODDistance = SimpleMeshMassiveLODDistance;
				ReattachComponent(SimpleMeshComp);
			}
		}

		// Set collision on the brush component based on exposed flag
		BrushComponent->CollideActors = bBuildingBrushCollision;

		// delay post an update
		PostUpdateCallback(this);
	}
}

void AProcBuilding::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);

	if(bFinished)
	{
		PostUpdateCallback(this);
	}
}

void AProcBuilding::PostEditImport()
{
	Super::PostEditImport();

	// After import/duplicate etc., make sure that LODs are still ok
	GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("FixProcBuildingLODs %s"), *GetPathName()));
}

/** Called after using geom mode to edit thie brush's geometry */
void AProcBuilding::PostEditBrush()
{
	Super::PostEditBrush();
	
	PostUpdateCallback(this);
}

/**
 * If this building is linked to another staticmesh actor, it should be destroyed as well
 */
void AProcBuilding::PostScriptDestroyed()
{
	// destroy the low LOD actor if it exists
	if (LowLODPersistentActor)
	{
		GWorld->DestroyActor(LowLODPersistentActor);
	}
}

/** Used to update AttachedBuildings list. */
void AProcBuilding::SetBase(AActor *NewBase, FVector NewFloor, INT bNotifyActor, USkeletalMeshComponent* SkelComp, FName BoneName)
{
	// First remove myself from previous base list
	AProcBuilding* OldBaseBuilding = Cast<AProcBuilding>(Base);
	if(OldBaseBuilding)
	{
#if WITH_EDITORONLY_DATA
		OldBaseBuilding->AttachedBuildings.RemoveItem(this);
#endif // WITH_EDITORONLY_DATA
		OldBaseBuilding->UpdateBuildingBrushColor();
	}
	
	// Do normal basing code
	Super::SetBase(NewBase, NewFloor, bNotifyActor, SkelComp, BoneName);
	
	// Now add myself to my new base's list
	AProcBuilding* NewBaseBuilding = Cast<AProcBuilding>(Base);
	if(NewBaseBuilding)
	{
#if WITH_EDITORONLY_DATA
		NewBaseBuilding->AttachedBuildings.AddItem(this);
#endif // WITH_EDITORONLY_DATA
		NewBaseBuilding->UpdateBuildingBrushColor();
	}	
}


/** Util to track max/min Z value of scopes */
static void UpdateScopeMinMaxZ(const FPBScope2D& Scope, FLOAT& MinZ, FLOAT& MaxZ)
{
	FLOAT OriginZ = Scope.ScopeFrame.GetOrigin().Z;
	MinZ = Min(MinZ, OriginZ);
	MaxZ = Max(MaxZ, OriginZ + (Scope.ScopeFrame.GetAxis(2).Z * Scope.DimZ));
}

/** Apply a transformation matrix to TM */
static void TransformScope2D(FPBScope2D& Scope, const FMatrix& TM)
{
	Scope.ScopeFrame *= TM;
}


static FLOAT EDGE_TEST_EPSILON = 1.f;

/** Struct for holding an edge segment, specified by start and end location */
struct FEdgeSegment
{
	/** Start location of edge segment */
	FVector	Start;
	/** End location of edge segment */
	FVector	End;

	/** Function to return length of edge segment */
	FLOAT GetLength()
	{
		return (End-Start).Size();
	}
};


static EScopeEdge ScopeIsTopOrRightOf(const FPBScope2D& Scope, const FPBScope2D& TestScope, FEdgeSegment& OutEdge)
{
	OutEdge.Start = OutEdge.End = FVector(0.f);

	// These vectors should all be unit length
	FVector ScopeX = Scope.ScopeFrame.GetAxis(0);
	FVector ScopeY = Scope.ScopeFrame.GetAxis(1);
	FVector ScopeZ = Scope.ScopeFrame.GetAxis(2);

	FVector TestScopeX = TestScope.ScopeFrame.GetAxis(0);
	FVector TestScopeY = TestScope.ScopeFrame.GetAxis(1);
	FVector TestScopeZ = TestScope.ScopeFrame.GetAxis(2);

	// Vector from Scope to TestSCope origin
	FVector TestFromScopeOrigin = TestScope.ScopeFrame.GetOrigin() - Scope.ScopeFrame.GetOrigin();
	
	// Check for scopes on top of each other
	FLOAT ScopeDist = TestFromScopeOrigin.Size();	
	if(ScopeDist < EDGE_TEST_EPSILON)
	{
		//debugf(TEXT("Scopes have same location!"));
		return EPSA_None;
	}
	
	// See if the origin of TestScope lies on the plane of Scope
	UBOOL bTestOriginOnScope = (Abs(TestFromScopeOrigin | ScopeY) < EDGE_TEST_EPSILON);
	
	// If this is not the case, cannot be top or right
	if(bTestOriginOnScope)
	{
		// Find out distance of test origin along scope's X and Z axis
		FLOAT TestOriginXDist = TestFromScopeOrigin | ScopeX;
		FLOAT TestOriginZDist = TestFromScopeOrigin | ScopeZ;
	
		// See if X axis are parallel - this means they could be joined along X edge
		if( appIsNearlyEqual(ScopeX | TestScopeX, 1.f, 0.01f) )
		{
			// See if origin is on top edge of scope (ie DimZ away)
			if( appIsNearlyEqual(TestOriginZDist, Scope.DimZ, EDGE_TEST_EPSILON) )
			{
				FLOAT OverlapMin = Max(TestOriginXDist, 0.f);
				FLOAT OverlapMax = Min(TestOriginXDist + TestScope.DimX, Scope.DimX);
				FLOAT OverlapAmount = OverlapMax - OverlapMin;


				// Finally check that there is at least some overlap between X edges
				if( OverlapAmount > 0.1f )
				{
					OutEdge.Start = TestScope.ScopeFrame.GetOrigin() + (OverlapMin * TestScopeX);
					OutEdge.End = TestScope.ScopeFrame.GetOrigin() + (OverlapMax * TestScopeX);
			
					return EPSA_Top;
				}				
			}
		}

		// See if Z axis are parallel - this means they could be joined along Z edge
		if( appIsNearlyEqual(ScopeZ | ScopeZ, 1.f, 0.01f) )
		{
			// See if origin is on top edge of scope (ie DimX away)
			if( appIsNearlyEqual(TestOriginXDist, Scope.DimX, EDGE_TEST_EPSILON) )
			{
				FLOAT OverlapMin = Max(TestOriginZDist, 0.f);
				FLOAT OverlapMax = Min(TestOriginZDist + TestScope.DimZ, Scope.DimZ);
				FLOAT OverlapAmount = OverlapMax - OverlapMin;

				// Finally check that there is at least some overlap between Z edges
				if( OverlapAmount > 0.1f )
				{
					OutEdge.Start = TestScope.ScopeFrame.GetOrigin() + (OverlapMin * TestScopeZ);
					OutEdge.End = TestScope.ScopeFrame.GetOrigin() + (OverlapMax * TestScopeZ);

					return EPSA_Right;
				}
			}
		}
	}
	
	return EPSA_None;
}

static EScopeEdge ScopeIsAdjacentTo(const FPBScope2D& Scope, const FPBScope2D& TestScope, FEdgeSegment& OutEdge)
{
	OutEdge.Start = OutEdge.End = FVector(0.f);

	// First see if TestScope is top or right of Scope
	EScopeEdge Result = ScopeIsTopOrRightOf(Scope, TestScope, OutEdge);	
	if(Result != EPSA_None)
	{
		return Result;
	}
	
	// If not, reverse order and see if Scope is top or right of TestScope
	Result = ScopeIsTopOrRightOf(TestScope, Scope, OutEdge);
	// If it is, we need to change things around so we are relative to Scope
	if(Result == EPSA_Top)
	{
		return EPSA_Bottom;
	}
	else if(Result == EPSA_Right)
	{
		return EPSA_Left;
	}
	
	// No adjacency found
	return EPSA_None;
}

/** Given the index to a scope in the supplied array, find which other scopes are the neighbors on each edge. */
static void FindScopeNeighbors(TArray<FPBScope2D>& AllScopes, INT NumMeshedScopes, INT ScopeIndex, TArray<FLOAT>& EdgeAngles, TArray<FEdgeSegment>& EdgeSegments, TArray<INT>& NeighborScopeIndex)
{
	check(ScopeIndex < AllScopes.Num());
	check(ScopeIndex <= NumMeshedScopes);
	check(NumMeshedScopes <= AllScopes.Num());

	EdgeAngles.Empty();
	EdgeAngles.AddZeroed(4);

	// Init all neighbors to 'index none'
	NeighborScopeIndex.Empty();
	NeighborScopeIndex.AddZeroed(4);
	for(INT NeighIdx=0; NeighIdx<4; NeighIdx++)
	{
		NeighborScopeIndex(NeighIdx) = INDEX_NONE;
	}

	EdgeSegments.Empty();
	EdgeSegments.AddZeroed(4);

	FVector ScopeNormal = AllScopes(ScopeIndex).ScopeFrame.GetAxis(1);
	
	// Remember how much faces found overlap the edges of this scope, so we can pick the one with the most overlap
	TArray<FLOAT> EdgeOverlaps;
	EdgeOverlaps.AddZeroed(4);
	
	//debugf(TEXT("FindScopeEdgeAngles %d"), ScopeIndex);
	for(INT i=0; i<NumMeshedScopes; i++)	
	{
		if(i != ScopeIndex)
		{
			// Get axes normal and in plane of test scope
			FVector TestNormal = AllScopes(i).ScopeFrame.GetAxis(1);
			FVector TestScopeX = AllScopes(i).ScopeFrame.GetAxis(0);
			FVector TestScopeZ = AllScopes(i).ScopeFrame.GetAxis(2);

			// See if this scope connects, and where
			FEdgeSegment Segment;
			Segment.End = Segment.Start = FVector(0.f);
			EScopeEdge Adj = ScopeIsAdjacentTo( AllScopes(ScopeIndex), AllScopes(i),  Segment);
						
			INT AngleIndex = INDEX_NONE;
			
			// This is a vector that points along surface of adjacent test scope, away from the chosen scope			
			FVector SurfaceVec(0,0,0); 
			
			if(Adj == EPSA_Top)
			{
				AngleIndex = 0;
				SurfaceVec = TestScopeZ;
			}
			else if(Adj == EPSA_Bottom)
			{
				AngleIndex = 1;
				SurfaceVec = -TestScopeZ;
			}
			else if(Adj == EPSA_Left)
			{			
				AngleIndex = 2;
				SurfaceVec = -TestScopeX;
			}
			else if(Adj == EPSA_Right)
			{			
				AngleIndex = 3;
				SurfaceVec = TestScopeX;
			}
			
			// Calculate angle (in degrees)
			FLOAT Angle = appAcos(TestNormal | ScopeNormal) * (180.f / (FLOAT)PI);
			// Detect acute angles by seeing if surface continues 'in front' of current scope
			if((SurfaceVec | ScopeNormal) > 0.f)
			{
				Angle *= -1.f;
			}

			// If this is connected, and has a smaller angle (ie corner is move concave)
			if( (AngleIndex != INDEX_NONE) && (NeighborScopeIndex(AngleIndex) == INDEX_NONE || Angle < EdgeAngles(AngleIndex)) )
			{
				NeighborScopeIndex(AngleIndex) = i;

				//debugf(TEXT("GOOD OVERLAP %f %d"), EdgeOverlap, AngleIndex);
				EdgeOverlaps(AngleIndex) = Segment.GetLength();
				EdgeSegments(AngleIndex) = Segment;
				
				// Fill in entry
				EdgeAngles(AngleIndex) = Angle;
			}
		}
	}
}

/** 
 * Given an index of a scope in the TopLevelsScopes array (and which edge of that scope), returns index into EdgeInfos with that edge's info.
 * Value of INDEX_NONE may be returned, indicating edge could not be found, which may indicate this is a scope-poly edge instead of scope-scope.
 */
INT AProcBuilding::FindEdgeForTopLevelScope(INT TopLevelScopeIndex, BYTE Edge)
{
#if WITH_EDITORONLY_DATA
	for(INT EdgeIdx=0; EdgeIdx<EdgeInfos.Num(); EdgeIdx++)
	{
		const FPBEdgeInfo& EdgeInfo = EdgeInfos(EdgeIdx);
		if( ((EdgeInfo.ScopeAIndex == TopLevelScopeIndex) && (EdgeInfo.ScopeAEdge == Edge)) || 
			((EdgeInfo.ScopeBIndex == TopLevelScopeIndex) && (EdgeInfo.ScopeBEdge == Edge)) )
		{
			return EdgeIdx;
		}
	}
#endif // WITH_EDITORONLY_DATA

	return INDEX_NONE;
}


/** Util to take one edge, and return the opposite one */
EScopeEdge GetOppositeEdge(EScopeEdge Edge)
{
	switch(Edge)
	{
	case EPSA_Top:
		return EPSA_Bottom;
	case EPSA_Bottom:
		return EPSA_Top;
	case EPSA_Left:
		return EPSA_Right;
	case EPSA_Right:
		return EPSA_Left;
	case EPSA_None:
		return EPSA_None;
	}

	return EPSA_None;
}

/** Util to return info about the face at an edge which is not the specified one. */
void EdgeInfoGetOtherScope(const FPBEdgeInfo& EdgeInfo, INT ThisScopsIndex, INT& OtherScopeIndex, EScopeEdge& OtherScopeEdge)
{
	// Should never have an edge with the same face, or an edge where both faces are INDEX_NONE!
	check(EdgeInfo.ScopeAIndex != EdgeInfo.ScopeBIndex);

	OtherScopeIndex = INDEX_NONE;
	OtherScopeEdge = EPSA_None;

	if(EdgeInfo.ScopeAIndex == ThisScopsIndex)
	{
		OtherScopeIndex = EdgeInfo.ScopeBIndex;
		OtherScopeEdge = EScopeEdge(EdgeInfo.ScopeBEdge);
	}
	else if(EdgeInfo.ScopeBIndex == ThisScopsIndex)
	{
		OtherScopeIndex = EdgeInfo.ScopeAIndex;
		OtherScopeEdge = EScopeEdge(EdgeInfo.ScopeAEdge);
	}
}


/** Update the internal EdgeInfos array, using the ToplevelScopes array. Only scope-scope edges currently. */
void AProcBuilding::UpdateEdgeInfos()
{
#if WITH_EDITORONLY_DATA
	EdgeInfos.Empty();

	for(INT ScopeIdx=0; ScopeIdx<TopLevelScopes.Num(); ScopeIdx++)
	{
		TArray<FLOAT> EdgeAngles;
		TArray<INT> NeighborIndex;
		TArray<FEdgeSegment> Segments;
		FindScopeNeighbors(TopLevelScopes, TopLevelScopes.Num(), ScopeIdx, EdgeAngles, Segments, NeighborIndex);

		// Iterate over each edge
		for(INT EdgeIdx=0; EdgeIdx<4; EdgeIdx++)
		{
			// If something is connected along this edge
			if(NeighborIndex(EdgeIdx) != INDEX_NONE)
			{
				// see if we already have this connection
				INT FoundEdgeIdx = FindEdgeForTopLevelScope(ScopeIdx, EdgeIdx);

				// Not found - add entry
				if(FoundEdgeIdx == INDEX_NONE)
				{
					INT NewEdgeIdx = EdgeInfos.AddZeroed();
					EdgeInfos(NewEdgeIdx).ScopeAIndex = ScopeIdx;
					EdgeInfos(NewEdgeIdx).ScopeAEdge = EdgeIdx;

					EdgeInfos(NewEdgeIdx).ScopeBIndex = NeighborIndex(EdgeIdx);
					EdgeInfos(NewEdgeIdx).ScopeBEdge = GetOppositeEdge(EScopeEdge(EdgeIdx));

					EdgeInfos(NewEdgeIdx).EdgeAngle = EdgeAngles(EdgeIdx);
					EdgeInfos(NewEdgeIdx).EdgeStart = Segments(EdgeIdx).Start;
					EdgeInfos(NewEdgeIdx).EdgeEnd = Segments(EdgeIdx).End;
				}
				// Found entry - check that we  match the other way around
				else
				{
					const FPBEdgeInfo& EdgeInfo = EdgeInfos(FoundEdgeIdx);

					INT OtherScopeIdx;
					EScopeEdge OtherEdge;
					EdgeInfoGetOtherScope(EdgeInfo, ScopeIdx, OtherScopeIdx, OtherEdge);					

					if(OtherScopeIdx != NeighborIndex(EdgeIdx) || OtherEdge != GetOppositeEdge(EScopeEdge(EdgeIdx)))
					{
						debugf(TEXT("Warning (%s) - Unmatched scope relationship found!"), *GetPathName());
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}


/** Find other buildings that are grouped on, or overlap, this one */
void AProcBuilding::FindOverlappingBuildings(TArray<AProcBuilding*>& OutOverlappingBuildings)
{
	OutOverlappingBuildings.Empty();

	// Find all other buildings whose bounding boxes overlap this one
	if(BrushComponent)
	{
		// Find this buildings bounding box
		FBox MyBox = BrushComponent->Bounds.GetBox().ExpandBy(16.f);

		// Check against octree to find all overlapping components
		TArray<UPrimitiveComponent*> Components;
		GWorld->Hash->GetIntersectingPrimitives(MyBox, Components);
		
		// Iterate over what we found
		for(INT CompIdx=0; CompIdx<Components.Num(); CompIdx++)
		{
			// .. check if its a brush component
			UBrushComponent* BrushComp = Cast<UBrushComponent>(Components(CompIdx));
			if(BrushComp)
			{
				// .. and its owner is a building
				AProcBuilding* OverlapBuilding = Cast<AProcBuilding>(BrushComp->GetOwner());
				if(OverlapBuilding)
				{
					// add to output list
					OutOverlappingBuildings.AddUniqueItem(OverlapBuilding);
				}
			}
		}
	}
}

/** Recursively add all buildings attached to this one */
static void GetAllGroupedBuildings(AProcBuilding* InBuilding, TArray<AProcBuilding*>& InAllBuildings)
{
#if WITH_EDITORONLY_DATA
	// If no building passed in, nothing to do!
	if(!InBuilding)
	{
		return;
	}

	// do not add the building if the brush has some how gone missing
	if(InBuilding->Brush)
	{
		InAllBuildings.AddItem(InBuilding);
	}


	// Find all things we connect to
	TArray<AProcBuilding*> ExploreSet = InBuilding->AttachedBuildings;
	
	AProcBuilding* BaseBuilding = Cast<AProcBuilding>(InBuilding->Base);
	if( (BaseBuilding) && (BaseBuilding->Brush) )
	{
		ExploreSet.AddItem(BaseBuilding);
	}
	
	// Go to them if not already visited
	for(INT i=0; i<ExploreSet.Num(); i++)
	{
		if(!InAllBuildings.ContainsItem(ExploreSet(i)))
		{
			GetAllGroupedBuildings(ExploreSet(i), InAllBuildings);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Find all SplineActors connected to (and including) this one. Note - SLOW, but works in Editor (where Attachments array is not up to date) */
void AProcBuilding::GetAllGroupedProcBuildings(TArray<class AProcBuilding*>& OutSet)
{
	OutSet.Empty();

	GetAllGroupedBuildings(this, OutSet);
}

TArray<FVector> GetSamplesAcrossScope(const FPBScope2D& Scope, const FMatrix& ScopeToWorld)
{
	TArray<FVector> SampleLocations;

	// Corners of testing region
	const FLOAT OffSurfaceCheck = 8.f;
	FVector MinXVec = Scope.ScopeFrame.GetAxis(0) * OffSurfaceCheck;
	FVector MaxXVec = Scope.ScopeFrame.GetAxis(0) * (Scope.DimX - OffSurfaceCheck);
	FVector MinZVec = Scope.ScopeFrame.GetAxis(2) * OffSurfaceCheck;
	FVector MaxZVec = Scope.ScopeFrame.GetAxis(2) * (Scope.DimZ - OffSurfaceCheck);
	FVector YVec = Scope.ScopeFrame.GetAxis(1) * OffSurfaceCheck;

	// How many steps in X and Z
	FLOAT XSize = (MaxXVec - MinXVec).Size();
	INT XSteps = Max<INT>( appCeil(XSize/256.f), 2 );
	FLOAT ZSize = (MaxZVec - MinZVec).Size();
	INT ZSteps = Max<INT>( appCeil(ZSize/256.f), 2 );

	const FVector ScopeOrigin = Scope.ScopeFrame.GetOrigin();

	// Iterate over X and then Z, generating points
	for(INT XIdx=0; XIdx<XSteps; XIdx++)
	{
		FLOAT XAlpha = (FLOAT)XIdx/(FLOAT)(XSteps-1);
		FVector XVec = Lerp(MinXVec, MaxXVec, XAlpha);

		for(INT ZIdx=0; ZIdx<ZSteps; ZIdx++)
		{
			FLOAT ZAlpha = (FLOAT)ZIdx/(FLOAT)(ZSteps-1);
			FVector ZVec = Lerp(MinZVec, MaxZVec, ZAlpha);

			FVector LocalVec = ScopeOrigin + XVec + ZVec + YVec;
			// Transform into world space and add to array
			SampleLocations.AddItem( ScopeToWorld.TransformFVector(LocalVec) );
		}
	}

	return SampleLocations;
}

/** Test to see if a scope is entirely inside any one convex piece of the supplied volume */
static UBOOL ScopeIsInsideBuilding(const TArray<AProcBuilding*> TestBuildings, const FPBScope2D& Scope, AProcBuilding* ScopeBuilding, const FMatrix& ScopeToWorld)
{
	// Get locations on surface of scope we want to test in world space.
	TArray<FVector> SampleLocations = GetSamplesAcrossScope(Scope, ScopeToWorld);

	// Array to track hit status of each sample
	TArray<UBOOL> SampleHit;
	SampleHit.AddZeroed(SampleLocations.Num());

	// Iterate over each building in group
	for(INT BuildingIdx=0; BuildingIdx<TestBuildings.Num(); BuildingIdx++)
	{
		AProcBuilding* TestBuilding = TestBuildings(BuildingIdx);

		// Check building has a brush component, and doesn't own the scope
		if(TestBuilding && TestBuilding->BrushComponent && (TestBuilding != ScopeBuilding))
		{
			// Get geom of building
			FKAggregateGeom* BrushAggGeom = &TestBuilding->BrushComponent->BrushAggGeom;

			// Check each geometry that makes up this building, to see which samples are inside it
			for(INT GeomIdx=0; GeomIdx<BrushAggGeom->ConvexElems.Num(); GeomIdx++)
			{
				// Iterate over each sample
				for(INT SampleIdx=0; SampleIdx<SampleLocations.Num(); SampleIdx++)
				{
					// If not already hit, test this sample.
					if(!SampleHit(SampleIdx))
					{
						// Transform world-space corner location into brush space
						FVector LocalSample = TestBuilding->BrushComponent->LocalToWorld.InverseTransformFVector(SampleLocations(SampleIdx));

						// Test point against convex
						FVector OutNormal;
						FLOAT OutDist;
						const UBOOL bInConvex = BrushAggGeom->ConvexElems(GeomIdx).PointIsWithin(LocalSample, OutNormal, OutDist);		

						// If any corner is not inside, give up
						if(bInConvex)
						{
							//GWorld->PersistentLineBatcher->DrawLine(FVector(0,0,0), SampleLocations(SampleIdx), FColor(255,0,0), SDPG_World);
							SampleHit(SampleIdx) = TRUE;
						}
					}
				}
			}
		}
	}

	// Check if all samples got hit
	for(INT SampleIdx=0; SampleIdx<SampleLocations.Num(); SampleIdx++)
	{
		// Found one sample that was not hit - we cannot remove this scope, its not all interior
		if(!SampleHit(SampleIdx))
		{
			return FALSE;
		}
	}

	// Scope was not found inside any geometry
	return TRUE;
}

/** Util to remove scopes that are entirely occluded by another building in the group */
static void RemoveOccludedScopes(TArray<FPBScope2D>& TopLevelScopes, TArray<FPBScopeProcessInfo>& ScopeInfos, const FMatrix& ScopesToWorld, const TArray<AProcBuilding*> GroupBuildings)
{
	check(TopLevelScopes.Num() == ScopeInfos.Num());

	// For each scope that we found..
	for(INT ScopeIdx=TopLevelScopes.Num()-1; ScopeIdx>=0; ScopeIdx--)
	{
		FPBScope2D& Scope = TopLevelScopes(ScopeIdx);
		AProcBuilding* ScopeBuilding = ScopeInfos(ScopeIdx).OwningBuilding;

		if( ScopeIsInsideBuilding(GroupBuildings, Scope, ScopeBuilding, ScopesToWorld) )
		{
			TopLevelScopes.Remove(ScopeIdx);
			ScopeInfos.Remove(ScopeIdx);
		}
	}	
}

/** Utility to find min and max Z value for a polygon */
static void GetPolyMinMaxZ(const FPoly& Poly, FLOAT& OutMinZ, FLOAT& OutMaxZ)
{
	OutMaxZ = -BIG_NUMBER;
	OutMinZ = BIG_NUMBER;
	
	for(INT VertIdx=0; VertIdx<Poly.Vertices.Num(); VertIdx++)
	{
		FVector ActorVert = Poly.Vertices(VertIdx);
		OutMaxZ = Max(ActorVert.Z, OutMaxZ);
		OutMinZ = Min(ActorVert.Z, OutMinZ);
	}
}

/** Tranforms an FPoly by a supplied matrix */
static void TransformFPoly(FPoly& Poly, const FMatrix& TM)
{
	for(INT VertIdx=0; VertIdx<Poly.Vertices.Num(); VertIdx++)
	{
		Poly.Vertices(VertIdx) = TM.TransformFVector(Poly.Vertices(VertIdx));
	}

	Poly.CalcNormal();
	Poly.TextureU = TM.TransformNormal(Poly.TextureU);
	Poly.TextureV = TM.TransformNormal(Poly.TextureV);
}

/** Iterate over all EdgeInfos and find the first one that starts or ends at the location provided */
static INT FindEdgeEndingAt(AProcBuilding* Building, const FVector& Location)
{
#if WITH_EDITORONLY_DATA
	for(INT EdgeIdx=0; EdgeIdx<Building->EdgeInfos.Num(); EdgeIdx++)
	{
		const FPBEdgeInfo& Edge = Building->EdgeInfos(EdgeIdx);
		if(Edge.EdgeEnd.Equals(Location, 0.1f) || Edge.EdgeStart.Equals(Location, 0.1f))
		{
			return EdgeIdx;
		}
	}
#endif // WITH_EDITORONLY_DATA

	return INDEX_NONE;
}

/** Util to remove any duplicate verts (ie edges that are too short) that may be present in a poly */
static void RemoveDuplicatePolyVerts(FPoly& InPoly)
{
	// Iterate over each vert
	for(INT VertIdx=0; VertIdx<InPoly.Vertices.Num(); VertIdx++)
	{
		INT NextVertIdx = (VertIdx + 1)%InPoly.Vertices.Num();

		// Get distance to next vert
		FVector VertPos = InPoly.Vertices(VertIdx);
		FVector NextVertPos = InPoly.Vertices(NextVertIdx);

		// If distance is too small, remove this one, and decrement current index
		if((NextVertPos - VertPos).Size() < 1.f)
		{
			InPoly.Vertices.Remove(VertIdx);
			VertIdx--;
		}
	}
}

/** Try and merge BPoly into APoly. Will only succeed if APoly and BPoly have some amount of edge overlap. */
static UBOOL AttemptCombinePolys(FPoly& APoly, const FPoly& BPoly)
{
	const FLOAT MinEdgeLenToCombine = 1.f;
	const FLOAT MinEdgeDistToCombine = 1.f;

	// First, check polys have same normal
	if( !appIsNearlyEqual(APoly.Normal | BPoly.Normal, 1.f, (FLOAT)KINDA_SMALL_NUMBER) )
	{
		return FALSE;
	}

	INT NumAVerts = APoly.Vertices.Num();
	INT NumBVerts = BPoly.Vertices.Num();

	TArray<FVector> NewAVerts;

	// Iterate over each edge of A
	for(INT AEdgeIdx=0; AEdgeIdx<NumAVerts; AEdgeIdx++)
	{
		// Get edge start and end vertex
		INT AStartVertIdx = AEdgeIdx;
		INT AEndVertIdx = (AStartVertIdx+1)%NumAVerts;
		// and edge vector and magnitude
		FVector AEdge = APoly.Vertices(AEndVertIdx) - APoly.Vertices(AStartVertIdx);
		FLOAT AEdgeLen = AEdge.Size();

		// If reasonable length, calc direction as well
		if(AEdgeLen > MinEdgeLenToCombine)
		{
			FVector AEdgeDir = AEdge/AEdgeLen;

			// Iterate over each edge of B
			for(INT BEdgeIdx=0; BEdgeIdx<NumBVerts; BEdgeIdx++)
			{
				// Get edge dir/mag
				INT BStartVertIdx = BEdgeIdx;
				INT BEndVertIdx = (BStartVertIdx+1)%NumBVerts;
				FVector BEdge = BPoly.Vertices(BEndVertIdx) - BPoly.Vertices(BStartVertIdx);
				FLOAT BEdgeLen = BEdge.Size();

				// If this is also reasonable length
				if(BEdgeLen > MinEdgeLenToCombine)
				{
					FVector BEdgeDir = BEdge/BEdgeLen;

					// Can only combine these polys at this edge if edges run in opposite directions
					if(appIsNearlyEqual(AEdgeDir | BEdgeDir, -1.f, (FLOAT)KINDA_SMALL_NUMBER) )
					{
						// Project BEnd along AEdgeDir
						FVector BEndLocal = BPoly.Vertices(BEndVertIdx) - APoly.Vertices(AStartVertIdx);
						FLOAT BEndDist = BEndLocal | AEdgeDir;

						// We have to do a test here to check edges are co-linear
						// taking the vec from AStart to BEnd, we remove any component that is along AEdgeDir
						// if there is anything left, they are not co-linear, so we reject this merge.
						FVector NonEdgeComponent = BEndLocal - (BEndDist * AEdgeDir);
						if(NonEdgeComponent.Size() < MinEdgeDistToCombine)
						{
							// Now project AEnd and BStart as well
							// Remember that B edge runs opposite to A edge, so BStart is furthest point from AStart
							FVector AEndLocal = APoly.Vertices(AEndVertIdx) - APoly.Vertices(AStartVertIdx);
							FLOAT AEndDist = AEndLocal | AEdgeDir;

							FVector BStartLocal = BPoly.Vertices(BStartVertIdx) - APoly.Vertices(AStartVertIdx);
							FLOAT BStartDist = BStartLocal | AEdgeDir;

							// Now check that we have a region of overlap
							UBOOL bFailOverlap = (BStartDist < 0.f) || (BEndDist > AEndDist);
							if(!bFailOverlap)
							{
								// Ok - we can merge! Will now modify APoly

								// First copy verts from A, up to and including AStart
								for(INT AddIdx=0; AddIdx<=AStartVertIdx; AddIdx++)
								{
									NewAVerts.AddItem( APoly.Vertices(AddIdx) );
								}

								// Then copy all the verts from B, starting with BEnd
								for(INT AddIdx=BEndVertIdx; AddIdx<NumBVerts; AddIdx++)
								{
									NewAVerts.AddItem( BPoly.Vertices(AddIdx) );								
								}

								for(INT AddIdx=0; AddIdx<BEndVertIdx; AddIdx++)
								{
									NewAVerts.AddItem( BPoly.Vertices(AddIdx) );								
								}

								// Finally copy the rest of the A verts
								for(INT AddIdx=AStartVertIdx+1; AddIdx<NumAVerts; AddIdx++)
								{
									NewAVerts.AddItem( APoly.Vertices(AddIdx) );
								}

								// Update APoly 
								APoly.Vertices = NewAVerts;
								RemoveDuplicatePolyVerts(APoly);
								// And say that we succeeded
								return TRUE;
							}
						}						
					}
				}
			}
		}
	}

	return FALSE;
}

/** Given an input set of polys/rulesets, attempt to combine polys where they touch along an edge.  */
static void CombineAdjacentPolys(TArray<FPoly>& InPolys, TArray<UProcBuildingRuleset*>& InRulesets, TArray<FPoly>& OutPolys, TArray<UProcBuildingRuleset*>& OutRulesets)
{
	OutPolys.Empty();

	// Iterate over each input poly
	for(INT InPolyIdx=0; InPolyIdx<InPolys.Num(); InPolyIdx++)
	{
		FPoly& InPoly = InPolys(InPolyIdx);
		UBOOL bMerged = FALSE;

		// See if we can merge it into one of our existing output polys
		for(INT OutPolyIdx=0; OutPolyIdx<OutPolys.Num(); OutPolyIdx++)
		{
			if( AttemptCombinePolys(OutPolys(OutPolyIdx), InPoly) )
			{
				bMerged = TRUE;
				break;
			}
		}

		if(!bMerged)
		{
			OutPolys.AddItem(InPoly);
			OutRulesets.AddItem( InRulesets(InPolyIdx) );
		}
	}
}

/** Take a poly, and move all verts in along the average of the two edge 'normals' (in plane of poly) */
static void InsetPolyVerts(FPoly& Poly, FLOAT InsetAmount)
{
	// We have to make a copy of the verts, so we use the 'original' edge directions, not the half-modified ones as we go around
	TArray<FVector> OldVerts = Poly.Vertices;
	INT NumInVerts = Poly.Vertices.Num();

	for(INT VertIdx=0; VertIdx<NumInVerts; VertIdx++)
	{
		// Find this vert, and vert on either side
		const FVector ThisVert = OldVerts(VertIdx);
		const FVector NextVert = OldVerts((VertIdx+1)%NumInVerts);
		INT PrevVertIndex = (VertIdx == 0) ? (NumInVerts-1) : (VertIdx-1);
		const FVector PrevVert = OldVerts(PrevVertIndex);

		// Find each edge, and then bi-normal (in plane of poly)
		const FVector Edge1 = (ThisVert - PrevVert).SafeNormal();
		const FVector Edge1Normal = (Poly.Normal ^ Edge1).SafeNormal();

		const FVector Edge2 = (NextVert - ThisVert).SafeNormal();
		const FVector Edge2Normal = (Poly.Normal ^ Edge2).SafeNormal();
		
		FPlane Plane1(ThisVert + (Edge1Normal * InsetAmount), Edge1Normal);
		FPlane Plane2(ThisVert + (Edge2Normal * InsetAmount), Edge2Normal);
		FPlane PolyPlane(ThisVert, Poly.Normal);

		FVector Intersect;
		UBOOL bSuccess = FIntersectPlanes3(Intersect, Plane1, Plane2, PolyPlane);

		if(!bSuccess)
		{
			Intersect = Poly.Vertices(VertIdx) + (Edge1Normal * InsetAmount);
		}

		// And move along that direction by pull in amount
		Poly.Vertices(VertIdx) = Intersect;
	}
}

/** Util for finding the corner node that 'owns' a particular edge in the building */
static UPBRuleNodeCorner* FindCornerNodeForEdge(AProcBuilding* Building, INT EdgeInfoIndex, UBOOL bTop)
{
	UPBRuleNodeCorner* CornerNode = NULL;

#if WITH_EDITORONLY_DATA
	if(EdgeInfoIndex != INDEX_NONE)
	{
		FPBEdgeInfo& EdgeInfo = Building->EdgeInfos(EdgeInfoIndex);

		// if edge is found, which scopes's left edge is it
		INT CornerScopeIndex = INDEX_NONE;
		if(EdgeInfo.ScopeAEdge == EPSA_Left)
		{
			CornerScopeIndex = EdgeInfo.ScopeAIndex;
		}
		else if(EdgeInfo.ScopeBEdge == EPSA_Left)
		{
			CornerScopeIndex = EdgeInfo.ScopeBIndex;
		}

		// If we got a scope, get its ruleset..
		if(CornerScopeIndex != INDEX_NONE)
		{
			UProcBuildingRuleset* Ruleset = Building->TopLevelScopeInfos(CornerScopeIndex).Ruleset;
			if(Ruleset)
			{
				// .. and find the Corner node that is at the top/bottom
				CornerNode = Ruleset->GetRulesetCornerNode(bTop, Building, CornerScopeIndex);
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return CornerNode;
}

/** Iterate over each vertex of input poly, then find appropropriate corner node and adjust corner (round, bevel) if desired */
static void AdjustPolyCorners(AProcBuilding* Building, FPoly& Poly, UBOOL bRoof)
{
#if WITH_EDITORONLY_DATA
	TArray<FVector> NewVerts;

	INT NumInVerts = Poly.Vertices.Num();
	for(INT VertIdx=0; VertIdx<NumInVerts; VertIdx++)
	{
		// Look for an edge ending at this roof vertex
		INT EdgeInfoIndex = FindEdgeEndingAt(Building, Poly.Vertices(VertIdx));
		// Find corner node that 'owns' this edge
		UPBRuleNodeCorner* CornerNode = FindCornerNodeForEdge(Building, EdgeInfoIndex, bRoof);

		// Grab info from corner node
		EPBCornerType CornerType = EPBC_Default;
		FLOAT CornerSize = 0.f;
		INT CornerTesselation = 0;
		FLOAT CornerCurvature = 0.f;
		FLOAT CornerShapeOffset = 0.f;

		if(CornerNode)
		{
			FPBEdgeInfo& EdgeInfo = Building->EdgeInfos(EdgeInfoIndex);
			FLOAT EdgeAngle = EdgeInfo.EdgeAngle;

			// Don't adjust this poly at this corner if no mesh is going to be inserted here
			if( (Abs(EdgeAngle) > CornerNode->FlatThreshold) && (!CornerNode->bNoMeshForConcaveCorners || EdgeAngle > 0.f) )
			{
				// .. and read corner values from it
				CornerSize = CornerNode->GetCornerSizeForAngle(EdgeAngle);
				CornerType = EPBCornerType(CornerNode->CornerType);
				CornerTesselation = CornerNode->RoundTesselation;
				CornerCurvature = CornerNode->RoundCurvature;
				CornerShapeOffset = CornerNode->CornerShapeOffset;
			}
		}

		// Find edges from this vertex to next and previous vertices
		const FVector ThisVert = Poly.Vertices(VertIdx);
		const FVector NextVert = Poly.Vertices((VertIdx+1)%NumInVerts);
		INT PrevVertIndex = (VertIdx == 0) ? (NumInVerts-1) : (VertIdx-1);
		const FVector PrevVert = Poly.Vertices(PrevVertIndex);

		const FVector ToNextDir = (NextVert - ThisVert).SafeNormal();
		const FVector ToPrevDir = (PrevVert - ThisVert).SafeNormal();

		const FLOAT PolyCornerSize = (CornerSize - CornerShapeOffset);

		// Chamfer
		if(CornerType == EPBC_Chamfer)
		{
			NewVerts.AddItem(ThisVert + (PolyCornerSize * ToPrevDir));
			NewVerts.AddItem(ThisVert + (PolyCornerSize * ToNextDir));
		}
		// Round
		else if(CornerType == EPBC_Round)
		{
			FVector StartVert = ThisVert + (PolyCornerSize * ToPrevDir);
			FVector StartTangent = CornerCurvature * -ToPrevDir * PolyCornerSize;

			FVector EndVert = ThisVert + (PolyCornerSize * ToNextDir);
			FVector EndTangent = CornerCurvature * ToNextDir * PolyCornerSize;

			for(INT StepIdx=0; StepIdx<=CornerTesselation; StepIdx++)
			{
				FLOAT Alpha = ((FLOAT)StepIdx)/((FLOAT)CornerTesselation);
				FVector StepVert = CubicInterp(StartVert, StartTangent, EndVert, EndTangent, Alpha);

				NewVerts.AddItem(StepVert);
			}
		}
		// Default
		else
		{
			NewVerts.AddItem(ThisVert);
		}		
	}

	Poly.Vertices = NewVerts;
#endif // WITH_EDITORONLY_DATA
}

/** Slice a set of scopes by a set of planes. Will only perform cut if slicing the scope will result in two new rectangular scopes */
void SliceScopesWithPlanes(TArray<FPBScope2D>& Scopes, TArray<FPBScopeProcessInfo>& ScopeInfos, const TArray<FPlane>& SlicingPlanes)
{
	check(Scopes.Num() == ScopeInfos.Num());

	const FLOAT MinSliceAmount = 8.f;
	for(INT PlaneIdx=0; PlaneIdx<SlicingPlanes.Num(); PlaneIdx++)
	{
		const FPlane& Plane = SlicingPlanes(PlaneIdx);

		// Grab the number of scopes we have before creating more by splitting.
		// We don't need to try and split newly created scopes (which are added to end of array)
		INT InitialNumScopes = Scopes.Num();
		for(INT ScopeIdx=0; ScopeIdx<InitialNumScopes; ScopeIdx++)
		{
			// Check the building that owns this scope wants it split
			if(ScopeInfos(ScopeIdx).OwningBuilding && ScopeInfos(ScopeIdx).OwningBuilding->bSplitWallsAtRoofLevels)
			{
				const FVector ScopeZ = Scopes(ScopeIdx).ScopeFrame.GetAxis(2);

				// Only allow slicing if plane normal is parallel to 
				if( appIsNearlyEqual(ScopeZ | Plane, 1.f, 0.01f) )
				{
					// Find dist of origin and top-left corner of scope from plane
					FVector ScopeOrigin = Scopes(ScopeIdx).ScopeFrame.GetOrigin();
					FLOAT OriginDist = Plane.PlaneDot(ScopeOrigin);

					FVector ScopeTopLeft = ScopeOrigin + (Scopes(ScopeIdx).DimZ * ScopeZ);
					FLOAT TopLeftDist = Plane.PlaneDot(ScopeTopLeft);

					// We have an intersect if one point is below plane and one point is below plane
					if((OriginDist < -MinSliceAmount) && (TopLeftDist > MinSliceAmount))
					{
						// We are going to go ahead and slice
						FLOAT CutDist = -OriginDist;

						INT NewScopeIdx = Scopes.AddZeroed();
						Scopes(NewScopeIdx) = Scopes(ScopeIdx);

						INT NewInfoIdx = ScopeInfos.AddZeroed();
						ScopeInfos(NewInfoIdx) = ScopeInfos(ScopeIdx);

						check(NewScopeIdx == NewInfoIdx);

						// New one is copy of this one, shrunk and offset
						Scopes(NewScopeIdx).DimZ -= CutDist;
						Scopes(NewScopeIdx).OffsetLocal(FVector(0,0,CutDist));

						// Just shrink down the current one to plane
						Scopes(ScopeIdx).DimZ = CutDist;
					}
				}
			}
		}
	}
}

/** 
 *	Given a scope and the start/end points of an edge, see if we should split this scope at this edge. 
 *	If we do, fill in SplitXDim with how far from origin along X to split. 
 */
UBOOL TestEdgeSplitsScope(const FVector& EdgeStart, const FVector& EdgeEnd, const FPBScope2D& Scope, FLOAT& SplitXDim)
{
	FVector EdgeVec = EdgeEnd-EdgeStart;
	FLOAT EdgeDist = EdgeVec.Size();

	// Don't split if edge is very short
	if(EdgeDist < 8.f)
	{
		return FALSE;
	}

	FVector EdgeDir = EdgeVec/EdgeDist;

	const FVector ScopeOrigin = Scope.ScopeFrame.GetOrigin();
	const FVector ScopeX = Scope.ScopeFrame.GetAxis(0);
	const FVector ScopeY = Scope.ScopeFrame.GetAxis(1);
	const FVector ScopeZ = Scope.ScopeFrame.GetAxis(2);

	// Only split if edge is parallel to scopes Z direction
	if( !appIsNearlyEqual(ScopeZ | EdgeDir, 1.f, 0.01f) )
	{
		return FALSE;
	}

	// Get start and end relative to scope origin
	const FVector LocalStart = EdgeStart - ScopeOrigin;
	const FVector LocalEnd = EdgeEnd - ScopeOrigin;

	// If start is not at 0 or end is not at DimZ (along Z relative to origin) - no split.
	const FLOAT StartZDist = LocalStart | ScopeZ;
	const FLOAT EndZDist = LocalEnd | ScopeZ;
	if(!appIsNearlyEqual(StartZDist, 0.f, 1.f) || !appIsNearlyEqual(EndZDist, Scope.DimZ, 1.f))
	{
		return FALSE;
	}

	// If start is off the plane of the scope, no split.
	const FLOAT StartYDist = LocalStart | ScopeY;
	if(!appIsNearlyEqual(StartYDist, 0.f, 1.f))
	{
		return FALSE;
	}

	// If start is outside X range, no split
	const FLOAT MinSliceAmount = 8.f;
	const FLOAT StartXDist = LocalStart | ScopeX;
	if(StartXDist < MinSliceAmount || StartXDist > (Scope.DimX-MinSliceAmount))
	{
		return FALSE;
	}

	SplitXDim = StartXDist;
	return TRUE;
}

/** 
 *	Try and split scopes using the vertical edges of other scopes. 
 *	Only do this if edge lies actually on the surface of another scope, and runs from top to bottom. 
 */
void SliceScopesWithEdges(TArray<FPBScope2D>& Scopes, TArray<FPBScopeProcessInfo>& ScopeInfos)
{
	// Now we have split at roof-tops, we need to see if we can split any face vertically where the edge of a scope runs down the middle of another.
	INT InitialNumScopes = Scopes.Num();
	for(INT ScopeIdx=0; ScopeIdx<InitialNumScopes; ScopeIdx++)
	{
		if(ScopeInfos(ScopeIdx).OwningBuilding && ScopeInfos(ScopeIdx).OwningBuilding->bSplitWallsAtWallEdges)
		{
			for(INT SplitScopeIdx=0; SplitScopeIdx<Scopes.Num(); SplitScopeIdx++)
			{
				if(ScopeIdx != SplitScopeIdx)
				{
					FPBScope2D& SplitScope = Scopes(SplitScopeIdx);

					const FVector SplitOrigin = SplitScope.ScopeFrame.GetOrigin();
					const FVector SplitX = SplitScope.ScopeFrame.GetAxis(0);
					const FVector SplitZ = SplitScope.ScopeFrame.GetAxis(2);

					// Start/end of left hand edge
					FVector Edge1Start = SplitOrigin;
					FVector Edge1End = SplitOrigin + (SplitScope.DimZ * SplitZ);

					// Start/end of right hand edge
					FVector Edge2Start = Edge1Start + (SplitScope.DimX * SplitX);
					FVector Edge2End = Edge1End + (SplitScope.DimX * SplitX);

					// See if either edge splits the scope
					FLOAT SplitDimX = 0.f;
					UBOOL bSplit = TestEdgeSplitsScope(Edge1Start, Edge1End, Scopes(ScopeIdx), SplitDimX);

					if(!bSplit)
					{
						bSplit = TestEdgeSplitsScope(Edge2Start, Edge2End, Scopes(ScopeIdx), SplitDimX);
					}

					// if it does, make new scope
					if(bSplit)
					{
						INT NewScopeIdx = Scopes.AddZeroed();
						Scopes(NewScopeIdx) = Scopes(ScopeIdx);

						INT NewInfoIdx = ScopeInfos.AddZeroed();
						ScopeInfos(NewInfoIdx) = ScopeInfos(ScopeIdx);

						check(NewScopeIdx == NewInfoIdx);

						// New one is copy of this one, shrunk and offset
						Scopes(NewScopeIdx).DimX -= SplitDimX;
						Scopes(NewScopeIdx).OffsetLocal(FVector(SplitDimX,0,0));

						// Just shrink down the current one to plane
						Scopes(ScopeIdx).DimX = SplitDimX;
					}
				}
			}
		}
	}
}

/** Util to find index of a variation given its name */
static INT FindIndexOfVariation(const UProcBuildingRuleset* Ruleset, FName VarName)
{
	INT OutIndex = INDEX_NONE;
	for(INT VarIdx=0; VarIdx<Ruleset->Variations.Num(); VarIdx++)
	{
		if(Ruleset->Variations(VarIdx).VariationName == VarName)
		{
			OutIndex = VarIdx;
			break;
		}
	}

	return OutIndex;
}

/** Update TopLevelScopes array based on brush, and all buildings based on this one. Generates entry for each scope added  */
void AProcBuilding::UpdateTopLevelScopes(const TArray<AProcBuilding*> GroupBuildings, TArray<FPoly>& OutHighDetailPolys, TArray<FPoly>& OutLowDetailPolys)
{
#if WITH_EDITORONLY_DATA
	TopLevelScopes.Empty();
	TopLevelScopeInfos.Empty();
	OutHighDetailPolys.Empty();
	OutLowDetailPolys.Empty();


	// Init max/min facade numbers
	MaxFacadeZ = -BIG_NUMBER;
	MinFacadeZ = BIG_NUMBER;

	const FMatrix WorldToBaseBuildingTM = WorldToLocal();

	// Base building needs a ruleset
	if(!Ruleset)
	{	
		return;
	}

	TArray<FPoly> Polys;
	TArray<UProcBuildingRuleset*> PolyRulesets;

	TArray<FPlane> SlicingPlanes;

	for(INT BuildIdx=0; BuildIdx<GroupBuildings.Num(); BuildIdx++)
	{
		AProcBuilding* ChildBuilding = GroupBuildings(BuildIdx);
		UModel* BuildingBrush = ChildBuilding->Brush;

		// Calc matrix to transform from child building to base building ref frames.
		const FMatrix ChildToParentTM = ChildBuilding->LocalToWorld() * WorldToBaseBuildingTM;

		// First grab all the surfaces of the brush
		if(BuildingBrush && BuildingBrush->Polys)
		{
			for(INT i=0; i<BuildingBrush->Polys->Element.Num(); i++)
			{
				const FPoly& Poly = BuildingBrush->Polys->Element(i);

				// If ruleset not set on child, use base building ruleset instead
				UProcBuildingRuleset* ScopeRuleset = ChildBuilding->GetRuleset();

				// Use per-face ruleset override if present
				FName ScopeVariation = Poly.RulesetVariation;

				// See if we want to mesh on top of face poly for this variation
				INT VariationIndex = FindIndexOfVariation(ScopeRuleset, ScopeVariation);
				UBOOL bMeshOnFacePoly = FALSE;
				if(VariationIndex != INDEX_NONE)
				{
					bMeshOnFacePoly = ScopeRuleset->Variations(VariationIndex).bMeshOnTopOfFacePoly;
				}

				// Check poly seems valid
				if((Poly.Vertices.Num() > 3) && (Poly.Normal.Size() > KINDA_SMALL_NUMBER))
				{
					// See if this is a horizontal surface ie roof
					UBOOL bIsRoof = (Poly.Normal.Z) > UCONST_ROOF_MINZ;
					UBOOL bIsFloor = (Poly.Normal.Z) < -UCONST_ROOF_MINZ;
						
					// If a non-roof poly, try and extract a rectangular scope from poly.
					FPBScope2D Scope;
					FPoly UsePoly[2];
					UBOOL bUseScope = FALSE;
					UBOOL bMakeScopePoly = TRUE;

					if(bIsRoof || bIsFloor)
					{
						SlicingPlanes.AddItem( FPlane(Poly.Vertices(0), Poly.Normal) );
					}

					if(bIsRoof && !ChildBuilding->bApplyRulesToRoof)
					{				
						if(ChildBuilding->bGenerateRoofMesh)
						{
							UsePoly[0] = Poly;
						}
					}
					else if(bIsFloor && !ChildBuilding->bApplyRulesToFloor)
					{
						if(ChildBuilding->bGenerateFloorMesh)
						{
							UsePoly[0] = Poly;
						}
					}
					else
					{
						// Extract rectangular scope from poly, may leave some area in UsePoly
						bUseScope = FPolyToWallScope2D(Poly, Scope, UsePoly[0], UsePoly[1]);

						// If we want to add meshes on top of poly covering entire region, keep bUseScope as TRUE, but replace UsePoly[0] with entire poly, and empty UsePoly[1]
						if(bUseScope && bMeshOnFacePoly)
						{
							UsePoly[0] = Poly;
							UsePoly[1].Vertices.Empty();
							// We also want to avoid making a poly for this scope in the low LOD mesh, because the entire poly we are within (Poly) will do this
							bMakeScopePoly = FALSE;
						}
					}
					
					// Add any polys that are valid
					UBOOL bPolyAdded = FALSE;
					for(INT UsePolyIdx=0; UsePolyIdx<2; UsePolyIdx++)
					{
						if(UsePoly[UsePolyIdx].Vertices.Num() > 0)
						{
							INT NewPolyIndex = Polys.AddZeroed();
							Polys(NewPolyIndex) = UsePoly[UsePolyIdx];
							TransformFPoly(Polys(NewPolyIndex), ChildToParentTM);

							PolyRulesets.AddItem(ScopeRuleset);

							bPolyAdded = TRUE;
						}
					}

					// If we got a scope, add it to scopes set to process
					if(bUseScope)
					{
						// Transform scope from child actor to base actor frame
						TransformScope2D(Scope, ChildToParentTM);

						//Scope.DrawScope(FColor(255,128,128), LocalToWorld(), TRUE);

						TopLevelScopes.AddItem(Scope);

						// Create info about this scope
						INT ScopeInfoIndex = TopLevelScopeInfos.AddZeroed();
						TopLevelScopeInfos(ScopeInfoIndex).OwningBuilding = ChildBuilding;
						TopLevelScopeInfos(ScopeInfoIndex).Ruleset = ScopeRuleset;
						TopLevelScopeInfos(ScopeInfoIndex).bGenerateLODPoly = bMakeScopePoly;
						TopLevelScopeInfos(ScopeInfoIndex).bPartOfNonRect = bPolyAdded;
						TopLevelScopeInfos(ScopeInfoIndex).RulesetVariation = ScopeVariation;
					}	

				}
			}			
		}				
	}
	
	// Cut through scopes using the roof/floor planes
	SliceScopesWithPlanes(TopLevelScopes, TopLevelScopeInfos, SlicingPlanes);

	// Cut scopes using the vertical edges of other scopes
	SliceScopesWithEdges(TopLevelScopes, TopLevelScopeInfos);

	// Remove any scopes that are completely occluded
	RemoveOccludedScopes(TopLevelScopes, TopLevelScopeInfos, LocalToWorld(), GroupBuildings);

	// Update edge connectivity structure 
	UpdateEdgeInfos();

	// Extend any scopes that touch a roof a bit (as defined in ruleset)
	for(INT ScopeIdx=0; ScopeIdx<TopLevelScopes.Num(); ScopeIdx++)
	{
		UProcBuildingRuleset* ScopeRuleset = TopLevelScopeInfos(ScopeIdx).Ruleset;

		// Check this scope is not part of a non-rect face (otherwise raising scope will overlap the filler-poly
		// Also check we have a ruleset, and that it actually wants the scope raised
		if(	!TopLevelScopeInfos(ScopeIdx).bPartOfNonRect &&
			ScopeRuleset && 
			(ScopeRuleset->RoofEdgeScopeRaise > KINDA_SMALL_NUMBER) )
		{		
			// Look at bottom edge
			INT EdgeIndex = FindEdgeForTopLevelScope(ScopeIdx, 1);
			// If no other face there, extend scope up
			if(EdgeIndex == INDEX_NONE)
			{
				TopLevelScopes(ScopeIdx).DimZ += ScopeRuleset->RoofEdgeScopeRaise;
			}
			// If another face there, we move scope up
			else
			{
				TopLevelScopes(ScopeIdx).OffsetLocal(FVector(0, 0, ScopeRuleset->RoofEdgeScopeRaise));
			}
		}

		// Keep track of highest and lowest point on building facades
		UpdateScopeMinMaxZ(TopLevelScopes(ScopeIdx), MinFacadeZ, MaxFacadeZ);
	}

	// Now combine any adjacent polys
	TArray<UProcBuildingRuleset*> MergedPolyRulesets;
	CombineAdjacentPolys(Polys, PolyRulesets, OutLowDetailPolys, MergedPolyRulesets);

	// Offset floor and roof polys
	// We need to do this as a second loop so that Min/MaxFacadeZ are correct
	for(INT LowPolyIdx=0; LowPolyIdx<OutLowDetailPolys.Num(); LowPolyIdx++)
	{
		const FPoly& Poly = OutLowDetailPolys(LowPolyIdx);
		UProcBuildingRuleset* ScopeRuleset = MergedPolyRulesets(LowPolyIdx);

		// See if this is a horizontal surface ie roof
		UBOOL bIsRoof = (Poly.Normal.Z) > UCONST_ROOF_MINZ;
		UBOOL bIsFloor = (Poly.Normal.Z) < -UCONST_ROOF_MINZ;

		INT HighPolyIdx = INDEX_NONE;
		if(!ScopeRuleset->bLODOnlyRoof)
		{
			HighPolyIdx = OutHighDetailPolys.AddItem( OutLowDetailPolys(LowPolyIdx) );

			if(bIsFloor || bIsRoof)
			{
				// chamfer/round corners if desired
				AdjustPolyCorners(this, OutHighDetailPolys(HighPolyIdx), bIsRoof);

				// Inset poly verts if desired
				InsetPolyVerts(OutHighDetailPolys(HighPolyIdx), bIsFloor ? ScopeRuleset->FloorPolyInset : ScopeRuleset->RoofPolyInset);
			}
		}


		FLOAT MinZ, MaxZ;
		GetPolyMinMaxZ(Poly, MinZ, MaxZ);

		// Set up PolyOffset up or down depending on if this is a roof/floor, and if its the top/bottom or not
		FVector PolyOffset(0,0,0);

		if(bIsRoof)
		{
			UBOOL bIsTop = appIsNearlyEqual(MaxZ, MaxFacadeZ, 0.1f);

			// Pick offset depending on whether we are at the top of not.
			FLOAT ZOffset = bIsTop ? ScopeRuleset->RoofZOffset : ScopeRuleset->NotRoofZOffset;
			PolyOffset.Z += ZOffset;
		}
		else if(bIsFloor)
		{
			UBOOL bIsBottom = appIsNearlyEqual(MinZ, MinFacadeZ, 0.1f);

			// Pick offset depending on whether we are at the bottom of not.
			FLOAT ZOffset = bIsBottom ? ScopeRuleset->FloorZOffset : ScopeRuleset->NotFloorZOffset;							
			PolyOffset.Z += ZOffset;							
		}

		// Apply translation
		if(HighPolyIdx != INDEX_NONE)
		{
			TransformFPoly(OutHighDetailPolys(HighPolyIdx), FTranslationMatrix(PolyOffset));
		}

		TransformFPoly(OutLowDetailPolys(LowPolyIdx), FTranslationMatrix(PolyOffset));
	}	
#endif // WITH_EDITORONLY_DATA
}




/** Remove all the building meshes from this building */
void AProcBuilding::ClearBuildingMeshes()
{
#if WITH_EDITORONLY_DATA
	// Clean up (instanced)staticmeshcomponents
	for(INT i=0; i<BuildingMeshCompInfos.Num(); i++)
	{
		UStaticMeshComponent* MeshComp = BuildingMeshCompInfos(i).MeshComp;
		if(MeshComp)
		{
			DetachComponent(MeshComp);
		}
	}	
	BuildingMeshCompInfos.Empty();
	
	// Clean up fracturedmeshcomps
	for(INT i=0; i<BuildingFracMeshCompInfos.Num(); i++)
	{
		UFracturedStaticMeshComponent* FracComp = BuildingFracMeshCompInfos(i).FracMeshComp;
		if(FracComp)
		{
			DetachComponent(FracComp);
		}
	}	
	BuildingFracMeshCompInfos.Empty();

	LODMeshComps.Empty();
	LODMeshUVInfos.Empty();

	BuildingMatParamMICs.Empty();
#endif // WITH_EDITORONLY_DATA
}


/** Util for finding all building components that form one top level scope. */
TArray<UStaticMeshComponent*> AProcBuilding::FindComponentsForTopLevelScope(INT TopLevelScopeIndex)
{
	// Find all components related to that face
	TArray<UStaticMeshComponent*> ScopeComps;
	
	if(TopLevelScopeIndex != INDEX_NONE)
	{
		// Iterate over all components
		for(INT CompIdx=0; CompIdx<BuildingMeshCompInfos.Num(); CompIdx++)
		{
			// If this is from scope we are interested in..
			if(BuildingMeshCompInfos(CompIdx).MeshComp && (BuildingMeshCompInfos(CompIdx).TopLevelScopeIndex == TopLevelScopeIndex))
			{
				// .. add to results
				ScopeComps.AddItem( BuildingMeshCompInfos(CompIdx).MeshComp );
			}
		}	
	}
	
	return ScopeComps;
}

/** Walks up Base chain to find the root building of the attachment chain */
AProcBuilding* AProcBuilding::GetBaseMostBuilding()
{
	AProcBuilding* Building = this;
	AProcBuilding* BuildingBase = Cast<AProcBuilding>(Building->Base);
	
	while(BuildingBase)
	{
		Building = BuildingBase;
		BuildingBase = Cast<AProcBuilding>(Building->Base);
	}
	
	return Building;
}


/** Will break pieces off the specified fracture component that are within the specified box. */
void AProcBuilding::BreakFractureComponent(UFracturedStaticMeshComponent* FracComp,FVector BoxMin,FVector BoxMax)
{
	// First, find this component in the set owned by this building.
	INT FracIndex = INDEX_NONE;
	for(INT FracInfoIdx=0; FracInfoIdx<BuildingFracMeshCompInfos.Num(); FracInfoIdx++)
	{
		if(BuildingFracMeshCompInfos(FracInfoIdx).FracMeshComp == FracComp)
		{
			FracIndex = FracInfoIdx;
			break;
		}
	}

	// Do nothing if this component does not belong to this building
	if(FracIndex == INDEX_NONE)
	{
		debugf(TEXT("BreakFractureComponent: FSMC does not belong to building (%s)"), *GetName());
		return;
	}

	// Make box based on info passed in
	FBox HideBox(BoxMin, BoxMax);

	// Now find which fragments of this mesh overlap the supplied box
	TArray<BYTE> FragmentVis = FracComp->GetVisibleFragments();
	INT PartsHidden = 0;
	for(INT ChunkIdx=0; ChunkIdx<FragmentVis.Num(); ChunkIdx++)
	{
		if((FragmentVis(ChunkIdx) != 0) && (ChunkIdx != FracComp->GetCoreFragmentIndex()))
		{
			FBox FracBox = FracComp->GetFragmentBox(ChunkIdx);
			FVector FracCenter = FracBox.GetCenter();
			UBOOL bOverlap = HideBox.IsInside(FracCenter);

			if(bOverlap)
			{
				FragmentVis(ChunkIdx) = 0; 
				PartsHidden++;
			}
		}
	}

	// If at least one part needs to be hidden, call function to do so
	if(PartsHidden)
	{
		FracComp->SetVisibleFragments(FragmentVis);

		// Determine which sound to play, based on the number of parts hidden
		const UBOOL bShouldPlayExplosionSound = (PartsHidden > 3);

		// Iterate up through the PhysMat parents until we find an appropriate sound effect
		UPhysicalMaterial* PhysMat = FracComp->GetFracturedMeshPhysMaterial();
		USoundCue* ShatterSound = NULL;
		while( (PhysMat != NULL) && (ShatterSound == NULL) )
		{
			ShatterSound = bShouldPlayExplosionSound ? PhysMat->FractureSoundExplosion : PhysMat->FractureSoundSingle;
			PhysMat = PhysMat->Parent;
		}

		if( ShatterSound != NULL )
		{
			FVector SoundLoc = (BoxMax + BoxMin) / 2.f;
			PlaySound(ShatterSound, TRUE, TRUE, TRUE, &SoundLoc, TRUE);
		}
	}
}

FColor GetEdgeSideColor(BYTE Edge)
{
	if(Edge == EPSA_Top)
	{	
		return FColor(0,255,255);
	}
	else if(Edge == EPSA_Bottom)
	{
		return FColor(0,255,0);
	}
	else if(Edge == EPSA_Left)
	{
		return FColor(25,25,255);
	}
	else if(Edge == EPSA_Right)
	{
		return FColor(255,25,25);
	}
	else
	{
		return FColor(255,255,255);
	}
}

/** Draw the edge connectivity map in 3D space */
void AProcBuilding::DrawDebugEdgeInfo(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITORONLY_DATA
	FMatrix BuildingToWorld = LocalToWorld();
	FColor EdgeColor = FColor(255,128,0);

	for(INT EdgeIdx=0; EdgeIdx<EdgeInfos.Num(); EdgeIdx++)
	{
		FPBEdgeInfo& Edge = EdgeInfos(EdgeIdx);

		// Transform start/end to world space
		FVector WorldStart = BuildingToWorld.TransformFVector(EdgeInfos(EdgeIdx).EdgeStart);
		FVector WorldEnd = BuildingToWorld.TransformFVector(EdgeInfos(EdgeIdx).EdgeEnd);
		FVector EdgeCenter = 0.5f * (WorldStart + WorldEnd);

		// Get edge dir and length
		FVector EdgeDir = (WorldEnd - WorldStart).SafeNormal();
		FLOAT EdgeLen = (WorldEnd - WorldStart).Size();

		// Find some vector orth to edge
		FVector Axis1, Axis2;
		EdgeDir.FindBestAxisVectors(Axis1, Axis2);

		FQuat EdgeDrawQuat = FQuat(EdgeDir, (FLOAT(EdgeIdx)/FLOAT(EdgeInfos.Num())) * 2.f * PI);
		Axis1 = EdgeDrawQuat.RotateVector(Axis1);

		// Find point off to one side, and draw edge as a long triangle (easier to see length)
		FVector SidePoint = WorldStart + (0.05f * EdgeLen * Axis1) + (0.5f * EdgeLen * EdgeDir);
		PDI->DrawLine(WorldStart, WorldEnd, EdgeColor, SDPG_World);
		PDI->DrawLine(WorldStart, SidePoint, EdgeColor, SDPG_World);
		PDI->DrawLine(SidePoint, WorldEnd, EdgeColor, SDPG_World);

		// Draw from ScopeA to middle of edge
		if(Edge.ScopeAIndex != INDEX_NONE)
		{
			FColor EdgeSideColor = GetEdgeSideColor(Edge.ScopeAEdge);
			FPBScope2D& Scope = TopLevelScopes(Edge.ScopeAIndex);
			FVector LocalScopeCenter = Scope.GetCenter() - (Scope.ScopeFrame.GetAxis(1) * 50.f);
			FVector ScopeCenter = BuildingToWorld.TransformFVector(LocalScopeCenter);
			
			PDI->DrawLine(SidePoint, ScopeCenter, EdgeSideColor, SDPG_World);
		}

		// Draw from ScopeB to middle of edge
		if(Edge.ScopeBIndex != INDEX_NONE)
		{
			FColor EdgeSideColor = GetEdgeSideColor(Edge.ScopeBEdge);
			FPBScope2D& Scope = TopLevelScopes(Edge.ScopeBIndex);
			FVector LocalScopeCenter = Scope.GetCenter() - (Scope.ScopeFrame.GetAxis(1) * 50.f);
			FVector ScopeCenter = BuildingToWorld.TransformFVector(LocalScopeCenter);

			PDI->DrawLine(SidePoint, ScopeCenter, EdgeSideColor, SDPG_World);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Draw scopes in 3D viewport */
void AProcBuilding::DrawDebugScopes(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITORONLY_DATA
	for(INT ScopeIdx=0; ScopeIdx<TopLevelScopes.Num(); ScopeIdx++)
	{
		FPBScope2D& Scope = TopLevelScopes(ScopeIdx);
		FMatrix WorldFrame = Scope.ScopeFrame * LocalToWorld();
		FVector Origin = WorldFrame.GetOrigin();

		FVector ScopeX = WorldFrame.GetAxis(0);
		FVector ScopeY = WorldFrame.GetAxis(1);
		FVector ScopeZ = WorldFrame.GetAxis(2);

		FColor DrawColor = FColor(255,0,110);

		// Draw rect region
		PDI->DrawLine(Origin,						Origin + ScopeX*Scope.DimX,							DrawColor, SDPG_World);
		PDI->DrawLine(Origin,						Origin + ScopeZ*Scope.DimZ,							DrawColor, SDPG_World);
		PDI->DrawLine(Origin + ScopeX*Scope.DimX,	Origin + ScopeX*Scope.DimX + ScopeZ*Scope.DimZ,		DrawColor, SDPG_World);
		PDI->DrawLine(Origin + ScopeZ*Scope.DimZ,	Origin + ScopeX*Scope.DimX + ScopeZ*Scope.DimZ,		DrawColor, SDPG_World);

		PDI->DrawLine(Origin,						Origin + ScopeX*Scope.DimX + ScopeZ*Scope.DimZ,		DrawColor, SDPG_World);

		// Draw normal (Y)
		PDI->DrawLine(Origin,						Origin + ScopeY*50.f,								DrawColor, SDPG_World);
	}
#endif // WITH_EDITORONLY_DATA
}


/** Get LOD texture usage */
static void GetLODTextureMemFromBuilding(const AProcBuilding* InBuilding, FPBMemUsageInfo& OutInfo)
{
	INT LODTextureMem = 0;

	UStaticMeshComponent* SimpleMeshComp = InBuilding->LowLODPersistentActor ? InBuilding->LowLODPersistentActor->StaticMeshComponent : InBuilding->SimpleMeshComp;
	if(	SimpleMeshComp && SimpleMeshComp->StaticMesh )
	{
		UStaticMesh* SimpleMesh = SimpleMeshComp->StaticMesh;
		check(SimpleMesh->LODModels.Num() > 0);
		check(SimpleMesh->LODModels(0).Elements.Num() > 0);
		UMaterialInstanceConstant* SimpleMIC = Cast<UMaterialInstanceConstant>(SimpleMesh->LODModels(0).Elements(0).Material);

		if(SimpleMIC)
		{
			UTexture* DiffuseTex = NULL; 
			UBOOL bGotDiffuse = SimpleMIC->GetTextureParameterValue(FName(TEXT("DiffuseTexture")), DiffuseTex);
			if(bGotDiffuse)
			{
				UTexture2D* DiffuseTex2D = CastChecked<UTexture2D>(DiffuseTex);
				OutInfo.LODDiffuseMemBytes = (DiffuseTex2D->SizeX * DiffuseTex2D->SizeY)/2;
			}

			UTexture* LightTex = NULL; 
			UBOOL bGotLight = SimpleMIC->GetTextureParameterValue(FName(TEXT("LightTexture")), LightTex);
			if(bGotLight)
			{
				UTexture2D* LightTex2D = CastChecked<UTexture2D>(LightTex);
				OutInfo.LODLightingMemBytes = (LightTex2D->SizeX * LightTex2D->SizeY)/2;
			}
		}


		// Also count light/shadow map info from the low res LOD if it has any
		// Note: Really low LODs have no business using light/shadow maps so these should be zero
		INT ComponentLightmapMem = 0;
		INT ComponentShadowmapMem = 0;
		SimpleMeshComp->GetLightAndShadowMapMemoryUsage(ComponentLightmapMem, ComponentShadowmapMem);
		OutInfo.LightmapMemBytes += ComponentLightmapMem;
		OutInfo.ShadowmapMemBytes += ComponentShadowmapMem;
	}
}

/** Get information about the amount of memory used by this building for various things */
FPBMemUsageInfo AProcBuilding::GetBuildingMemUsageInfo()
{
	FPBMemUsageInfo OutputInfo(0);

#if WITH_EDITORONLY_DATA
	OutputInfo.Building = this;
	OutputInfo.Ruleset = Ruleset;

	// Iterate over each component owned by this building
	for(INT CompIdx=0; CompIdx<BuildingMeshCompInfos.Num(); CompIdx++)
	{
		INT ComponentInstancedTris = 0;
		INT ComponentLightmapMem = 0;
		INT ComponentShadowmapMem = 0;

		UStaticMeshComponent* MeshComp = BuildingMeshCompInfos(CompIdx).MeshComp;
		if( MeshComp )
		{
			// Count the tris in the mesh
			UStaticMesh* Mesh = MeshComp->StaticMesh;
			if(Mesh)
			{
				check(Mesh->LODModels.Num() > 0);
				FStaticMeshRenderData* LODModel = &Mesh->LODModels(0);
				check(LODModel);
				ComponentInstancedTris = LODModel->IndexBuffer.Indices.Num() / 3;	
			}

			// See if this is an instanced component
			UInstancedStaticMeshComponent* InstComp = Cast<UInstancedStaticMeshComponent>(BuildingMeshCompInfos(CompIdx).MeshComp);
			if(InstComp)
			{
				OutputInfo.NumInstancedStaticMeshComponents++;

				// Scale the instanced tris by the number of instances
				ComponentInstancedTris *= InstComp->PerInstanceSMData.Num();
			}
			else
			{
				OutputInfo.NumStaticMeshComponent++;
			}

			// Get lightmap info
			MeshComp->GetLightAndShadowMapMemoryUsage(ComponentLightmapMem, ComponentShadowmapMem);
		}

		// Add instanced tris from this component to the total
		OutputInfo.NumInstancedTris += ComponentInstancedTris;
		OutputInfo.LightmapMemBytes += ComponentLightmapMem;
		OutputInfo.ShadowmapMemBytes += ComponentShadowmapMem;
	}

	// Get LOD texture info
	GetLODTextureMemFromBuilding(this, OutputInfo);
#endif // WITH_EDITORONLY_DATA

	return OutputInfo;
}

//////////////////////////////////////////////////////////////////////////
// FPBMemUsageInfo

/** Add the supplied info to this one */
void FPBMemUsageInfo::AddInfo(FPBMemUsageInfo& Info)
{
	NumStaticMeshComponent				+= Info.NumStaticMeshComponent;
	NumInstancedStaticMeshComponents	+= Info.NumInstancedStaticMeshComponents;
	NumInstancedTris					+= Info.NumInstancedTris;
	LightmapMemBytes					+= Info.LightmapMemBytes;
	ShadowmapMemBytes					+= Info.ShadowmapMemBytes;
	LODDiffuseMemBytes					+= Info.LODDiffuseMemBytes;
	LODLightingMemBytes					+= Info.LODLightingMemBytes;
}

/** Return comma-separated string indicating names of each category */
FString FPBMemUsageInfo::GetHeaderString()
{
	FString InfoString = FString(TEXT("Building,Ruleset,NumStaticMeshComponents,NumInstancedStaticMeshComponents,NumInstancedTris,LightmapMemBytes,ShadowmapMemBytes,LODDiffuseMemBytes,LODLightingMemBytes"));
	return InfoString;
}

/** Returns a comma-separated string version of the info in this struct. */
FString FPBMemUsageInfo::GetString()
{
	FString InfoString = FString::Printf(TEXT("%s,%s,%d,%d,%d,%d,%d,%d,%d"), *Building->GetPathName(), *Ruleset->GetPathName(), NumStaticMeshComponent, NumInstancedStaticMeshComponents, NumInstancedTris, LightmapMemBytes, ShadowmapMemBytes, LODDiffuseMemBytes, LODLightingMemBytes);
	return InfoString;
}

	/**
	* Returns a string representation of the selected column.  The returned string
	* is in human readable form (i.e. 12345 becomes "12,345")
	*
	* @param	Column	Column to retrieve float representation of - cannot be 0!
	* @return	FString	The human readable representation
	*/
FString FPBMemUsageInfo::GetColumnDataString( INT Column ) const
{
	INT Val = GetColumnData( Column );
	return FFormatIntToHumanReadable( Val );
}

/**
 * Returns a float representation of the selected column. Doesn't work for the first
 * column as it is text. This code is slow but it's not performance critical.
 *
 * @param	Column	Column to retrieve float representation of - cannot be BSBC_Name!
 * @return	float	representation of column
 */
INT FPBMemUsageInfo::GetColumnData( INT Column ) const
{
	check(Column > 0);
	switch( Column )
	{
	case BSBC_Name:
	case BSBC_Ruleset:
	default:
		appErrorf(TEXT("Unhandled case"));
		break;
	case BSBC_NumStaticMeshComps:
		return NumStaticMeshComponent;
	case BSBC_NumInstancedStaticMeshComps:
		return NumInstancedStaticMeshComponents;
	case BSBC_NumInstancedTris:
		return NumInstancedTris;
	case BSBC_LightmapMemBytes:
		return LightmapMemBytes;
	case BSBC_ShadowmapMemBytes:
		return ShadowmapMemBytes;
	case BSBC_LODDiffuseMemBytes:
		return LODDiffuseMemBytes;
	case BSBC_LODLightingMemBytes:
		return LODLightingMemBytes;
	}
	return 0; // Can't get here.
}

/**
 * Compare helper function used by the Compare used by Sort function.
 *
 * @param	Other		Other object to compare against
 * @param	SortIndex	Index to compare
 * @return	1 if >, -1 if < and 0 if ==
 */
INT FPBMemUsageInfo::Compare( const FPBMemUsageInfo& Other, INT SortIndex ) const
{
	if( SortIndex <= 1 )
	{
		UObject* ThisObj = NULL;
		UObject* OtherObj = NULL;

		if(SortIndex == 0)
		{
			ThisObj = Building;
			OtherObj = Other.Building;
		}
		else
		{
			ThisObj = Ruleset;
			OtherObj = Other.Ruleset;
		}

		// Check 
		if( !ThisObj || !OtherObj )
		{
			return 0;
		}
		else if( ThisObj->GetPathName() > OtherObj->GetPathName() )
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		FLOAT SortKeyA = GetColumnData(SortIndex);
		FLOAT SortKeyB = Other.GetColumnData(SortIndex);
		if( SortKeyA > SortKeyB )
		{
			return 1;
		}
		else if( SortKeyA < SortKeyB )
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UProcBuildingRuleset

UPBRuleNodeCorner* UProcBuildingRuleset::GetRulesetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	if(RootRule)
	{
		return RootRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
	}
	else
	{
		return NULL;
	}
}

/** Returns all rulesets ref'd by this ruleset (via SubRuleset node) */
void UProcBuildingRuleset::GetReferencedRulesets(TArray<UProcBuildingRuleset*>& OutRulesets)
{
	if(RootRule)
	{
		TArray<UPBRuleNodeBase*> AllRules;
		RootRule->GetRuleNodes(AllRules);

		for(INT RuleIdx=0; RuleIdx<AllRules.Num(); RuleIdx++)
		{
			UPBRuleNodeSubRuleset* SubRuleNode = Cast<UPBRuleNodeSubRuleset>( AllRules(RuleIdx) );
			if(SubRuleNode && SubRuleNode->SubRuleset)
			{
				OutRulesets.AddItem(SubRuleNode->SubRuleset);

				// Now let this ruleset find anything that IT ref's
				SubRuleNode->SubRuleset->GetReferencedRulesets(OutRulesets);
			}
		}
	}
}

/** Pick a random swatch name from this ruleset  */
FName UProcBuildingRuleset::GetRandomSwatchName()
{
	FName ResultName = NAME_None;

	if(ParamSwatches.Num() > 0)
	{
		INT RandIndex = RandHelper( ParamSwatches.Num() );
		ResultName = ParamSwatches(RandIndex).SwatchName;
	}

	return ResultName;
}

/** Get the index of a swatch in the  */
INT UProcBuildingRuleset::GetSwatchIndexFromName(FName SearchName)
{
	// and try and find a swatch with that name.
	for(INT SwatchIdx=0; SwatchIdx<ParamSwatches.Num(); SwatchIdx++)
	{
		if(ParamSwatches(SwatchIdx).SwatchName == SearchName)
		{
			// if we find one, remember its index
			return SwatchIdx;
		}
	}

	return INDEX_NONE;
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeBase

/** Get list of all rule nodes that follow this one (including this one) */
void UPBRuleNodeBase::GetRuleNodes(TArray<UPBRuleNodeBase*>& OutRuleNodes)
{
	// Add myself
	OutRuleNodes.AddUniqueItem(this);
	
	// And call on next rule(s)
	for(INT i=0; i<NextRules.Num(); i++)
	{
		UPBRuleNodeBase* Rule = NextRules(i).NextRule;
		if(Rule)
		{
			Rule->GetRuleNodes(OutRuleNodes);
		}
	}
}

FString UPBRuleNodeBase::GetRuleNodeTitle()
{
	return GetClass()->GetDescription();
	
}

FColor UPBRuleNodeBase::GetRuleNodeTitleColor()
{
	return FColor(101,138,67);
}

FString UPBRuleNodeBase::GetRuleNodeOutputName(INT ConnIndex)
{
	if((ConnIndex < 0) || (ConnIndex >= NextRules.Num()))
	{
		return FString(TEXT(""));
	}
	
	return NextRules(ConnIndex).LinkName.ToString();
}

FIntPoint UPBRuleNodeBase::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( RulePosX - LO_CONNECTOR_LENGTH, InDrawY );
	}
	else if(ConnType == LOC_OUTPUT)
	{
		check( ConnIndex >= 0 && ConnIndex < NextRules.Num() );
		return FIntPoint( RulePosX + DrawWidth + LO_CONNECTOR_LENGTH, NextRules(ConnIndex).DrawY );
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint(0,0);
}

UPBRuleNodeCorner* UPBRuleNodeBase::GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	// If looking for top node, we start exploring from the beginning of the array (top of scope)
	// This is also the left side if this is an X-split

	if(bTop)
	{
		for(INT RuleIdx=0; RuleIdx<NextRules.Num(); RuleIdx++)
		{
			if(NextRules(RuleIdx).NextRule)
			{
				return NextRules(RuleIdx).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
			}
		}
	}
	else
	{
		for(INT RuleIdx=NextRules.Num()-1; RuleIdx>=0; RuleIdx--)
		{
			if(NextRules(RuleIdx).NextRule)
			{
				return NextRules(RuleIdx).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
			}
		}
	}

	return NULL;
}

/** Util to try and fix up current connections based on an old set of connections, using connection name */
void UPBRuleNodeBase::FixUpConnections(TArray<FPBRuleLink>& OldConnections)
{
	for(INT i=0; i<NextRules.Num(); i++)
	{
		// Fix up connection
		for(INT j=0; j<OldConnections.Num(); j++)
		{
			if((OldConnections(j).LinkName != NAME_None) && (OldConnections(j).LinkName == NextRules(i).LinkName))
			{
				NextRules(i).NextRule = OldConnections(j).NextRule;
				OldConnections.Remove(j);
				break;
			}
		}
	}
}

void UPBRuleNodeBase::DrawRuleNode(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, UBOOL bSelected)
{
#if WITH_EDITORONLY_DATA
	// Create linked obj description
	FLinkedObjDrawInfo ObjInfo;

	// 'In' connection
	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("In"), FColor(0,0,0)) );

	// 'Next' connections
	for(INT i=0; i< NextRules.Num(); i++)
	{		
		ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*GetRuleNodeOutputName(i), FColor(0,0,0)) );
	}

	// Pointer to owning object
	ObjInfo.ObjObject = this;

	// Border color
	const FColor BorderColor = bSelected ? FColor(255, 255, 0) : FColor(0, 0, 0);

	// Font color
	const FColor FontColor = FColor( 255, 255, 128 );

	// Use util to draw this object
	FString ControlDesc = GetRuleNodeTitle();
	ObjInfo.VisualizationSize = GetVisualizationSize();

	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *ControlDesc, *Comment, FontColor, BorderColor, GetRuleNodeTitleColor(), FIntPoint(RulePosX, RulePosY) );
	//draw custom visualization if you have it, but only when not testing hit proxies
	if (!Canvas->IsHitTesting())
	{
		DrawVisualization(InHelper, Viewport, Canvas, ObjInfo.VisualizationPosition);
	}

	// Now read back connector location info

	// In Y location
	InDrawY = ObjInfo.InputY(0);

	// Next Y locations
	for(INT i=0; i<NextRules.Num(); i++)
	{
		NextRules(i).DrawY = ObjInfo.OutputY(i);
	}

	// Size of this node
	DrawWidth = ObjInfo.DrawWidth;
	DrawHeight = ObjInfo.DrawHeight;


	// Now draw links to other rules
	for(INT i=0; i< NextRules.Num(); i++)
	{
		UPBRuleNodeBase* Rule = NextRules(i).NextRule;
		if(Rule)
		{
			FIntPoint Start = GetConnectionLocation(LOC_OUTPUT, i);
			FIntPoint End = Rule->GetConnectionLocation(LOC_INPUT, 0);

			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy(new HLinkedObjLineProxy(this, i, Rule, 0));
			}

			FLOAT Tension = Abs<INT>(End.X - Start.X);
			FLinkedObjDrawUtils::DrawSpline(Canvas, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), FColor(0,0,0), TRUE);

			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy(NULL);
			}			
		}
	}
#endif // WITH_EDITORONLY_DATA
}


//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeMesh

/** Get a set of overrides, one for each section, by picking from each SectionOverrides struct. CAn be random, or first for each section */
TArray<UMaterialInterface*> FBuildingMeshInfo::GetMaterialOverrides(UBOOL bRandom) const
{
	// Convert from array of option to simple array of overrides - picking first option for each section
	TArray<UMaterialInterface*> MatOverrides;
	for(INT i=0; i<SectionOverrides.Num(); i++)
	{
		const FBuildingMatOverrides& Overrides = SectionOverrides(i);
		INT NumOptions = Overrides.MaterialOptions.Num();
		if(NumOptions > 0)
		{
			INT OptionIndex = 0;

			if(bRandom)
			{
				OptionIndex = RandHelper(NumOptions);
			}

			MatOverrides.AddItem( Overrides.MaterialOptions(OptionIndex) );
		}
		else
		{
			MatOverrides.AddItem( NULL );
		}
	}

	return MatOverrides;
}

void UPBRuleNodeMesh::PostLoad()
{
	Super::PostLoad();

	// Convert old overrides to new form
	if(GetLinker() && GetLinker()->Ver() < VER_PROCBUILDING_MATERIAL_OPTIONS)
	{
		// Iterate over each mesh entry..
		for(INT MeshIdx=0; MeshIdx<BuildingMeshes.Num(); MeshIdx++)
		{
			FBuildingMeshInfo& MeshInfo = BuildingMeshes(MeshIdx);

			// Add empty struct for each old material entry
			MeshInfo.SectionOverrides.AddZeroed( MeshInfo.MaterialOverrides.Num() );

			for(INT OverrideIdx=0; OverrideIdx<MeshInfo.MaterialOverrides.Num(); OverrideIdx++)
			{
				// Get the material for this section
				UMaterialInterface* OverrideMat = MeshInfo.MaterialOverrides(OverrideIdx);

				// Add current override material as first entry to this struct
				if(OverrideMat)
				{
					MeshInfo.SectionOverrides(OverrideIdx).MaterialOptions.AddItem( OverrideMat );
				}
			}

			MeshInfo.MaterialOverrides.Empty();
		}
	}
}

/** Util to pick a random building mesh from the BuildingMeshes array, using the Chance specified */
INT UPBRuleNodeMesh::PickRandomBuildingMesh()
{
	INT ResultIndex = INDEX_NONE;

	if(BuildingMeshes.Num() > 0)
	{
		// First see the sum of the chance vars are - this basically defines the size of all our buckets combined
		FLOAT TotalChance = 0.f;
		for(INT i=0; i<BuildingMeshes.Num(); i++)
		{
			TotalChance += BuildingMeshes(i).Chance;
		}

		// Make a random selection somewhere in that range
		FLOAT Choice = RandRange(0.f, TotalChance);

		// And then see what that corresponds to
		FLOAT CurrentChance = 0.f;
		for(INT i=0; i<BuildingMeshes.Num(); i++)
		{
			CurrentChance += BuildingMeshes(i).Chance;
			// This is our bucket - use this one
			if(CurrentChance >= Choice)
			{
				ResultIndex = i;
				break;
			}
		}
	}
	
	return ResultIndex;
}


static void SetPBMeshCollisionSettings(UStaticMeshComponent* MeshComp, UBOOL bBlockAll)
{
	MeshComp->bAllowApproximateOcclusion = TRUE;
	MeshComp->bCastDynamicShadow = TRUE;
	MeshComp->bForceDirectLightMap = TRUE;
	MeshComp->bUsePrecomputedShadows = TRUE;
	MeshComp->BlockNonZeroExtent = bBlockAll;
	MeshComp->BlockZeroExtent = TRUE;
	MeshComp->BlockActors = TRUE;
	MeshComp->CollideActors = TRUE;
	MeshComp->bAllowCullDistanceVolume = FALSE;
}

/** 
 *  Util for finding getting an MIC with the supplied parent, and parameters set correctly for this building. 
 *  Will either return one from the cache, or create a new one and set params if not found.
 */
UMaterialInstanceConstant* AProcBuilding::GetBuildingParamMIC(AProcBuilding* ScopeBuilding, UMaterialInterface* ParentMat)
{
	UMaterialInstanceConstant* NewMIC = NULL;
#if WITH_EDITORONLY_DATA
	for(INT MICIdx=0; MICIdx<BuildingMatParamMICs.Num(); MICIdx++)
	{
		UMaterialInstanceConstant* MIC = BuildingMatParamMICs(MICIdx);
		if(MIC && (MIC->Parent == ParentMat))
		{
			return MIC;
		}
	}

	// Don't have one already - create one now
	NewMIC = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), this);										
	NewMIC->SetParent(ParentMat);

	//debugf(TEXT("NEW MIC %d: %s -> (%s %d) "), BaseBuilding->BuildingMatParamMICs.Num(), *NewMIC->GetName(), *ScopeComp->GetPathName(), MatIdx);

	// Then let building set params
	ScopeBuilding->SetBuildingMaterialParamsOnMIC(NewMIC);

	// Put into the cache
	BuildingMatParamMICs.AddItem(NewMIC);
#endif // WITH_EDITORONLY_DATA

	return NewMIC;
}

void UPBRuleNodeMesh::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	DOUBLE StartTime = appSeconds();

	// First, check if this scope is completely occluded.
	INT OcclusionStatus = bDoOcclusionTest ? CheckScopeOcclusionStatus(InScope, BaseBuilding, ScopeBuilding) : 0;
	
	UBOOL bUsePartialMesh = FALSE;
	
	// If blocked - do not place a mesh
	if(OcclusionStatus == 2)
	{
		MeshTime += (1000.f * (appSeconds() - StartTime));
	
		return;
	}
	else if((OcclusionStatus == 1) && PartialOccludedBuildingMesh.Mesh)
	{
		bUsePartialMesh = TRUE;
	}

	// Pick the mesh we want to use..
	INT MeshIndex = PickRandomBuildingMesh();

	// .. and if its valid, make component and attach
	if(MeshIndex != INDEX_NONE && BuildingMeshes(MeshIndex).Mesh)
	{
		FBuildingMeshInfo& BuildingMesh = (bUsePartialMesh) ? PartialOccludedBuildingMesh : BuildingMeshes(MeshIndex);

		// Set up scale so mesh fills desired space
		// If the scope's Dim is 0, we just don't scale in the mesh in that dimension
		FVector MeshScale3D;
		MeshScale3D.X = (InScope.DimX > KINDA_SMALL_NUMBER) ? (InScope.DimX / BuildingMesh.DimX) : 1.f;		
		MeshScale3D.Y = 1.f;
		MeshScale3D.Z = (InScope.DimZ > KINDA_SMALL_NUMBER) ? (InScope.DimZ / BuildingMesh.DimZ) : 1.f;

		// Don't create an instance if this mesh is scaled to zero
		if( Abs(MeshScale3D.X * MeshScale3D.Y * MeshScale3D.Z) < KINDA_SMALL_NUMBER )
		{
			return;
		}


		// Find offset given by distributions if present
		FVector CompTrans(0,0,0);
		if(BuildingMesh.Translation)
		{
			CompTrans = BuildingMesh.Translation->GetValue();

			// scale by mesh scaling if desired
			if(BuildingMesh.bMeshScaleTranslation)
			{
				CompTrans *= MeshScale3D;
			}
		}

		FRotator CompRot(0,0,0);
		if(BuildingMesh.Rotation)
		{
			FVector RotVec = BuildingMesh.Rotation->GetValue();
			CompRot = FRotator( appFloor(RotVec.Y * Deg2U), appFloor(RotVec.Z * Deg2U), appFloor(RotVec.X * Deg2U) );
		}

		// Offset matrix from scope for mesh
		FMatrix CompOffset = FRotationTranslationMatrix(CompRot, CompTrans);

		// Component from actor origin
		FMatrix CompFrame = CompOffset * InScope.ScopeFrame;

		// Set correct transform on component (relative to actor)
		FVector MeshTranslation = CompFrame.GetOrigin();
		FRotator MeshRotation = CompFrame.Rotator();


		UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(BuildingMesh.Mesh);
		if(FracMesh)
		{
			UFracturedStaticMeshComponent* FracComp = ConstructObject<UFracturedStaticMeshComponent>(UFracturedStaticMeshComponent::StaticClass(), BaseBuilding);
			check(FracComp);

			// Set mesh
			FracComp->SetStaticMesh(FracMesh);

			// Set correct transform on component (relative to actor)
			FracComp->Translation = MeshTranslation;
			FracComp->Rotation = MeshRotation;
			FracComp->Scale3D = MeshScale3D;

			// Settings from StaticMeshActor
			SetPBMeshCollisionSettings(FracComp, bBlockAll);

			// Override lightmap res 
			if(BuildingMesh.bOverrideMeshLightMapRes)
			{
				FracComp->bOverrideLightMapRes = TRUE;
				FracComp->OverriddenLightMapRes = BuildingMesh.OverriddenMeshLightMapRes;
			}

			// This will stop us creating an RB_BodyInstance for each component
			FracComp->bDisableAllRigidBody = TRUE;

			// Set up parent LOD component
			FracComp->ReplacementPrimitive = LODParent;

			// Add to BuildingMeshCompInfos so it gets saved		
			INT InfoIndex = BaseBuilding->BuildingFracMeshCompInfos.AddZeroed();
			BaseBuilding->BuildingFracMeshCompInfos(InfoIndex).FracMeshComp = FracComp;
			BaseBuilding->BuildingFracMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;

			return;
		}

#if USE_INSTANCING_FOR_NEW_MESHES
		UInstancedStaticMeshComponent* ScopeComp = NULL;

		// if the static mesh has a lightmap resolution already, use it, but if it doesn't, we need to override it so we can use texture lighting
		UBOOL bShouldOverrideLightMapRes = (BuildingMesh.bOverrideMeshLightMapRes || (BuildingMesh.Mesh->LightMapResolution == 0));

		DOUBLE StartFindTime = appSeconds();

		// Generate set of random material overrides
		TArray<UMaterialInterface*> MatOverrides = BuildingMesh.GetMaterialOverrides(TRUE);

		// look for an existing component with this mesh, top-level scope and lightmap settings
		for (INT InfoIndex = 0; InfoIndex < BaseBuilding->BuildingMeshCompInfos.Num(); InfoIndex++)
		{
			FPBMeshCompInfo& Info = BaseBuilding->BuildingMeshCompInfos(InfoIndex);
			if(	(Info.MeshComp->StaticMesh == BuildingMesh.Mesh) && 
				//(Info.TopLevelScopeIndex == TopLevelScopeIndex) && 
				(Info.MeshComp->bOverrideLightMapRes == bShouldOverrideLightMapRes) &&
				(Info.MeshComp->OverriddenLightMapRes == BuildingMesh.OverriddenMeshLightMapRes) &&
				(Info.MeshComp->ReplacementPrimitive == LODParent) )
			{
				// Check material overrides match on this component
				UBOOL bMaterialsCorrect = TRUE;
				INT NumMatsToCheck = ::Max<INT>(MatOverrides.Num(), Info.MeshComp->Materials.Num());
				for(INT MatIdx=0; MatIdx<NumMatsToCheck; MatIdx++)
				{
					// Find what material we WANT on this component
					UMaterialInterface* DesiredMat = NULL;
					// First see if an override is wanted
					if((MatIdx < MatOverrides.Num()) && (MatOverrides(MatIdx) != NULL))
					{
						DesiredMat = MatOverrides(MatIdx);
					}
					// If not, get the material from the mesh
					else if(MatIdx < BuildingMesh.Mesh->LODModels(0).Elements.Num())
					{
						DesiredMat = BuildingMesh.Mesh->LODModels(0).Elements(MatIdx).Material;
					}

					// We may not have this actual material on the component, as we might be using an MIC with properties set instead, in which case we get the MIC from the map cache
					// This will not generate extra MICs, because it is either already in the map, or we are about to create/apply it in a minute anyway
					if(ScopeBuilding->HasBuildingParamsForMIC())
					{
						DesiredMat = BaseBuilding->GetBuildingParamMIC(ScopeBuilding, DesiredMat);	
					}

					// Get override specified by this component
					UMaterialInterface* ComponentMat = NULL;
					if(MatIdx < Info.MeshComp->Materials.Num())
					{
						ComponentMat = Info.MeshComp->Materials(MatIdx);
					}

					// If not the same, then we can't use this component
					if( DesiredMat != ComponentMat )
					{
						bMaterialsCorrect = FALSE;
						break;
					}
				}

				// make sure it's an instanced component
				if (bMaterialsCorrect && Info.MeshComp->IsA(UInstancedStaticMeshComponent::StaticClass()))
				{
					ScopeComp = (UInstancedStaticMeshComponent*)Info.MeshComp;
					break;
				}
			}
		}
		
		MeshFindTime += (1000.f * (appSeconds() - StartFindTime));

		// make a new one if needed
		if (ScopeComp == NULL)
		{
			ScopeComp = ConstructObject<UInstancedStaticMeshComponent>(UInstancedStaticMeshComponent::StaticClass(), BaseBuilding);
			check(ScopeComp);

			// Set mesh
			ScopeComp->SetStaticMesh(BuildingMesh.Mesh);
					
			// If we want to override materials, do that
			for(INT MatIdx=0; MatIdx<MatOverrides.Num(); MatIdx++)
			{
				if(MatOverrides(MatIdx) != NULL)
				{
					ScopeComp->SetMaterial(MatIdx, MatOverrides(MatIdx));
				}
			}
						
			// If we want to override some params, we need to make an MIC for this component
			if(ScopeBuilding->HasBuildingParamsForMIC())
			{
				// Iterate over each section
				INT NumSections = BuildingMesh.Mesh->LODModels(0).Elements.Num();
				for(INT MatIdx=0; MatIdx<NumSections; MatIdx++)
				{
					UMaterialInterface* ParentMat = ScopeComp->GetMaterial(MatIdx);

					// Get an MIC with all the correct parent/params set (will get from cache if possible)
					UMaterialInstanceConstant* NewMIC = BaseBuilding->GetBuildingParamMIC(ScopeBuilding, ParentMat);				
					
					// and apply MIC to this component
					ScopeComp->SetMaterial(MatIdx, NewMIC);
				}			
			}
			
			static INT CompCount = 0;
			ScopeComp->ComponentJoinKey = CompCount++;

			// Flag it properly, to duplicate the indices in the index buffer for instancing (on consoles).
			if(!BuildingMesh.Mesh->bUsedForInstancing)
			{
				BuildingMesh.Mesh->PreEditChange(NULL);
				BuildingMesh.Mesh->bUsedForInstancing = TRUE;
				BuildingMesh.Mesh->PostEditChange();
				BuildingMesh.Mesh->MarkPackageDirty();			
			}

			// Settings from StaticMeshActor
			SetPBMeshCollisionSettings(ScopeComp, bBlockAll);

			// Set up parent LOD component
			ScopeComp->ReplacementPrimitive = LODParent;

			// Set lighting options
			ScopeComp->bOverrideLightMapRes = bShouldOverrideLightMapRes;
			ScopeComp->OverriddenLightMapRes = BuildingMesh.OverriddenMeshLightMapRes;

			// Add to BuildingMeshCompInfos so it gets saved		
			INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
			BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
			BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;		
		}

		FMatrix InstanceTransform = FScaleMatrix(MeshScale3D);
		InstanceTransform *= FRotationMatrix(MeshRotation);
		InstanceTransform *= FTranslationMatrix(MeshTranslation);

		// add an instance
		INT NewIndex = ScopeComp->PerInstanceSMData.AddZeroed(1);
		ScopeComp->PerInstanceSMData(NewIndex).Transform = InstanceTransform;
		ScopeComp->PerInstanceSMData(NewIndex).LightmapUVBias = FVector2D( -1.0f, -1.0f );
		ScopeComp->PerInstanceSMData(NewIndex).ShadowmapUVBias = FVector2D( -1.0f, -1.0f );
	}
#else
		UStaticMeshComponent* ScopeComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), BaseBuilding);
		check(ScopeComp);

		// Set mesh
		ScopeComp->SetStaticMesh(BuildingMesh.Mesh);
	
		// Set correct transform on component (relative to actor)
		ScopeComp->Translation = MeshTranslation;
		ScopeComp->Rotation = MeshRotation;
		ScopeComp->Scale3D = MeshScale3D;

		// Settings from StaticMeshActor
		SetPBMeshCollisionSettings(ScopeComp, bBlockAll);

		// Override lightmap res 
		if(BuildingMesh.bOverrideMeshLightMapRes)
		{
			ScopeComp->bOverrideLightMapRes = TRUE;
			ScopeComp->OverriddenLightMapRes = BuildingMesh.OverriddenMeshLightMapRes;
		}

		// This will stop us creating an RB_BodyInstance for each component
		ScopeComp->bDisableAllRigidBody = TRUE;
		
		// Set up parent LOD component
		ScopeComp->ReplacementPrimitive = LODParent;
		
		// Add to BuildingMeshCompInfos so it gets saved		
		INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
		BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
		BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;		
	}
#endif

	MeshTime += (1000.f * (appSeconds() - StartTime));
#endif // WITH_EDITORONLY_DATA
}
 
FString UPBRuleNodeMesh::GetRuleNodeTitle()
{
	// Show how many meshes are assigned in this node
	return FString::Printf(TEXT("%s (%d)"), *Super::GetRuleNodeTitle(), BuildingMeshes.Num());
}

FColor UPBRuleNodeMesh::GetRuleNodeTitleColor()
{
	return FColor(101,76,138);
}


namespace MeshPreviewDefs
{
	const INT MeshPreviewSize = 128;
	const INT MeshPreviewBorder = 2;
}

/** Allows custom visualization drawing*/
FIntPoint UPBRuleNodeMesh::GetVisualizationSize(void)
{
	FIntPoint VisualizationSize = FIntPoint::ZeroValue();
	INT NumPreviews = BuildingMeshes.Num() + ((PartialOccludedBuildingMesh.Mesh != NULL) ? 1 : 0);

	if (NumPreviews)
	{
		//1 = 1x1 grid
		//2-4 = 2x2 grid
		//5-9 = 3x3 grid
		INT BestGridSize = appTrunc(appSqrt(NumPreviews-1)) + 1;
		INT MaxCapacity = BestGridSize*BestGridSize;

		//find out how many rows we need
		INT RowCount = BestGridSize;
		if (NumPreviews <= (MaxCapacity - BestGridSize)) 
		{
			RowCount--;
		}

		//(previews + padding)* number of entries + one extra border at the end
		VisualizationSize.X = BestGridSize*(MeshPreviewDefs::MeshPreviewSize+MeshPreviewDefs::MeshPreviewBorder) + MeshPreviewDefs::MeshPreviewBorder;
		VisualizationSize.Y = RowCount*(MeshPreviewDefs::MeshPreviewSize+MeshPreviewDefs::MeshPreviewBorder) + MeshPreviewDefs::MeshPreviewBorder;
	}
	return VisualizationSize;
}

/**
 * Custom visualization that can be specified per node
 */
void UPBRuleNodeMesh::DrawVisualization(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, const FIntPoint& InDrawPosition)
{
	INT NumPreviews = BuildingMeshes.Num() + ((PartialOccludedBuildingMesh.Mesh != NULL) ? 1 : 0);

	if (NumPreviews)
	{
		INT BestGridSize = appTrunc(appSqrt(NumPreviews-1)) + 1;
		INT CurrentRow = 0;
		INT CurrentCol = 0;

		for (INT i = 0; i < BuildingMeshes.Num(); ++i)
		{
			//draw the mesh preview
			const FColor Green(0,255,0);
			DrawPreviewMesh (InHelper, Viewport, Canvas, BuildingMeshes(i), InDrawPosition, CurrentRow, CurrentCol, Green);

			//setup for the next item
			++CurrentCol;
			if (CurrentCol >= BestGridSize)
			{
				//next row
				++CurrentRow;
				CurrentCol = 0;
			}
		}
		if (PartialOccludedBuildingMesh.Mesh != NULL)
		{
			const FColor Purple(255,0,255);
			DrawPreviewMesh (InHelper, Viewport, Canvas, PartialOccludedBuildingMesh, InDrawPosition, CurrentRow, CurrentCol, Purple);
		}
	}
}

/**
 * Render function that retrieves the thumbnail from the mesh and draws it in the grid
 */
void UPBRuleNodeMesh::DrawPreviewMesh (FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, const FBuildingMeshInfo& MeshInfo, const FIntPoint& InDrawPosition, const INT InRow, const INT InCol, const FColor& BorderColor)
{
	check(Viewport);
	check(Canvas);

	INT ThumbX = InDrawPosition.X + InCol*(MeshPreviewDefs::MeshPreviewSize+MeshPreviewDefs::MeshPreviewBorder)+MeshPreviewDefs::MeshPreviewBorder;
	INT ThumbY = InDrawPosition.Y + InRow*(MeshPreviewDefs::MeshPreviewSize+MeshPreviewDefs::MeshPreviewBorder)+MeshPreviewDefs::MeshPreviewBorder;

	DrawTile( Canvas, ThumbX-1,	ThumbY-1, MeshPreviewDefs::MeshPreviewSize+2, MeshPreviewDefs::MeshPreviewSize+2, 0.0f,0.0f,0.0f,0.0f, BorderColor );

	const FMatrix& CanvasTransform = Canvas->GetTransform();
	const FVector4 UpperLeftPosition  = CanvasTransform.TransformFVector4(FVector4(ThumbX, ThumbY, 0));
	const FVector4 LowerRightPosition = CanvasTransform.TransformFVector4(FVector4(ThumbX+MeshPreviewDefs::MeshPreviewSize, ThumbY+MeshPreviewDefs::MeshPreviewSize, 0));

	INT ScreenLeftX   = appRound(UpperLeftPosition.X);
	INT ScreenTopY    = appRound(UpperLeftPosition.Y);
	INT ScreenRightX  = appRound(LowerRightPosition.X);
	INT ScreenBottomY = appRound(LowerRightPosition.Y);

	FIntRect ThumbRect (ScreenLeftX, ScreenTopY, ScreenRightX, ScreenBottomY);
	check(InHelper);

	// Handle NULL InMesh by drawing a black box
	if(MeshInfo.Mesh)
	{
		TArray<UMaterialInterface*> MatOverrides = MeshInfo.GetMaterialOverrides(FALSE);

		InHelper->DrawThumbnail(MeshInfo.Mesh, MatOverrides, Viewport, Canvas, ThumbRect);
	}
	else
	{
		DrawTile( Canvas, ThumbX, ThumbY, MeshPreviewDefs::MeshPreviewSize, MeshPreviewDefs::MeshPreviewSize, 0.0f,0.0f,0.0f,0.0f, FColor(64,64,64) );
	}
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeRepeat

void UPBRuleNodeRepeat::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	TArray<FPBScope2D> ResultScopes;

	check((RepeatAxis == EPBAxis_X) || (RepeatAxis == EPBAxis_Z));

	if(RepeatMaxSize < KINDA_SMALL_NUMBER)
	{
		debugf(TEXT("%s : RepeatMaxSize too small."), *GetName());
		return;
	}

	const FVector InScopeX = InScope.ScopeFrame.GetAxis(0);
	const FVector InScopeZ = InScope.ScopeFrame.GetAxis(2);

	if(RepeatAxis == EPBAxis_X)
	{	
		// Calc how many repeats we need to use to satisfy the max repeat size
		INT NumSplits = appCeil(InScope.DimX / RepeatMaxSize);
		NumSplits = Max(NumSplits, 1); // ensure we don't have 0 outputs, even if scope is size 0 in that axis
		// Then see how big that makes each repeat
		const FLOAT RepeatDimX = InScope.DimX / FLOAT(NumSplits);

		// now generate all the new scopes
		for(INT i=0; i<NumSplits; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = RepeatDimX;
			NewScope.DimZ = InScope.DimZ; // Unchanged from parent scope

			// Scope frame is the same, only shifted along X for each one			
			FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (i * RepeatDimX * InScopeX);
			NewScope.ScopeFrame = InScope.ScopeFrame;		
			NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

			// Add to output
			ResultScopes.AddItem(NewScope);
		}
	}
	else
	{
		// same as above, but for Z

		INT NumSplits = appCeil(InScope.DimZ / RepeatMaxSize);
		NumSplits = Max(NumSplits, 1);
		const FLOAT RepeatDimZ = InScope.DimZ / FLOAT(NumSplits);

		for(INT i=0; i<NumSplits; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = InScope.DimX;
			NewScope.DimZ = RepeatDimZ;

			FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (i * RepeatDimZ * InScopeZ);
			NewScope.ScopeFrame = InScope.ScopeFrame;		
			NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

			ResultScopes.AddItem(NewScope);
		}	
	}

	// Pass on each result to the next rule
	check(NextRules.Num() == 1);
	if(NextRules(0).NextRule)
	{
		for(INT i=0; i<ResultScopes.Num(); i++)
		{
			NextRules(0).NextRule->ProcessScope( ResultScopes(i), TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
		}	
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeRepeat::GetRuleNodeTitle()
{
	FString DirString = (RepeatAxis == EPBAxis_X) ? TEXT("X") : TEXT("Z");

	return FString::Printf(TEXT("%s %s:%3.0f"), *Super::GetRuleNodeTitle(), *DirString, RepeatMaxSize);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeSplit

void UPBRuleNodeSplit::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	UpdateRuleConnectors();
}

/** 
 *	Util to output array of size SplitSetup.Num(), indicating how to split a certain size using the splitting rules 
 *	A value of -1.0 indicates no split should be made
 */
TArray<FLOAT> UPBRuleNodeSplit::CalcSplitSizes(FLOAT TotalSize)
{
	// First thing to do is figure out how much to allocate to each split
	TArray<FLOAT> SplitSizes;
	SplitSizes.AddZeroed(SplitSetup.Num());

	// This is the amount of space we have left to allocate
	FLOAT SizeLeft = TotalSize;
	// Keep track of the total of the ExpandRatios for non-fixed splits
	FLOAT ExpandRatioTotal = 0.f;

	// Iterate over each split..
	for(INT i=0; i<SplitSetup.Num(); i++)
	{
		// Fixed size, give it what it wants, if its available
		if(SplitSetup(i).bFixSize)
		{
			// We only give space if we can give what is wanted
			if(SplitSetup(i).FixedSize < SizeLeft)
			{
				SplitSizes(i) = SplitSetup(i).FixedSize;
				SizeLeft -= SplitSizes(i);
			}
			else
			{
				SplitSizes(i) = -1.f;
			}			
		}
		// Variable size, we just rememeber the total ExpandRatio
		else
		{
			ExpandRatioTotal += SplitSetup(i).ExpandRatio;
		}
	}

	// We have now allocated all the space to the fixed size, so we divide what is left up
	if((SizeLeft > KINDA_SMALL_NUMBER) && (ExpandRatioTotal > KINDA_SMALL_NUMBER))
	{
		FLOAT SizePerExpandPoint = SizeLeft/ExpandRatioTotal;

		for(INT i=0; i<SplitSetup.Num(); i++)
		{
			if(!SplitSetup(i).bFixSize)
			{
				SplitSizes(i) = SplitSetup(i).ExpandRatio * SizePerExpandPoint;
			}
		}
	}
	else
	{
		for(INT i=0; i<SplitSetup.Num(); i++)
		{
			if(!SplitSetup(i).bFixSize)
			{
				SplitSizes(i) = -1.f;
			}
		}
	}
	
	return SplitSizes;
}

void UPBRuleNodeSplit::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	if(NextRules.Num() != SplitSetup.Num())
	{
		debugf(TEXT("UPBRuleNodeSplit: NextRules and SplitSetup sizes do not match!"));
		return;
	}

	// First thing to do is figure out how much to allocate to each split
	FLOAT TotalSize = (SplitAxis == EPBAxis_X) ? InScope.DimX : InScope.DimZ;	
	TArray<FLOAT> SplitSizes = CalcSplitSizes(TotalSize);
	
	// At this point, SplitSizes is complete, but may contain zeroes
	
	const FVector InScopeX = InScope.ScopeFrame.GetAxis(0);
	const FVector InScopeZ = InScope.ScopeFrame.GetAxis(2);

	
	// Call next rule for all non-zero sized results
	
	FLOAT OffsetDist = 0.f;
		
	if(SplitAxis == EPBAxis_X)
	{
		for(INT i=0; i<SplitSizes.Num(); i++)
		{
			// Ignore 'stretch' splits which are zero size
			if((SplitSizes(i) > -0.5f) && (SplitSetup(i).bFixSize || SplitSizes(i) > KINDA_SMALL_NUMBER))
			{
				FPBScope2D NewScope;
				NewScope.DimX = SplitSizes(i);
				NewScope.DimZ = InScope.DimZ;

				// Move origin of scope along split axis
				FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (OffsetDist * InScopeX);				
				NewScope.ScopeFrame = InScope.ScopeFrame;		
				NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);
				
				OffsetDist += SplitSizes(i);
				
				// If we have something connected here, pass scope on
				if(NextRules(i).NextRule)
				{
					NextRules(i).NextRule->ProcessScope(NewScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
				}
			}
		}	
	}
	else
	{
		// We work from the end to the start of the array - so that the lowest connector corresponds to the bottom of the building	
		for(INT i=SplitSizes.Num()-1; i>=0; i--)
		{
			if((SplitSizes(i) > -0.5f) && (SplitSetup(i).bFixSize || SplitSizes(i) > KINDA_SMALL_NUMBER))
			{		
				FPBScope2D NewScope;
				NewScope.DimX = InScope.DimX;
				NewScope.DimZ = SplitSizes(i);

				FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (OffsetDist * InScopeZ);				
				NewScope.ScopeFrame = InScope.ScopeFrame;		
				NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);
								
				OffsetDist += SplitSizes(i);
				
				// If we have something connected here, pass scope on
				if(NextRules(i).NextRule)
				{
					NextRules(i).NextRule->ProcessScope(NewScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
				}				
			}
		}	
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeSplit::GetRuleNodeTitle()
{
	FString DirString = (SplitAxis == EPBAxis_X) ? TEXT("X") : TEXT("Z");

	return FString::Printf(TEXT("%s %s:%d"), *Super::GetRuleNodeTitle(), *DirString, SplitSetup.Num());
}


FString UPBRuleNodeSplit::GetRuleNodeOutputName(INT ConnIndex)
{
	if( (ConnIndex < 0) || (ConnIndex >= NextRules.Num()) || (NextRules.Num() != SplitSetup.Num()) )
	{
		return FString(TEXT(""));
	}

	FString LinkName = NextRules(ConnIndex).LinkName.ToString();

	if(SplitSetup(ConnIndex).bFixSize)
	{
		LinkName += FString::Printf(TEXT(" (F %1.1f)"), SplitSetup(ConnIndex).FixedSize);
	}
	else
	{
		LinkName += FString::Printf(TEXT(" (V %1.1f)"), SplitSetup(ConnIndex).ExpandRatio);	
	}

	return LinkName;
}


/** Update the NextRules array based on the SplitSetup array */
void UPBRuleNodeSplit::UpdateRuleConnectors()
{
	// First, save off the old connections
	TArray<FPBRuleLink> OldConnections = NextRules;
	
	// Then recreate the NextRules array using the SplitSetup array
	NextRules.Empty();	
	NextRules.AddZeroed( SplitSetup.Num() );
	
	for(INT i=0; i<SplitSetup.Num(); i++)
	{		
		// Copy name over
		NextRules(i).LinkName = SplitSetup(i).SplitName;		
	}

	FixUpConnections(OldConnections);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeQuad


void UPBRuleNodeQuad::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	UStaticMeshComponent* ScopeComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), BaseBuilding);
	check(ScopeComp);

	// Set mesh
	ScopeComp->Scale3D.X = InScope.DimX/100.f;
	ScopeComp->Scale3D.Z = InScope.DimZ/100.f;
	ScopeComp->Scale3D.Y = 0.5f * (ScopeComp->Scale3D.X + ScopeComp->Scale3D.Z); // Hack here to try and keep scale vaguely uniform, as this messes up cubemaps

	ScopeComp->SetStaticMesh(GEngine->BuildingQuadStaticMesh);
	
	// Set offset from actor
	ScopeComp->Translation = InScope.ScopeFrame.GetOrigin();
	ScopeComp->Translation += (YOffset * InScope.ScopeFrame.GetAxis(1)); // apply y offset
	ScopeComp->Rotation = InScope.ScopeFrame.Rotator();

	// Settings from StaticMeshActor
	ScopeComp->bAllowApproximateOcclusion = TRUE;
	ScopeComp->bCastDynamicShadow = TRUE;
	ScopeComp->bForceDirectLightMap = TRUE;
	ScopeComp->bUsePrecomputedShadows = TRUE;
	ScopeComp->BlockNonZeroExtent = FALSE;
	ScopeComp->BlockZeroExtent = TRUE;

	// This will stop us creating an RB_BodyInstance for each component
	ScopeComp->bDisableAllRigidBody = TRUE;

	// Set up parent LOD component
	ScopeComp->ReplacementPrimitive = LODParent;

	// Set lighting res
	ScopeComp->bOverrideLightMapRes = TRUE;
	ScopeComp->OverriddenLightMapRes = QuadLightmapRes;

	// Add to info array
	INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;
	
	// reattach the quad component if it's already attached, because the rendering thread
	// may be using the existing MIC, and the code below will detroy the MIC in place without
	// detaching the component first
	FComponentReattachContext Reattach(ScopeComp);
	
	// If we want to disable any UV repeats, just slap material on
	if(bDisableMaterialRepeat)
	{
		//  No UVs to fiddle, just assign Material to this quad
		ScopeComp->SetMaterial(0, Material);
	}
	else
	{
		// Make an MIC that sets the UV correctly..
		UMaterialInstanceConstant* QuadMIC = CastChecked<UMaterialInstanceConstant>(UObject::StaticConstructObject(
			UMaterialInstanceConstant::StaticClass(), 
			BaseBuilding->GetOutermost(), 
			FName(*FString::Printf(TEXT("%s_QUADMIC_%d"), *BaseBuilding->GetName(), InfoIndex)),
			0
			));

		QuadMIC->SetParent(Material);

		ScopeBuilding->SetBuildingMaterialParamsOnMIC(QuadMIC);

		FName UOffsetParamName(TEXT("U_Offset"));
		FName UScaleParamName(TEXT("U_Scale"));
		FName VOffsetParamName(TEXT("V_Offset"));
		FName VScaleParamName(TEXT("V_Scale"));

		FLOAT UTile = appCeil(InScope.DimX/RepeatMaxSizeX);
		FLOAT VTile = appCeil(InScope.DimZ/RepeatMaxSizeZ);

		QuadMIC->SetScalarParameterValue(UOffsetParamName, 0.f);
		QuadMIC->SetScalarParameterValue(UScaleParamName, UTile);
		QuadMIC->SetScalarParameterValue(VOffsetParamName, 0.f);
		QuadMIC->SetScalarParameterValue(VScaleParamName, VTile);

		// ..and assign to quad
		ScopeComp->SetMaterial(0, QuadMIC);
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeQuad::GetRuleNodeTitle()
{
	return Super::GetRuleNodeTitle();
}

FColor UPBRuleNodeQuad::GetRuleNodeTitleColor()
{
	return FColor(70,86,209);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeExtractTopBottom


void UPBRuleNodeExtractTopBottom::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	check(NextRules.Num() == 5);

	FLOAT ScopeZRemaining = InScope.DimZ;
	FVector InScopeZ = InScope.ScopeFrame.GetAxis(2);

	// Bottom
	FLOAT InScopeMinZ = InScope.ScopeFrame.GetOrigin().Z;
	FLOAT RemovedFromBottom = 0.f;
	UBOOL bRemoveBottom = TRUE;
	INT BottomRuleIndex = INDEX_NONE;
	
	// Bottom of building
	if( appIsNearlyEqual(InScopeMinZ, BaseBuilding->MinFacadeZ, 0.1f) )
	{
		RemovedFromBottom = ExtractBottomZ;
		BottomRuleIndex = 3;		
	}
	// Not bottom of building
	else
	{
		RemovedFromBottom = ExtractNotBottomZ;
		BottomRuleIndex = 4;
	}
	
	// Top
	FLOAT InScopeMaxZ = InScopeMinZ + (InScope.ScopeFrame.GetAxis(2).Z * InScope.DimZ);
	FLOAT RemovedFromTop = 0.f;
	UBOOL bUseRemainderForTop = FALSE;
	UBOOL bRemoveTop = TRUE;
	INT TopRuleIndex = INDEX_NONE;

	// Top of building
	if( appIsNearlyEqual(InScopeMaxZ, BaseBuilding->MaxFacadeZ, 0.1f) )
	{
		RemovedFromTop = ExtractTopZ;
		TopRuleIndex = 0;		
	}
	// Not bottom of building
	else
	{
		RemovedFromTop = ExtractNotTopZ;
		TopRuleIndex = 1;
	}

	// Special handling when scope size is less than top and bottom together
	FLOAT TotalRemoved = RemovedFromTop + RemovedFromBottom;
	if(ScopeZRemaining <= TotalRemoved)
	{
		// If its smaller than even bottom, we only put in a bottom, and shrink it down
		if(ScopeZRemaining <= RemovedFromBottom)
		{
			RemovedFromBottom = ScopeZRemaining;
			bRemoveTop = FALSE;
		}
		// Otherwise we scale top and bottom (in ratio) so we can fit them both in
		else if(TotalRemoved > KINDA_SMALL_NUMBER)
		{
			FLOAT ScaleDown = ScopeZRemaining/TotalRemoved;
			RemovedFromTop *= ScaleDown;
			RemovedFromBottom *= ScaleDown;
			bUseRemainderForTop = TRUE;
		}
	}


	// Check we can remove bottom amount much from what we have, and that there is a 'next' rule
	if((ScopeZRemaining >= RemovedFromBottom) && NextRules(BottomRuleIndex).NextRule)
	{
		FPBScope2D NewScope;
		NewScope.DimX = InScope.DimX;
		NewScope.DimZ = RemovedFromBottom;
		NewScope.ScopeFrame = InScope.ScopeFrame;		

		NextRules(BottomRuleIndex).NextRule->ProcessScope(NewScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
		
		ScopeZRemaining -= RemovedFromBottom;
	}
	else
	{
		RemovedFromBottom = 0.f;
	}
	
	// Slight hack - when just a top and bottom, use the remainder for the top, to avoid precision issues
	if(bUseRemainderForTop)
	{
		RemovedFromTop = ScopeZRemaining;
	}

	// Check we can remove top amount from what we have, and that there is a 'next' rule
	if(bRemoveTop && (ScopeZRemaining >= RemovedFromTop) && NextRules(TopRuleIndex).NextRule)
	{
		FPBScope2D NewScope;
		NewScope.DimX = InScope.DimX;
		NewScope.DimZ = RemovedFromTop;
		
		// Offset origin up to top of scope, minus what we want to split off
		FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + ((InScope.DimZ - RemovedFromTop) * InScopeZ);
		NewScope.ScopeFrame = InScope.ScopeFrame;		
		NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

		NextRules(TopRuleIndex).NextRule->ProcessScope(NewScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
		
		ScopeZRemaining -= RemovedFromTop;		
	}
	else
	{
		RemovedFromTop = 0.f;
	}
	
	
	// Now output 'middle' section
	if((ScopeZRemaining > KINDA_SMALL_NUMBER) && NextRules(2).NextRule)
	{
		FPBScope2D NewScope;
		NewScope.DimX = InScope.DimX;
		NewScope.DimZ = ScopeZRemaining;

		// Offset origin up to top of scope, minus what we want to split off
		FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (RemovedFromBottom * InScopeZ);
		NewScope.ScopeFrame = InScope.ScopeFrame;		
		NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

		NextRules(2).NextRule->ProcessScope(NewScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
	}
#endif // WITH_EDITORONLY_DATA
}

UPBRuleNodeCorner* UPBRuleNodeExtractTopBottom::GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	// Top
	if(bTop)
	{
		// Use top if there, or mid otherwise
		if(NextRules(0).NextRule)
		{
			return NextRules(0).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
		}
		else if(NextRules(2).NextRule)
		{
			return NextRules(2).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
		}
	}
	// Bottom
	else
	{
		// Use bottom if there, or mid otherwise
		if(NextRules(3).NextRule)
		{
			return NextRules(3).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
		}
		else if(NextRules(2).NextRule)
		{
			return NextRules(2).NextRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
		}
	}

	return NULL;
}

FString UPBRuleNodeExtractTopBottom::GetRuleNodeTitle()
{
	return Super::GetRuleNodeTitle();
}


//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeAlternate

void UPBRuleNodeAlternate::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	TArray<FPBScope2D> ResultScopes;

	check((RepeatAxis == EPBAxis_X) || (RepeatAxis == EPBAxis_Z));

	// If both sizes are zero, cannot work
	if((ASize < KINDA_SMALL_NUMBER) && (BMaxSize < KINDA_SMALL_NUMBER))
	{
		debugf(TEXT("%s : RepeatMaxSize too small."), *GetName());
		return;
	}

	const FVector InScopeX = InScope.ScopeFrame.GetAxis(0);
	const FVector InScopeZ = InScope.ScopeFrame.GetAxis(2);

	// See what our total size is we need to divide up
	const FLOAT TotalSize = (RepeatAxis == EPBAxis_X) ? InScope.DimX : InScope.DimZ;	
	
	// Work out how many As and Bs we have, and how big they are
	INT NumA, NumB;
	FLOAT ScopeSize[2];

	if(bEqualSizeAB)
	{
		NumB = appCeil(0.5f * ((TotalSize/BMaxSize) - 1.f));
		NumB = Max(NumB, 1);

		//  There will be one more As. (ABABA)
		NumA = NumB + 1;

		if(bInvertPatternOrder)
		{
			Swap(NumA, NumB);
		}

		ScopeSize[0] = ScopeSize[1] = TotalSize/(NumA + NumB);
	}
	else
	{ 
		if(bInvertPatternOrder)
		{
			// Calc how many A's we are going to have.
			NumB = appCeil( (TotalSize + ASize) / (ASize + BMaxSize) );
			NumB = Max(NumB, 1);

			//  There will be one less As. (BABAB)
			NumA = NumB-1;
		}
		else
		{
			// Calc how many A's we are going to have.
			NumA = appCeil( (TotalSize + BMaxSize) / (ASize + BMaxSize) );

			if((TotalSize < 2*ASize) || (NumA <= 1))
			{
				debugf(TEXT("%s : Too small to fit two A into."), *GetName());
				return;
			}

			//  There will be one less Bs. (ABABA)
			NumB = NumA-1;
		}

		// Calc how big this makes each B
		FLOAT BSize = (TotalSize - (NumA * ASize)) / NumB;

		if(bInvertPatternOrder)
		{
			ScopeSize[0] = BSize;
			ScopeSize[1] = ASize;
		}
		else
		{
			ScopeSize[0] = ASize;
			ScopeSize[1] = BSize;	
		}
	}

	// Total number of output scopes
	INT TotalResultScopes = NumA + NumB;
	
	FLOAT TotalDist = 0.f;
	
	// Now do repeats
	if(RepeatAxis == EPBAxis_X)
	{		
		// now generate all the new scopes
		for(INT i=0; i<TotalResultScopes; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = ScopeSize[i%2];
			NewScope.DimZ = InScope.DimZ;

			// Scope frame is the same, only shifted along X for each one			
			FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (TotalDist * InScopeX);
			NewScope.ScopeFrame = InScope.ScopeFrame;
			NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

			// Add to output set
			ResultScopes.AddItem(NewScope);
			
			// Move along split axis
			TotalDist += NewScope.DimX;
		}
	}
	else
	{
		// same as above, but for Z

		for(INT i=0; i<TotalResultScopes; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = InScope.DimX;
			NewScope.DimZ = ScopeSize[i%2];

			FVector NewScopeOrigin = InScope.ScopeFrame.GetOrigin() + (TotalDist * InScopeZ);
			NewScope.ScopeFrame = InScope.ScopeFrame;
			NewScope.ScopeFrame.SetOrigin(NewScopeOrigin);

			ResultScopes.AddItem(NewScope);

			TotalDist += NewScope.DimZ;
		}	
	}

	// Pass on each result to the next rule, alternating between A and B
	check(NextRules.Num() == 2);
	for(INT i=0; i<ResultScopes.Num(); i++)
	{
		INT RuleIdx = (bInvertPatternOrder) ? i+1 : i;
	
		if(NextRules(RuleIdx%2).NextRule)
		{
			NextRules(RuleIdx%2).NextRule->ProcessScope( ResultScopes(i), TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
		}
	}	
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeAlternate::GetRuleNodeTitle()
{
	return FString::Printf(TEXT("%s %s"), *Super::GetRuleNodeTitle(), bInvertPatternOrder ? TEXT("BAB") : TEXT("ABA"));
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeOcclusion




void UPBRuleNodeOcclusion::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	check(NextRules.Num() == 3);
	
	INT OcclusionStatus = CheckScopeOcclusionStatus(InScope, BaseBuilding, ScopeBuilding);
	
	if(NextRules(OcclusionStatus).NextRule)
	{
		NextRules(OcclusionStatus).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeOcclusion::GetRuleNodeTitle()
{
	return Super::GetRuleNodeTitle();
}


//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeRandom

void UPBRuleNodeRandom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Make sure NextRules length matches the number we want

	while (NextRules.Num() < NumOutputs)
	{
		INT idx = NextRules.AddZeroed();
		NextRules(idx).LinkName = FName( *FString::Printf(TEXT("%d"),idx) );
	}

	while (NextRules.Num() > NumOutputs)
	{
		NextRules.Remove(NextRules.Num()-1);
	}
}

void UPBRuleNodeRandom::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	// First build set of possible options
	TArray<INT> RandomChoices;	
	for(INT i=0; i<NextRules.Num(); i++)
	{
		RandomChoices.AddItem(i);
	}

	// Find how many outputs to fire
	INT NumToChoose;
	if(MaxNumExecuted > MinNumExecuted)
	{
		NumToChoose = MinNumExecuted + RandHelper(1 + (MaxNumExecuted - MinNumExecuted));
	}
	else
	{
		NumToChoose = MinNumExecuted;
	}

	// Now iterate removing from the choice set randomly	
	NumToChoose = Min(NumToChoose, RandomChoices.Num());
	for(INT i=0; i<NumToChoose; i++)
	{
		INT ChoiceIndex = RandHelper( RandomChoices.Num() );
		INT NextIndex = RandomChoices(ChoiceIndex);
		RandomChoices.Remove(ChoiceIndex);

		// Pass scope to chosen next rule
		if(NextRules(NextIndex).NextRule)
		{
			NextRules(NextIndex).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeRandom::GetRuleNodeTitle()
{
	return FString::Printf(TEXT("%s (%d-%d)/%d"), *Super::GetRuleNodeTitle(), MinNumExecuted, MaxNumExecuted, NumOutputs);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeEdgeAngle

void UPBRuleNodeEdgeAngle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateRuleConnectors();
}


void UPBRuleNodeEdgeAngle::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	if(NextRules.Num() != Angles.Num())
	{
		debugf(TEXT("UPBRuleNodeSplit: NextRules and SplitSetup sizes do not match!"));
		return;
	}

	// Find desired edge of this scope
	INT EdgeInfoIndex = BaseBuilding->FindEdgeForTopLevelScope(TopLevelScopeIndex, Edge);
	if(EdgeInfoIndex != INDEX_NONE)
	{
		// .. and its angle
		FLOAT EdgeAngle = BaseBuilding->EdgeInfos(EdgeInfoIndex).EdgeAngle;

		// Now find output closest to this angle
		INT OutputIndex = INDEX_NONE;		
		FLOAT ClosestDist = BIG_NUMBER;
		for(INT i=0; i<Angles.Num(); i++)
		{
			FLOAT Dist = Abs(Angles(i).Angle - EdgeAngle);
			if(Dist < ClosestDist)
			{
				ClosestDist = Dist;
				OutputIndex = i;
			}
		}

		// And fire output (if present)
		if((OutputIndex != INDEX_NONE) && (NextRules(OutputIndex).NextRule))
		{
			NextRules(OutputIndex).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
		}
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeEdgeAngle::GetRuleNodeTitle()
{
	FString EdgeText;
	if(Edge == EPBE_Top)
	{
		EdgeText = FString(TEXT("Top"));
	}
	else if(Edge == EPBE_Bottom)
	{
		EdgeText = FString(TEXT("Bottom"));
	}
	else if(Edge == EPBE_Left)
	{
		EdgeText = FString(TEXT("Left"));
	}
	else if(Edge == EPBE_Right)
	{
		EdgeText = FString(TEXT("Right"));
	}

	return FString::Printf(TEXT("%s %s:%d"), *Super::GetRuleNodeTitle(), *EdgeText, Angles.Num());
}


FString UPBRuleNodeEdgeAngle::GetRuleNodeOutputName(INT ConnIndex)
{
	if( (ConnIndex < 0) || (ConnIndex >= NextRules.Num()) || (NextRules.Num() != Angles.Num()) )
	{
		return FString(TEXT(""));
	}

	FString LinkName = NextRules(ConnIndex).LinkName.ToString();

	//LinkName += FString::Printf(TEXT(" (%3.1f)"), Angles(ConnIndex).Angle);

	return LinkName;
}


/** Update the NextRules array based on the SplitSetup array */
void UPBRuleNodeEdgeAngle::UpdateRuleConnectors()
{
	// First, save off the old connections
	TArray<FPBRuleLink> OldConnections = NextRules;

	// Then recreate the NextRules array using the SplitSetup array
	NextRules.Empty();	
	NextRules.AddZeroed( Angles.Num() );

	for(INT i=0; i<Angles.Num(); i++)
	{		
		// Ensure each connector has a name
		NextRules(i).LinkName = FName( *FString::Printf(TEXT("%3.1f"), Angles(i).Angle) );
	}

	FixUpConnections(OldConnections);
}
//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeLODQuad

void UPBRuleNodeLODQuad::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	FVector2D MinUV(0.f, 0.f);
	FVector2D MaxUV(1.f, 1.f);


	// Get the top level scope that this forms part of, and UV info for it
	const FPBScope2D& TopLevelScope = BaseBuilding->TopLevelScopes(TopLevelScopeIndex);
	const FPBFaceUVInfo& TopLevelUVInfo = BaseBuilding->TopLevelScopeUVInfos(TopLevelScopeIndex);
	
	// Get our offset from the top level scope origin
	FVector ScopeOffset = InScope.ScopeFrame.GetOrigin() - TopLevelScope.ScopeFrame.GetOrigin();
	
	// Find out how far across the top level scope the origin of this scope is (0-1)
	FLOAT XOffsetFactor = (ScopeOffset | TopLevelScope.ScopeFrame.GetAxis(0))/TopLevelScope.DimX;
	FLOAT ZOffsetFactor = (ScopeOffset | TopLevelScope.ScopeFrame.GetAxis(2))/TopLevelScope.DimZ;
	

	// Find out how big this scope is relative to top level scope (0-1)
	FLOAT XSizeFactor = InScope.DimX/TopLevelScope.DimX;
	FLOAT ZSizeFactor = InScope.DimZ/TopLevelScope.DimZ;
	
	
	// Now calculate UVs for this quad region
	MinUV.X = TopLevelUVInfo.Offset.X + (XOffsetFactor * TopLevelUVInfo.Size.X);
	MaxUV.Y = TopLevelUVInfo.Offset.Y + ((1.f-ZOffsetFactor) * TopLevelUVInfo.Size.Y);
	
	MaxUV.X = MinUV.X + (XSizeFactor * TopLevelUVInfo.Size.X);
	MinUV.Y = MaxUV.Y - (ZSizeFactor * TopLevelUVInfo.Size.Y);

	UStaticMeshComponent* ScopeComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), BaseBuilding);
	check(ScopeComp);

	// Set mesh
	ScopeComp->Scale3D.X = InScope.DimX/100.f;
	ScopeComp->Scale3D.Z = InScope.DimZ/100.f;
	ScopeComp->Scale3D.Y = 0.5f * (ScopeComp->Scale3D.X + ScopeComp->Scale3D.Z); // Hack here to try and keep scale vaguely uniform, as this messes up cubemaps
	
	ScopeComp->SetStaticMesh(GEngine->BuildingQuadStaticMesh);
	
	// Set offset from actor
	ScopeComp->Translation = InScope.ScopeFrame.GetOrigin();
	ScopeComp->Rotation = InScope.ScopeFrame.Rotator();

	// Settings from StaticMeshActor
	ScopeComp->bAllowApproximateOcclusion = TRUE;
	ScopeComp->bCastDynamicShadow = TRUE;
	ScopeComp->bForceDirectLightMap = FALSE;
	ScopeComp->bUsePrecomputedShadows = FALSE;
	ScopeComp->BlockNonZeroExtent = FALSE;
	ScopeComp->BlockZeroExtent = FALSE;
	ScopeComp->bAcceptsLights = TRUE;

	// This will stop us creating an RB_BodyInstance for each component
	ScopeComp->bDisableAllRigidBody = TRUE;

	// Set up parent LOD component
	ScopeComp->ReplacementPrimitive = LODParent;
	ScopeComp->MassiveLODDistance = MassiveLODDistanceScale * BaseBuilding->SimpleMeshMassiveLODDistance;

	// Add to info array
	INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;

	// remember that this component is a LOD quad component
	BaseBuilding->LODMeshComps.AddItem(ScopeComp);	
	
	// Save info for UV
	INT UVInfoIndex = BaseBuilding->LODMeshUVInfos.AddZeroed();
	FPBFaceUVInfo& UVInfo = BaseBuilding->LODMeshUVInfos(UVInfoIndex);
	
	UVInfo.Offset.X = MinUV.X;
	UVInfo.Offset.Y = MinUV.Y;
	UVInfo.Size.X = (MaxUV.X - MinUV.X);
	UVInfo.Size.Y = (MaxUV.Y - MinUV.Y);
	
	// Fire next rule, but passing in quad as the LOD parent
	if(NextRules(0).NextRule)
	{
		NextRules(0).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, ScopeComp );		
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeLODQuad::GetRuleNodeTitle()
{
	return Super::GetRuleNodeTitle();
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeCorner

void UPBRuleNodeCorner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateRuleConnectors();
}

/** Util to find the index of the closest match  */
static INT FindBestAngleIndex(const TArray<FRBCornerAngleInfo>& Angles, FLOAT InAngle)
{
	// Now find output closest to this angle
	INT AngleIndex = INDEX_NONE;		
	FLOAT ClosestDist = BIG_NUMBER;
	for(INT i=0; i<Angles.Num(); i++)
	{
		FLOAT Dist = Abs(Angles(i).Angle - InAngle);
		if(Dist < ClosestDist)
		{
			ClosestDist = Dist;
			AngleIndex = i;
		}
	}

	return AngleIndex;
}

void UPBRuleNodeCorner::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	// If not enough space to fit in corners, or no corner meshes supplied, just pass on scope
	if((InScope.DimX < 2*CornerSize) || NextRules.Num() <= 1)
	{
		if(NextRules.Num() > 0 && NextRules(0).NextRule)
		{
			NextRules(0).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
		}
		return;
	}	

	// Copy to make 'main' scope we will modify pass on
	FPBScope2D MainScope = InScope;

	// First, see if we want to cut off the left edge, if there is an edge/angle there
	INT LeftEdgeInfoIndex = BaseBuilding->FindEdgeForTopLevelScope(TopLevelScopeIndex, EPSA_Left);
	if(LeftEdgeInfoIndex != INDEX_NONE)
	{
		FLOAT LeftEdgeAngle = BaseBuilding->EdgeInfos(LeftEdgeInfoIndex).EdgeAngle;
		if( (Abs(LeftEdgeAngle) > FlatThreshold) && (!bNoMeshForConcaveCorners || LeftEdgeAngle > 0.f) )
		{	
			FLOAT LeftCornerSize = GetCornerSizeForAngle(LeftEdgeAngle);

			// Create scope for left-edge 'corner' mesh
			FPBScope2D LeftScope = MainScope;
			LeftScope.DimX = LeftCornerSize;

			INT AngleIndex = FindBestAngleIndex(Angles, LeftEdgeAngle);
			if(NextRules(AngleIndex+1).NextRule)
			{
				NextRules(AngleIndex+1).NextRule->ProcessScope( LeftScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
			}

			// And shift scope over
			MainScope.OffsetLocal(FVector(LeftCornerSize,0,0));
			MainScope.DimX -= LeftCornerSize;
		}
	}
	
	// Then see if we want to cut off the right edge, again if there is an edge/angle
	INT RightEdgeInfoIndex = BaseBuilding->FindEdgeForTopLevelScope(TopLevelScopeIndex, EPSA_Right);
	if(RightEdgeInfoIndex != INDEX_NONE)
	{
		FLOAT RightEdgeAngle = BaseBuilding->EdgeInfos(RightEdgeInfoIndex).EdgeAngle;

		FLOAT RightCornerSize = 0.f;

		if(bUseAdjacentRulesetForRightGap)
		{
			// Look at corner node of adjacent face for how much gap to leave
			// TODO: This always looks a the top node at the moment! Should find closest z match? hmm tricky
			UPBRuleNodeCorner* Corner = FindCornerNodeForEdge(BaseBuilding, RightEdgeInfoIndex, TRUE);
			if(Corner)
			{
				RightCornerSize = Corner->GetCornerSizeForAngle(RightEdgeAngle);
			}
		}
		else
		{
			RightCornerSize = GetCornerSizeForAngle(RightEdgeAngle);
		}

		if( (Abs(RightEdgeAngle) > FlatThreshold) && (!bNoMeshForConcaveCorners || RightEdgeAngle > 0.f) )
		{
			MainScope.DimX -= RightCornerSize;
		}
	}

	// Process scope for main front of building
	if(NextRules(0).NextRule)
	{
		NextRules(0).NextRule->ProcessScope( MainScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
	}
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeCorner::GetRuleNodeTitle()
{
	return FString::Printf(TEXT("%s (%3.0f)"), *Super::GetRuleNodeTitle(), CornerSize);
}

FColor UPBRuleNodeCorner::GetRuleNodeTitleColor()
{
	return FColor(71,138,97);
}

void UPBRuleNodeCorner::RuleNodeCreated(UProcBuildingRuleset* Ruleset)
{
	Super::RuleNodeCreated(Ruleset);

	UpdateRuleConnectors();
}

/** Update the NextRules array based on the Angles array */
void UPBRuleNodeCorner::UpdateRuleConnectors()
{
	// First, save off the old connections
	TArray<FPBRuleLink> OldConnections = NextRules;

	// Then recreate the NextRules array using the SplitSetup array
	NextRules.Empty();	
	NextRules.AddZeroed( 1 + Angles.Num() );
	
	// Set name for main surface
	NextRules(0).LinkName = FName(TEXT("Main"));
	
	// Set names for angle connectors
	for(INT i=0; i<Angles.Num(); i++)
	{		
		NextRules(i+1).LinkName = FName( *FString::Printf(TEXT("%3.1f"), Angles(i).Angle) );
	}

	FixUpConnections(OldConnections);
}

/** For a given angle, return the size that this corner node will use on the left of the face */
FLOAT UPBRuleNodeCorner::GetCornerSizeForAngle(FLOAT EdgeAngle)
{
	FLOAT Size = CornerSize;

	INT AngleIndex = FindBestAngleIndex(Angles, EdgeAngle);
	if((AngleIndex != INDEX_NONE) && (Angles(AngleIndex).CornerSize != 0.f))
	{
		Size = Angles(AngleIndex).CornerSize;
	}

	return Size;
}


UPBRuleNodeCorner* UPBRuleNodeCorner::GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	return this;
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeSize

void UPBRuleNodeSize::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	FPBScope2D& UseScope = bUseTopLevelScopeSize ? BaseBuilding->TopLevelScopes(TopLevelScopeIndex) : InScope;

	// Get the size we are interested in
	FLOAT Size = (SizeAxis == EPBAxis_X) ? UseScope.DimX : UseScope.DimZ;
	
	ensure(NextRules.Num() == 2);
	
	// Then fire output based on that
	if(Size < DecisionSize)
	{
		if(NextRules(0).NextRule)
		{
			NextRules(0).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
		}
	}
	else
	{
		if(NextRules(1).NextRule)
		{
			NextRules(1).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
		}
	}	
#endif // WITH_EDITORONLY_DATA
}

FString UPBRuleNodeSize::GetRuleNodeTitle()
{
	FString DirString = (SizeAxis == EPBAxis_X) ? TEXT("X") : TEXT("Z");
	
	return FString::Printf(TEXT("%s (%s: %3.0f)"), *Super::GetRuleNodeTitle(), *DirString, DecisionSize);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeEdgeMesh

void UPBRuleNodeEdgeMesh::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	check(NextRules.Num() == 2);

	// We will always pass the whole face through to the first rule
	if(NextRules(0).NextRule)
	{
		FPBScope2D MainScope = InScope;

		// Ensure we don't pull in more than the size of the face
		FLOAT PullInAmount = Min(MainXPullIn, 0.5f*MainScope.DimX);

		MainScope.OffsetLocal( FVector(PullInAmount,0,0) );
		MainScope.DimX -= (2.f * PullInAmount);

		NextRules(0).NextRule->ProcessScope( MainScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
	}

	// Now the interesting part- extract a zero-width scope for the left edge, and rotate to half angle

	INT LeftEdgeInfoIndex = BaseBuilding->FindEdgeForTopLevelScope(TopLevelScopeIndex, EPSA_Left);
	if(LeftEdgeInfoIndex != INDEX_NONE)
	{
		FLOAT LeftEdgeAngle = BaseBuilding->EdgeInfos(LeftEdgeInfoIndex).EdgeAngle;
	
		// If not flat, and there is a next rule for the edge scope, work it out
		if( (Abs(LeftEdgeAngle) > FlatThreshold) && (NextRules(1).NextRule) )
		{
			// Start by copying input scope
			FPBScope2D EdgeScope = InScope;
			EdgeScope.DimX = 0.f;
			
			// Now we want to rotate the X and Y axes of the frame around Z, by half the angle between the faces
			FVector RotAxis = EdgeScope.ScopeFrame.GetAxis(2);
			FQuat RotQuat(RotAxis, 0.5f * LeftEdgeAngle * (PI/180.f));
			
			// Now rotate the vectors of the matrix 
			FVector NewX = RotQuat.RotateVector(EdgeScope.ScopeFrame.GetAxis(0));	
			FVector NewY = RotQuat.RotateVector(EdgeScope.ScopeFrame.GetAxis(1));
			
			// and update matrix
			EdgeScope.ScopeFrame.SetAxis(0, NewX);
			EdgeScope.ScopeFrame.SetAxis(1, NewY);
			
			// Finally pass to next rule (we checked this was valid above)
			NextRules(1).NextRule->ProcessScope( EdgeScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
		}
	}
#endif // WITH_EDITORONLY_DATA
}


//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeVariation

// UObject interface
void UPBRuleNodeVariation::RegenVariationOutputs(UProcBuildingRuleset* Ruleset)
{
	check(Ruleset);

	// First, save off the old connections
	TArray<FPBRuleLink> OldConnections = NextRules;

	// Then recreate the NextRules array using the SplitSetup array
	NextRules.Empty();
	NextRules.AddZeroed( Ruleset->Variations.Num() + 1 );

	NextRules(0) = OldConnections(0);
	NextRules(0).LinkName = FName(TEXT("Default"));

	for(INT VarIdx=0; VarIdx<Ruleset->Variations.Num(); VarIdx++)
	{		
		INT ConnIdx = VarIdx+1;

		// Copy name over
		NextRules(ConnIdx).LinkName = Ruleset->Variations(VarIdx).VariationName;
	}

	FixUpConnections(OldConnections);
}

INT UPBRuleNodeVariation::GetVariationOutputIndex(AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	// Default to first ('default') output
	INT ExecuteIndex = 0;

#if WITH_EDITORONLY_DATA
	// Grab the variation name from the top-level scope info
	FName VariationName = NAME_None;

	// From scope on left
	if(bVariationOfScopeOnLeft)
	{
		// Get the edge to the left of this scope
		INT LeftEdgeInfoIndex = BaseBuilding->FindEdgeForTopLevelScope(TopLevelScopeIndex, EPSA_Left);
		const FPBEdgeInfo& EdgeInfo = BaseBuilding->EdgeInfos(LeftEdgeInfoIndex);
		// Find the scope whose right edge is this edge
		INT LeftScopeIndex = (EdgeInfo.ScopeAEdge == EPSA_Right) ? EdgeInfo.ScopeAIndex : EdgeInfo.ScopeBIndex;
		// If we found a scope to the left, use its variation name
		if(LeftScopeIndex != INDEX_NONE)
		{
			VariationName = BaseBuilding->TopLevelScopeInfos(LeftScopeIndex).RulesetVariation;
		}
	}
	// From this scope
	else
	{
		VariationName = BaseBuilding->TopLevelScopeInfos(TopLevelScopeIndex).RulesetVariation;
	}

	// If a name was specified, see if we have an output named that
	if(VariationName != NAME_None)
	{
		for(INT ConnIdx=1; ConnIdx<NextRules.Num(); ConnIdx++)
		{
			if(NextRules(ConnIdx).LinkName == VariationName)
			{
				ExecuteIndex = ConnIdx;
				break;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return ExecuteIndex;
}

// PBRuleNodeBase interface
void UPBRuleNodeVariation::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	INT ExecuteIndex = GetVariationOutputIndex(BaseBuilding, TopLevelScopeIndex);

	// Fire the desired output
	if(NextRules(ExecuteIndex).NextRule)
	{
		NextRules(ExecuteIndex).NextRule->ProcessScope( InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
	}
#endif // WITH_EDITORONLY_DATA
}

UPBRuleNodeCorner* UPBRuleNodeVariation::GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	INT ExecuteIndex = GetVariationOutputIndex(BaseBuilding, TopLevelScopeIndex);
	UPBRuleNodeCorner* CornerNode = NULL;

	// Fire the desired output
	if(NextRules(ExecuteIndex).NextRule)
	{
		CornerNode = NextRules(ExecuteIndex).NextRule->GetCornerNode( bTop, BaseBuilding, TopLevelScopeIndex );		
	}

	return CornerNode;
}

FString UPBRuleNodeVariation::GetRuleNodeTitle()
{
	FString TitleString = Super::GetRuleNodeTitle();

	if(bVariationOfScopeOnLeft)
	{
		TitleString += TEXT(" (Left)");
	}

	return TitleString;
}

void UPBRuleNodeVariation::RuleNodeCreated(UProcBuildingRuleset* Ruleset)
{
	Super::RuleNodeCreated(Ruleset);

	RegenVariationOutputs(Ruleset);
}

//////////////////////////////////////////////////////////////////////////
// UPBRuleNodeComment


void UPBRuleNodeComment::DrawRuleNode(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, UBOOL bSelected)
{
#if WITH_EDITORONLY_DATA
	if(bFilled)
	{
		DrawTile(Canvas, RulePosX, RulePosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor );
	}

	// Draw frame
	const FColor FrameColor = bSelected ? FColor(255,255,0) : BorderColor;

	const INT MinDim = Min(SizeX, SizeY);
	const INT UseBorderWidth = Clamp( BorderWidth, 0, (MinDim/2)-3 );

	for(INT i=0; i<UseBorderWidth; i++)
	{
		DrawLine2D(Canvas, FVector2D(RulePosX,				RulePosY + i),			FVector2D(RulePosX + SizeX,		RulePosY + i),			FrameColor );
		DrawLine2D(Canvas, FVector2D(RulePosX + SizeX - i,	RulePosY),				FVector2D(RulePosX + SizeX - i,	RulePosY + SizeY),		FrameColor );
		DrawLine2D(Canvas, FVector2D(RulePosX + SizeX,		RulePosY + SizeY - i),	FVector2D(RulePosX,				RulePosY + SizeY - i),	FrameColor );
		DrawLine2D(Canvas, FVector2D(RulePosX + i,			RulePosY + SizeY),		FVector2D(RulePosX + i,			RulePosY - 1),			FrameColor );
	}

	// Draw little sizing triangle in bottom left.
	const INT HandleSize = 16;
	const FIntPoint A(RulePosX + SizeX,				RulePosY + SizeY);
	const FIntPoint B(RulePosX + SizeX,				RulePosY + SizeY - HandleSize);
	const FIntPoint C(RulePosX + SizeX - HandleSize,	RulePosY + SizeY);
	const BYTE TriangleAlpha = (bSelected) ? 255 : 32; // Make it more transparent if comment is not selected.

	const UBOOL bHitTesting = Canvas->IsHitTesting();

	if(bHitTesting)  Canvas->SetHitProxy( new HLinkedObjProxySpecial(this, 1) );
	DrawTriangle2D(Canvas, A, FVector2D(0,0), B, FVector2D(0,0), C, FVector2D(0,0), FColor(0,0,0,TriangleAlpha) );
	if(bHitTesting)  Canvas->SetHitProxy( NULL );

	// Draw comment text

	// Check there are some valid chars in string. If not - we can't select it! So we force it back to default.
	const FLOAT OldZoom2D = FLinkedObjDrawUtils::GetUniformScaleFromMatrix(Canvas->GetTransform());

	FTextSizingParameters Parameters(GEngine->SmallFont, 1.f, 1.f);
	FLOAT& XL = Parameters.DrawXL, &YL = Parameters.DrawYL;

	UCanvas::CanvasStringSize( Parameters, *Comment );

	// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
	const INT x = appTrunc(RulePosX*OldZoom2D + 2);
	const INT y = appTrunc(RulePosY*OldZoom2D - YL - 2);


	// Viewport cull at a zoom of 1.0, because that's what we'll be drawing with.
	if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, RulePosX, RulePosY-YL-2, SizeX * OldZoom2D, YL ) )
	{
		Canvas->PushRelativeTransform(FScaleMatrix(1.0f / OldZoom2D));
		{
			const UBOOL bHitTesting = Canvas->IsHitTesting();

			DrawShadowedString(Canvas, x, y, *Comment, GEngine->SmallFont, FColor(64,64,192) );

			// We only set the hit proxy for the area above the comment box with the height of the comment text
			if(bHitTesting)
			{
				Canvas->SetHitProxy(new HLinkedObjProxy(this));

				// account for the +2 when x was assigned
				DrawTile(Canvas, x - 2, y, SizeX * OldZoom2D, YL, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.0f, 0.0f, 0.0f));

				Canvas->SetHitProxy(NULL);
			}
		}
		Canvas->PopTransform();
	}

	// Fill in base SequenceObject rendering info (used by bounding box calculation).
	DrawWidth = SizeX;
	DrawHeight = SizeY;
#endif // WITH_EDITORONLY_DATA
}


///////////////////////////////////////////////////////////////////////////
// UPBRuleNodeSubRuleset

void UPBRuleNodeSubRuleset::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	if(SubRuleset && SubRuleset->RootRule)
	{
		SubRuleset->RootRule->ProcessScope(InScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent);
	}
#endif // WITH_EDITORONLY_DATA
}

UPBRuleNodeCorner* UPBRuleNodeSubRuleset::GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex)
{
	UPBRuleNodeCorner* CornerNode = NULL;

	if(SubRuleset && SubRuleset->RootRule)
	{
		CornerNode = SubRuleset->RootRule->GetCornerNode(bTop, BaseBuilding, TopLevelScopeIndex);
	}

	return CornerNode;
}

FString UPBRuleNodeSubRuleset::GetRuleNodeTitle()
{
	FString SubName = TEXT("None");
	if(SubRuleset)
	{
		SubName = *SubRuleset->GetName();
	}

	FString TitleString = FString::Printf(TEXT("%s : %s"), *Super::GetRuleNodeTitle(), *SubName);

	return TitleString;
}

FColor UPBRuleNodeSubRuleset::GetRuleNodeTitleColor()
{
	return FColor(209,86,70);
}

///////////////////////////////////////////////////////////////////////////
// UPBRuleNodeWindowWall

static FVector2D CalcUVFromBounds(const FVector& Vert, const FBox& Bounds)
{
	FVector2D Result(0,0);

	Result.X = (Vert.X - Bounds.Min.X)/(Bounds.Max.X - Bounds.Min.X);
	Result.Y = 1.f - ((Vert.Z - Bounds.Min.Z)/(Bounds.Max.Z - Bounds.Min.Z));

	return Result;
}

static void PlanarMapTris(TArray<FStaticMeshTriangle>& Triangles, INT UVChannel)
{
	check((UVChannel >= 0) && (UVChannel < 8));

	FBox MeshBounds(0);
	for(INT TriIdx=0; TriIdx<Triangles.Num(); TriIdx++)
	{
		MeshBounds += Triangles(TriIdx).Vertices[0];
		MeshBounds += Triangles(TriIdx).Vertices[1];
		MeshBounds += Triangles(TriIdx).Vertices[2];
	}

	for(INT TriIdx=0; TriIdx<Triangles.Num(); TriIdx++)
	{
		for(INT VertIdx=0; VertIdx<3; VertIdx++)
		{
			FVector2D NewUV = CalcUVFromBounds(Triangles(TriIdx).Vertices[VertIdx], MeshBounds);
			Triangles(TriIdx).UVs[VertIdx][UVChannel] = NewUV;
		}
	}
}


/** Util to create a quad give a scope, with the supplied UV coords */
static void AddTrisForCell(const FVector& CellOrigin, const FVector& CellSize,  const FLOAT WinOffset[4], TArray<FStaticMeshTriangle>& OutTriangles)
{
	TArray<FStaticMeshTriangle> Triangles;

	// Generate vertex info
	FVector Vert[8];

	//  3      2
	//   7    6
	//
	//   4    5
	//  0      1

	// Outside verts
	Vert[0] = CellOrigin + FVector(0,			0,	0);
	Vert[1] = CellOrigin + FVector(CellSize.X,	0,	0);
	Vert[2] = CellOrigin + FVector(CellSize.X,	0,	CellSize.Z);
	Vert[3] = CellOrigin + FVector(0,			0,	CellSize.Z);

	// Inside verts
	Vert[4] = Vert[0] + FVector(WinOffset[0],	0, WinOffset[2]);
	Vert[5] = Vert[1] + FVector(-WinOffset[1],	0, WinOffset[2]);
	Vert[6] = Vert[2] + FVector(-WinOffset[1],	0, -WinOffset[3]);
	Vert[7] = Vert[3] + FVector(WinOffset[0],	0, -WinOffset[3]);

	INT TriVerts[8][3] = 
	{
		{0, 1, 5},
		{0, 5, 4},
		{1, 2, 5},
		{5, 2, 6},
		{6, 2, 7},
		{7, 2, 3},
		{0, 4, 7},
		{0, 7, 3}
	};

	// Now make triangles
	for(INT i=0; i<8; i++)
	{
		FStaticMeshTriangle NewTri;
		appMemzero(&NewTri, sizeof(FStaticMeshTriangle));

		NewTri.Vertices[0] = Vert[ TriVerts[i][0] ];
		NewTri.Vertices[1] = Vert[ TriVerts[i][1] ];
		NewTri.Vertices[2] = Vert[ TriVerts[i][2] ];

		NewTri.NumUVs = 2;

		Triangles.AddItem(NewTri);
	}

	// Generate UVs for this cell
	PlanarMapTris(Triangles, 0);

	OutTriangles.Append(Triangles);
}


void UPBRuleNodeWindowWall::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	check(NextRules.Num() >= 1);

	// Create static mesh object
	UStaticMesh* WallMesh = ConstructObject<UStaticMesh>(UStaticMesh::StaticClass(), BaseBuilding->GetOuter());
	check(WallMesh);

	new(WallMesh->LODModels) FStaticMeshRenderData();
	WallMesh->LODInfo.AddItem(FStaticMeshLODInfo());

	// Fill in tri data
	WallMesh->LODModels(0).RawTriangles.RemoveBulkData();	
	WallMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);

	TArray<FStaticMeshTriangle> Triangles;

	INT NumX = appCeil(InScope.DimX/CellMaxSizeX);
	INT NumZ = appCeil(InScope.DimZ/CellMaxSizeZ);

	FVector CellSize(InScope.DimX/NumX, 0, InScope.DimZ/NumZ);
	FVector WinSize(WindowSizeX, 0, WindowSizeZ);
	FVector WinPos(WindowPosX, 0, WindowPosZ);

	// If desired, scale down window based on how much cell is scaled down
	if(bScaleWindowWithCell)
	{
		WinSize.X *= (CellSize.X/CellMaxSizeX);
		WinSize.Z *= (CellSize.Z/CellMaxSizeZ);
	}

	// Calc difference between win and cell size
	FVector WinSpace = (CellSize - WinSize);

	FLOAT WinBorder[4];
	WinBorder[0] = WinPos.X * WinSpace.X; // left
	WinBorder[1] = (1.f - WinPos.X) * WinSpace.X; // right
	WinBorder[2] = WinPos.Z * WinSpace.Z; // bottom 
	WinBorder[3] = (1.f - WinPos.Z) * WinSpace.Z; // top

	// Iterate over each window 'cell'
	for(INT XIdx=0; XIdx<NumX; XIdx++)
	{
		for(INT ZIdx=0; ZIdx<NumZ; ZIdx++)
		{
			FVector CellOrigin(CellSize.X * XIdx, 0, CellSize.Z * ZIdx);

			// Add tris for region around window
			AddTrisForCell(CellOrigin, CellSize, WinBorder, Triangles);

			// Pass small window scope on to next rule
			if(NextRules(0).NextRule)
			{
				FPBScope2D WindowScope = InScope;
				WindowScope.DimX = WinSize.X;
				WindowScope.DimZ = WinSize.Z;
				WindowScope.OffsetLocal( CellOrigin + FVector(WinBorder[0], 0, WinBorder[2]) );

				NextRules(0).NextRule->ProcessScope( WindowScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
			}
		}
	}

	// Generate UVs for entire area
	PlanarMapTris(Triangles, 1);

	void* RawTriangleData = WallMesh->LODModels(0).RawTriangles.Realloc(Triangles.Num());
	check( WallMesh->LODModels(0).RawTriangles.GetBulkDataSize() == Triangles.Num() * Triangles.GetTypeSize() );
	appMemcpy( RawTriangleData, Triangles.GetData(), WallMesh->LODModels(0).RawTriangles.GetBulkDataSize() );

	WallMesh->LODModels(0).RawTriangles.Unlock();

	new(WallMesh->LODModels(0).Elements) FStaticMeshElement(Material, 0);

	WallMesh->Build(FALSE, TRUE);

	WallMesh->LightMapCoordinateIndex = 1;
	WallMesh->LightMapResolution = ScopeBuilding->NonRectWallLightmapRes;

	// Create component for this mesh
	UStaticMeshComponent* ScopeComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), BaseBuilding);
	check(ScopeComp);

	// Set mesh
	ScopeComp->SetStaticMesh(WallMesh);

	// Set offset from actor
	ScopeComp->Translation = InScope.ScopeFrame.GetOrigin();
	ScopeComp->Translation += (YOffset * InScope.ScopeFrame.GetAxis(1)); // apply y offset
	ScopeComp->Rotation = InScope.ScopeFrame.Rotator();

	SetPBMeshCollisionSettings(ScopeComp, FALSE);

	// Set up parent LOD component
	ScopeComp->ReplacementPrimitive = LODParent;

	// Add to info array
	INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
	BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = TopLevelScopeIndex;
#endif // WITH_EDITORONLY_DATA
}


FString UPBRuleNodeWindowWall::GetRuleNodeTitle()
{
	return Super::GetRuleNodeTitle();
}

FColor UPBRuleNodeWindowWall::GetRuleNodeTitleColor()
{
	return Super::GetRuleNodeTitleColor();
}

///////////////////////////////////////////////////////////////////////////
// UPBRuleNodeTransform

void UPBRuleNodeTransform::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	check(NextRules.Num() == 1);

	// Get translation for scope
	FVector ScopeTrans(0,0,0);
	if(Translation)
	{
		ScopeTrans = Translation->GetValue();
	}

	// Get rotation for scope
	FRotator ScopeRot(0,0,0);
	if(Rotation)
	{
		FVector RotVec = Rotation->GetValue();
		ScopeRot = FRotator( appFloor(RotVec.Y * Deg2U), appFloor(RotVec.Z * Deg2U), appFloor(RotVec.X * Deg2U) );
	}

	// Get scale for scope
	FVector ScopeScale(1,1,1);
	if(Scale)
	{
		ScopeScale = Scale->GetValue();
	}

	// Build a matrix from the result
	FMatrix ScopeTransform = FScaleRotationTranslationMatrix(ScopeScale, ScopeRot, ScopeTrans);

	// Make a new scope, starting with the one passed in, and apply transformation
	FPBScope2D TransformedScope = InScope;
	TransformedScope.ScopeFrame = ScopeTransform * TransformedScope.ScopeFrame;

	// Fire the desired output
	if(NextRules(0).NextRule)
	{
		NextRules(0).NextRule->ProcessScope( TransformedScope, TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );		
	}
#endif // WITH_EDITORONLY_DATA
}

///////////////////////////////////////////////////////////////////////////
// UPBRuleNodeCycle

FString UPBRuleNodeCycle::GetRuleNodeTitle()
{
	FString DirString = (RepeatAxis == EPBAxis_X) ? TEXT("X") : TEXT("Z");

	return FString::Printf(TEXT("%s %s:%3.0f"), *Super::GetRuleNodeTitle(), *DirString, RepeatSize);
}


/** Util to regenerate the outputs, base on CycleSize */
void UPBRuleNodeCycle::UpdateOutputs()
{
	// First, save off the old connections
	TArray<FPBRuleLink> OldConnections = NextRules;

	// Then recreate the NextRules array using the SplitSetup array
	NextRules.Empty();	
	NextRules.AddZeroed( 1 + CycleSize );

	NextRules(0).LinkName = FName(TEXT("Remainder"));

	for(INT i=0; i<CycleSize; i++)
	{		
		// Copy name over
		NextRules(i+1).LinkName = FName( *FString::Printf(TEXT("Step %d"), i) );
	}

	FixUpConnections(OldConnections);	
}

void UPBRuleNodeCycle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateOutputs();
}

void UPBRuleNodeCycle::ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent)
{
#if WITH_EDITORONLY_DATA
	TArray<FPBScope2D> ResultScopes;
	INT RemainderResultIndex = INDEX_NONE;

	check((RepeatAxis == EPBAxis_X) || (RepeatAxis == EPBAxis_Z));

	if(RepeatSize < KINDA_SMALL_NUMBER)
	{
		debugf(TEXT("%s : RepeatMaxSize too small."), *GetName());
		return;
	}

	const FVector InScopeX = InScope.ScopeFrame.GetAxis(0);
	const FVector InScopeZ = InScope.ScopeFrame.GetAxis(2);

	if(RepeatAxis == EPBAxis_X)
	{	
		INT NumSplits = 0;
		FLOAT RepeatDimX = 0.f;

		if(bFixRepeatSize)
		{
			RepeatDimX = RepeatSize;
			NumSplits = appFloor(InScope.DimX / RepeatSize);
		}
		else
		{
			NumSplits = appCeil(InScope.DimX / RepeatSize);
			NumSplits = Max(NumSplits, 1);
			RepeatDimX = InScope.DimX / FLOAT(NumSplits);
		}

		// now generate all the new scopes
		for(INT i=0; i<NumSplits; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = RepeatDimX;
			NewScope.DimZ = InScope.DimZ; // Unchanged from parent scope

			// Scope frame is the same, only shifted along X for each one			
			NewScope.ScopeFrame = InScope.ScopeFrame;
			NewScope.OffsetLocal( FVector(i * RepeatDimX,0,0) );

			// Add to output
			ResultScopes.AddItem(NewScope);
		}

		if(bFixRepeatSize)
		{			
			FLOAT CoveredDist = (NumSplits * RepeatSize);
			FLOAT RemainderDist = InScope.DimX - CoveredDist;

			if(RemainderDist > KINDA_SMALL_NUMBER)
			{			
				FPBScope2D RemainderScope;
				RemainderScope.DimX = RemainderDist;
				RemainderScope.DimZ = InScope.DimZ;

				// Scope frame is the same, only shifted along X for each one			
				RemainderScope.ScopeFrame = InScope.ScopeFrame;		
				RemainderScope.OffsetLocal(FVector(CoveredDist,0,0));

				RemainderResultIndex = ResultScopes.AddItem(RemainderScope);
			}
		}
	}
	// As above, but along Z
	else
	{
		INT NumSplits = 0;
		FLOAT RepeatDimZ = 0.f;

		if(bFixRepeatSize)
		{
			RepeatDimZ = RepeatSize;
			NumSplits = appFloor(InScope.DimZ / RepeatSize);
		}
		else
		{
			NumSplits = appCeil(InScope.DimZ / RepeatSize);
			NumSplits = Max(NumSplits, 1);
			RepeatDimZ = InScope.DimZ / FLOAT(NumSplits);
		}

		for(INT i=0; i<NumSplits; i++)
		{
			FPBScope2D NewScope;
			NewScope.DimX = InScope.DimX;
			NewScope.DimZ = RepeatDimZ; 

			NewScope.ScopeFrame = InScope.ScopeFrame;
			NewScope.OffsetLocal( FVector(0,0,i * RepeatDimZ) );

			ResultScopes.AddItem(NewScope);
		}

		if(bFixRepeatSize)
		{			
			FLOAT CoveredDist = (NumSplits * RepeatSize);
			FLOAT RemainderDist = InScope.DimZ - CoveredDist;

			if(RemainderDist > KINDA_SMALL_NUMBER)
			{			
				FPBScope2D RemainderScope;
				RemainderScope.DimX = InScope.DimX;
				RemainderScope.DimZ = RemainderDist;

				RemainderScope.ScopeFrame = InScope.ScopeFrame;		
				RemainderScope.OffsetLocal(FVector(0,0,CoveredDist));

				RemainderResultIndex = ResultScopes.AddItem(RemainderScope);
			}
		}
	}

	// Pass on each result to the next rule
	INT CycleStep = 0;
	for(INT i=0; i<ResultScopes.Num(); i++)
	{
		// See if this is the 'remainder' scope, pass it to output 0
		if(i == RemainderResultIndex)
		{
			if(NextRules(0).NextRule)
			{
				NextRules(0).NextRule->ProcessScope( ResultScopes(i), TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
			}
		}
		// This is a regular scope, workin around the outputs
		else
		{
			INT OutIndex = CycleStep+1; // output 0 is the remainder one
			if((OutIndex < NextRules.Num()) && (NextRules(OutIndex).NextRule))
			{
				NextRules(OutIndex).NextRule->ProcessScope( ResultScopes(i), TopLevelScopeIndex, BaseBuilding, ScopeBuilding, LODParent );
			}	

			// Increment cycle step, and reset if necessary
			CycleStep++;
			if(CycleStep == CycleSize)
			{
				CycleStep = 0;
			}
		}
	}	
#endif // WITH_EDITORONLY_DATA
}