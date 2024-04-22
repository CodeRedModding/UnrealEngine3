/*=============================================================================
	UnVisi.cpp: Unreal visibility computation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

Description:
	Visibility and zoining code.
=============================================================================*/

#include "UnrealEd.h"
#include "BSPOps.h"

/*-----------------------------------------------------------------------------
	Temporary.
-----------------------------------------------------------------------------*/

// Options.
#define DEBUG_PORTALS	0	/* Debugging hull code by generating hull brush */
#define DEBUG_WRAPS		0	/* Debugging sheet wrapping code */
#define DEBUG_BADSHEETS 0   /* Debugging sheet discrepancies */
#define DEBUG_LVS       0   /* Debugging light volumes */

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
		for( INT i=2; i<Vertices.Num(); i++ )
			Cross += (Vertices(i-1)-Vertices(0)) ^ (Vertices(i)-Vertices(0));
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
	FEditorVisibility( UModel* InModel, INT InDebug );

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
	INT iNewNode = FBSPOps::bspAddNode( Model, iGeneratingNode, FBSPOps::NODE_Plane, Model->Nodes(iGeneratingNode).NodeFlags | NF_IsNew, &Poly );

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
			new(GMainThreadMemStack)FPortal
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
	FPoly Poly = FBSPOps::BuildInfiniteFPoly( Model, iNode );

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
	FMemMark Mark(GMainThreadMemStack);

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
// 	debugf( NAME_Log, TEXT("Found %i zones"), NumZones );

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
	Volume visibility test.
-----------------------------------------------------------------------------*/

//
// Test visibility.
//
void FEditorVisibility::TestVisibility()
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("Zoning")), TRUE );

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
	LeafPortals  = new( GMainThreadMemStack, MEM_Zeroed, Model->Leaves.Num()      )FPortal*;
	NodePortals  = new( GMainThreadMemStack, MEM_Zeroed, Model->Nodes.Num()*2+256)FPortal*; // Allow for 2X expansion from zone portal fragments!!

	// Build all portals, with references to their front and back leaves.
	MakePortals( 0 );

	// Form zones.
	FormZonesFromLeaves();
	AssignAllZones( 0, Model->RootOutside );

	// Cleanup the bsp.
	//!!unsafe: screws up the node portals required for visibility checking.
	//!!but necessary for proper rendering.
	GEditor->bspCleanup( Model );
	FBSPOps::bspRefresh( Model, 1 );
	FBSPOps::bspBuildBounds( Model );

	// Build zone interconnectivity info.
	FBSPOps::BuildZoneMasks( Model, 0 );
	BuildConnectivity();

// 	debugf( NAME_Log, TEXT("Portalized: %i portals, %i zone portals (%i fragments), %i leaves, %i nodes"), NumPortals, NumZonePortals, NumZoneFragments, Model->Leaves.Num(), Model->Nodes.Num() );

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
FEditorVisibility::FEditorVisibility( UModel* InModel, INT InExtra )
:	Mark			(GMainThreadMemStack),
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
void UEditorEngine::TestVisibility( UModel* Model, int A, int B )
{
	if( Model->Nodes.Num() )
	{
		// Test visibility.
		FEditorVisibility Visi( Model, A );
		Visi.TestVisibility();
	}
}
#if DEBUG_HULLS
	UModel *DEBUG_Brush;
#endif
/*-----------------------------------------------------------------------------
	Non-class functions.
-----------------------------------------------------------------------------*/

