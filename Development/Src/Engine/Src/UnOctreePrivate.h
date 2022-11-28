/*=============================================================================
	UnOctree.h: Octree implementation header
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

//
//	FOctreeNodeBounds - An octree node bounding cube.
//

struct FOctreeNodeBounds
{
	FVector	Center;
	FLOAT	Extent;

	FOctreeNodeBounds(const FVector& InCenter,FLOAT InExtent):
		Center(InCenter),
		Extent(InExtent)
	{}

	// Given a child index and the parent's bounding cube, construct the child's bounding cube.
	FOctreeNodeBounds(const FOctreeNodeBounds& InParentCube,INT InChildIndex);

	// GetBox - Returns a FBox representing the cube.
	FORCEINLINE FBox GetBox() const
	{
		return FBox(Center - FVector(Extent,Extent,Extent),Center + FVector(Extent,Extent,Extent));
	}

	// IsInsideBox - Returns whether this cube is inside the given box.
	FORCEINLINE UBOOL IsInsideBox(const FBox& InBox) const
	{
		if(Center.X - Extent < InBox.Min.X || Center.X + Extent > InBox.Max.X)
			return 0;
		else if(Center.Y - Extent < InBox.Min.Y || Center.Y + Extent > InBox.Max.Y)
			return 0;
		else if(Center.Z - Extent < InBox.Min.Z || Center.Z + Extent > InBox.Max.Z )
			return 0;
		else
			return 1;
	}
};

class FOctreeNode
{
public:
	TArray<UPrimitiveComponent*>	Primitives;	// Primitives held at this node.
	class FOctreeNode*				Children;	// Child nodes. If NULL, this is a leaf. Otherwise, always size 8.

	FOctreeNode();
	~FOctreeNode();

	void ActorNonZeroExtentLineCheck(class FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorZeroExtentLineCheck(class FPrimitiveOctree* octree, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FOctreeNodeBounds& Bounds);
	void ActorEncroachmentCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorPointCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorRadiusCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorOverlapCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);

	void SingleNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
	void MultiNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
	void RemoveAllPrimitives(FPrimitiveOctree* o);

	void GetPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree);

	void GetVisiblePrimitives(
		const FLevelVisibilitySet& VisibilitySet,
		const FLevelVisibilitySet::FSubset& NodeSubset,
		TArray<UPrimitiveComponent*>& OutPrimitives,
		FPrimitiveOctree* Octree,
		const FOctreeNodeBounds& Bounds
		);
	void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds);

	void Draw(FPrimitiveRenderInterface* PRI,FColor DrawColor, UBOOL bAndChildren, const FOctreeNodeBounds& Bounds);

	void FilterTest(const FBox& TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FOctreeNodeBounds& Bounds);

private:
	void StoreActor(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
};

class FPrimitiveOctree : public FPrimitiveHashBase
{
public:
	ULevel*			Level;

	// Root node - assumed to have size WORLD_MAX
	FOctreeNode*	RootNode;
	INT				OctreeTag;

	/// This is a bit nasty...
	// Temporary storage while recursing for line checks etc.
	FCheckResult*	ChkResult;
	FMemStack*		ChkMem;
	FVector			ChkEnd;
	FVector			ChkStart; // aka Location
	FRotator		ChkRotation;
	FVector			ChkDir;
	FVector			ChkOneOverDir;
	FVector			ChkExtent;
	DWORD			ChkTraceFlags;
	AActor*			ChkActor;
	FLOAT			ChkRadiusSqr;
	FBox			ChkBox;
	UBOOL		    ChkBlockRigidBodyOnly;
	/// 

	// Keeps track of shortest hit time so far.
	FCheckResult*	ChkFirstResult;

	FVector			RayOrigin;
	INT				ParallelAxis;
	INT				NodeTransform;

	UBOOL			bShowOctree;

	// FPrimitiveHashBase Interface
	FPrimitiveOctree(ULevel* InLevel);
	virtual ~FPrimitiveOctree();

	virtual void Tick();
	virtual void AddPrimitive(UPrimitiveComponent* Primitive);
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive);
	virtual FCheckResult* ActorLineCheck(FMemStack& Mem, 
		const FVector& End, 
		const FVector& Start, 
		const FVector& Extent, 
		DWORD TraceFlags, 
		AActor *SourceActor);
	virtual FCheckResult* ActorPointCheck(FMemStack& Mem, 
		const FVector& Location, 
		const FVector& Extent, 
		DWORD TraceFlags, 
		UBOOL bSingleResult=0);
	virtual FCheckResult* ActorRadiusCheck(FMemStack& Mem, 
		const FVector& Location, 
		FLOAT Radius);
	virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, 
		AActor* Actor, 
		FVector Location, 
		FRotator Rotation, 
		DWORD TraceFlags);
	virtual FCheckResult* ActorOverlapCheck( FMemStack& Mem, 
		AActor* Actor,
		const FBox& Box, 
		UBOOL bBlockRigidBodyOnly);

	virtual void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives);
	virtual void GetVisiblePrimitives(const FLevelVisibilitySet& VisibilitySet,TArray<UPrimitiveComponent*>& Primitives);

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
};

FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags);

struct FOctreeStatGroup : FStatGroup
{
	FCycleCounter	ZE_SNF_PrimMillisec, 
					ZE_MNF_PrimMillisec, 
					NZE_SNF_PrimMillisec, 
					NZE_MNF_PrimMillisec,
					BoxBox_Millisec,
					ZE_LineBox_Millisec,
					NZE_LineBox_Millisec,
					Add_Millisec,
					Remove_Millisec,
					NZELineCheck_Millisec,
					ZELineCheck_Millisec,
					PointCheck_Millisec,
					EncroachCheck_Millisec,
					RadiusCheck_Millisec;

	FStatCounter	ZE_SNF_PrimCount, 
					ZE_MNF_PrimCount, 
					NZE_SNF_PrimCount, 
					NZE_MNF_PrimCount,
					BoxBox_Count,
					ZE_LineBox_Count,
					NZE_LineBox_Count,
					Add_Count,
					Remove_Count,
					NZELineCheck_Count,
					ZELineCheck_Count,
					PointCheck_Count,
					EncroachCheck_Count,
					RadiusCheck_Count;

	FOctreeStatGroup():
		FStatGroup(TEXT("Octree")),
		ZE_SNF_PrimMillisec(this,TEXT("Zero-extent SingleNodeFilter time")),
		ZE_MNF_PrimMillisec(this,TEXT("Zero-extent MultiNodeFilter time")),
		NZE_SNF_PrimMillisec(this,TEXT("Non-zero-extent SingleNodeFilter time")),
		NZE_MNF_PrimMillisec(this,TEXT("Non-zero-extent MultiNodeFilter time")),
		BoxBox_Millisec(this,TEXT("Box-box time")),
		ZE_LineBox_Millisec(this,TEXT("Zero-extent line-box time")),
		NZE_LineBox_Millisec(this,TEXT("Non-zero-extent line-box time")),
		Add_Millisec(this,TEXT("Add time")),
		Remove_Millisec(this,TEXT("Remove time")),
		NZELineCheck_Millisec(this,TEXT("Non-zero-extent line check time")),
		ZELineCheck_Millisec(this,TEXT("Zero-extent line check time")),
		PointCheck_Millisec(this,TEXT("Point check time")),
		EncroachCheck_Millisec(this,TEXT("Encroach check time")),
		RadiusCheck_Millisec(this,TEXT("Radius check time")),
		ZE_SNF_PrimCount(this,TEXT("Zero-extent SingleNodeFilter primitives")),
		ZE_MNF_PrimCount(this,TEXT("Zero-extent MultiNodeFilter primitives")),
		NZE_SNF_PrimCount(this,TEXT("Non-zero-extent SingleNodeFilter primitives")),
		NZE_MNF_PrimCount(this,TEXT("Non-zero-extent MultiNodeFilter primitives")),
		BoxBox_Count(this,TEXT("Box-box checks")),
		ZE_LineBox_Count(this,TEXT("Zero-extent line box checks")),
		NZE_LineBox_Count(this,TEXT("Non-zero-extent line box checks")),
		Add_Count(this,TEXT("Adds")),
		Remove_Count(this,TEXT("Removes")),
		NZELineCheck_Count(this,TEXT("Non-zero-extent line checks")),
		ZELineCheck_Count(this,TEXT("Zero-extent line checks")),
		PointCheck_Count(this,TEXT("Point checks")),
		EncroachCheck_Count(this,TEXT("Encroach checks")),
		RadiusCheck_Count(this,TEXT("Radius checks"))
	{}
};
extern FOctreeStatGroup GOctreeStats;