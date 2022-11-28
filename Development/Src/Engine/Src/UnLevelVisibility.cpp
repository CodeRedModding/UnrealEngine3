/*=============================================================================
	UnLevelVisibility.cpp: Level visibility-related functions
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

FLevelVisibilitySet::FSubset FLevelVisibilitySet::GetBoxIntersectionSubset(const FLevelVisibilitySet::FSubset& Subset,const FVector& BoxOrigin,const FVector& BoxExtent) const
{
	FZoneSet	IntersectionVisibleZones = Subset.EntirelyVisibleZones,
				IntersectionEntirelyVisibleZones = Subset.EntirelyVisibleZones;

	// Check each visibility volume for intersection with the box.
	for(TMultiMap<INT,FConvexVolume>::TConstIterator It(VisibilityVolumes);It;++It)
	{
		FZoneSet ZoneMask = FZoneSet::IndividualZone(It.Key());

		// Skip checking volumes which are entirely within the parent subset.
		if(Subset.EntirelyVisibleZones.ContainsZone(It.Key()))
			continue;

		// Only check volumes which are partially contained within the parent subset.
		if(Subset.VisibleZones.ContainsZone(It.Key()))
		{
            FOutcode ChildOutcode = It.Value().GetBoxIntersectionOutcode(BoxOrigin,BoxExtent);
			if(ChildOutcode.GetInside())
			{
				// Some points of the box are inside the volume, flag the volume's zone as visible.
				IntersectionVisibleZones |= ZoneMask;
				if(!ChildOutcode.GetOutside())
				{
					// No points of the box are outside the volume, flag the volume's zone as entirely visible.
					IntersectionEntirelyVisibleZones |= ZoneMask;
				}
			}
		}
	}

	return FSubset(IntersectionVisibleZones,IntersectionEntirelyVisibleZones);
}

FLevelVisibilitySet::EContainmentResult FLevelVisibilitySet::ContainsPrimitive(UPrimitiveComponent* Primitive,const FSubset& Subset) const
{
	// Only check the primitive for visibility if it has some zones in common with the subset.
	if((Primitive->ZoneMask & Subset.VisibleZones).IsNotEmpty())
	{
		// If the primitive's CullDistance>0, check that the center of the primitive's bounding sphere is within the CullDistance.
		if(Primitive->CullDistance > 0.0f && ViewOrigin.W >= 1.0f)
		{
			FLOAT PrimitiveDistanceSquared = (Primitive->Bounds.Origin - ViewOrigin).SizeSquared();
			if(PrimitiveDistanceSquared >= Square(Primitive->CullDistance))
			{
				// The primitive is farther than CullDistance, cull it.
				return CR_NotContainedBySet;
			}
		}

		if((Primitive->ZoneMask & Subset.EntirelyVisibleZones).IsNotEmpty())
		{
			// The volume used to create the subset is entirely contained by a visibility volume for
			// a zone the primitive is in.  Since the primitive intersects the volume used to create
			// the subset, that means it also intersects the volume, meaning it's visible.
			return CR_ContainedBySet;
		}

		// Check the visibility volumes contained by the same zones as the primitive for intersection
		// with the primitive's bounds.
		for(TMap<INT,FConvexVolume>::TConstIterator It(VisibilityVolumes);It;++It)
		{
			if(!Primitive->ZoneMask.ContainsZone(It.Key()))
				continue;

			GVisibilityStats.PrimitiveTests.Value++;
			if(It.Value().IntersectBox(Primitive->Bounds.Origin,Primitive->Bounds.BoxExtent))
			{
				return CR_ContainedBySet;
			}
		}

		// The primitive is definitely not visible.
		return CR_NotContainedBySet;
	}
	else
		return CR_NotContainedBySubset;
}

FLevelVisibilitySet::FSubset FLevelVisibilitySet::GetFullSubset() const
{
	return FSubset(VisibleZones,FZoneSet::NoZones());
}

void FLevelVisibilitySet::AddVisibilityVolume(INT ZoneIndex,const FConvexVolume& Volume)
{
	static TArray<INT>	PortalStack;
	static TArray<INT>	ZoneStack;

	if(IgnoreZones.ContainsZone(ZoneIndex))
		return;

	check(ZoneIndex >= 0);
	check(ZoneIndex == 0 || ZoneIndex < Level->Model->NumZones);

	GVisibilityStats.VisibilityVolumes.Value++;
	VisibleZones.AddZone(ZoneIndex);
	VisibilityVolumes.Add(ZoneIndex,Volume);

	if(UsePortals)
	{
		ZoneStack.AddItem(ZoneIndex);

		// Extend visibility through the zone's portals.
		for(INT PortalIndex = 0;PortalIndex < Level->Model->PortalNodes.Num();PortalIndex++)
		{
			FBspNode&	PortalNode = Level->Model->Nodes(Level->Model->PortalNodes(PortalIndex));

			// Skip the portal if it's already on the portal stack.
			// This case is equivalent to seeing the portal through itself.
			if(PortalStack.FindItemIndex(Level->Model->PortalNodes(PortalIndex)) != INDEX_NONE)
				continue;

			// Determine which side of the portal is being viewed.
			INT	ThisSide = (((FVector)PortalNode.Plane | (FVector)ViewOrigin) - PortalNode.Plane.W * ViewOrigin.W) > 0.0 ? 1 : 0,
				OtherSide = 1 - ThisSide,
				OtherZoneIndex = PortalNode.iZone[OtherSide];
			if(PortalNode.iZone[ThisSide] == ZoneIndex && OtherZoneIndex != ZoneIndex && ZoneStack.FindItemIndex(OtherZoneIndex) == INDEX_NONE)
			{
				// Clip the portal node against the frustum 
				FPoly PortalPolygon;
				PortalPolygon.Init();
				for(INT VertexIndex = 0;VertexIndex < PortalNode.NumVertices;VertexIndex++)
					PortalPolygon.Vertex[VertexIndex] = Level->Model->Points(Level->Model->Verts(PortalNode.iVertPool + VertexIndex).pVertex);
				PortalPolygon.NumVertices = PortalNode.NumVertices;

				// Don't clip the portal polygon if the viewer is too close to get good precision.
				UBOOL SkipPolygonClip = ViewOrigin.W >= 1.0f && Abs(PortalNode.Plane.PlaneDot(ViewOrigin)) < 1.0f;

				if(SkipPolygonClip || Volume.ClipPolygon(PortalPolygon))
				{
					// Build a portal volume by extruding the portal polygon away from the view origin.
					FConvexVolume	PortalVolume;
					for(INT VertexIndex = 0;VertexIndex < PortalPolygon.NumVertices;VertexIndex++)
					{
						FVector	EdgeVertices[2] =
						{
							PortalPolygon.Vertex[VertexIndex],
							PortalPolygon.Vertex[(VertexIndex + 1) % PortalPolygon.NumVertices]
						};

						new(PortalVolume.Planes) FPlane(
							EdgeVertices[0] + ((FVector)ViewOrigin - EdgeVertices[0] * ViewOrigin.W),
							ThisSide ? EdgeVertices[0] : EdgeVertices[1],
							ThisSide ? EdgeVertices[1] : EdgeVertices[0]
							);
					}
					new(PortalVolume.Planes) FPlane(
						ThisSide ? PortalNode.Plane : PortalNode.Plane.Flip()
						);

					GVisibilityStats.VisiblePortals.Value++;

					// Add the volume to the visibility volumes for the zone on the other side of the portal.
					PortalStack.AddItem(Level->Model->PortalNodes(PortalIndex));
					AddVisibilityVolume(PortalNode.iZone[OtherSide],PortalVolume);
					PortalStack.Pop();
				}
			}
		}

		ZoneStack.Pop();
	}
}

void FLevelVisibilitySet::AddGlobalVisibilityVolume(const FConvexVolume& Volume)
{
	GVisibilityStats.VisibilityVolumes.Value++;
	VisibilityVolumes.Set(0,Volume);
	for(INT ZoneIndex = 1;ZoneIndex < Level->Model->NumZones;ZoneIndex++)
	{
		if(!IgnoreZones.ContainsZone(ZoneIndex))
		{
			VisibilityVolumes.Set(ZoneIndex,Volume);
		}
	}
	VisibleZones = FZoneSet::AllZones();
}

void ULevel::AddPrimitive(UPrimitiveComponent* Primitive)
{
	GVisibilityStats.PrimitiveAdds.Value++;
	FCycleCounterSection	CycleCounter(GVisibilityStats.AddPrimitiveTime);

	// Determine the set of zones the primitive is in.
	if(Model->NumZones)
		Primitive->GetZoneMask(Model);
	else
		Primitive->ZoneMask = FZoneSet::IndividualZone(0);

	// Add the primitive to the octree.
	check(Hash);
	Hash->AddPrimitive(Primitive);

	// Find the lights which affect the primitive.
	TArray<ULightComponent*>	RelevantLights;
	GetRelevantLights(Primitive,RelevantLights);
	for(INT LightIndex = 0;LightIndex < RelevantLights.Num();LightIndex++)
		Primitive->AttachLight(RelevantLights(LightIndex));

	// Add dynamic primitives to map.
	if( !Primitive->Owner || !Primitive->Owner->bStatic )
	{
		DynamicPrimitives.Set( Primitive, GCurrentTime );
	}
}

void ULevel::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	GVisibilityStats.PrimitiveRemoves.Value++;
	FCycleCounterSection	CycleCounter(GVisibilityStats.RemovePrimitiveTime);

	// Detach all lights affecting the primitive.
	while(Primitive->Lights.Num())
		Primitive->DetachLight(Primitive->Lights(0).GetLight());

	// Remove the primitive from the octree.
	if(Hash)
		Hash->RemovePrimitive(Primitive);

	// Remove primitve from dynamic map.
	DynamicPrimitives.Remove( Primitive );
}

void ULevel::AddLight(ULightComponent* Light)
{
	GVisibilityStats.LightAdds.Value++;
	FCycleCounterSection	CycleCounter(GVisibilityStats.AddLightTime);

	FScene::AddLight(Light);

	// Compute the light frustum and originating zone.
	FConvexVolume		LightFrustum;
	INT					LightZoneIndex = 0;

	UPointLightComponent*		PointLight = Cast<UPointLightComponent>(Light);
	UDirectionalLightComponent*	DirectionalLight = Cast<UDirectionalLightComponent>(Light);

	if(PointLight)
	{
		// Build a cube frustum which bounds the point light's radius.
		new(LightFrustum.Planes) FPlane(+1,0,0,+(PointLight->GetOrigin().X + PointLight->Radius));
		new(LightFrustum.Planes) FPlane(-1,0,0,-(PointLight->GetOrigin().X - PointLight->Radius));
		new(LightFrustum.Planes) FPlane(0,+1,0,+(PointLight->GetOrigin().Y + PointLight->Radius));
		new(LightFrustum.Planes) FPlane(0,-1,0,-(PointLight->GetOrigin().Y - PointLight->Radius));
		new(LightFrustum.Planes) FPlane(0,0,+1,+(PointLight->GetOrigin().Z + PointLight->Radius));
		new(LightFrustum.Planes) FPlane(0,0,-1,-(PointLight->GetOrigin().Z - PointLight->Radius));

		if(Model->NumZones)
		{
			// Determine the zone the point light is in.
			LightZoneIndex = Model->PointRegion(GetLevelInfo(),PointLight->GetOrigin()).ZoneNumber;
		}
	}
	else if(DirectionalLight && Model->NumZones)
	{
		// Determine the zone the directional light is in.
		LightZoneIndex = Model->PointRegion(GetLevelInfo(),DirectionalLight->GetOrigin()).ZoneNumber;
	}

	// Compute the visibility set bounding volumes.
	FLightVisibility*	LightVisibility = new(Lights) FLightVisibility(Light,this);
	BEGINCYCLECOUNTER(GVisibilityStats.ZoneGraphTime);
	if(LightZoneIndex)
		LightVisibility->VisibilitySet.AddVisibilityVolume(LightZoneIndex,LightFrustum);
	else
		LightVisibility->VisibilitySet.AddGlobalVisibilityVolume(LightFrustum);
	ENDCYCLECOUNTER;

	// Find primitives within the visibility volume of the light.
	TArray<UPrimitiveComponent*>	RelevantPrimitives;
	check(Hash);
	Hash->GetVisiblePrimitives(LightVisibility->VisibilitySet,RelevantPrimitives);
	for(INT PrimitiveIndex = 0;PrimitiveIndex < RelevantPrimitives.Num();PrimitiveIndex++)
		if(Light->AffectsSphere(RelevantPrimitives(PrimitiveIndex)->Bounds.GetSphere()))
			RelevantPrimitives(PrimitiveIndex)->AttachLight(Light);
}

void ULevel::RemoveLight(ULightComponent* Light)
{
	GVisibilityStats.LightRemoves.Value++;
	FCycleCounterSection	CycleCounter(GVisibilityStats.RemoveLightTime);

	FScene::RemoveLight(Light);

	// Detach the light from all primitives it affects.
	while(Light->FirstDynamicPrimitiveLink)
		(*Light->FirstDynamicPrimitiveLink)->DetachLight(Light);
	while(Light->FirstStaticPrimitiveLink)
		(*Light->FirstStaticPrimitiveLink)->GetSource()->DetachLight(Light);

	for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
	{
		if(Lights(LightIndex).Light == Light)
		{
			Lights.Remove(LightIndex);
			break;
		}
	}
}

void ULevel::GetVisiblePrimitives(const FSceneContext& Context,const FConvexVolume& Frustum,TArray<UPrimitiveComponent*>& OutPrimitives)
{
	FCycleCounterSection	CycleCounter(GVisibilityStats.FrustumCheckTime);
	check(Hash);

	// Determine the view origin and zone from the view matrix.
	FPlane ViewOrigin;
	INT ViewZoneIndex = 0;
	if(Context.View->ProjectionMatrix.M[3][3] < 1.0f)
	{
		// ProjectionMatrix is a perspective projection.
		ViewOrigin = FPlane(Context.View->ViewMatrix.Inverse().GetOrigin(),1);
		ViewZoneIndex = Model->PointRegion(GetLevelInfo(),Context.View->ViewMatrix.Inverse().GetOrigin()).ZoneNumber;
	}
	else
	{
		// ProjectionMatrix is an orthographic projection.
		ViewOrigin = FPlane(Context.View->ViewMatrix.Inverse().TransformNormal(FVector(0,0,-1)),0);
	}

	AZoneInfo* ViewZoneInfo = GetZoneActor(ViewZoneIndex);

	// Build a set of zones which should be ignored from the ZoneInfo's ForceCullZones list.
	FZoneSet IgnoreZones = FZoneSet::NoZones();

	for(INT ZoneIndex = 0;ZoneIndex < ViewZoneInfo->ForceCullZones.Num();ZoneIndex++)
	{
		// Find the index of the zone with the specified cull tag.
		for(INT OtherZoneIndex = 0;OtherZoneIndex < Model->NumZones;OtherZoneIndex++)
		{
			AZoneInfo* OtherZoneInfo = GetZoneActor(OtherZoneIndex);
			if(OtherZoneInfo->CullTag == ViewZoneInfo->ForceCullZones(ZoneIndex))
			{
				IgnoreZones.AddZone(OtherZoneIndex);
				break;
			}
		}
	}

	// Create the visibility set.
	FLevelVisibilitySet	VisibilitySet(this,ViewOrigin,IgnoreZones,(Context.View->ShowFlags & SHOW_Portals) ? 1 : 0);

	// Determine whether to use zoned visibility.
	if(Context.View->ViewMode == SVM_BrushWireframe || !ViewZoneIndex)
	{
		// Use the view frustum for visibility in all zones.
		BEGINCYCLECOUNTER(GVisibilityStats.ZoneGraphTime);
		VisibilitySet.AddGlobalVisibilityVolume(Frustum);
		ENDCYCLECOUNTER;
	}
	else
	{
		// Find the volumes of each zone that are within the view frustum and visible to the view origin.
		BEGINCYCLECOUNTER(GVisibilityStats.ZoneGraphTime);
		VisibilitySet.AddVisibilityVolume(ViewZoneIndex,Frustum);
		ENDCYCLECOUNTER;
	}

	// Find the primitives whose bounds are contained by the visibility set.
	Hash->GetVisiblePrimitives(VisibilitySet,OutPrimitives);
}

void ULevel::GetRelevantLights(UPrimitiveComponent* Primitive,TArray<ULightComponent*>& OutLights)
{
	FCycleCounterSection	CycleCounter(GVisibilityStats.RelevantLightTime);

	// Find lights which have visibility volumes intersecting the primitive's bounds.
	for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
	{
		FLightVisibility&	LightVisibility = Lights(LightIndex);
		switch(LightVisibility.VisibilitySet.ContainsPrimitive(Primitive,LightVisibility.VisibilitySet.GetFullSubset()))
		{
		case FLevelVisibilitySet::CR_ContainedBySet:
			if(LightVisibility.Light->AffectsSphere(Primitive->Bounds.GetSphere()))
				OutLights.AddItem(LightVisibility.Light);
			break;
		}
	}
}
