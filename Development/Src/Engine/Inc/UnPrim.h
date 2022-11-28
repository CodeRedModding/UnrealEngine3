/*=============================================================================
	UnPrim.h: Definition of FCheckResult used by collision tests.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
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
	FIteratorActorList* GetNext()
	{ return (FIteratorActorList*) Next; }
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
	UMaterialInstance*			Material;	// Material of the item which was hit.
	class UPrimitiveComponent*	Component;	// PrimitiveComponent that the check hit.
	FName						BoneName;	// Name of bone we hit (for skeletal meshes).

	// Functions.
	FCheckResult()
	:	Material	(NULL)
	,	Item		(INDEX_NONE)
	,	Component	(NULL)
	,	BoneName	(NAME_None)
	{}
	FCheckResult( FLOAT InTime, FCheckResult* InNext=NULL )
	:	FIteratorActorList( InNext, NULL )
	,	Location	(0,0,0)
	,	Normal		(0,0,0)
	,	Time		(InTime)
	,	Item		(INDEX_NONE)
	,	Material	(NULL)
	,	Component	(NULL)
	,	BoneName	(NAME_None)
	{}
	FCheckResult*& GetNext()
		{ return *(FCheckResult**)&Next; }
	friend QSORT_RETURN CDECL CompareHits( const FCheckResult* A, const FCheckResult* B )
		{ return A->Time<B->Time ? -1 : A->Time>B->Time ? 1 : 0; }
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

