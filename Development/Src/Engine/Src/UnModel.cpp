/*=============================================================================
	UnModel.cpp: Unreal model functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineProcBuildingClasses.h"
#include "BSPOps.h"

/*-----------------------------------------------------------------------------
	FBspSurf object implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns TRUE if this surface is currently hidden in the editor
 *
 * @return TRUE if this surface is hidden in the editor; FALSE otherwise
 */
UBOOL FBspSurf::IsHiddenEd() const
{
	return bHiddenEdTemporary || bHiddenEdLevel;
}

#if WITH_EDITOR
/**
 * Returns TRUE if this surface is hidden at editor startup
 *
 * @return TRUE if this surface is hidden at editor startup; FALSE otherwise
 */
UBOOL FBspSurf::IsHiddenEdAtStartup() const
{
	return ( ( PolyFlags & PF_HiddenEd ) != 0 ); 
}
#endif

/*-----------------------------------------------------------------------------
	Struct serializers.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FBspSurf& Surf )
{
	Ar << Surf.Material;
	Ar << Surf.PolyFlags;	
	Ar << Surf.pBase << Surf.vNormal;
	Ar << Surf.vTextureU << Surf.vTextureV;
	Ar << Surf.iBrushPoly;
	Ar << Surf.Actor;
	Ar << Surf.Plane;
	Ar << Surf.ShadowMapScale;
	DWORD LightingChannels = Surf.LightingChannels.Bitfield;
	Ar << LightingChannels;
	Surf.LightingChannels.Bitfield = LightingChannels;
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		Surf.iLightmassIndex = 0;
	}
	else
	{
		Ar << Surf.iLightmassIndex;
	}

	// If transacting, we do want to serialize the temporary visibility
	// flags; but not in any other situation
	if ( Ar.IsTransacting() )
	{
		Ar << Surf.bHiddenEdTemporary;
		Ar << Surf.bHiddenEdLevel;
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FPoly& Poly )
{
	INT LegacyNumVertices = Poly.Vertices.Num();
	Ar << Poly.Base << Poly.Normal << Poly.TextureU << Poly.TextureV;
	Ar << Poly.Vertices;
	Ar << Poly.PolyFlags;
	Ar << Poly.Actor << Poly.ItemName;
	Ar << Poly.Material;
	Ar << Poly.iLink << Poly.iBrushPoly;
	Ar << Poly.ShadowMapScale;
	Ar << Poly.LightingChannels;
 	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
 	{
		Poly.LightmassSettings.bUseEmissiveForStaticLighting = FALSE;
		Poly.LightmassSettings.EmissiveBoost = 1.0f;
		Poly.LightmassSettings.DiffuseBoost = 1.0f;
		Poly.LightmassSettings.SpecularBoost = 1.0f;
 	}
	else
	{
		Ar << Poly.LightmassSettings;
	}
	
	if(Ar.Ver() < VER_FPOLY_RULESET_VARIATIONNAME)
	{
		// Handle old ruleset pointer in FPoly
		if(Ar.Ver() >= VER_ADD_FPOLY_PBRULESET_POINTER)
		{
			UProcBuildingRuleset* DummyRuleset;
			Ar << DummyRuleset;
		}

		Poly.RulesetVariation = NAME_None;
	}
	else
	{
		Ar << Poly.RulesetVariation;
	}
	
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FBspNode& N )
{
	// @warning BulkSerialize: FBSPNode is serialized as memory dump
	// See TArray::BulkSerialize for detailed description of implied limitations.

	// Serialize in the order of variable declaration so the data is compatible with BulkSerialize
	Ar	<< N.Plane;
	Ar	<< N.iVertPool
		<< N.iSurf
		<< N.iVertexIndex
		<< N.ComponentIndex 
		<< N.ComponentNodeIndex
		<< N.ComponentElementIndex;
	
	Ar	<< N.iChild[0]
		<< N.iChild[1]
		<< N.iChild[2]
		<< N.iCollisionBound
		<< N.iZone[0]
		<< N.iZone[1]
		<< N.NumVertices
		<< N.NodeFlags
		<< N.iLeaf[0]
		<< N.iLeaf[1];

	if( Ar.IsLoading() )
	{
		//@warning: this code needs to be in sync with UModel::Serialize as we use bulk serialization.
		N.NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FZoneProperties& P )
{
	Ar	<< P.ZoneActor
		<< P.Connectivity
		<< P.Visibility
		<< P.LastRenderTime;
	return Ar;
}

/**
* Serializer
*
* @param Ar - archive to serialize with
* @param V - vertex to serialize
* @return archive that was used
*/
FArchive& operator<<(FArchive& Ar,FModelVertex& V)
{
	Ar << V.Position;
	Ar << V.TangentX;
	Ar << V.TangentZ;	
	Ar << V.TexCoord;
	Ar << V.ShadowTexCoord;	

	return Ar;
}

/*---------------------------------------------------------------------------------------
	UModel object implementation.
---------------------------------------------------------------------------------------*/

void UModel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Bounds;

	Vectors.BulkSerialize( Ar );
	Points.BulkSerialize( Ar );
	Nodes.BulkSerialize( Ar );
	if( Ar.IsLoading() )
	{
		for( INT NodeIndex=0; NodeIndex<Nodes.Num(); NodeIndex++ )
		{
			Nodes(NodeIndex).NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
		}
	}
	Ar << Surfs;
	Verts.BulkSerialize( Ar, FVert::GetSizeForBulkSerialization(Ar) );
	Ar << NumSharedSides << NumZones;
	for( INT i=0; i<NumZones; i++ )
	{
		Ar << Zones[i];
	}
	Ar << Polys;

	LeafHulls.BulkSerialize( Ar );
	Leaves.BulkSerialize( Ar );
	Ar << RootOutside << Linked;
	PortalNodes.BulkSerialize( Ar );

	if (Ar.Ver() < VER_REMOVED_SHADOW_VOLUMES)
	{
		TArray<FMeshEdge> LegacyEdges;
		LegacyEdges.BulkSerialize(Ar);
	}
	
	Ar << NumUniqueVertices; 
	// load/save vertex buffer
	Ar << VertexBuffer;

	if(GIsEditor)
	{
		CalculateUniqueVertCount();
	}

	// serialize the lighting guid if it's there
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		LightingGuid = appCreateGuid();
	}
	else
	{
		Ar << LightingGuid;
	}

 	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
 	{
		FLightmassPrimitiveSettings TempSettings(EC_NativeConstructor);
		LightmassSettings.AddItem(TempSettings);
 	}
	else
	{
		Ar << LightmassSettings;
	}
}

void UModel::CalculateUniqueVertCount()
{
	NumUniqueVertices = Points.Num();

	if(NumUniqueVertices == 0 && Polys != NULL)
	{
		TArray<FVector> UniquePoints;

		for(INT PolyIndex(0); PolyIndex < Polys->Element.Num(); ++PolyIndex)
		{
			for(INT VertIndex(0); VertIndex < Polys->Element(PolyIndex).Vertices.Num(); ++VertIndex)
			{
				UBOOL bAlreadyAdded(FALSE);
				for(INT UniqueIndex(0); UniqueIndex < UniquePoints.Num(); ++UniqueIndex)
				{
					if(Polys->Element(PolyIndex).Vertices(VertIndex) == UniquePoints(UniqueIndex))
					{
						bAlreadyAdded = TRUE;
						break;
					}
				}

				if(!bAlreadyAdded)
				{
					UniquePoints.Push(Polys->Element(PolyIndex).Vertices(VertIndex));
				}
			}
		}

		NumUniqueVertices = UniquePoints.Num();
	}
}

void UModel::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// reset our not load flags
	ClearFlags(RF_NotForEdit | RF_NotForClient | RF_NotForServer);

	// propagate the not for flags from the source
	if (GetOuter()->HasAnyFlags(RF_NotForEdit))
	{
		ClearFlags(RF_LoadForEdit);
		SetFlags(RF_NotForEdit);
	}
	if (GetOuter()->HasAnyFlags(RF_NotForClient))
	{
		ClearFlags(RF_LoadForClient);
		SetFlags(RF_NotForClient);
	}
	if (GetOuter()->HasAnyFlags(RF_NotForServer))
	{
		ClearFlags(RF_LoadForServer);
		SetFlags(RF_NotForServer);
	}
#endif // WITH_EDITORONLY_DATA
}

void UModel::PostLoad()
{
	Super::PostLoad();
	
	if( !GIsUCC && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ForceUpdateVertices();
	}

	// If in the editor, initialize each surface to hidden or not depending upon
	// whether the poly flag dictates being hidden at editor startup or not
	if ( GIsEditor )
	{
		for ( TArray<FBspSurf>::TIterator SurfIter( Surfs ); SurfIter; ++SurfIter )
		{
			FBspSurf& CurSurf = *SurfIter;
			CurSurf.bHiddenEdTemporary = ( ( CurSurf.PolyFlags & PF_HiddenEd ) != 0 );
			CurSurf.bHiddenEdLevel = 0;
		}
	}
}

void UModel::PreEditUndo()
{
	Super::PreEditUndo();
}

void UModel::PostEditUndo()
{
	InvalidSurfaces = TRUE;

	Super::PostEditUndo();
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UModel::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// Strip vertex buffer data for brushes if we aren't
	// keeping any non-stripped platforms
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped) && GetOuter() && GetOuter()->IsA(ABrush::StaticClass()))
	{
		VertexBuffer.Vertices.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

void UModel::ModifySurf( INT InIndex, UBOOL UpdateMaster )
{
	Surfs.ModifyItem( InIndex );
	FBspSurf& Surf = Surfs(InIndex);
	if( UpdateMaster && Surf.Actor )
	{
		Surf.Actor->Brush->Polys->Element.ModifyItem( Surf.iBrushPoly );
	}
}
void UModel::ModifyAllSurfs( UBOOL UpdateMaster )
{
	for( INT i=0; i<Surfs.Num(); i++ )
		ModifySurf( i, UpdateMaster );

}
void UModel::ModifySelectedSurfs( UBOOL UpdateMaster )
{
	for( INT i=0; i<Surfs.Num(); i++ )
		if( Surfs(i).PolyFlags & PF_Selected )
			ModifySurf( i, UpdateMaster );

}

UBOOL UModel::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	// Also rename the UPolys.
    if (NewOuter && Polys && Polys->GetOuter() == GetOuter())
	{
		if (Polys->Rename(*MakeUniqueObjectName(NewOuter, Polys->GetClass()).ToString(), NewOuter, Flags) == FALSE)
		{
			return FALSE;
		}
	}

    return Super::Rename( InName, NewOuter, Flags );
}

/**
 * Called after duplication & serialization and before PostLoad. Used to make sure UModel's FPolys
 * get duplicated as well.
 */
void UModel::PostDuplicate()
{
	Super::PostDuplicate();
	if( Polys )
	{
		Polys = CastChecked<UPolys>(UObject::StaticDuplicateObject( Polys, Polys, GetOuter(), NULL ));
	}
}

void UModel::BeginDestroy()
{
	Super::BeginDestroy();
#if EXPERIMENTAL_FAST_BOOT_IPHONE
	// the class default object gets tossed in UClass:Link() time, and blocks the game thread,
	// and CDO shouldn't have RT resources anyway?
	if (!IsTemplate())
#endif
	{
		BeginReleaseResources();
	}
}

UBOOL UModel::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.GetNumPendingFences() == 0 && Super::IsReadyForFinishDestroy();
}

INT UModel::GetResourceSize()
{
	INT ResourceSize;
	if (GExclusiveResourceSizeMode)
	{
		ResourceSize = 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize(this);
		ResourceSize = CountBytesSize.GetNum();
	}

	// I'm adding extra stuff that haven't been covered by Serialize 
	// I don't have to include VertexFactories (based on Sam Z)
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TConstIterator IndexBufferIt(MaterialIndexBuffers);IndexBufferIt;++IndexBufferIt)
	{
		const TScopedPointer<FRawIndexBuffer16or32> &IndexBuffer = IndexBufferIt.Value();
		ResourceSize += IndexBuffer->Indices.Num() * sizeof(DWORD);
	}
	
	return ResourceSize;
}

IMPLEMENT_CLASS(UModel);

/*---------------------------------------------------------------------------------------
	UModel implementation.
---------------------------------------------------------------------------------------*/

//
// Lock a model.
//
void UModel::Modify( UBOOL bAlwaysMarkDirty/*=FALSE*/ )
{
	Super::Modify(bAlwaysMarkDirty);

	// make a new guid whenever this model changes
	LightingGuid = appCreateGuid();

	// Modify all child objects.
	if( Polys )
	{
		Polys->Modify(bAlwaysMarkDirty);
	}
}

//
// Empty the contents of a model.
//
void UModel::EmptyModel( INT EmptySurfInfo, INT EmptyPolys )
{
	Nodes			.Empty();
	LeafHulls		.Empty();
	Leaves			.Empty();
	Verts			.Empty();
	PortalNodes		.Empty();

	if( EmptySurfInfo )
	{
		Vectors.Empty();
		Points.Empty();
		Surfs.Empty();
	}
	if( EmptyPolys )
	{
		Polys = new( GetOuter(), NAME_None, RF_Transactional )UPolys;
	}

	// Init variables.
	NumSharedSides	= 4;
	NumZones = 0;
	for( INT i=0; i<FBspNode::MAX_ZONES; i++ )
	{
		Zones[i].ZoneActor    = NULL;
		Zones[i].Connectivity = FZoneSet::IndividualZone(i);
		Zones[i].Visibility   = FZoneSet::AllZones();
	}
}

//
// Create a new model and allocate all objects needed for it.
//
UModel::UModel( ABrush* Owner, UBOOL InRootOutside )
:	Nodes		( this )
,	Verts		( this )
,	Vectors		( this )
,	Points		( this )
,	Surfs		( this )
,	VertexBuffer( this )
,	LightingGuid( appCreateGuid() )
,	RootOutside	( InRootOutside )
{
	SetFlags( RF_Transactional );
	EmptyModel( 1, 1 );
	if( Owner )
	{
		check(Owner->BrushComponent);
		Owner->Brush = this;
		Owner->InitPosRotScale();
	}
	if( GIsEditor && !GIsGame )
	{
		UpdateVertices();
	}
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UModel::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UModel, Polys ) );
	const DWORD SkipIndexIndex = TheClass->EmitStructArrayBegin( STRUCT_OFFSET( UModel, Surfs ), sizeof(FBspSurf) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Material ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Actor ) );
	TheClass->EmitStructArrayEnd( SkipIndexIndex );
}

//
// Build the model's bounds (min and max).
//
void UModel::BuildBound()
{
	if( Polys && Polys->Element.Num() )
	{
		TArray<FVector> NewPoints;
		for( INT i=0; i<Polys->Element.Num(); i++ )
			for( INT j=0; j<Polys->Element(i).Vertices.Num(); j++ )
				NewPoints.AddItem(Polys->Element(i).Vertices(j));
		Bounds = FBoxSphereBounds( &NewPoints(0), NewPoints.Num() );
	}
}

//
// Transform this model by its coordinate system.
//
void UModel::Transform( ABrush* Owner )
{
	check(Owner);

	Polys->Element.ModifyAllItems();

	for( INT i=0; i<Polys->Element.Num(); i++ )
		Polys->Element( i ).Transform( Owner->PrePivot, Owner->Location);

}

/*---------------------------------------------------------------------------------------
	UModel basic implementation.
---------------------------------------------------------------------------------------*/

//
// Shrink all stuff to its minimum size.
//
void UModel::ShrinkModel()
{
	Vectors		.Shrink();
	Points		.Shrink();
	Verts		.Shrink();
	Nodes		.Shrink();
	Surfs		.Shrink();
	if( Polys     ) Polys    ->Element.Shrink();
	LeafHulls	.Shrink();
	PortalNodes	.Shrink();
}

void UModel::BeginReleaseResources()
{
	// Release the index buffers.
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(MaterialIndexBuffers);IndexBufferIt;++IndexBufferIt)
	{
		BeginReleaseResource(IndexBufferIt.Value());
	}

	// Release the vertex buffer and factory.
	BeginReleaseResource(&VertexBuffer);
	BeginReleaseResource(&VertexFactory);

	// Use a fence to keep track of the release progress.
	ReleaseResourcesFence.BeginFence();
}

void UModel::UpdateVertices()
{
	// Wait for pending resource release commands to execute.
	ReleaseResourcesFence.Wait();

	// Don't initialize brush rendering resources on consoles
	if (!GetOuter() || !GetOuter()->IsA(ABrush::StaticClass()) ||
		!(appGetPlatformType() & UE3::PLATFORM_Stripped))
	{
#if WITH_REALD || !CONSOLE
		// rebuild vertex buffer if the resource array is not static 
		if (GIsEditor && !GIsGame && !VertexBuffer.Vertices.IsStatic() && !GIsSimMobile)
		{	
			INT NumVertices = 0;

			NumVertices = BuildVertexBuffers();

			// We want to check whenever we build the vertex buffer that we have the
			// appropriate number of verts, but since we no longer serialize the total
			// non-unique vertcount we only do this check when building the buffer.
			check(NumVertices == VertexBuffer.Vertices.Num());	
		}
#endif
		BeginInitResource(&VertexBuffer);
		if( GIsEditor && !GIsGame )
		{
			// needed since we may call UpdateVertices twice and the first time
			// NumVertices might be 0. 
			BeginUpdateResourceRHI(&VertexBuffer);
		}

		// Set up the vertex factory.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitModelVertexFactory,
			FLocalVertexFactory*,VertexFactory,&VertexFactory,
			FVertexBuffer*,VertexBuffer,&VertexBuffer,
			{
				FLocalVertexFactory::DataType Data;
				Data.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,Position,VET_Float3);
				Data.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentX,VET_PackedNormal);
				Data.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentZ,VET_PackedNormal);
				Data.TextureCoordinates.Empty();
				Data.TextureCoordinates.AddItem(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TexCoord,VET_Float2));
				Data.ShadowMapCoordinateComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,ShadowTexCoord,VET_Float2);
				VertexFactory->SetData(Data);
			});
		BeginInitResource(&VertexFactory);
	}
}

void UModel::ForceUpdateVertices()
{
	// Wait for pending resource release commands to execute.
	ReleaseResourcesFence.Wait();
 
	// Don't initialize brush rendering resources on consoles
	if (!GetOuter() || !GetOuter()->IsA(ABrush::StaticClass()) ||
		!(appGetPlatformType() & UE3::PLATFORM_Stripped))
	{
#if WITH_REALD || !CONSOLE
		if (GIsGame || GIsCooking)
		{
			// rebuild vertex buffer if the resource array is not static
			if( !VertexBuffer.Vertices.IsStatic() )
			{
				BuildVertexBuffers();
			}
		}
#endif
		// we should have the same # of vertices in the loaded vertex buffer
		// check(NumVertices == VertexBuffer.Vertices.Num());   
		BeginInitResource(&VertexBuffer);
       
		if( GIsEditor && !GIsGame )
		{
			// needed since we may call UpdateVertices twice and the first time
			// NumVertices might be 0.
			BeginUpdateResourceRHI(&VertexBuffer);
		}
 
		// Set up the vertex factory.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitModelVertexFactory,
			FLocalVertexFactory*,VertexFactory,&VertexFactory,
			FVertexBuffer*,VertexBuffer,&VertexBuffer,
			{
				FLocalVertexFactory::DataType Data;
				Data.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,Position,VET_Float3);
				Data.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentX,VET_PackedNormal);
				Data.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentZ,VET_PackedNormal);
				Data.TextureCoordinates.Empty();
				Data.TextureCoordinates.AddItem(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TexCoord,VET_Float2));
				Data.ShadowMapCoordinateComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,ShadowTexCoord,VET_Float2);
				VertexFactory->SetData(Data);
			});
		BeginInitResource(&VertexFactory);
	}
}
 
/** 
 *	Compute the "center" location of all the verts 
 */
FVector UModel::GetCenter()
{
	FVector Center(0.f);
	UINT Cnt = 0;
	for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Nodes(NodeIndex);
		UINT NumVerts = (Node.NodeFlags & PF_TwoSided) ? Node.NumVertices / 2 : Node.NumVertices;
		for(UINT VertexIndex = 0;VertexIndex < NumVerts;VertexIndex++)
		{
			const FVert& Vert = Verts(Node.iVertPool + VertexIndex);
			const FVector& Position = Points(Vert.pVertex);
			Center += Position;
			Cnt++;
		}
	}

	if( Cnt > 0 )
	{
		Center /= Cnt;
	}
	
	return Center;
}

#if WITH_REALD || !CONSOLE
/**
* Initialize vertex buffer data from UModel data
* Returns the number of vertices in the vertex buffer.
*/
INT UModel::BuildVertexBuffers()
{
	// Calculate the size of the vertex buffer and the base vertex index of each node.
	INT NumVertices = 0;
	for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Nodes(NodeIndex);
		FBspSurf& Surf = Surfs(Node.iSurf);
		Node.iVertexIndex = NumVertices;
		NodePolys* polys = NodePolys::create(this, &Node);
		NumVertices += (Surf.PolyFlags & PF_TwoSided) ? (polys->Vertices.Num() * 2) : polys->Vertices.Num();
	}

	// size vertex buffer data
	VertexBuffer.Vertices.Empty(NumVertices);
	VertexBuffer.Vertices.Add(NumVertices);

	if(NumVertices > 0)
	{
		// Initialize the vertex data
		FModelVertex* DestVertex = (FModelVertex*)VertexBuffer.Vertices.GetData();
		for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Nodes(NodeIndex);
			FBspSurf& Surf = Surfs(Node.iSurf);
			const FVector& TextureBase = Points(Surf.pBase);
			const FVector& TextureX = Vectors(Surf.vTextureU);
			const FVector& TextureY = Vectors(Surf.vTextureV);

			// Use the texture coordinates and normal to create an orthonormal tangent basis.
			FVector TangentX = TextureX;
			FVector TangentY = TextureY;
			FVector TangentZ = Vectors(Surf.vNormal);
			CreateOrthonormalBasis(TangentX,TangentY,TangentZ);

			NodePolys* polys = NodePolys::create(this, &Node);
			for (INT VertexIndex = 0; VertexIndex < polys->Vertices.Num(); ++VertexIndex)
			{
				FVector& Position = polys->Vertices(VertexIndex);
				DestVertex->Position = Position;
				DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / 128.0f;
				DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / 128.0f;
				DestVertex->ShadowTexCoord = polys->ShadowTexCoords(VertexIndex);
				DestVertex->TangentX = TangentX;
				DestVertex->TangentZ = TangentZ;

				// store the sign of the determinant in TangentZ.W
				DestVertex->TangentZ.Vector.W = GetBasisDeterminantSign( TangentX, TangentY, TangentZ ) < 0 ? 0 : 255;

				DestVertex++;
			}

			if(Surf.PolyFlags & PF_TwoSided)
			{
				for (INT VertexIndex = 0; VertexIndex < polys->Vertices.Num(); ++VertexIndex)
				{
					FVector& Position = polys->Vertices(VertexIndex);
					DestVertex->Position = Position;
					DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / 128.0f;
					DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / 128.0f;
					DestVertex->ShadowTexCoord = polys->ShadowTexCoords(VertexIndex);
					DestVertex->TangentX = TangentX;
					DestVertex->TangentZ = -TangentZ;

					// store the sign of the determinant in TangentZ.W
					DestVertex->TangentZ.Vector.W = GetBasisDeterminantSign( TangentX, TangentY, -TangentZ ) < 0 ? 0 : 255;

					DestVertex++;
				}
			}
		}
	}

	return NumVertices;
}

#endif


