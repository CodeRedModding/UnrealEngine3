/*=============================================================================
	UnBrushComponent.cpp: Unreal brush component implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"

#include "DebuggingDefines.h"

IMPLEMENT_CLASS(UBrushComponent);

struct FModelWireVertex
{
	FVector Position;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FVector2D UV;
};

class FModelWireVertexBuffer : public FVertexBuffer
{
public:

	/** Initialization constructor. */
	FModelWireVertexBuffer(UModel* InModel):
		Model(InModel),
		NumVertices(0)
	{
		for(INT PolyIndex = 0;PolyIndex < Model->Polys->Element.Num();PolyIndex++)
		{
			NumVertices += Model->Polys->Element(PolyIndex).Vertices.Num();
		}
	}

    // FRenderResource interface.
	virtual void InitRHI()
	{
		if(NumVertices)
		{
			VertexBufferRHI = RHICreateVertexBuffer(NumVertices * sizeof(FModelWireVertex),NULL,RUF_Static);

			FModelWireVertex* DestVertex = (FModelWireVertex*)RHILockVertexBuffer(VertexBufferRHI,0,NumVertices * sizeof(FModelWireVertex),FALSE);
			for(INT PolyIndex = 0;PolyIndex < Model->Polys->Element.Num();PolyIndex++)
			{
				FPoly& Poly = Model->Polys->Element(PolyIndex);
				for(INT VertexIndex = 0;VertexIndex < Poly.Vertices.Num();VertexIndex++)
				{
					DestVertex->Position = Poly.Vertices(VertexIndex);
					DestVertex->TangentX = FVector(1,0,0);
					DestVertex->TangentZ = FVector(0,0,1);
					// TangentZ.w contains the sign of the tangent basis determinant. Assume +1
					DestVertex->TangentZ.Vector.W = 255;
					DestVertex->UV.X	 = 0.0f;
					DestVertex->UV.Y	 = 0.0f;
					DestVertex++;
				}
			}
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}

	// Accessors.
	UINT GetNumVertices() const { return NumVertices; }

private:
	UModel*	Model;
	UINT NumVertices;
};

class FModelWireIndexBuffer : public FIndexBuffer
{
public:

	/** Initialization constructor. */
	FModelWireIndexBuffer(UModel* InModel):
		Model(InModel),
		NumEdges(0)
	{
		for(UINT PolyIndex = 0;PolyIndex < (UINT)Model->Polys->Element.Num();PolyIndex++)
		{
			NumEdges += Model->Polys->Element(PolyIndex).Vertices.Num();
		}
	}

	// FRenderResource interface.
	virtual void InitRHI()
	{
		if(NumEdges)
		{
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(WORD),NumEdges * 2 * sizeof(WORD),NULL,RUF_Static);

			WORD* DestIndex = (WORD*)RHILockIndexBuffer(IndexBufferRHI,0,NumEdges * 2 * sizeof(WORD));
			WORD BaseIndex = 0;
			for(INT PolyIndex = 0;PolyIndex < Model->Polys->Element.Num();PolyIndex++)
			{
				FPoly&	Poly = Model->Polys->Element(PolyIndex);
				for(INT VertexIndex = 0;VertexIndex < Poly.Vertices.Num();VertexIndex++)
				{
					*DestIndex++ = BaseIndex + VertexIndex;
					*DestIndex++ = BaseIndex + ((VertexIndex + 1) % Poly.Vertices.Num());
				}
				BaseIndex += Poly.Vertices.Num();
			}
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}

	// Accessors.
	UINT GetNumEdges() const { return NumEdges; }

private:
	UModel*	Model;
	UINT NumEdges;
};

class FBrushSceneProxy : public FPrimitiveSceneProxy
{
public:
	FBrushSceneProxy(UBrushComponent* Component, ABrush* Owner):
		FPrimitiveSceneProxy(Component),
		WireIndexBuffer(Component->Brush),
		WireVertexBuffer(Component->Brush),
		bStatic(FALSE),
		bVolume(FALSE),
		bBuilder(FALSE),
		bCurrentBuilder(FALSE),
		bCollideActors(Component->CollideActors),
		bBlockZeroExtent(Component->BlockZeroExtent),
		bBlockNonZeroExtent(Component->BlockNonZeroExtent),
		bBlockRigidBody(Component->BlockRigidBody),
		bSolidWhenSelected(FALSE),
		BrushColor(GEngine->C_BrushWire),
		LevelColor(255,255,255),
		PropertyColor(255,255,255)
	{
		if(Owner)
		{
			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.
			if( !GEngine->ShouldDrawBrushWireframe( Owner ) )
			{
				return;
			}

			bSelected = Owner->IsSelected();

			// Determine the type of brush this is.
			bStatic = Owner->IsStaticBrush();
			bVolume = Owner->IsVolumeBrush();
			bBuilder = Owner->IsABuilderBrush();
			bCurrentBuilder = Owner->IsCurrentBuilderBrush();
			BrushColor = Owner->GetWireColor();
			bSolidWhenSelected = Owner->bSolidWhenSelected;

			// Builder brushes should be unaffected by level coloration, so if this is a builder brush, use
			// the brush color as the level color.
			if ( bCurrentBuilder )
			{
				LevelColor = BrushColor;
			}
			else
			{
				// Try to find a color for level coloration.
				ULevel* Level = Owner->GetLevel();
				ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
				if ( LevelStreaming )
				{
					LevelColor = LevelStreaming->DrawColor;
				}
			}
		}

		// Get a color for property coloration.
		GEngine->GetPropertyColorationColor( (UObject*)Component, PropertyColor );

		// Build index/vertex buffers for drawing solid.
		for(INT i=0; i<Component->BrushAggGeom.ConvexElems.Num(); i++)
		{
			// Get verts/triangles from this hull.
			Component->BrushAggGeom.ConvexElems(i).AddCachedSolidConvexGeom(ConvexVertexBuffer.Vertices, ConvexIndexBuffer.Indices, FColor(255,255,255));
		}

		ConvexVertexFactory.InitConvexVertexFactory(&ConvexVertexBuffer);

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBrushVertexFactory,
			FLocalVertexFactory*,VertexFactory,&VertexFactory,
			FVertexBuffer*,WireVertexBuffer,&WireVertexBuffer,
			{
				FLocalVertexFactory::DataType Data;
				Data.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,Position,VET_Float3);
				Data.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,TangentX,VET_PackedNormal);
				Data.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,TangentZ,VET_PackedNormal);
				Data.TextureCoordinates.AddItem( STRUCTMEMBER_VERTEXSTREAMCOMPONENT(WireVertexBuffer,FModelWireVertex,UV,VET_Float2) );
				VertexFactory->SetData(Data);
			});
	}

	virtual ~FBrushSceneProxy()
	{
		VertexFactory.ReleaseResource();
		WireIndexBuffer.ReleaseResource();
		WireVertexBuffer.ReleaseResource();

		ConvexVertexBuffer.ReleaseResource();
		ConvexIndexBuffer.ReleaseResource();
		ConvexVertexFactory.ReleaseResource();
	}

	/** Determines if any collision should be drawn for this mesh. */
	UBOOL ShouldDrawCollision(const FSceneView* View)
	{
		if((View->Family->ShowFlags & SHOW_CollisionNonZeroExtent) && bBlockNonZeroExtent && bCollideActors)
		{
			return TRUE;
		}

		if((View->Family->ShowFlags & SHOW_CollisionZeroExtent) && bBlockZeroExtent && bCollideActors)
		{
			return TRUE;
		}	

		if((View->Family->ShowFlags & SHOW_CollisionRigidBody) && bBlockRigidBody)
		{
			return TRUE;
		}

		return FALSE;
	}

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		if( AllowDebugViewmodes() )
		{
			// If we want to draw this as solid for collision view.
			if(IsCollisionView(View) || (bSolidWhenSelected && bSelected))
			{
				if(ShouldDrawCollision(View) || (bSolidWhenSelected && bSelected))
				{
					if(ConvexVertexBuffer.Vertices.Num() > 0 && ConvexIndexBuffer.Indices.Num() > 0)
					{
						// Setup the material instance used to render the brush wireframe.
						FLinearColor WireframeColor = BrushColor;
						if(View->Family->ShowFlags & SHOW_PropertyColoration)
						{
							WireframeColor = PropertyColor;
						}
						else if(View->Family->ShowFlags & SHOW_LevelColoration)
						{
							WireframeColor = LevelColor;
						}

						const UMaterial* LevelColorationMaterial = (View->Family->ShowFlags & SHOW_ViewMode_Lit) ? GEngine->ShadedLevelColorationLitMaterial : GEngine->ShadedLevelColorationUnlitMaterial;
						const FColoredMaterialRenderProxy CollisionMaterialInstance(
							LevelColorationMaterial->GetRenderProxy(bSelected, bHovered),
							ConditionalAdjustForMobileEmulation(View, WireframeColor)
							);

						// Draw the mesh.
						FMeshBatch Mesh;
						FMeshBatchElement& BatchElement = Mesh.Elements(0);
						BatchElement.IndexBuffer = &ConvexIndexBuffer;
						Mesh.VertexFactory = &ConvexVertexFactory;
						Mesh.MaterialRenderProxy = &CollisionMaterialInstance;
						BatchElement.LocalToWorld = LocalToWorld;
						BatchElement.WorldToLocal = LocalToWorld.Inverse();
						// previous l2w not used so treat as static
						BatchElement.FirstIndex = 0;
						BatchElement.NumPrimitives = ConvexIndexBuffer.Indices.Num() / 3;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = ConvexVertexBuffer.Vertices.Num() - 1;
						Mesh.ReverseCulling = LocalToWorld.Determinant() < 0.0f ? TRUE : FALSE;
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bUsePreVertexShaderCulling = FALSE;
						Mesh.PlatformMeshData = NULL;
						PDI->DrawMesh(Mesh);
					}
				}
			}
			else
			{
				if(WireIndexBuffer.GetNumEdges() && WireVertexBuffer.GetNumVertices())
				{
					// Setup the material instance used to render the brush wireframe.
					FLinearColor WireframeColor = BrushColor;
					if(View->Family->ShowFlags & SHOW_PropertyColoration)
					{
						WireframeColor = PropertyColor;
					}
					else if(View->Family->ShowFlags & SHOW_LevelColoration)
					{
						WireframeColor = LevelColor;
					}
					FColoredMaterialRenderProxy WireframeMaterial(
						GEngine->LevelColorationUnlitMaterial->GetRenderProxy(bSelected, bHovered),
						ConditionalAdjustForMobileEmulation(View, GetSelectionColor(WireframeColor,!(GIsEditor && (View->Family->ShowFlags & SHOW_Selection)) || bSelected, bHovered))
						);

					FMeshBatch Mesh;
					FMeshBatchElement& BatchElement = Mesh.Elements(0);
					BatchElement.IndexBuffer = &WireIndexBuffer;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = &WireframeMaterial;
					BatchElement.LocalToWorld = LocalToWorld;
					BatchElement.WorldToLocal = LocalToWorld.Inverse();
					BatchElement.FirstIndex = 0;
					BatchElement.NumPrimitives = WireIndexBuffer.GetNumEdges();
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = WireVertexBuffer.GetNumVertices() - 1;
					Mesh.CastShadow = FALSE;
					Mesh.Type = PT_LineList;
					Mesh.DepthPriorityGroup = bSelected || bBuilder ? SDPG_Foreground : SDPG_World;
					Mesh.bUsePreVertexShaderCulling = FALSE;
					Mesh.PlatformMeshData           = NULL;
					PDI->DrawMesh(Mesh);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		UBOOL bVisible = FALSE;

		// We render volumes in collision view. In game, always, in editor, if the SHOW_Volumes option is on.
		if((bSolidWhenSelected && bSelected) || (bVolume && IsCollisionView(View) && (View->Family->ShowFlags & (SHOW_Volumes | SHOW_Game))))
		{
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = TRUE;
			Result.bForceDirectionalLightsDynamic = TRUE;
			Result.SetDPG(SDPG_World,TRUE);
			return Result;
		}

		if(IsShown(View))
		{
			UBOOL bNeverShow = FALSE;

			if( GIsEditor )
			{
				const UBOOL bShowBuilderBrush = (View->Family->ShowFlags & SHOW_BuilderBrush) ? TRUE : FALSE;

				// Only render current builder brush and only if the show flags indicate that we should render builder brushes.
				if( bBuilder && (!bCurrentBuilder || !bShowBuilderBrush) )
				{
					bNeverShow = TRUE;
				}
			}

			if(bNeverShow == FALSE)
			{
				const UBOOL bBSPVisible = (View->Family->ShowFlags & SHOW_BSP) != 0;
				const UBOOL bBrushesVisible = (View->Family->ShowFlags & SHOW_Brushes) != 0;

				if ( !bVolume ) //SHOW_Collision does not apply to volumes
				{
					if( (bBSPVisible && bBrushesVisible)  || ((View->Family->ShowFlags & SHOW_Collision) && bCollideActors) )
					{
						bVisible = TRUE;
					}
				}

				// Always show the build brush and any brushes that are selected in the editor.
				if( GIsEditor )
				{
					if( bBuilder || bSelected )
					{
						bVisible = TRUE;
					}
				}

				if ( bVolume )
				{
					const UBOOL bVolumesVisible = ((View->Family->ShowFlags & SHOW_Volumes) != 0);
					if(GIsEditor==FALSE || GIsGame==TRUE || bVolumesVisible == TRUE)
					{
						bVisible = TRUE;
					}
				}		
			}
		}
		
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = bVisible;
		BYTE DynamicDepthPriority = bSelected || bBuilder ? SDPG_Foreground : SDPG_World;
		Result.SetDPG(DynamicDepthPriority,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual UBOOL CreateRenderThreadResources()
	{
		VertexFactory.InitResource();
		WireIndexBuffer.InitResource();
		WireVertexBuffer.InitResource();

		if(ConvexVertexBuffer.Vertices.Num() > 0 && ConvexIndexBuffer.Indices.Num() > 0)
		{
			ConvexVertexBuffer.InitResource();
			ConvexIndexBuffer.InitResource();
			ConvexVertexFactory.InitResource();
		}

		return TRUE;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FLocalVertexFactory VertexFactory;
	FModelWireIndexBuffer WireIndexBuffer;
	FModelWireVertexBuffer WireVertexBuffer;

	FConvexCollisionVertexBuffer ConvexVertexBuffer;
	FConvexCollisionIndexBuffer ConvexIndexBuffer;
	FConvexCollisionVertexFactory ConvexVertexFactory;

	BITFIELD bStatic : 1;
	BITFIELD bVolume : 1;
	BITFIELD bBuilder : 1;
	BITFIELD bCurrentBuilder : 1;
	BITFIELD bCollideActors : 1;
	BITFIELD bBlockZeroExtent : 1;
	BITFIELD bBlockNonZeroExtent : 1;
	BITFIELD bBlockRigidBody : 1;
	BITFIELD bSolidWhenSelected : 1;

	FColor BrushColor;
	FColor LevelColor;
	FColor PropertyColor;
};

FPrimitiveSceneProxy* UBrushComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	
	if (Brush != NULL)	
	{
		// Check to make sure that we want to draw this brushed based on editor settings.
		ABrush*	Owner = Cast<ABrush>(GetOwner());
		if(Owner)
		{
			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.
			if( GEngine->ShouldDrawBrushWireframe( Owner ) )
			{
				Proxy = new FBrushSceneProxy(this, Owner);
			}
		}
		else
		{
			Proxy = new FBrushSceneProxy(this, Owner);
		}
	}

	return Proxy;
}

void UBrushComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if DEBUG_DISTRIBUTED_COOKING
	if ( Ar.IsPersistent()
	&&	GetName() == TEXT("BrushComponent_10")
	&&	GetOuter()->GetName() == TEXT("PostProcessVolume_0")
	&&	GetOutermost()->GetName() == TEXT("WarStart") )
	{
		debugf(TEXT(""));
	}
#endif
	Ar << CachedPhysBrushData;
}

void UBrushComponent::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// Don't want to do this for default UBrushComponent objects - only brushes that are part of volumes.
	if( !IsTemplate() && Owner && Owner->IsVolumeBrush() )
	{
		// When saving in the editor, compute an FKAggregateGeom based on the Brush UModel.
		UPackage* Package = CastChecked<UPackage>(GetOutermost());
		if( !(Package->PackageFlags & PKG_Cooked) )
		{
			BuildSimpleBrushCollision();
			// When saving in editor or cooking, pre-process the physics data based on the FKAggregateGeom into CachedPhysBrushData.
			BuildPhysBrushData();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Returns whether the component is valid to be attached.
 *
 * @return TRUE if it is valid to be attached, FALSE otherwise
 */
UBOOL UBrushComponent::IsValidComponent() const 
{ 
	const UBOOL bCanCollide = (BrushPhysDesc != NULL) || (CachedPhysBrushData.CachedConvexElements.Num() > 0) || (Brush != NULL);
	return bCanCollide && Super::IsValidComponent();
}

static UBOOL OwnerIsActiveBrush(const UBrushComponent* BrushComp)
{
	if( !GIsEditor || !BrushComp->GetOwner() || (BrushComp->GetOwner() != GWorld->GetBrush()) )
	{
		return false;
	}
	else
	{
		return true;
	}
}

UBOOL UBrushComponent::LineCheck(FCheckResult &Result,
								 const FVector& End,
								 const FVector& Start,
								 const FVector& Extent,
								 DWORD TraceFlags)
{
	// A blocking volume is 'pure simplified collision', so when tracing for complex collision, never collide
	if((TraceFlags & TRACE_ComplexCollision) && !bBlockComplexCollisionTrace)
	{
		return TRUE;
	}

	FMatrix Matrix;
	FVector Scale3D;
	GetTransformAndScale(Matrix, Scale3D);
	UBOOL bStopAtAnyHit = (TraceFlags & TRACE_StopAtAnyHit);

	UBOOL bMiss = BrushAggGeom.LineCheck(Result, Matrix, Scale3D, End, Start, Extent, bStopAtAnyHit, FALSE);
	if(!bMiss)
	{
		FVector Vec = End - Start;
		if (TraceFlags & TRACE_Accurate)
		{
			Result.Time = Clamp(Result.Time,0.0f,1.0f);
		}
		else
		{
			// Pull back result.
			FLOAT Dist = Vec.Size();
			Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f/Dist, 1.f/Dist),0.f,1.f);
		}
		Result.Location = Start + (Vec * Result.Time);

		// Fill in other information
		Result.Component = this;
		Result.Actor = Owner;
		Result.PhysMaterial = PhysMaterialOverride;
	}

	return bMiss;
}

UBOOL UBrushComponent::PointCheck(FCheckResult&	Result, const FVector& Location, const FVector& Extent, DWORD TraceFlags)
{
	// A blocking volume is 'pure simplified collision', so when tracing for complex collision, never collide
	if(TraceFlags & TRACE_ComplexCollision)
	{
		return TRUE;
	}

	FMatrix Matrix;
	FVector Scale3D;
	GetTransformAndScale(Matrix, Scale3D);

	UBOOL bMiss = BrushAggGeom.PointCheck(Result, Matrix, Scale3D, Location, Extent);
	if(!bMiss)
	{
		Result.Component = this;
		Result.Actor = Owner;
		Result.PhysMaterial = PhysMaterialOverride;
	}

	return bMiss;
}

/*GJKResult*/ BYTE UBrushComponent::ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB)
{
	FKAggregateGeom& AggGeom = BrushAggGeom;
	//AggGeom.DrawAggGeom(GWorld->LineBatcher, LocalToWorld, FVector(1.0, 1.0, 1.0), FColor(255,0,0,255), NULL, FALSE, FALSE);

	GJKResult Result = (GJKResult)BrushAggGeom.ClosestPointOnAggGeomToComponent(LocalToWorld, OtherComponent, PointOnComponentA, PointOnComponentB);

	return Result;
}

/*GJKResult*/ BYTE UBrushComponent::ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB)
{
	FKAggregateGeom& AggGeom = BrushAggGeom;
	//AggGeom.DrawAggGeom(GWorld->LineBatcher, LocalToWorld, FVector(1.0, 1.0, 1.0), FColor(255,0,0,255), NULL, FALSE, FALSE);

	GJKResult Result = (GJKResult)AggGeom.ClosestPointOnAggGeomToPoint(LocalToWorld, ExtentHelper, OutPointA, OutPointB);		

	return Result;
}

void UBrushComponent::UpdateBounds()
{
	if(Brush && Brush->Polys && Brush->Polys->Element.Num())
	{
		TArray<FVector> Points;
		for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
			for( INT j=0; j<Brush->Polys->Element(i).Vertices.Num(); j++ )
				Points.AddItem(Brush->Polys->Element(i).Vertices(j));
		Bounds = FBoxSphereBounds( &Points(0), Points.Num() ).TransformBy(LocalToWorld);
	}
	else if (BrushAggGeom.GetElementCount() > 0)
	{
		FMatrix EffectiveTransform;
		FVector EffectiveScale3D;
		GetTransformAndScale(EffectiveTransform, EffectiveScale3D);

		BrushAggGeom.CalcBoxSphereBounds(Bounds, EffectiveTransform, EffectiveScale3D);
	}
	else
	{
		Super::UpdateBounds();
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UBrushComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	// Get the material from each polygon making up the brush.
	if( Brush && Brush->Polys )
	{
		UPolys* Polys = Brush->Polys;
		if( Polys )
		{
			for( INT ElementIdx = 0; ElementIdx < Polys->Element.Num(); ++ElementIdx )
			{
				OutMaterials.AddItem( Polys->Element( ElementIdx ).Material );
			}
		}
	}
}

BYTE UBrushComponent::GetStaticDepthPriorityGroup() const
{
	ABrush* BrushOwner = Cast<ABrush>(GetOwner());

	// Draw selected and builder brushes in the foreground DPG.
	if(BrushOwner && (IsOwnerSelected() || BrushOwner->IsABuilderBrush()))
	{
		return SDPG_Foreground;
	}
	else
	{
		return DepthPriorityGroup;
	}
}

/**
 *  Retrieve various actor metrics depending on the provided type.  All of
 *  these will total the values for this component.
 *
 *  @param MetricsType The type of metric to calculate.
 *
 *  METRICS_VERTS    - Get the number of vertices.
 *  METRICS_TRIS     - Get the number of triangles.
 *  METRICS_SECTIONS - Get the number of sections.
 *
 *  @return INT The total of the given type for this actor.
 */
INT UBrushComponent::GetActorMetrics(EActorMetricsType MetricsType)
{
	if(Brush != NULL)
	{
		if(MetricsType == METRICS_VERTS)
		{
			return Brush->NumUniqueVertices;
		}
		else if(MetricsType == METRICS_TRIS)
		{
			INT TotalCount(0);

			for(INT PolyIndex(0); PolyIndex < Brush->Polys->Element.Num(); ++PolyIndex)
			{
				TotalCount += Brush->Polys->Element(PolyIndex).Vertices.Num()-2;
			}

			return TotalCount;
		}
	}

	return 0;
}





