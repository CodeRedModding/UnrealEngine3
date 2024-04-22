/*=============================================================================
	UnAnimTree.h:	Animation tree element definitions and helper structs.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNANIMTREE_H__
#define __UNANIMTREE_H__

#if _DEBUG
#define DEBUG_QUATERNION 0 // Enable comparison of QST transform to Matrix result
#else
#define DEBUG_QUATERNION 0
#endif
/**
* Engine stats
*/
enum EAnimStats
{
	/** Skeletal stats */
	STAT_AnimBlendTime = STAT_AnimFirstStat,
	STAT_SkelComposeTime,
	STAT_UpdateSkelPose,
	STAT_AnimTickTime,
	STAT_AnimSyncGroupTime,
	STAT_SkelControlTickTime,
	STAT_SkelComponentTickTime,
	STAT_GetAnimationPose,
	STAT_MirrorBoneAtoms,
	STAT_UpdateFaceFX,
	STAT_SkelControl,
	STAT_UpdateChildComponents,
	STAT_BlendInPhysics,
	STAT_MeshObjectUpdate,
	STAT_SkelCompUpdateTransform,
	STAT_UpdateRBBones,
	STAT_UpdateSkelMeshBounds,
	STAT_UpdateFloorConform,
	STAT_GetFloorConformNormal,
	STAT_SkelCtrl_FootPlacement,
	STAT_SkelComponentTickCount,
	STAT_SkelComponentTickNodesCount,
	STAT_SkelComponentTickGBACount,
};

/** Controls compilation of per-node GetBoneAtom stats. */
// LOOKING_FOR_PERF_ISSUES
#define PERF_ENABLE_GETBONEATOM_STATS 0
#define PERF_ENABLE_INITANIM_STATS 0
#define PERF_SHOW_COPYANIMTREE_TIMES 0


#if PERF_ENABLE_GETBONEATOM_STATS || PERF_ENABLE_INITANIM_STATS || PERF_SHOW_COPYANIMTREE_TIMES

/** One timing for calling GetBoneAtoms on an AnimNode. */
struct FAnimNodeTimeStat
{
	FName	NodeName;
	DOUBLE	NodeExclusiveTime;
	INT		Count;

	FAnimNodeTimeStat(FName InNodeName, DOUBLE InExcTime, INT InCount):
		NodeName(InNodeName),
		NodeExclusiveTime(InExcTime),
		Count(InCount)
	{}
};

/** Used for timing how long GetBoneAtoms takes, excluding time taken by children nodes. */
class FScopedBlendTimer
{
public:
	const DOUBLE StartTime;
	DOUBLE TotalChildTime;
	FName NodeName;
	TArray<FAnimNodeTimeStat>* StatArray;
	TMap<FName, INT>* NameToIndexLUT;

	FScopedBlendTimer(FName InNodeName, TArray<FAnimNodeTimeStat>* InStatArray, TMap<FName, INT>* InNameToIndexLUT):
		StartTime( appSeconds() ),
		TotalChildTime(0.0),
		NodeName(InNodeName),
		StatArray(InStatArray),
		NameToIndexLUT(InNameToIndexLUT)
	{}

	~FScopedBlendTimer()
	{
		DOUBLE TimeTaken = appSeconds() - StartTime;
		TimeTaken -= TotalChildTime;
		INT* Index = NameToIndexLUT->Find(NodeName);
		if( Index && (*Index) < StatArray->Num() )
		{
			(*StatArray)(*Index).NodeExclusiveTime += TimeTaken;
			(*StatArray)(*Index).Count++;
		}
		else
		{
			INT NewIndex = StatArray->AddItem( FAnimNodeTimeStat(NodeName, TimeTaken, 1) );
			NameToIndexLUT->Set(NodeName, NewIndex);
		}
	}
};

/** Used for timing how long a child takes to return its BoneAtoms. */
class FScopedChildTimer
{
public:
	const DOUBLE StartTime;
	FScopedBlendTimer* Timer;

	FScopedChildTimer(FScopedBlendTimer* InTimer):
		StartTime( appSeconds() ),
		Timer(InTimer)
	{}

	~FScopedChildTimer()
	{
		DOUBLE ChildTime = appSeconds() - StartTime;
		Timer->TotalChildTime += ChildTime;
	}
};
#endif // PERF_ENABLE_GETBONEATOM_STATS || PERF_ENABLE_INITANIM_STATS

#if PERF_ENABLE_GETBONEATOM_STATS

/** Array of timings for different AnimNodes within the tree. */
extern TArray<FAnimNodeTimeStat> BoneAtomBlendStats;
extern TMap<FName, INT> BoneAtomBlendStatsTMap;

#define START_GETBONEATOM_TIMER	FScopedBlendTimer BoneAtomTimer( GetClass()->GetFName(), &BoneAtomBlendStats, &BoneAtomBlendStatsTMap );
#define EXCLUDE_CHILD_TIME		FScopedChildTimer ChildTimer( &BoneAtomTimer );

#else // PERF_ENABLE_GETBONEATOM_STATS

#define START_GETBONEATOM_TIMER
#define EXCLUDE_CHILD_TIME

#endif // PERF_ENABLE_GETBONEATOM_STATS


#if PERF_ENABLE_INITANIM_STATS

/** Array of timings for different AnimNodes within the tree. */
extern TArray<FAnimNodeTimeStat> InitAnimStats;
extern TMap<FName, INT> InitAnimStatsTMap;

#define INITANIM_CUSTOM(SectionName)	FScopedBlendTimer InitAnimTimer( SectionName, &InitAnimStats, &InitAnimStatsTMap );
#define START_INITANIM_TIMER			FScopedBlendTimer InitAnimTimer( GetClass()->GetFName(), &InitAnimStats, &InitAnimStatsTMap );
#define EXCLUDE_PARENT_TIME				FScopedChildTimer ParentTimer( &InitAnimTimer );

#else // PERF_ENABLE_INITANIM_STATS

#define INITANIM_CUSTOM(SectionName)
#define START_INITANIM_TIMER
#define EXCLUDE_PARENT_TIME

#endif // PERF_ENABLE_INITANIM_STATS

#if PERF_SHOW_COPYANIMTREE_TIMES

/** Array of timings for different AnimNodes within the tree. */
extern TArray<FAnimNodeTimeStat> PostAnimNodeInstanceStats;
extern TMap<FName, INT> PostAnimNodeInstanceStatsTMap;

#define POSTANIMNODEINSTANCE_CUSTOM(SectionName)	FScopedBlendTimer PostAnimNodeInstanceTimer( SectionName, &PostAnimNodeInstanceStats, &PostAnimNodeInstanceStatsTMap );
#define START_POSTANIMNODEINSTANCE_TIMER			FScopedBlendTimer PostAnimNodeInstanceTimer( GetClass()->GetFName(), &PostAnimNodeInstanceStats, &PostAnimNodeInstanceStatsTMap );
#define EXCLUDE_POSTANIMNODEINSTANCE_PARENT_TIME	FScopedChildTimer PostAnimNodeInstanceParentTimer( &PostAnimNodeInstanceTimer );

#else // PERF_ENABLE_INITANIM_STATS

#define POSTANIMNODEINSTANCE_CUSTOM(SectionName)
#define START_POSTANIMNODEINSTANCE_TIMER
#define EXCLUDE_POSTANIMNODEINSTANCE_PARENT_TIME

#endif // PERF_ENABLE_INITANIM_STATS




/** Curve Key
	@CurveName	: Morph Target name to blend
	@Weight		: Weight of the Morph Target
**/
struct FCurveKey
{
	FName	CurveName;
	FLOAT	Weight;

	FCurveKey()
		:CurveName(NAME_None), Weight(0.0f) {}
	FCurveKey(FName InCurveName, FLOAT InWeight)
		:CurveName(InCurveName), Weight(InWeight){}
};

/** Array of FQuat */
typedef TArray< FQuat,TMemStackAllocator<GMainThreadMemStack> >				FQuatArray;
/** Array of FQuat Arrays */
typedef TArray< FQuatArray,TMemStackAllocator<GMainThreadMemStack> >		FArrayQuatArray;
/** Array of FBoneAtoms */
typedef TArray< FBoneAtom,TMemStackAllocator<GMainThreadMemStack> >			FBoneAtomArray;
/** Array of FBoneAtoms Arrays */
typedef TArray< FBoneAtomArray,TMemStackAllocator<GMainThreadMemStack> >	FArrayBoneAtomArray;
/** Array of FMatrix using Memory Stack allocator */
typedef TArray< FMatrix,TMemStackAllocator<GMainThreadMemStack> >			FMatrixArray;
/** Array of Arrays of FMatrix using Memory Stack allocator */
typedef TArray< FMatrixArray,TMemStackAllocator<GMainThreadMemStack> >		FArrayMatrixArray;
/** Array of FBoneAtoms */
typedef TArray< FCurveKey ,TMemStackAllocator<GMainThreadMemStack> >		FCurveKeyArray;
/** Array of FBoneAtoms Arrays */
typedef TArray< FCurveKeyArray,TMemStackAllocator<GMainThreadMemStack> >	FArrayCurveKeyArray;

template <> struct TIsPODType<FBoneAtom> { enum { Value = true }; };

inline FLOAT UnWindNormalizedAimAngle(FLOAT Angle)
{
	appFmod(Angle, 4.f);
	
	if (Angle > 2.f)
	{
		Angle -= 4.f;
	}
	else if (Angle < -2.f)
	{
		Angle += 4.f;
	}

	return Angle;
}

#if DEBUG_QUATERNION

// You can use this function to compare the result of BoneAtom vs Matrix for your code
UBOOL DebugCompare(const FBoneAtom & BoneAtom, const FMatrix & InMatrix);

#endif

#endif // __UNANIMTREE_H__
