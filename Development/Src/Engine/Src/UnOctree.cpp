/*=============================================================================
	UnOctree.cpp: Octree implementation
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "UnOctreePrivate.h"

static DWORD GOctreeBytesUsed = 0;

// urgh
#define MY_FLTMAX (3.402823466e+38F)
#define MY_INTMAX (2147483647)

//////////////////////////////////////////// UTIL //////////////////////////////////////////////////
#define MAX_PRIMITIVES_PER_NODE	(3)		// Max number of actors held in a node before adding children.
#define MIN_NODE_SIZE			(100)	// Mininmum size a node can ever be.

#define CHECK_FALSE_NEG			(0)		// Check for hits with primitive, but not with bounding box.

enum 
{
	CHILDXMAX = 0x004,
	CHILDYMAX = 0x002,
	CHILDZMAX = 0x001
};

static const FOctreeNodeBounds	RootNodeBounds(FVector(0,0,0),HALF_WORLD_MAX);

FOctreeStatGroup	GOctreeStats;

//
//	FOctreeNodeBounds::FOctreeNodeBounds - Given a child index and the parent's bounding cube, construct the child's bounding cube.
//

FOctreeNodeBounds::FOctreeNodeBounds(const FOctreeNodeBounds& InParentBounds,INT InChildIndex)
{
	Extent = InParentBounds.Extent * 0.5f;
	Center.X = InParentBounds.Center.X + (((InChildIndex & CHILDXMAX) >> 1) - 1) * Extent;
	Center.Y = InParentBounds.Center.Y + (((InChildIndex & CHILDYMAX)     ) - 1) * Extent;
	Center.Z = InParentBounds.Center.Z + (((InChildIndex & CHILDZMAX) << 1) - 1) * Extent;
}

//#define MY_FABS(x) (Abs(x))
#define MY_FABS(x) (fabsf(x))

// New, hopefully faster, 'slabs' box test.
static UBOOL LineBoxIntersect
(
	const FVector&	BoxCenter,
	const FVector&  BoxRadii,
	const FVector&	Start,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	//const FVector* boxPlanes = &Box.Min;
	
	FLOAT tf, tb;
	FLOAT tnear = 0.f;
	FLOAT tfar = 1.f;
	
	FVector LocalStart = Start - BoxCenter;

	// X //
	// First - see if ray is parallel to slab.
	if(Direction.X != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.X * OneOverDirection.X) - BoxRadii.X * MY_FABS(OneOverDirection.X);
		tb = - (LocalStart.X * OneOverDirection.X) + BoxRadii.X * MY_FABS(OneOverDirection.X);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		// If it is parallel, early return if start is outiside slab.
		if(!(MY_FABS(LocalStart.X) <= BoxRadii.X))
			return 0;
	}

	// Y //
	if(Direction.Y != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Y * OneOverDirection.Y) - BoxRadii.Y * MY_FABS(OneOverDirection.Y);
		tb = - (LocalStart.Y * OneOverDirection.Y) + BoxRadii.Y * MY_FABS(OneOverDirection.Y);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxRadii.Y))
			return 0;
	}

	// Z //
	if(Direction.Z != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Z * OneOverDirection.Z) - BoxRadii.Z * MY_FABS(OneOverDirection.Z);
		tb = - (LocalStart.Z * OneOverDirection.Z) + BoxRadii.Z * MY_FABS(OneOverDirection.Z);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(MY_FABS(LocalStart.Z) <= BoxRadii.Z))
			return 0;
	}

	// we hit!
	return 1;
}

#define LINE_BOX LineBoxIntersect

static UBOOL BoxBoxIntersect(const FBox& box1,const FBox& box2)
{
	if( box1.Min.X > box2.Max.X || box2.Min.X > box1.Max.X )
		return false;
	if( box1.Min.Y > box2.Max.Y || box2.Min.Y > box1.Max.Y )
		return false;
	if( box1.Min.Z > box2.Max.Z || box2.Min.Z > box1.Max.Z )
		return false;
	return true;
}

// If TRACE_SingleResult, only return 1 result (the first hit).
// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace).
FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags)
{
	FCheckResult* FirstResult = NULL;

	if(Hits)
	{
		FLOAT firstTime = MY_FLTMAX;
		for(FCheckResult* res = Hits; res!=NULL; res=res->GetNext())
		{
			if(res->Time < firstTime)
			{
				FirstResult = res;
				firstTime = res->Time;
			}
		}

		if(FirstResult)
			FirstResult->GetNext() = NULL;
	}

	return FirstResult;

}

// Create array of children node indices that this box overlaps.
static inline INT FindChildren(const FOctreeNodeBounds& ParentBounds, const FBox& testBox, INT* childIXs)
{
	INT childCount = 0;

	if(testBox.Max.X > ParentBounds.Center.X) // XMAX
	{ 
		if(testBox.Max.Y > ParentBounds.Center.Y) // YMAX
		{
			if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
				childIXs[childCount++] = CHILDXMAX+CHILDYMAX+CHILDZMAX;
			if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
				childIXs[childCount++] = CHILDXMAX+CHILDYMAX          ;
		}

		if(testBox.Min.Y <= ParentBounds.Center.Y) // YMIN
		{
			if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
				childIXs[childCount++] = CHILDXMAX+          CHILDZMAX;
			if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
				childIXs[childCount++] = CHILDXMAX                    ;
		}
	}

	if(testBox.Min.X <= ParentBounds.Center.X) // XMIN
	{ 
		if(testBox.Max.Y > ParentBounds.Center.Y) // YMAX
		{
			if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
				childIXs[childCount++] =           CHILDYMAX+CHILDZMAX;
			if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
				childIXs[childCount++] =           CHILDYMAX          ;	
		}

		if(testBox.Min.Y <= ParentBounds.Center.Y) // YMIN
		{
			if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
				childIXs[childCount++] =                     CHILDZMAX;
			if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
				childIXs[childCount++] = 0                            ;
		}
	}

	return childCount;
}

// Returns which child node 'testBox' would fit into.
// Returns -1 if box overlaps any planes, and therefore wont fit into a child.
// Assumes testBox would fit into this (parent) node.

static inline INT FindChild(const FOctreeNodeBounds& ParentBounds, const FBox& testBox)
{
	INT result = 0;

	if(testBox.Min.X > ParentBounds.Center.X)
		result |= CHILDXMAX;
	else if(testBox.Max.X > ParentBounds.Center.X)
		return -1;

	if(testBox.Min.Y > ParentBounds.Center.Y)
		result |= CHILDYMAX;
	else if(testBox.Max.Y > ParentBounds.Center.Y)
		return -1;

	if(testBox.Min.Z > ParentBounds.Center.Z)
		result |= CHILDZMAX;
	else if(testBox.Max.Z > ParentBounds.Center.Z)
		return -1;

	return result;

}

///////////////////////////////////////////// NODE /////////////////////////////////////////////////
FOctreeNode::FOctreeNode() 
: Primitives( )
{
	Children = NULL;
	// Bounding box is set up by FOctreeNode.StoreActor
	// (or FPrimitiveOctree constructor for root node).

}

FOctreeNode::~FOctreeNode()
{
	// We call RemovePrimitives on nodes in the Octree destructor, 
	// so we should never have primitives in nodes when we destroy them.
	check(Primitives.Num() == 0);

	if(Children)
	{
		delete[] Children;
		Children = NULL;
	}

}

// Remove all primitives in this node from the octree
void FOctreeNode::RemoveAllPrimitives(FPrimitiveOctree* o)
{
	// All primitives found at this octree node, remove from octree.
	while(Primitives.Num() != 0)
	{
		UPrimitiveComponent* Primitive = Primitives(0);

		if(Primitive->OctreeNodes.Num() > 0)
		{
			o->RemovePrimitive(Primitive);
		}
		else
		{
			Primitives.RemoveItem(Primitive);
			debugf(TEXT("PrimitiveComponent (%s) in Octree, but Primitive->OctreeNodes empty."), Primitive->GetName());
		}
	}

	// Then do the same for the children (if present).
	if(Children)
	{
		for(INT i=0; i<8; i++)
			Children[i].RemoveAllPrimitives(o);
	}

}

// Filter node through tree, allowing Actor to only reside in one node.
// Assumes that Actor fits inside this node, but it might go lower.
void FOctreeNode::SingleNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	INT childIx = FindChild(Bounds, Primitive->Bounds.GetBox());

	if(!Children || childIx == -1)
		this->StoreActor(Primitive, o, Bounds);
	else
	{
		this->Children[childIx].SingleNodeFilter(Primitive, o, FOctreeNodeBounds(Bounds,childIx));
	}

}

// Filter node through tree, allowing actor to reside in multiple nodes.
// Assumes that Actor overlaps this node, but it might go lower.
void FOctreeNode::MultiNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// If there are no children, or this primitives bounding box completely contains this nodes
	// bounding box, store the actor at this node.
	if(!Children || Bounds.IsInsideBox(Primitive->Bounds.GetBox()) )
		this->StoreActor(Primitive, o, Bounds);
	else
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, Primitive->Bounds.GetBox(), childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			this->Children[childIXs[i]].MultiNodeFilter(Primitive, o, FOctreeNodeBounds(Bounds,childIXs[i]));
		}
	}
}

//
//	FOctreeNode::GetPrimitives - Return all primitives contained by this octree node or children.
//

void FOctreeNode::GetPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree)
{
	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
	{
		if(Primitives(PrimitiveIndex)->OctreeTag != Octree->OctreeTag)
		{
			Primitives(PrimitiveIndex)->OctreeTag = Octree->OctreeTag;
			OutPrimitives.AddItem(Primitives(PrimitiveIndex));
		}
	}
	if(Children)
		for(INT ChildIndex = 0;ChildIndex < 8;ChildIndex++)
			Children[ChildIndex].GetPrimitives(OutPrimitives,Octree);
}

//
//	FOctreeNode::GetIntersectingPrimitives - Filter bounding box through octree, returning primitives which intersect the bounding box.
//

void FOctreeNode::GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds)
{
	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
	{
		if(Primitives(PrimitiveIndex)->OctreeTag != Octree->OctreeTag)
		{
			Primitives(PrimitiveIndex)->OctreeTag = Octree->OctreeTag;
			if(BoxBoxIntersect(Box,Primitives(PrimitiveIndex)->Bounds.GetBox()))
				OutPrimitives.AddItem(Primitives(PrimitiveIndex));
		}
	}
	if(Children)
	{
		INT	ChildIndices[8],
			NumChildren = FindChildren(Bounds,Box,ChildIndices);
		for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,ChildIndices[ChildIndex]);
			if(ChildBounds.IsInsideBox(Box))
				Children[ChildIndices[ChildIndex]].GetPrimitives(OutPrimitives,Octree);
			else
				Children[ChildIndices[ChildIndex]].GetIntersectingPrimitives(Box,OutPrimitives,Octree,ChildBounds);
		}
	}
}

//
//	FOctreeNode::GetVisiblePrimitives - Filter convex volume through octree, returning primitives which intersect the convex volume.
//

void FOctreeNode::GetVisiblePrimitives(
	const FLevelVisibilitySet& VisibilitySet,
	const FLevelVisibilitySet::FSubset& NodeSubset,
	TArray<UPrimitiveComponent*>& OutPrimitives,
	FPrimitiveOctree* Octree,
	const FOctreeNodeBounds& Bounds
	)
{
	GVisibilityStats.VisibleOctreeNodes.Value++;

	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
	{
		UPrimitiveComponent*	Primitive = Primitives(PrimitiveIndex);
		// Only check the primitive if it hasn't been checked against this visibility set yet.
		if(Primitive->OctreeTag != Octree->OctreeTag)
		{
			switch(VisibilitySet.ContainsPrimitive(Primitive,NodeSubset))
			{
			case FLevelVisibilitySet::CR_ContainedBySet:
				OutPrimitives.AddItem(Primitive);
				Primitive->OctreeTag = Octree->OctreeTag;
				break;
			case FLevelVisibilitySet::CR_NotContainedBySet:
				Primitive->OctreeTag = Octree->OctreeTag;
				break;
			};
		}
	}
	if(Children)
	{
		for(INT ChildIndex = 0;ChildIndex < 8;ChildIndex++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,ChildIndex);

			GVisibilityStats.OctreeNodeTests.Value++;

			// Get the visibility subset contained by the child node's bounding cube.
			FLevelVisibilitySet::FSubset ChildSubset = VisibilitySet.GetBoxIntersectionSubset(
														NodeSubset,
														ChildBounds.Center,
														FVector(ChildBounds.Extent,ChildBounds.Extent,ChildBounds.Extent)
														);

			// Skip the child node if the intersection of this node's visibility subset and the child bounding cube is empty.
			if(!ChildSubset.IsEmpty())
			{
				Children[ChildIndex].GetVisiblePrimitives(VisibilitySet,ChildSubset,OutPrimitives,Octree,ChildBounds);
			}
		}
	}
}

// Just for testing, this tells you which nodes the box should be in. 
void FOctreeNode::FilterTest(const FBox& TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FOctreeNodeBounds& Bounds)
{
	if(bMulti) // Multi-Node Filter
	{
		if(!Children || Bounds.IsInsideBox(TestBox) )
			Nodes->AddItem(this);
		else
		{
			for(INT i=0; i<8; i++)
			{
				this->Children[i].FilterTest(TestBox, 1, Nodes, FOctreeNodeBounds(Bounds,i));
			}
		}
	}
	else // Single Node Filter
	{
		INT childIx = FindChild(Bounds, TestBox);

		if(!Children || childIx == -1)
			Nodes->AddItem(this);
		else
		{
			INT childIXs[8];
			INT numChildren = FindChildren(Bounds, TestBox, childIXs);
			for(INT i=0; i<numChildren; i++)
			{
				this->Children[childIXs[i]].FilterTest(TestBox, 0, Nodes, FOctreeNodeBounds(Bounds,childIXs[i]));
			}
		}
	}
}


// We have decided to actually add an Actor at this node,
// so do whatever work needs doing.
void FOctreeNode::StoreActor(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// If we are over the limit of primitives in a node, and have not already split,
	// and are not already too small, add children, and re-check each actors held here against this node.
	// 
	// Note, because we don't only hold things in leaves, 
	// we can still have more than MAX_PRIMITIVES_PER_NODE in a node.
	if(	Primitives.Num() >= MAX_PRIMITIVES_PER_NODE
		&& Children == NULL 
		&& 0.5f * Bounds.Extent > MIN_NODE_SIZE)
	{
		// Allocate memory for children nodes.
		Children = new FOctreeNode[8];
		GOctreeBytesUsed += 8 * sizeof(FOctreeNode);

		// Set up children. Calculate child bounding boxes
		//for(INT i=0; i<8; i++)
		//	CalcChildBox(&Children[i], this, i);

		// Now we need to remove each actor from this node and re-check it,
		// in case it needs to move down the Octree.
		TArray<UPrimitiveComponent*> PendingPrimitives = Primitives;
		PendingPrimitives.AddItem(Primitive);

		Primitives.Empty();

		for(INT i=0; i<PendingPrimitives.Num(); i++)
		{
			// Remove this primitives reference to this node.
			PendingPrimitives(i)->OctreeNodes.RemoveItem(this);

			// Then re-check it against this node, which will then check against children.
			if(PendingPrimitives(i)->bWasSNFiltered)
				this->SingleNodeFilter(PendingPrimitives(i), o, Bounds);
			else
				this->MultiNodeFilter(PendingPrimitives(i), o, Bounds);
		}
	}
	// We are just going to add this actor here.
	else
	{
		// Add actor to this nodes list of primitives,
		Primitives.AddItem(Primitive);

		// and add this node to the primitives list of nodes.
		Primitive->OctreeNodes.AddItem(this);
	}

}

/*-----------------------------------------------------------------------------
	Recursive ZERO EXTENT line checker
-----------------------------------------------------------------------------*/

// Given plane crossing points, find first sub-node that plane hits
static inline INT FindFirstNode(FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
							    FLOAT TMX, FLOAT TMY, FLOAT TMZ)
{
	// First, figure out which plane ray hits first.
	INT FirstNode = 0;
	if(T0X > T0Y)
		if(T0X > T0Z)
		{ // T0X is max - Entry Plane is YZ
			if(TMY < T0X) FirstNode |= 2;
			if(TMZ < T0X) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}
	else
		if(T0Y > T0Z)
		{ // T0Y is max - Entry Plane is XZ
			if(TMX < T0Y) FirstNode |= 4;
			if(TMZ < T0Y) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}

	return FirstNode;
}

// Returns the INT whose corresponding FLOAT is smallest.
static inline INT GetNextNode(FLOAT X, INT nX, FLOAT Y, INT nY, FLOAT Z, INT nZ)
{
	if(X<Y)
		if(X<Z)
			return nX;
		else
			return nZ;
	else
		if(Y<Z)
			return nY;
		else
			return nZ;
}

void FOctreeNode::ActorZeroExtentLineCheck(FPrimitiveOctree* o, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FOctreeNodeBounds& Bounds)
{
	// If node if before start of line (ie. exit times are negative)
	if(T1X < 0.0f || T1Y < 0.0f || T1Z < 0.0f)
		return;

	FLOAT MaxHitTime;

	// If we are only looking for the first hit, dont check this node if its beyond the
	// current first hit time.
	if((o->ChkTraceFlags & TRACE_SingleResult) && o->ChkFirstResult )
		MaxHitTime = o->ChkFirstResult->Time; // Check a little beyond current best hit.
	else
		MaxHitTime = 1.0f;

	// If node is beyond end of line (ie. any entry times are > MaxHitTime)
	if(T0X > MaxHitTime || T0Y > MaxHitTime || T0Z > MaxHitTime)
		return;

#if DRAW_LINE_TRACE
	Draw(FColor(255,0,0), 0);
#endif

	// If it does touch this box, first check line against each thing in the node.
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);
		if(TestPrimitive->OctreeTag != o->OctreeTag)
		{
			// Check collision.
			TestPrimitive->OctreeTag = o->OctreeTag;

			if(!TestPrimitive->Owner)
				continue;

			AActor* PrimOwner = TestPrimitive->Owner;

			UBOOL	BlockingPrimitive = TestPrimitive->ShouldCollide() && TestPrimitive->BlockZeroExtent,
				ShadowingPrimitive = TestPrimitive->CastShadow && TestPrimitive->HasStaticShadowing(),
					ShadowingCheck = (o->ChkTraceFlags & TRACE_ShadowCast);

			if( ((!ShadowingCheck && BlockingPrimitive) || (ShadowingCheck && ShadowingPrimitive)) &&
				PrimOwner != o->ChkActor &&
				!o->ChkActor->IsOwnedBy(PrimOwner) &&
				PrimOwner->ShouldTrace(TestPrimitive,o->ChkActor, o->ChkTraceFlags) )
			{
				UBOOL hitActorBox;

				// Check line against actor's bounding box
				//FBox ActorBox = testActor->OctreeBox;
				BEGINCYCLECOUNTER(GOctreeStats.ZE_LineBox_Millisec);
					hitActorBox = LINE_BOX(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
				ENDCYCLECOUNTER;
				GOctreeStats.ZE_LineBox_Count.Value++;

#if !CHECK_FALSE_NEG
				if(!hitActorBox)
					continue;
#endif

				UBOOL lineChkRes;
				FCheckResult Hit(0);
				BEGINCYCLECOUNTER(TestPrimitive->bWasSNFiltered ? GOctreeStats.ZE_SNF_PrimMillisec : GOctreeStats.ZE_MNF_PrimMillisec);
				lineChkRes = TestPrimitive->LineCheck(Hit, 
					o->ChkEnd, 
					o->ChkStart, 
					o->ChkExtent, 
					o->ChkTraceFlags)==0;
				ENDCYCLECOUNTER;

				if(TestPrimitive->bWasSNFiltered)
					GOctreeStats.ZE_SNF_PrimCount.Value++;
				else
					GOctreeStats.ZE_MNF_PrimCount.Value++;

				if( lineChkRes )
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("ZELC False Neg! : %s"), testActor->GetName());
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(Hit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

#if 0
					// DEBUG CHECK - Hit time should never before any of the entry times for this node.
					if(T0X > NewResult->Time || T0Y > NewResult->Time || T0Z > NewResult->Time)
					{
						int sdf = 0;
					}
#endif
					// Keep track of smallest hit time.
					if(!o->ChkFirstResult || NewResult->Time < o->ChkFirstResult->Time)
						o->ChkFirstResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
						return;
				}
			}
		}
	}

	// If we have children - then traverse them, in the order the ray passes through them.
	if(Children)
	{
		// Find middle point of node.
		FLOAT TMX = 0.5f*(T0X+T1X);
		FLOAT TMY = 0.5f*(T0Y+T1Y);
		FLOAT TMZ = 0.5f*(T0Z+T1Z);

		// Fix for parallel-axis case.
		if(o->ParallelAxis)
		{
			if(o->ParallelAxis & 4)
				TMX = (o->RayOrigin.X < Bounds.Center.X) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 2)
				TMY = (o->RayOrigin.Y < Bounds.Center.Y) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 1)
				TMZ = (o->RayOrigin.Z < Bounds.Center.Z) ? MY_FLTMAX : -MY_FLTMAX;
		}

		INT currNode = FindFirstNode(T0X, T0Y, T0Z, TMX, TMY, TMZ);
		do 
		{
			FOctreeNodeBounds	ChildBounds(Bounds,currNode);
			switch(currNode) 
			{
			case 0:
				Children[0 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, T0Z, TMX, TMY, TMZ, ChildBounds);
				currNode = GetNextNode(TMX, 4, TMY, 2, TMZ, 1);
				break;
			case 1:
				Children[1 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, TMZ, TMX, TMY, T1Z, ChildBounds);
				currNode = GetNextNode(TMX, 5, TMY, 3, T1Z, 8);
				break;
			case 2:
				Children[2 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, T0Z, TMX, T1Y, TMZ, ChildBounds);
				currNode = GetNextNode(TMX, 6, T1Y, 8, TMZ, 3);
				break;
			case 3:
				Children[3 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, TMZ, TMX, T1Y, T1Z, ChildBounds);
				currNode = GetNextNode(TMX, 7, T1Y, 8, T1Z, 8);
				break;
			case 4:
				Children[4 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, T0Z, T1X, TMY, TMZ, ChildBounds);
				currNode = GetNextNode(T1X, 8, TMY, 6, TMZ, 5);
				break;
			case 5:
				Children[5 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, TMZ, T1X, TMY, T1Z, ChildBounds);
				currNode = GetNextNode(T1X, 8, TMY, 7, T1Z, 8);
				break;
			case 6:
				Children[6 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, T0Z, T1X, T1Y, TMZ, ChildBounds);
				currNode = GetNextNode(T1X, 8, T1Y, 8, TMZ, 7);
				break;
			case 7:
				Children[7 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, TMZ, T1X, T1Y, T1Z, ChildBounds);
				currNode = 8;
				break;
			}
		} while(currNode < 8);
	}

}

/*-----------------------------------------------------------------------------
	Recursive NON-ZERO EXTENT line checker
-----------------------------------------------------------------------------*/
// This assumes that the ray check overlaps this node.
void FOctreeNode::ActorNonZeroExtentLineCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(i);

		if(TestPrimitive->OctreeTag != o->OctreeTag)
		{
			TestPrimitive->OctreeTag = o->OctreeTag;

			if(!TestPrimitive->Owner)
				continue;

			AActor* PrimOwner = TestPrimitive->Owner;			

			// Check collision.
			if( TestPrimitive->ShouldCollide() &&
				TestPrimitive->BlockNonZeroExtent &&
				PrimOwner != o->ChkActor &&
				!o->ChkActor->IsOwnedBy(PrimOwner) &&
				PrimOwner->ShouldTrace(TestPrimitive, o->ChkActor, o->ChkTraceFlags) )
			{
				// Check line against actor's bounding box
				UBOOL hitActorBox;
				BEGINCYCLECOUNTER(GOctreeStats.NZE_LineBox_Millisec);
					hitActorBox = LINE_BOX(TestPrimitive->Bounds.Origin, 
					TestPrimitive->Bounds.BoxExtent + o->ChkExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
				ENDCYCLECOUNTER;
				GOctreeStats.NZE_LineBox_Count.Value++;

#if !CHECK_FALSE_NEG
				if(!hitActorBox)
					continue;
#endif

				FCheckResult TestHit(0);
				UBOOL lineChkRes;
				BEGINCYCLECOUNTER(TestPrimitive->bWasSNFiltered ? GOctreeStats.NZE_SNF_PrimMillisec : GOctreeStats.NZE_MNF_PrimMillisec);
				lineChkRes = TestPrimitive->LineCheck(TestHit, 
					o->ChkEnd, 
					o->ChkStart, 
					o->ChkExtent, 
					o->ChkTraceFlags)==0;
				ENDCYCLECOUNTER;

				if(TestPrimitive->bWasSNFiltered)
					GOctreeStats.NZE_SNF_PrimCount.Value++;
				else
					GOctreeStats.NZE_MNF_PrimCount.Value++;

				if(lineChkRes)
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("NZELC False Neg! : %s"), testActor->GetName());
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
						return;
				}

			}

		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			UBOOL hitsChild;
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);

			BEGINCYCLECOUNTER(GOctreeStats.NZE_LineBox_Millisec);
			// First - check extent line against child bounding box. 
			// We expand box it by the extent of the line.
			hitsChild = LINE_BOX(ChildBounds.Center, 
				FVector(ChildBounds.Extent + o->ChkExtent.X, ChildBounds.Extent + o->ChkExtent.Y, ChildBounds.Extent + o->ChkExtent.Z),
				o->ChkStart, o->ChkDir, o->ChkOneOverDir);
			ENDCYCLECOUNTER;
			GOctreeStats.NZE_LineBox_Count.Value++;

			// If ray hits child node - go into it.
			if(hitsChild)
			{
				this->Children[childIXs[i]].ActorNonZeroExtentLineCheck(o, ChildBounds);

				// If that child resulted in a hit, and we only want one, return now.
				if ( o->ChkResult && (o->ChkTraceFlags & TRACE_StopAtFirstHit) )
					return;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive encroachment check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorEncroachmentCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// o->ChkActor is the non-cylinder thing that is moving (mover, karma etc.).
	// Actors(i) is the thing (Pawn, Volume, Projector etc.) that its moving into.
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		if(!TestPrimitive->Owner)
			continue;

		AActor* PrimOwner = TestPrimitive->Owner;					

		// Skip if we've already checked this actor, or we're joined to the encroacher,
		// or this is a mover and the other thing is the world (static mesh, terrain etc.)
		if(	TestPrimitive->ShouldCollide() &&
			TestPrimitive->OctreeTag != o->OctreeTag && 
			TestPrimitive->Owner->OverlapTag != o->OctreeTag &&
			!PrimOwner->IsBasedOn(o->ChkActor) &&
			PrimOwner->ShouldTrace(TestPrimitive,o->ChkActor,o->ChkTraceFlags) &&
			!((o->ChkActor->Physics == PHYS_Interpolating) && PrimOwner->bWorldGeometry) )
		{
			TestPrimitive->OctreeTag = o->OctreeTag;
			TestPrimitive->Owner->OverlapTag = o->OctreeTag;

			// Check bounding boxes against each other
			UBOOL hitActorBox;
			BEGINCYCLECOUNTER(GOctreeStats.BoxBox_Millisec);
			hitActorBox = BoxBoxIntersect(TestPrimitive->Bounds.GetBox(), o->ChkBox);
			ENDCYCLECOUNTER;

			GOctreeStats.BoxBox_Count.Value++;

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
				continue;
#endif

			FCheckResult TestHit(1.f);
			if(o->ChkActor->IsOverlapping(TestPrimitive->Owner, &TestHit))
			{
#if CHECK_FALSE_NEG
				if(!hitActorBox)
					debugf(TEXT("ENC False Neg! : %s %s"), o->ChkActor->GetName(), TestPrimitive->Owner->GetName());
#endif

				TestHit.Actor = TestPrimitive->Owner;				

				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorEncroachmentCheck(o, ChildBounds);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive point (with extent) check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorPointCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		if(!TestPrimitive->Owner)
			continue;

		AActor* PrimOwner = TestPrimitive->Owner;					

		// Skip if we've already checked this actor.
		if(	TestPrimitive->ShouldCollide() &&
			TestPrimitive->BlockNonZeroExtent &&
			TestPrimitive->OctreeTag != o->OctreeTag &&
			PrimOwner->ShouldTrace(TestPrimitive,NULL, o->ChkTraceFlags) )
		{
			// Collision test.
			TestPrimitive->OctreeTag = o->OctreeTag;

			UBOOL hitActorBox;
			BEGINCYCLECOUNTER(GOctreeStats.BoxBox_Millisec);
			// Check actor box against query box.
			hitActorBox = BoxBoxIntersect(TestPrimitive->Bounds.GetBox(), o->ChkBox);
			ENDCYCLECOUNTER;
			GOctreeStats.BoxBox_Count.Value++;

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
				continue;
#endif

			FCheckResult TestHit(1.f);
			if( TestPrimitive->PointCheck( TestHit, o->ChkStart, o->ChkExtent )==0 )
			{
				check(TestHit.Actor == PrimOwner);

#if CHECK_FALSE_NEG
				if(!hitActorBox)
					debugf(TEXT("PC False Neg! : %s %s"), testActor->GetName());
#endif

				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
				if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
					return;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorPointCheck(o,ChildBounds);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive radius check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorRadiusCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		if ( !TestPrimitive->Owner )
		{
			continue;
		}

		AActor* PrimOwner = TestPrimitive->Owner;					

		// Skip if we've already checked this actor.
		if( TestPrimitive->OctreeTag != o->OctreeTag)
		{
			TestPrimitive->OctreeTag = o->OctreeTag;

			// FIXME LAURENT
			// this is checking for primitives and returning the owner.
			// need to make sure actors returned are unique in the list
			FCheckResult	*Chk;
			UBOOL			bAbort = false;

			Chk = o->ChkResult;
			while( Chk != NULL )
			{
				if( Chk->Actor == PrimOwner )
				{
					bAbort = true;
					break;
				}
				Chk = Chk->GetNext();
			}

			if( bAbort )
			{
				continue;
			}
			// END FIXME LAURENT

			// Add to result linked list
			if( (TestPrimitive->Bounds.Origin - o->ChkStart).SizeSquared() < o->ChkRadiusSqr )
			{
				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult;
				NewResult->Actor		= PrimOwner;
				NewResult->Component	= TestPrimitive;
				NewResult->GetNext()	= o->ChkResult;
				o->ChkResult			= NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorRadiusCheck(o,ChildBounds);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive box overlap check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorOverlapCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		if(!TestPrimitive->Owner)
			continue;

		AActor* PrimOwner = TestPrimitive->Owner;					

		if(	PrimOwner != o->ChkActor && 
			TestPrimitive->OctreeTag != o->OctreeTag)
		{
			TestPrimitive->OctreeTag = o->OctreeTag;

			// Dont bother if we are only looking for things with BlockRigidBody == true
			if( o->ChkBlockRigidBodyOnly && !TestPrimitive->BlockRigidBody )
				continue;

			UBOOL hitActorBox = BoxBoxIntersect(TestPrimitive->Bounds.GetBox(), o->ChkBox);

			if(hitActorBox)
			{
				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult();
				NewResult->Actor = PrimOwner;
				NewResult->Component = TestPrimitive;
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			this->Children[childIXs[i]].ActorOverlapCheck(o, FOctreeNodeBounds(Bounds,childIXs[i]));
		}
	}

}

/*-----------------------------------------------------------------------------
	Debug drawing function
-----------------------------------------------------------------------------*/


void FOctreeNode::Draw(FPrimitiveRenderInterface* PRI,FColor DrawColor, UBOOL bAndChildren, const FOctreeNodeBounds& Bounds)
{
	// Draw this node
	PRI->DrawWireBox(Bounds.GetBox(),DrawColor);

	// And draw children, if desired.
	if(Children && bAndChildren)
	{
		for(INT i=0; i<8; i++)
		{
			this->Children[i].Draw(PRI, DrawColor, bAndChildren, FOctreeNodeBounds(Bounds,i));
		}
	}
}

///////////////////////////////////////////// TREE ////////////////////////////////////////////////

//
// Constructor
//
FPrimitiveOctree::FPrimitiveOctree(ULevel* InLevel):
	Level(InLevel),
	bShowOctree(0)
{
	GOctreeBytesUsed = sizeof(FPrimitiveOctree);

	OctreeTag = MY_INTMAX/4;

	// Create root node (doesn't use block allocate)
	RootNode = new FOctreeNode;
	GOctreeBytesUsed += sizeof(FOctreeNode);
}

#define DISPLAY_STAT(name) {if(name##_Count > 0) debugf(TEXT("  ") TEXT(#name) TEXT(" : %f (%d) Average: %f"), name##_Millisec, name##_Count, name##_Millisec/(FLOAT)name##_Count);}

//
// Destructor
//
FPrimitiveOctree::~FPrimitiveOctree()
{
	debugf(TEXT(" Mem Used: %d bytes"), GOctreeBytesUsed);
	debugf(TEXT("    "));

	RootNode->RemoveAllPrimitives(this);

	delete RootNode;
}

//
//	Tick
//
void FPrimitiveOctree::Tick()
{
	// Draw entire Octree.
	if(bShowOctree)
		RootNode->Draw(Level->LineBatcher,FColor(0,255,255),1,RootNodeBounds);
}

//
//	AddPrimitive
//
void FPrimitiveOctree::AddPrimitive(UPrimitiveComponent* Primitive)
{
	GOctreeStats.Add_Count.Value++;
	BEGINCYCLECOUNTER(GOctreeStats.Add_Millisec);

	//debugf(TEXT("Add: %x : %s CollideActors: %d"), Primitive, Primitive->GetName(), Primitive->CollideActors);

	// Just to be sure - if the actor is already in the octree, remove it and re-add it.
	if(Primitive->OctreeNodes.Num() > 0)
	{
		if(!GIsEditor)
			debugf(TEXT("Octree Warning (AddActor): %s Already In Octree."), Primitive->GetName());
		RemovePrimitive(Primitive);
	}

	// Check if this actor is within world limits!
	const FBox&	PrimitiveBox = Primitive->Bounds.GetBox();
	if(	PrimitiveBox.Max.X < -HALF_WORLD_MAX || PrimitiveBox.Min.X > HALF_WORLD_MAX ||
		PrimitiveBox.Max.Y < -HALF_WORLD_MAX || PrimitiveBox.Min.Y > HALF_WORLD_MAX ||
		PrimitiveBox.Max.Z < -HALF_WORLD_MAX || PrimitiveBox.Min.Z > HALF_WORLD_MAX)
	{
		debugf(TEXT("Octree Warning (AddPrimitive): %s (Owner: %s) Outside World."), Primitive->GetName(), Primitive->Owner ? Primitive->Owner->GetName() : TEXT("None"));
		return;
	}

	AActor* PrimOwner = Primitive->Owner;

	// If we have not yet started play, filter things into many leaves,
	// this is slower to add, but is more accurate.
	if(PrimOwner && PrimOwner->GetLevel() && !PrimOwner->GetLevel()->GetLevelInfo()->bBegunPlay)
	{
		Primitive->bWasSNFiltered = 0;
		RootNode->MultiNodeFilter(Primitive, this, RootNodeBounds);
	}
	else
	{
		Primitive->bWasSNFiltered = 1;
		RootNode->SingleNodeFilter(Primitive, this, RootNodeBounds);
	}

	ENDCYCLECOUNTER;
}

//
//	RemovePrimitive
//
void FPrimitiveOctree::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	GOctreeStats.Remove_Count.Value++;
	BEGINCYCLECOUNTER(GOctreeStats.Remove_Millisec);

#if 0
	if(!GIsEditor && Actor->OctreeNodes.Num() == 0)
	{
		debugf(TEXT("Octree Warning (RemoveActor): %s Not In Octree."), Actor->GetName() );
		return;
	}
#endif

	//debugf(TEXT("Remove: %p : %s"), Actor, Actor->GetName());

	// Work through this actors list of nodes, removing itself from each one.
	for(INT i=0; i<Primitive->OctreeNodes.Num(); i++)
	{
		FOctreeNode* node = Primitive->OctreeNodes(i);
		check(node);
		node->Primitives.RemoveItem(Primitive);
	}
	// Then empty the list of nodes.
	Primitive->OctreeNodes.Empty();

	ENDCYCLECOUNTER;
}

static FLOAT ToInfinity(FLOAT f)
{
	if(f > 0)
		return MY_FLTMAX;
	else
		return -MY_FLTMAX;
}

//
//	ActorLineCheck
//
FCheckResult* FPrimitiveOctree::ActorLineCheck(FMemStack& Mem, 
											   const FVector& End, 
											   const FVector& Start, 
											   const FVector& Extent, 
											   DWORD TraceFlags,
											   AActor *SourceActor)
{
	if(Extent.IsZero())
		GOctreeStats.ZELineCheck_Count.Value++;
	else
		GOctreeStats.NZELineCheck_Count.Value++;
	BEGINCYCLECOUNTER(Extent.IsZero() ? GOctreeStats.ZELineCheck_Millisec : GOctreeStats.NZELineCheck_Millisec);

	// Fill in temporary data.
	OctreeTag++;
	//debugf(TEXT("LINE CHECK: CT: %d"), OctreeTag);

	ChkResult = NULL;

	ChkMem = &Mem;
	ChkEnd = End;
	ChkStart = Start;
	ChkExtent = Extent;
	ChkTraceFlags = TraceFlags;
	ChkActor = SourceActor;

	ChkDir = (End-Start);
	ChkOneOverDir = FVector(1.0f/ChkDir.X, 1.0f/ChkDir.Y, 1.0f/ChkDir.Z);

	ChkFirstResult = NULL;

	// This will recurse down, adding results to ChkResult as it finds them.
	// Taken from the Revelles/Urena/Lastra paper: http://wscg.zcu.cz/wscg2000/Papers_2000/X31.pdf
	if(Extent.IsZero())
	{		
		FVector RayDir = ChkDir;
		RayOrigin = ChkStart;
		NodeTransform = 0;
		ParallelAxis = 0;

		if(RayDir.X < 0.0f)
		{
			RayOrigin.X = -RayOrigin.X;
			RayDir.X = -RayDir.X;
			NodeTransform |= 4;
		}

		if(RayDir.Y < 0.0f)
		{
			RayOrigin.Y = -RayOrigin.Y;
			RayDir.Y = -RayDir.Y;
			NodeTransform |= 2;
		}

		if(RayDir.Z < 0.0f)
		{
			RayOrigin.Z = -RayOrigin.Z;
			RayDir.Z = -RayDir.Z;
			NodeTransform |= 1;
		}

		// T's should be between 0 and 1 for a hit on the tested ray.
		FVector T0, T1;


		// Check for parallel cases.
		// X //
		if(RayDir.X > 0.0f)
		{
			T0.X = (RootNodeBounds.Center.X - RootNodeBounds.Extent - RayOrigin.X)/RayDir.X;
			T1.X = (RootNodeBounds.Center.X + RootNodeBounds.Extent - RayOrigin.X)/RayDir.X;
		}
		else
		{
			T0.X = ToInfinity(RootNodeBounds.Center.X - RootNodeBounds.Extent - RayOrigin.X);
			T1.X = ToInfinity(RootNodeBounds.Center.X + RootNodeBounds.Extent - RayOrigin.X);
			ParallelAxis |= 4;
		}

		// Y //
		if(RayDir.Y > 0.0f)
		{
			T0.Y = (RootNodeBounds.Center.Y - RootNodeBounds.Extent - RayOrigin.Y)/RayDir.Y;
			T1.Y = (RootNodeBounds.Center.Y + RootNodeBounds.Extent - RayOrigin.Y)/RayDir.Y;
		}
		else
		{
			T0.Y = ToInfinity(RootNodeBounds.Center.Y - RootNodeBounds.Extent - RayOrigin.Y);
			T1.Y = ToInfinity(RootNodeBounds.Center.Y + RootNodeBounds.Extent - RayOrigin.Y);
			ParallelAxis |= 2;
		}

		// Z //
		if(RayDir.Z > 0.0f)
		{
			T0.Z = (RootNodeBounds.Center.X - RootNodeBounds.Extent - RayOrigin.Z)/RayDir.Z;
			T1.Z = (RootNodeBounds.Center.Z + RootNodeBounds.Extent - RayOrigin.Z)/RayDir.Z;
		}
		else
		{
			T0.Z = ToInfinity(RootNodeBounds.Center.Z - RootNodeBounds.Extent - RayOrigin.Z);
			T1.Z = ToInfinity(RootNodeBounds.Center.Z + RootNodeBounds.Extent - RayOrigin.Z);
			ParallelAxis |= 1;
		}

		// Only traverse if ray hits RootNode box.
		if(T0.GetMax() < T1.GetMax())
		{
			RootNode->ActorZeroExtentLineCheck(this, T0.X, T0.Y, T0.Z, T1.X, T1.Y, T1.Z, RootNodeBounds);
		}

		// Only return one (first) result if TRACE_SingleResult set.
		if(TraceFlags & TRACE_SingleResult)
		{
			ChkResult = ChkFirstResult;
			if(ChkResult)
				ChkResult->GetNext() = NULL;
		}
	}
	else
	{
		// Create box around fat ray check.
		ChkBox = FBox(0);
		ChkBox += Start;
		ChkBox += End;
		ChkBox.Min -= Extent;
		ChkBox.Max += Extent;

		// Then recurse through Octree
		RootNode->ActorNonZeroExtentLineCheck(this, RootNodeBounds);
	}

	// If TRACE_SingleResult, only return 1 result (the first hit).
	// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace)
	if(ChkResult && TraceFlags & TRACE_SingleResult)
	{
		return FindFirstResult(ChkResult, TraceFlags);
	}

	return ChkResult;

	ENDCYCLECOUNTER;	
}

//
//	ActorPointCheck
//
FCheckResult* FPrimitiveOctree::ActorPointCheck(FMemStack& Mem, 
												const FVector& Location, 
												const FVector& Extent, 
												DWORD TraceFlags, 
												UBOOL bSingleResult)
{
	GOctreeStats.PointCheck_Count.Value++;
	BEGINCYCLECOUNTER(GOctreeStats.PointCheck_Millisec);

	// Fill in temporary data.
	OctreeTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkStart = Location;
	ChkExtent = Extent;
	ChkTraceFlags = TraceFlags;
	if(bSingleResult)
		ChkTraceFlags |= TRACE_StopAtFirstHit;
	ChkBox = FBox(ChkStart - ChkExtent, ChkStart + ChkExtent);

	RootNode->ActorPointCheck(this, RootNodeBounds);

	return ChkResult;

	ENDCYCLECOUNTER;
}

//
//	ActorRadiusCheck
//
FCheckResult* FPrimitiveOctree::ActorRadiusCheck(FMemStack& Mem, 
												 const FVector& Location, 
												 FLOAT Radius)
{
	GOctreeStats.RadiusCheck_Count.Value++;
	BEGINCYCLECOUNTER(GOctreeStats.RadiusCheck_Millisec);

	// Fill in temporary data.
	OctreeTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkStart = Location;
	ChkRadiusSqr = Radius * Radius;
	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));

	RootNode->ActorRadiusCheck(this, RootNodeBounds);

	return ChkResult;

	ENDCYCLECOUNTER;
}

//
//	ActorEncroachmentCheck
//
FCheckResult* FPrimitiveOctree::ActorEncroachmentCheck(FMemStack& Mem, 
													   AActor* Actor, 
													   FVector Location, 
													   FRotator Rotation, 
													   DWORD TraceFlags)
{
	GOctreeStats.EncroachCheck_Count.Value++;
	BEGINCYCLECOUNTER(GOctreeStats.EncroachCheck_Millisec);

	// Fill in temporary data.
	ChkResult = NULL;

	if(!Actor->CollisionComponent)
		return ChkResult;

	OctreeTag++;

	ChkMem = &Mem;
	ChkActor = Actor;
	ChkTraceFlags = TraceFlags;

	// Save actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	// Get collision component bounding box at new location.
	if(Actor->CollisionComponent->IsValidComponent())
	{
		check(Actor->CollisionComponent->Initialized);
		ChkBox = Actor->CollisionComponent->Bounds.GetBox();

		if(ChkBox.IsValid)
			RootNode->ActorEncroachmentCheck(this, RootNodeBounds);
	}

	// Restore actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	return ChkResult;

	ENDCYCLECOUNTER;
}

//
//	ActorOverlapCheck
//
FCheckResult* FPrimitiveOctree::ActorOverlapCheck(FMemStack& Mem, 
												  AActor* Actor, 
												  const FBox& Box,
												  UBOOL bBlockRigidBodyOnly)
{
	OctreeTag++;
	ChkResult	= NULL;

	ChkBox		= Box;
	ChkActor	= Actor;
	ChkMem		= &Mem;
	ChkBlockRigidBodyOnly = bBlockRigidBodyOnly;

	if(ChkBox.IsValid)
		RootNode->ActorOverlapCheck(this, RootNodeBounds);

	return ChkResult;

}

//
//	FPrimitiveOctree::GetVisiblePrimitives - Returns the primitives contained by the given bounding volume.
//

void FPrimitiveOctree::GetVisiblePrimitives(const FLevelVisibilitySet& VisibilitySet,TArray<UPrimitiveComponent*>& Primitives)
{
	OctreeTag++;
	RootNode->GetVisiblePrimitives(VisibilitySet,VisibilitySet.GetFullSubset(),Primitives,this,RootNodeBounds);
}

//
//	FPrimitiveOctree::GetIntersectingPrimitives - Returns the primitives contained by the given bounding box.
//

void FPrimitiveOctree::GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives)
{
	OctreeTag++;
	RootNode->GetIntersectingPrimitives(Box,Primitives,this,RootNodeBounds);
}

//
//	FPrimitiveOctree::Exec
//

UBOOL FPrimitiveOctree::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(ParseCommand(&Cmd,TEXT("SHOWOCTREE")))
	{
		bShowOctree = !bShowOctree;
		return 1;
	}
	else
		return 0;
}

//
//	GNewCollisionHash
//

FPrimitiveHashBase* GNewCollisionHash(ULevel* InLevel)
{
	return new FPrimitiveOctree(InLevel);

}