/*=============================================================================
	UnVisi.cpp: Unreal visibility computation
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	Visibility and zoining code.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	Temporary.
-----------------------------------------------------------------------------*/

// Options.
#define DEBUG_PORTALS	0	/* Debugging hull code by generating hull brush */
#define DEBUG_WRAPS		0	/* Debugging sheet wrapping code */
#define DEBUG_BADSHEETS 0   /* Debugging sheet discrepancies */
#define DEBUG_LVS       0   /* Debugging light volumes */
FPoly BuildInfiniteFPoly( UModel *Model, INT iNode );

// Thresholds.
#define VALID_SIDE         0.1   /* A normal must be at laest this long to be valid */
#define VALID_CROSS        0.001 /* A cross product can be safely normalized if this big */

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//
// Debugging.
//
#if DEBUG_PORTALS || DEBUG_WRAPS || DEBUG_BADSHEETS || DEBUG_LVS
	static ABrush *DEBUG_Brush;
#endif

//
// A portal.
//
class FPortal : public FPoly
{
public:
	// Variables.
	INT	    iFrontLeaf, iBackLeaf, iNode;
	FPortal *GlobalNext, *FrontLeafNext, *BackLeafNext, *NodeNext;
	BYTE	IsTesting, ShouldTest;
	INT		FragmentCount;
	INT	    iZonePortalSurf;

	// Constructor.
	FPortal( FPoly &InPoly, INT iInFrontLeaf, INT iInBackLeaf, INT iInNode, FPortal *InGlobalNext, FPortal *InNodeNext, FPortal *InFrontLeafNext, FPortal *InBackLeafNext )
	:	FPoly			(InPoly),
		iFrontLeaf		(iInFrontLeaf),
		iBackLeaf		(iInBackLeaf),
		iNode			(iInNode),
		GlobalNext		(InGlobalNext),
		NodeNext		(InNodeNext),
		FrontLeafNext	(InFrontLeafNext),
		BackLeafNext	(InBackLeafNext),
		IsTesting		(0),
		ShouldTest		(0),
		FragmentCount	(0),
		iZonePortalSurf (INDEX_NONE)
	{}
	
	// Get the leaf on the opposite side of the specified leaf.
	INT GetNeighborLeafOf( INT iLeaf )
	{
		check( iLeaf==iFrontLeaf || iLeaf==iBackLeaf );
		if     ( iFrontLeaf == iLeaf )	return iBackLeaf;
		else							return iFrontLeaf;
	}

	// Get the next portal for this leaf in the linked list of portals.
	FLOAT Area()
	{
		FVector Cross(0,0,0);
		for( INT i=2; i<NumVertices; i++ )
			Cross += (Vertex[i-1]-Vertex[0]) ^ (Vertex[i]-Vertex[0]);
		return Cross.Size();
	}
	FPortal* Next( INT iLeaf )
	{
		check( iLeaf==iFrontLeaf || iLeaf==iBackLeaf );
		if     ( iFrontLeaf == iLeaf )	return FrontLeafNext;
		else							return BackLeafNext;
	}

	// Return this portal polygon, facing outward from leaf iLeaf.
	void GetPolyFacingOutOf( INT iLeaf, FPoly &Poly )
	{
		check( iLeaf==iFrontLeaf || iLeaf==iBackLeaf );
		Poly = *(FPoly*)this;
		if( iLeaf == iFrontLeaf ) Poly.Reverse();
	}

	// Return this portal polygon, facing inward to leaf iLeaf.
	void GetPolyFacingInto( INT iLeaf, FPoly &Poly)
	{
		check( iLeaf==iFrontLeaf || iLeaf==iBackLeaf );
		Poly = *(FPoly*)this;
		if( iLeaf == iBackLeaf ) Poly.Reverse();
	}
};

//
// The visibility calculator class.
//
class FEditorVisibility
{
public:
	// Constants.
	enum {MAX_CLIPS=16384};
	enum {CLIP_BACK_FLAG=0x40000000};

	// Types.
	typedef void (FEditorVisibility::*PORTAL_FUNC)(FPoly&,INT,INT,INT,INT);

	// Variables.
	FMemMark		Mark;
	ULevel*			Level;
	UModel*			Model;
	INT				Clips[MAX_CLIPS];
	INT				NumPortals, NumLogicalLeaves;
	INT				NumClips, NumClipTests, NumPassedClips, NumUnclipped;
	INT				NumBspPortals, MaxFragments, NumZonePortals, NumZoneFragments;
	INT				Extra;
	INT				iZonePortalSurf;
	FPortal*		FirstPortal;
	FPortal**		NodePortals;
	FPortal**		LeafPortals;

	// Constructor.
	FEditorVisibility( ULevel* InLevel, UModel* InModel, INT InDebug );

	// Destructor.
	~FEditorVisibility();

	// Portal functions.
	void AddPortal( FPoly &Poly, INT iFrontLeaf, INT iBackLeaf, INT iGeneratingNode, INT iGeneratingBase );
	void BlockPortal( FPoly &Poly, INT iFrontLeaf, INT iBackLeaf, INT iGeneratingNode, INT iGeneratingBase );
	void TagZonePortalFragment( FPoly &Poly, INT iFrontLeaf, INT iBackLeaf, INT iGeneratingNode, INT iGeneratingBase );
	void FilterThroughSubtree( INT Pass, INT iGeneratingNode, INT iGeneratingBase, INT iParentLeaf, INT iNode, FPoly Poly, PORTAL_FUNC Func, INT iBackLeaf );
	void MakePortalsClip( INT iNode, FPoly Poly, INT Clip, PORTAL_FUNC Func );
	void MakePortals( INT iNode );
	void AssignLeaves( INT iNode, INT Outside );

	// Zone functions.
	void FormZonesFromLeaves();
	void AssignAllZones( INT iNode, int Outside );
	void BuildConnectivity();
	void BuildZoneInfo();

	// Visibility functions.
	void TestVisibility();
};

/*-----------------------------------------------------------------------------
	Portal building, a simple recursive hierarchy of functions.
-----------------------------------------------------------------------------*/

//
// Tag a zone portal fragment.
//
void FEditorVisibility::TagZonePortalFragment
(
	FPoly&	Poly,
	INT	    iFrontLeaf,
	INT		iBackLeaf,
	INT		iGeneratingNode,
	INT		iGeneratingBase
)
{
	// Add this node to the bsp as a coplanar to its generator.
	INT iNewNode = GEditor->bspAddNode( Model, iGeneratingNode, NODE_Plane, Model->Nodes(iGeneratingNode).NodeFlags | NF_IsNew, &Poly );

	// Set the node's zones.
	int Backward = (Poly.Normal | Model->Nodes(iGeneratingBase).Plane) < 0.0;
	Model->Nodes(iNewNode).iZone[Backward^0] = iBackLeaf ==INDEX_NONE ? 0 : Model->Leaves(iBackLeaf ).iZone;
	Model->Nodes(iNewNode).iZone[Backward^1] = iFrontLeaf==INDEX_NONE ? 0 : Model->Leaves(iFrontLeaf).iZone;

}

//
// Mark a portal as blocked.
//
void FEditorVisibility::BlockPortal
(
	FPoly&	Poly,
	INT		iFrontLeaf,
	INT		iBackLeaf,
	INT		iGeneratingNode,
	INT		iGeneratingBase
)
{
	if( iFrontLeaf!=INDEX_NONE && iBackLeaf!=INDEX_NONE )
	{
		for( FPortal* Portal=FirstPortal; Portal; Portal=Portal->GlobalNext )
		{
			if
			(	(Portal->iFrontLeaf==iFrontLeaf && Portal->iBackLeaf==iBackLeaf )
			||	(Portal->iFrontLeaf==iBackLeaf  && Portal->iBackLeaf==iFrontLeaf) )
			{
				Portal->iZonePortalSurf = iZonePortalSurf;
				NumZoneFragments++;
			}
		}
	}
}

//
// Add a portal to the portal list.
//
void FEditorVisibility::AddPortal
(
	FPoly&	Poly,
	INT		iFrontLeaf,
	INT		iBackLeaf,
	INT		iGeneratingNode,
	INT		iGeneratingBase
)
{
	if( iFrontLeaf!=INDEX_NONE && iBackLeaf!=INDEX_NONE )
	{
		// Add to linked list of all portals.
		FirstPortal						= 
		LeafPortals[iFrontLeaf]			= 
		LeafPortals[iBackLeaf]			= 
		NodePortals[iGeneratingNode]	= 
			new(GMem)FPortal
			(
				Poly,
				iFrontLeaf,
				iBackLeaf,
				iGeneratingNode,
				FirstPortal,
				NodePortals[iGeneratingNode],
				LeafPortals[iFrontLeaf],
				LeafPortals[iBackLeaf]
			);
		NumPortals++;

#if DEBUG_PORTALS
		//debugf("AddPortal: %i verts",Poly.NumVertices);
		Poly.PolyFlags |= PF_NotSolid;
		new(DEBUG_Brush->Brush->Polys)FPoly(Poly);
#endif
	}
}

//
// Filter a portal through a front or back subtree.
//
void FEditorVisibility::FilterThroughSubtree
(
	INT			Pass,
	INT			iGeneratingNode,
	INT			iGeneratingBase,
	INT			iParentLeaf,
	INT			iNode,
	FPoly		Poly,
	PORTAL_FUNC Func,
	INT			iBackLeaf
)
{
	while( iNode != INDEX_NONE )
	{
		// If overflow.
		if( Poly.NumVertices > FPoly::VERTEX_THRESHOLD )
		{
			FPoly Half;
			Poly.SplitInHalf( &Half );
			FilterThroughSubtree( Pass, iGeneratingNode, iGeneratingBase, iParentLeaf, iNode, Half, Func, iBackLeaf );
		}

		// Test split.
		FPoly Front,Back;
		int Split = Poly.SplitWithNode( Model, iNode, &Front, &Back, 1 );

		// Recurse with front.
		if( Split==SP_Front || Split==SP_Split )
			FilterThroughSubtree
			(
				Pass,
				iGeneratingNode,
				iGeneratingBase,
				Model->Nodes(iNode).iLeaf[1],
				Model->Nodes(iNode).iFront,
				Split==SP_Front ? Poly : Front,
				Func,
				iBackLeaf
			);

		// Consider back.
		if( Split!=SP_Back && Split!=SP_Split )
			return;

		// Loop with back.
		if( Split == SP_Split )
			Poly = Back;
		iParentLeaf = Model->Nodes(iNode).iLeaf[0];
		iNode       = Model->Nodes(iNode).iBack;
	}

	// We reached a leaf in this subtree.
	if( Pass == 0 ) FilterThroughSubtree
	(
		1,
		iGeneratingNode,
		iGeneratingBase,
		Model->Nodes(iGeneratingBase).iLeaf[1],
		Model->Nodes(iGeneratingBase).iFront,
		Poly,
		Func,
		iParentLeaf
	);
	else (this->*Func)( Poly, iParentLeaf, iBackLeaf, iGeneratingNode, iGeneratingBase );
}

//
// Clip a portal by all parent nodes above it.
//
void FEditorVisibility::MakePortalsClip
(
	INT			iNode,
	FPoly		Poly,
	INT			Clip,
	PORTAL_FUNC Func
)
{
	// Clip by all parents.
	while( Clip < NumClips )
	{
		INT iClipNode = Clips[Clip] & ~CLIP_BACK_FLAG;

		// Subdivide if poly vertices overflow.
		if( Poly.NumVertices >= FPoly::VERTEX_THRESHOLD )
		{
			FPoly TempPoly;
			Poly.SplitInHalf( &TempPoly );
			MakePortalsClip( iNode, TempPoly, Clip, Func );
		}

		// Split by parent.
		FPoly Front,Back;
		int Split = Poly.SplitWithNode(Model,iClipNode,&Front,&Back,1);

		// Make sure we generated a useful fragment.
		if(	(Split==SP_Front &&  (Clips[Clip] & CLIP_BACK_FLAG) )
		||	(Split==SP_Back  && !(Clips[Clip] & CLIP_BACK_FLAG) )
		||	(Split==SP_Coplanar))
		{
			// Clipped to oblivion, or useless coplanar.
			return;
		}

		if( Split==SP_Split )
		{
			// Keep the appropriate piece.
			Poly = (Clips[Clip] & CLIP_BACK_FLAG) ? Back : Front;
		}

		// Clip by next parent.
		Clip++;
	}

	// Filter poly down the back subtree.
	FilterThroughSubtree
	(
		0,
		iNode,
		iNode,
		Model->Nodes(iNode).iLeaf[0],
		Model->Nodes(iNode).iBack,
		Poly,
		Func,
		INDEX_NONE
	);
}

//
// Make all portals.
//
void FEditorVisibility::MakePortals( INT iNode )
{
	INT iOriginalNode = iNode;

	// Make an infinite edpoly for this node.
	FPoly Poly = BuildInfiniteFPoly( Model, iNode );

	// Filter the portal through this subtree.
	MakePortalsClip( iNode, Poly, 0, &FEditorVisibility::AddPortal );

	// Make portals for front.
	if( Model->Nodes(iNode).iFront != INDEX_NONE )
	{
		Clips[NumClips++] = iNode;
		MakePortals( Model->Nodes(iNode).iFront );
		NumClips--;
	}

	// Make portals for back.
	if( Model->Nodes(iNode).iBack != INDEX_NONE )
	{
		Clips[NumClips++] = iNode | CLIP_BACK_FLAG;
		MakePortals( Model->Nodes(iNode).iBack );
		NumClips--;
	}

	// For all zone portals at this node, mark the matching FPortals as blocked.
	while( iNode != INDEX_NONE )
	{
		FBspNode& Node = Model->Nodes( iNode      );
		FBspSurf& Surf = Model->Surfs( Node.iSurf );
		if( (Surf.PolyFlags & PF_Portal) && GEditor->bspNodeToFPoly( Model, iNode, &Poly ) )
		{
			Model->PortalNodes.AddItem(iNode);
			NumZonePortals++;
			iZonePortalSurf = Node.iSurf;
			FilterThroughSubtree
			(
				0,
				iNode,
				iOriginalNode,
				Model->Nodes(iOriginalNode).iLeaf[0],
				Model->Nodes(iOriginalNode).iBack,
				Poly,
				&FEditorVisibility::BlockPortal,
				INDEX_NONE
			);
		}
		iNode = Node.iPlane;
	}
}

/*-----------------------------------------------------------------------------
	Assign leaves.
-----------------------------------------------------------------------------*/

//
// Assign contiguous unique numbers to all front and back leaves in the BSP.
// Stores the leaf numbers in FBspNode::iLeaf[2].
//
void FEditorVisibility::AssignLeaves( INT iNode, INT Outside )
{
	FBspNode &Node = Model->Nodes(iNode);
	for( int IsFront=0; IsFront<2; IsFront++ )
	{
		if( Node.iChild[IsFront] != INDEX_NONE )
		{
			AssignLeaves( Node.iChild[IsFront], Node.ChildOutside( IsFront, Outside, NF_NotVisBlocking ) );
		}
		else if( Node.ChildOutside( IsFront, Outside, NF_NotVisBlocking ) )
		{
			Node.iLeaf[IsFront] = Model->Leaves.AddItem(FLeaf(Model->Leaves.Num()));
		}
	}
}

/*-----------------------------------------------------------------------------
	Zoning.
-----------------------------------------------------------------------------*/

//
// Form zones from the leaves.
//
void FEditorVisibility::FormZonesFromLeaves()
{
	FMemMark Mark(GMem);

	// Go through all portals and merge the adjoining zones.
	for( FPortal* Portal=FirstPortal; Portal; Portal=Portal->GlobalNext )
	{
		if( Portal->iZonePortalSurf==INDEX_NONE )//!!&& Abs(Portal->Area())>10.0 )
		{
			INT Original = Model->Leaves(Portal->iFrontLeaf).iZone;
			INT New      = Model->Leaves(Portal->iBackLeaf ).iZone;
			for( INT i=0; i<Model->Leaves.Num(); i++ )
			{
				if( Model->Leaves(i).iZone == Original )
					Model->Leaves(i).iZone = New;
			}
		}
	}
	// Renumber the leaves.
	INT NumZones=0;
	for( INT i=0; i<Model->Leaves.Num(); i++ )
	{
		if( Model->Leaves(i).iZone >= NumZones )
		{
			for( int j=i+1; j<Model->Leaves.Num(); j++ )
				if( Model->Leaves(j).iZone == Model->Leaves(i).iZone )
					Model->Leaves(j).iZone = NumZones;
			Model->Leaves(i).iZone = NumZones++;
		}
	}
	debugf( NAME_Log, TEXT("Found %i zones"), NumZones );

	// Confine the zones to 1-63.
	for( INT i=0; i<Model->Leaves.Num(); i++ )
		Model->Leaves(i).iZone = (Model->Leaves(i).iZone % 63) + 1;

	// Set official zone count.
	Model->NumZones = Clamp(NumZones+1,1,64);

	Mark.Pop();
}

/*-----------------------------------------------------------------------------
	Assigning zone numbers.
-----------------------------------------------------------------------------*/

//
// Go through the Bsp and assign zone numbers to all nodes.  Prior to this
// function call, only leaves have zone numbers.  The zone numbers for the entire
// Bsp can be determined from leaf zone numbers.
//
void FEditorVisibility::AssignAllZones( INT iNode, int Outside )
{
	INT iOriginalNode = iNode;

	// Recursively assign zone numbers to children.
	if( Model->Nodes(iOriginalNode).iFront != INDEX_NONE )
		AssignAllZones( Model->Nodes(iOriginalNode).iFront, Outside || Model->Nodes(iOriginalNode).IsCsg(NF_NotVisBlocking) );
	
	if( Model->Nodes(iOriginalNode).iBack != INDEX_NONE )
		AssignAllZones( Model->Nodes(iOriginalNode).iBack, Outside && !Model->Nodes(iOriginalNode).IsCsg(NF_NotVisBlocking) );

	// Make sure this node's polygon resides in a single zone.  In other words,
	// find all of the zones belonging to outside Bsp leaves and make sure their
	// zone number is the same, and assign that zone number to this node.
	while( iNode != INDEX_NONE )
	{
		FPoly Poly;
		if( !(Model->Nodes(iNode).NodeFlags & NF_IsNew) && GEditor->bspNodeToFPoly( Model, iNode, &Poly ) )
		{
			// Make sure this node is added to the BSP properly.
			int OriginalNumNodes = Model->Nodes.Num();
			FilterThroughSubtree
			(
				0,
				iNode,
				iOriginalNode,
				Model->Nodes(iOriginalNode).iLeaf [0],
				Model->Nodes(iOriginalNode).iChild[0],
				Poly,
				&FEditorVisibility::TagZonePortalFragment,
				INDEX_NONE
			);

			// See if all of all non-interior added fragments are in the same zone.
			if( Model->Nodes.Num() > OriginalNumNodes )
			{
				int CanMerge=1, iZone[2]={0,0};
				for( int i=OriginalNumNodes; i<Model->Nodes.Num(); i++ )
					for( int j=0; j<2; j++ )
						if( Model->Nodes(i).iZone[j] != 0 )
							iZone[j] = Model->Nodes(i).iZone[j];
				for( int i=OriginalNumNodes; i<Model->Nodes.Num(); i++ )
					for( int j=0; j<2; j++ )
						if( Model->Nodes(i).iZone[j]!=0 && Model->Nodes(i).iZone[j]!=iZone[j] )
							CanMerge=0;
				if( CanMerge )
				{
					// All fragments were in the same zone, so keep the original and discard the new fragments.
					for( int i=OriginalNumNodes; i<Model->Nodes.Num(); i++ )
						Model->Nodes(i).NumVertices = 0;
					for( int i=0; i<2; i++ )
						Model->Nodes(iNode).iZone[i] = iZone[i];
				}
				else
				{
					// Keep the multi-zone fragments and remove the original plus any interior unnecessary polys.
					Model->Nodes(iNode).NumVertices = 0;
					for( int i=OriginalNumNodes; i<Model->Nodes.Num(); i++ )
						if( Model->Nodes(i).iZone[0]==0 && Model->Nodes(i).iZone[1]==0 )
							Model->Nodes(i).NumVertices = 0;
				}
			}
		}
		iNode = Model->Nodes(iNode).iPlane;
	}
}

/*-----------------------------------------------------------------------------
	Bsp zone structure building.
-----------------------------------------------------------------------------*/

//
// Build a 64-bit zone mask for each node, with a bit set for every
// zone that's referenced by the node and its children.  This is used
// during rendering to reject entire sections of the tree when it's known
// that none of the zones in that section are active.
//
FZoneSet BuildZoneMasks( UModel* Model, INT iNode )
{
	FBspNode& Node = Model->Nodes(iNode);
	FZoneSet ZoneMask = FZoneSet::NoZones();

	if( Node.iZone[0]!=0 ) ZoneMask.AddZone(Node.iZone[0]);
	if( Node.iZone[1]!=0 ) ZoneMask.AddZone(Node.iZone[1]);

	if( Node.iFront != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iFront );
	if( Node.iBack  != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iBack );
	if( Node.iPlane != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iPlane );

	Node.ZoneMask = ZoneMask;

	return ZoneMask;
}

//
// Build 64x64 zone connectivity matrix.  Entry(i,j) is set if node i is connected
// to node j.  Entry(i,i) is always set by definition.  This structure is built by
// analyzing all portals in the world and tagging the two zones they connect.
//
// Called by: TestVisibility.
//
void FEditorVisibility::BuildConnectivity()
{
	for( int i=0; i<64; i++ )
	{
		// Init to identity.
		Model->Zones[i].Connectivity = FZoneSet::IndividualZone(i);
	}
	for( int i=0; i<Model->Nodes.Num(); i++ )
	{
		// Process zones connected by portals.
		FBspNode &Node = Model->Nodes(i);
		FBspSurf &Surf = Model->Surfs(Node.iSurf);

		if( Surf.PolyFlags & PF_Portal )
		{
			Model->Zones[Node.iZone[1]].Connectivity |= FZoneSet::IndividualZone(Node.iZone[0]);
			Model->Zones[Node.iZone[0]].Connectivity |= FZoneSet::IndividualZone(Node.iZone[1]);
		}
	}
}

/*-----------------------------------------------------------------------------
	Zone info assignment.
-----------------------------------------------------------------------------*/

//
// Attach ZoneInfo actors to the zones that they belong in.
// ZoneInfo actors are a class of actor which level designers may
// place in UnrealEd in order to specify the properties of the zone they
// reside in, such as water effects, zone name, etc.
//
void FEditorVisibility::BuildZoneInfo()
{
	int Infos=0, Duplicates=0, Zoneless=0;
	GWarn->StatusUpdatef( 0, 0, TEXT("Computing zones") );

	for( INT i=0; i<FBspNode::MAX_ZONES; i++ )
	{
		// By default, the LevelInfo (actor 0) acts as the ZoneInfo
		// for all zones which don't have individual ZoneInfo's.
		Model->Zones[i].ZoneActor = NULL;
	}
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		if( Level->Actors(iActor) )
			Level->Actors(iActor)->Region = FPointRegion( Level->GetLevelInfo(), INDEX_NONE, 0 );
	}
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AZoneInfo* Actor = Cast<AZoneInfo>( Level->Actors(iActor) );
		if( Actor && !Actor->IsA(ALevelInfo::StaticClass()) )
		{
			Actor->Region = Model->PointRegion( Level->GetLevelInfo(), Actor->Location );
			if( Actor->Region.ZoneNumber == 0 )
			{
				Zoneless++;
			}
			else if( Model->Zones[Actor->Region.ZoneNumber].ZoneActor )
			{
				Duplicates++;
			}
			else
			{
				// Associate zone info.
				Infos++;
				Model->Zones[Actor->Region.ZoneNumber].ZoneActor = (AZoneInfo*)Actor;

#if 0
				// Perform aural reverb raytracing.
				if( Actor->bReverbZone && Actor->bRaytraceReverb )
				{
					// Sample a bunch of random oriented vectors to get a feeling for how big the zone is.
					const int NumTraces = 256;
					const FLOAT MaxDist = 16384;
					TArray<FVector> Samples;
					for( int i=0; i<NumTraces; i++ )
					{
						FCheckResult Hit(1.0);
						Model->LineCheck( Hit, NULL, Actor->Location + MaxDist*VRand(), Actor->Location, FVector(0,0,0), 0, 0 );
						debugf( NAME_Log, TEXT("   dist=%f"), Hit.Time * MaxDist );
						new(Samples)FVector(Hit.Time * MaxDist / Actor->SpeedOfSound,1,0);
					}

					// Quantize the reverb times.
					while( Samples.Num() > ARRAY_COUNT(Actor->Delay) )
					{
						// Find best pair to merge.
						INT iBest=0, jBest=1;
						FLOAT BestCost=10000000000000.f;
						for( int i=0; i<Samples.Num(); i++ )
						{
							for( int j=0; j<i; j++ )
							{
								FLOAT Cost = Square(Samples(i).X-Samples(j).X) * (Samples(i).Y + Samples(j).Y);
								if( Cost < BestCost )
								{
									BestCost = Cost;
									iBest    = i;
									jBest    = j;
								}
							}
						}

						// Merge them.
						FVector A = Samples(iBest);
						FVector B = Samples(jBest);
						Samples(iBest) = FVector( (A.X*A.Y + B.X*B.Y)/(A.Y+B.Y), A.Y+B.Y, 0 );
						Samples.Remove(jBest);
					}

					// Sort them.
					for( int i=0; i<Samples.Num(); i++ )
						for( int j=0; j<i; j++ )
							if( Samples(i).X < Samples(j).X )
								Exchange( Samples(i), Samples(j) );

					// Copy reverb times to the zone info.
					for( int i=0; i<Samples.Num(); i++ )
					{
						Actor->Delay[i] = Clamp( Samples(i).X*1000.0, 0.0, 255.0 );
						Actor->Gain [i] = Clamp( Samples(i).Y*255.0/NumTraces, 0.0, 255.0 );
					}
				}
#endif
			}
		}
	}
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
		if( Level->Actors(iActor) )
			Level->Actors(iActor)->SetZone( 1, 1 );
	debugf( NAME_Log, TEXT("BuildZoneInfo: %i ZoneInfo actors, %i duplicates, %i zoneless"), Infos, Duplicates, Zoneless );
}

/*-----------------------------------------------------------------------------
	Volume visibility test.
-----------------------------------------------------------------------------*/

extern FRebuildTools		GRebuildTools;

//
// Test visibility.
//
void FEditorVisibility::TestVisibility()
{
	GWarn->BeginSlowTask(TEXT("Zoning"),1);

	// Init Bsp info.
	for( int i=0; i<Model->Nodes.Num(); i++ )
	{
		for( int j=0; j<2; j++ )
		{
			Model->Nodes(i).iLeaf [j] = INDEX_NONE;
			Model->Nodes(i).iZone [j] = 0;
		}
	}

	// Allocate objects.
	Model->Leaves.Empty();

	// Assign leaf numbers to convex outside volumes.
	AssignLeaves( 0, Model->RootOutside );

	// Allocate leaf info.
	LeafPortals  = new( GMem, MEM_Zeroed, Model->Leaves.Num()      )FPortal*;
	NodePortals  = new( GMem, MEM_Zeroed, Model->Nodes.Num()*2+256)FPortal*; // Allow for 2X expansion from zone portal fragments!!

	// Build all portals, with references to their front and back leaves.
	MakePortals( 0 );

	// Form zones.
	FormZonesFromLeaves();
	AssignAllZones( 0, Model->RootOutside );

	// Cleanup the bsp.
	//!!unsafe: screws up the node portals required for visibility checking.
	//!!but necessary for proper rendering.
	GEditor->bspCleanup( Model );
	GEditor->bspRefresh( Model, 1 );
	GEditor->bspBuildBounds( Model );

	// Build zone interconnectivity info.
	BuildZoneMasks( Model, 0 );
	BuildConnectivity();
	BuildZoneInfo();

	debugf( NAME_Log, TEXT("Portalized: %i portals, %i zone portals (%i fragments), %i leaves, %i nodes"), NumPortals, NumZonePortals, NumZoneFragments, Model->Leaves.Num(), Model->Nodes.Num() );

#if DEBUG_PORTALS || DEBUG_WRAPS || DEBUG_BADSHEETS || DEBUG_LVS
	GEditor->bspMergeCoplanars( Level->Brush()->Brush, 0, 1 );
#endif
	GWarn->EndSlowTask();
}

/*-----------------------------------------------------------------------------
	Visibility constructor/destructor.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
FEditorVisibility::FEditorVisibility( ULevel* InLevel, UModel* InModel, INT InExtra )
:	Mark			(GMem),
	Level			(InLevel),
	Model			(InModel),
	NumPortals		(0),
	NumClips		(0),
	NumClipTests	(0),
	NumPassedClips	(0),
	NumUnclipped	(0),
	NumBspPortals	(0),
	MaxFragments	(0),
	NumZonePortals	(0),
	NumZoneFragments(0),
	Extra			(InExtra),
	FirstPortal		(NULL),
	NodePortals		(NULL),
	LeafPortals		(NULL)
{
#if DEBUG_PORTALS || DEBUG_WRAPS || DEBUG_BADSHEETS || DEBUG_LVS
	// Init brush for debugging.
	DEBUG_Brush=InLevel->Brush();
	DEBUG_Brush->Brush->Polys->Element.Empty();
	DEBUG_Brush->Location=DEBUG_Brush->PrePivot=FVector(0,0,0);
	DEBUG_Brush->Rotation = FRotator(0,0,0);
#endif

}

//
// Destructor.
//
FEditorVisibility::~FEditorVisibility()
{
	Mark.Pop();
	//!!Visibility->~FSymmetricBitArray;
}

/*-----------------------------------------------------------------------------
	Main function.
-----------------------------------------------------------------------------*/

//
// Perform visibility testing within the level.
//
void UEditorEngine::TestVisibility( ULevel* Level, UModel* Model, int A, int B )
{
	if( Model->Nodes.Num() )
	{
		// Test visibility.
		FEditorVisibility Visi( Level, Model, A );
		Visi.TestVisibility();
	}
}


/*-----------------------------------------------------------------------------
	Bsp node bounding volumes.
-----------------------------------------------------------------------------*/

#if DEBUG_HULLS
	UModel *DEBUG_Brush;
#endif

//
// Update a bounding volume by expanding it to enclose a list of polys.
//
void UpdateBoundWithPolys( FBox& Bound, FPoly** PolyList, INT nPolys )
{
	for( INT i=0; i<nPolys; i++ )
		for( INT j=0; j<PolyList[i]->NumVertices; j++ )
			Bound += PolyList[i]->Vertex[j];
}

//
// Update a convolution hull with a list of polys.
//
void UpdateConvolutionWithPolys( UModel *Model, INT iNode, FPoly **PolyList, int nPolys )
{
	FBox Box(0);

	FBspNode &Node = Model->Nodes(iNode);
	Node.iCollisionBound = Model->LeafHulls.Num();
	for( int i=0; i<nPolys; i++ )
	{
		if( PolyList[i]->iBrushPoly != INDEX_NONE )
		{
			int j;
			for( j=0; j<i; j++ )
				if( PolyList[j]->iBrushPoly == PolyList[i]->iBrushPoly )
					break;
			if( j >= i )
				Model->LeafHulls.AddItem(PolyList[i]->iBrushPoly);
		}
		for( int j=0; j<PolyList[i]->NumVertices; j++ )
			Box += PolyList[i]->Vertex[j];
	}
	Model->LeafHulls.AddItem(INDEX_NONE);

	// Add bounds.
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.X );
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.Y );
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.Z );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.X );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.Y );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.Z );

}

//
// Cut a partitioning poly by a list of polys, and add the resulting inside pieces to the
// front list and back list.
//
void SplitPartitioner
(
	UModel*	Model,
	FPoly**	PolyList,
	FPoly**	FrontList,
	FPoly**	BackList,
	INT		n,
	INT		nPolys,
	INT&	nFront, 
	INT&	nBack, 
	FPoly	InfiniteEdPoly
)
{
	FPoly FrontPoly,BackPoly;
	while( n < nPolys )
	{
		if( InfiniteEdPoly.NumVertices >= FPoly::VERTEX_THRESHOLD )
		{
			FPoly Half;
			InfiniteEdPoly.SplitInHalf(&Half);
			SplitPartitioner(Model,PolyList,FrontList,BackList,n,nPolys,nFront,nBack,Half);
		}
		FPoly* Poly = PolyList[n];
		switch( InfiniteEdPoly.SplitWithPlane(Poly->Vertex[0],Poly->Normal,&FrontPoly,&BackPoly,0) )
		{
			case SP_Coplanar:
				// May occasionally happen.
				debugf( NAME_Log, TEXT("FilterBound: Got inficoplanar") );
				break;

			case SP_Front:
				// Shouldn't happen if hull is correct.
				debugf( NAME_Log, TEXT("FilterBound: Got infifront") );
				return;

			case SP_Split:
				InfiniteEdPoly = BackPoly;
				break;

			case SP_Back:
				break;
		}
		n++;
	}

	FPoly* New = new(GMem)FPoly;
	*New = InfiniteEdPoly;
	New->Reverse();
	New->iBrushPoly |= 0x40000000;
	FrontList[nFront++] = New;

	New = new(GMem)FPoly;
	*New = InfiniteEdPoly;
	BackList[nBack++] = New;
}

//
// Recursively filter a set of polys defining a convex hull down the Bsp,
// splitting it into two halves at each node and adding in the appropriate
// face polys at splits.
//
void FilterBound
(
	UModel*			Model,
	FBox*			ParentBound,
	INT				iNode,
	FPoly**			PolyList,
	INT				nPolys,
	INT				Outside
)
{
	FMemMark Mark(GMem);
	FBspNode&	Node	= Model->Nodes  (iNode);
	FBspSurf&	Surf	= Model->Surfs  (Node.iSurf);
	FVector		Base = Surf.Plane * Surf.Plane.W;
	FVector&	Normal	= Model->Vectors(Surf.vNormal);
	FBox		Bound(0);

	Bound.Min.X = Bound.Min.Y = Bound.Min.Z = +WORLD_MAX;
	Bound.Max.X = Bound.Max.Y = Bound.Max.Z = -WORLD_MAX;

	// Split bound into front half and back half.
	FPoly** FrontList = new(GMem,nPolys*2+16)FPoly*; int nFront=0;
	FPoly** BackList  = new(GMem,nPolys*2+16)FPoly*; int nBack=0;

	FPoly* FrontPoly  = new(GMem)FPoly;
	FPoly* BackPoly   = new(GMem)FPoly;

	for( INT i=0; i<nPolys; i++ )
	{
		FPoly *Poly = PolyList[i];
		switch( Poly->SplitWithPlane( Base, Normal, FrontPoly, BackPoly, 0 ) )
		{
			case SP_Coplanar:
				debugf( NAME_Log, TEXT("FilterBound: Got coplanar") );
				FrontList[nFront++] = Poly;
				BackList[nBack++] = Poly;
				break;
			
			case SP_Front:
				FrontList[nFront++] = Poly;
				break;
			
			case SP_Back:
				BackList[nBack++] = Poly;
				break;
			
			case SP_Split:
				if( FrontPoly->NumVertices >= FPoly::VERTEX_THRESHOLD )
				{
					FPoly *Half = new(GMem)FPoly;
					FrontPoly->SplitInHalf(Half);
					FrontList[nFront++] = Half;
				}
				FrontList[nFront++] = FrontPoly;

				if( BackPoly->NumVertices >= FPoly::VERTEX_THRESHOLD )
				{
					FPoly *Half = new(GMem)FPoly;
					BackPoly->SplitInHalf(Half);
					BackList[nBack++] = Half;
				}
				BackList [nBack++] = BackPoly;

				FrontPoly = new(GMem)FPoly;
				BackPoly  = new(GMem)FPoly;
				break;

			default:
				appErrorf( TEXT("FZoneFilter::FilterToLeaf: Unknown split code") );
		}
	}
	if( nFront && nBack )
	{
		// Add partitioner plane to front and back.
		FPoly InfiniteEdPoly = BuildInfiniteFPoly( Model, iNode );
		InfiniteEdPoly.iBrushPoly = iNode;

		SplitPartitioner(Model,PolyList,FrontList,BackList,0,nPolys,nFront,nBack,InfiniteEdPoly);
	}
	else
	{
		if( !nFront ) debugf( NAME_Log, TEXT("FilterBound: Empty fronthull") );
		if( !nBack  ) debugf( NAME_Log, TEXT("FilterBound: Empty backhull") );
	}

	// Recursively update all our childrens' bounding volumes.
	if( nFront > 0 )
	{
		if( Node.iFront != INDEX_NONE )
			FilterBound( Model, &Bound, Node.iFront, FrontList, nFront, Outside || Node.IsCsg() );
		else if( Outside || Node.IsCsg() )
			UpdateBoundWithPolys( Bound, FrontList, nFront );
		else
			UpdateConvolutionWithPolys( Model, iNode, FrontList, nFront );
	}
	if( nBack > 0 )
	{
		if( Node.iBack != INDEX_NONE)
			FilterBound( Model, &Bound,Node.iBack, BackList, nBack, Outside && !Node.IsCsg() );
		else if( Outside && !Node.IsCsg() )
			UpdateBoundWithPolys( Bound, BackList, nBack );
		else
			UpdateConvolutionWithPolys( Model, iNode, BackList, nBack );
	}

	// Update parent bound to enclose this bound.
	if( ParentBound )
		*ParentBound += Bound;

	Mark.Pop();
}

//
// Build bounding volumes for all Bsp nodes.  The bounding volume of the node
// completely encloses the "outside" space occupied by the nodes.  Note that 
// this is not the same as representing the bounding volume of all of the 
// polygons within the node.
//
// We start with a practically-infinite cube and filter it down the Bsp,
// whittling it away until all of its convex volume fragments land in leaves.
//
void UEditorEngine::bspBuildBounds( UModel* Model )
{
	if( Model->Nodes.Num()==0 )
		return;

	BuildZoneMasks( Model, 0 );

	FPoly Polys[6], *PolyList[6];
	for( int i=0; i<6; i++ )
	{
		PolyList[i] = &Polys[i];
		PolyList[i]->Init();
		PolyList[i]->NumVertices=4;
		PolyList[i]->iBrushPoly = INDEX_NONE;
	}

	Polys[0].Vertex[0]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
	Polys[0].Vertex[1]=FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
	Polys[0].Vertex[2]=FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
	Polys[0].Vertex[3]=FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
	Polys[0].Normal   =FVector( 0.000000,  0.000000,  1.000000 );
	Polys[0].Base     =Polys[0].Vertex[0];

	Polys[1].Vertex[0]=FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[1].Vertex[1]=FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[1].Vertex[2]=FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[1].Vertex[3]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[1].Normal   =FVector( 0.000000,  0.000000, -1.000000 );
	Polys[1].Base     =Polys[1].Vertex[0];

	Polys[2].Vertex[0]=FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[2].Vertex[1]=FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[2].Vertex[2]=FVector( HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[2].Vertex[3]=FVector( HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[2].Normal   =FVector( 0.000000,  1.000000,  0.000000 );
	Polys[2].Base     =Polys[2].Vertex[0];

	Polys[3].Vertex[0]=FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[3].Vertex[1]=FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[3].Vertex[2]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[3].Vertex[3]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[3].Normal   =FVector( 0.000000, -1.000000,  0.000000 );
	Polys[3].Base     =Polys[3].Vertex[0];

	Polys[4].Vertex[0]=FVector(HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[4].Vertex[1]=FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[4].Vertex[2]=FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[4].Vertex[3]=FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[4].Normal   =FVector( 1.000000,  0.000000,  0.000000 );
	Polys[4].Base     =Polys[4].Vertex[0];

	Polys[5].Vertex[0]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[5].Vertex[1]=FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[5].Vertex[2]=FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	Polys[5].Vertex[3]=FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[5].Normal   =FVector(-1.000000,  0.000000,  0.000000 );
	Polys[5].Base     =Polys[5].Vertex[0];
	// Empty hulls.
	Model->LeafHulls.Empty();
	for( int i=0; i<Model->Nodes.Num(); i++ )
		Model->Nodes(i).iCollisionBound  = INDEX_NONE;
	FilterBound( Model, NULL, 0, PolyList, 6, Model->RootOutside );
	debugf( NAME_Log, TEXT("bspBuildBounds: Generated %i hulls"), Model->LeafHulls.Num() );
}

/*-----------------------------------------------------------------------------
	Non-class functions.
-----------------------------------------------------------------------------*/

//
// Build an FPoly representing an "infinite" plane (which exceeds the maximum
// dimensions of the world in all directions) for a particular Bsp node.
//
FPoly BuildInfiniteFPoly( UModel* Model, INT iNode )
{
	FBspNode &Node   = Model->Nodes  (iNode       );
	FBspSurf &Poly   = Model->Surfs  (Node.iSurf  );
	FVector  Base    = Poly.Plane * Poly.Plane.W;
	FVector  Normal  = Poly.Plane;
	FVector	 Axis1,Axis2;

	// Find two non-problematic axis vectors.
	Normal.FindBestAxisVectors( Axis1, Axis2 );

	// Set up the FPoly.
	FPoly EdPoly;
	EdPoly.Init();
	EdPoly.NumVertices = 4;
	EdPoly.Normal      = Normal;
	EdPoly.Base        = Base;
	EdPoly.Vertex[0]   = Base + Axis1*WORLD_MAX + Axis2*WORLD_MAX;
	EdPoly.Vertex[1]   = Base - Axis1*WORLD_MAX + Axis2*WORLD_MAX;
	EdPoly.Vertex[2]   = Base - Axis1*WORLD_MAX - Axis2*WORLD_MAX;
	EdPoly.Vertex[3]   = Base + Axis1*WORLD_MAX - Axis2*WORLD_MAX;

	return EdPoly;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
