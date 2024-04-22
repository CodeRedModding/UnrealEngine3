/*=============================================================================
	UnPrim.h: Definition of FCheckResult used by collision tests.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	FCheckResult.
-----------------------------------------------------------------------------*/

//
// Results of an actor check.
//
struct FIteratorActorList : public FIteratorList
{
	// Variables.
	AActor* Actor;

	// Functions.
	FIteratorActorList()
	:	Actor	(NULL)
	{}


	FIteratorActorList( FIteratorActorList* InNext, AActor* InActor )
	:	FIteratorList	(InNext)
	,	Actor			(InActor)
	{}


	FIteratorActorList* GetNext() const
	{ 
		return (FIteratorActorList*) Next;
	}
};

//
// Results from a collision check.
//
struct FCheckResult : public FIteratorActorList
{
	// Variables.
	FVector						Location;	// Location of the hit in coordinate system of the returner.
	FVector						Normal;		// Normal vector in coordinate system of the returner. Zero=none.
	FLOAT						Time;		// Time until hit, if line check.
	INT							Item;		// Primitive data item which was hit, INDEX_NONE=none.
	UMaterialInterface*			Material;	// Material of the item which was hit.
	class UPhysicalMaterial*	PhysMaterial; // Physical material that was hit
	class UPrimitiveComponent*	Component;	// PrimitiveComponent that the check hit.
	class UPrimitiveComponent*  SourceComponent;// PrimitiveComponent of testing actor which collided with 'Component' (above)
	FName						BoneName;	// Name of bone we hit (for skeletal meshes).
	class ULevel*				Level;		// Level that was hit in case of BSPLineCheck
	INT							LevelIndex; // Index of the level that was hit in the case of BSP checks.

	/** This line check started penetrating the primitive. */
	UBOOL						bStartPenetrating;

	// Functions.
	FCheckResult()
	: Location	(0,0,0)
	, Normal	(0,0,0)
	, Time		(0.0f)
	, Item		(INDEX_NONE)
	, Material	(NULL)
	, PhysMaterial( NULL)
	, Component	(NULL)
	, SourceComponent (NULL)
	, BoneName	(NAME_None)
	, Level		(NULL)
	, LevelIndex	(INDEX_NONE)
	, bStartPenetrating	(FALSE)
	{}


	FCheckResult( FLOAT InTime, FCheckResult* InNext=NULL )
	:	FIteratorActorList( InNext, NULL )
	,	Location	(0,0,0)
	,	Normal		(0,0,0)
	,	Time		(InTime)
	,	Item		(INDEX_NONE)
	,	Material	(NULL)
	,   PhysMaterial( NULL)
	,	Component	(NULL)
	,	SourceComponent (NULL)
	,	BoneName	(NAME_None)
	,	Level		(NULL)
	,	LevelIndex	(INDEX_NONE)
	,	bStartPenetrating	(FALSE)
	{}


	FCheckResult*& GetNext() const
	{ 
		return *(FCheckResult**)&Next; 
	}


	static QSORT_RETURN CDECL CompareHits( const FCheckResult* A, const FCheckResult* B )
	{
		if (A->Time < B->Time)
		{
			return -1;
		}
		else if (A->Time > B->Time)
		{
			return 1;
		}
		// consider hits that started penetrating as infinitesimally smaller
		else if (A->bStartPenetrating && !B->bStartPenetrating)
		{
			return -1;
		}
		else if (!A->bStartPenetrating && B->bStartPenetrating)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
};

//
// Old actor positions (saved for reverting attached actor movement
//
struct FSavedPosition : public FIteratorActorList
{
	// Variables.
	FVector						OldLocation;	// Original location
	FRotator					OldRotation;	// Original rotation

	FSavedPosition()
	:	OldLocation	(0,0,0)
	,	OldRotation	(0,0,0)
	{}

	FSavedPosition(AActor* InActor, const FVector& InLocation, const FRotator& InRotation, FSavedPosition* InNext)
		: FIteratorActorList(InNext, InActor), OldLocation(InLocation), OldRotation(InRotation)
	{}

	FSavedPosition*& GetNext() const
	{ 
		return *(FSavedPosition**)&Next; 
	}
};

