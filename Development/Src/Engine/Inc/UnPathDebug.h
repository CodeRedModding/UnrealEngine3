/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_EDITOR
struct FPathStep
{
	struct FPathEnd
	{
		/** Node at end of this path item */
		ANavigationPoint*			End;
		/** Final weight of this path */
		INT							Weight;
		/** List of components of the weight + descriptions */
		TArray<FDebugNavCost>		Components;
	};

	/** Pawn that is doing this path search */
	APawn*						Pawn;
	/** Node that activated storage of this step */
	ANavigationPoint*			Origin;
	/** Node that is displayed by this step (typically Origin->previousPath) */
	ANavigationPoint*			Start;
	/** List of path ends from start */
	TArray<FPathEnd>			EndList;
	/** List of previous path back to start during this step */
	TArray<ANavigationPoint*>	Trail;
	/** Min/Max weights for all the end nodes */
	INT							MinWeight;
	INT							MaxWeight;

	FPathStep()
	{
		MinWeight = 0;
		MaxWeight = 0;
	}

	static void Clear();
	static void AddStep( ANavigationPoint* Nav, APawn* P );
	static void DrawStep( UINT StepInc, UINT ChildInc, UCanvas* Canvas );
	static void RegisterCost( ANavigationPoint* Nav, const TCHAR* Desc, INT Cost );
};

/** Array of path steps taken */
static TArray< TArray<FPathStep> >	PathStepCache;
/** Index for which path step to display */
static INT							PathStepIndex;
/** Index for which non optimal step to display */
static INT							PathStepChild;

#endif

//pathdebug
// path debugging and logging
#define PATH_LOOP_TESTING 0
#if 0 && !PS3 && !FINAL_RELEASE
static TArray<FString>			DebugPathLogCache;

#define DEBUGPATHONLY(x)			{ ##x }	
#define DEBUGPATHLOG(x)				{ DebugPathLogCache.AddItem(##x); }
#define DEBUGEMPTYPATHLOG			{ DebugPathLogCache.Empty(); }
#define PRINTDEBUGPATHLOG(x)		{ PrintDebugPathLogCache(##x); }	
#define DEBUGPRINTPATHLIST(x)		{ PrintPathList( ##x ); }
#define DEBUGPRINTSINGLEPATH(x)		{ PrintSinglePathList( ##x ); }

#define DEBUGSTOREPATHSTEP(N,P)		{ FPathStep::AddStep( N, P ); }
#define DEBUGEMPTYPATHSTEP			{ FPathStep::Clear(); }
#define DEBUGDRAWSTEPPATH(S,C,V)	{ FPathStep::DrawStep(S, C, V); }
#define DEBUGREGISTERCOST( N, D, C ){ FPathStep::RegisterCost( N, D, C ); }

static void PrintDebugPathLogCache( UBOOL bBreak )
{
	//turn off for now
	if( !bBreak )
		return;

	for( INT Idx = 0; Idx < DebugPathLogCache.Num(); Idx++ )
	{
		debugf( *(DebugPathLogCache(Idx)) );
	}

	if( bBreak )
	{
		appDebugBreak();
	}
}

static void PrintSinglePathList( ANavigationPoint* SourceNav )
{
	if( SourceNav != NULL )
	{
		DEBUGPATHLOG(FString::Printf(TEXT("Single Path: %s (%d) -> %s"), *SourceNav->GetName(), SourceNav->visitedWeight, *SourceNav->nextOrdered->GetName() ));

		TArray<ANavigationPoint*> LoopCheck;
		ANavigationPoint* Nav = SourceNav;
		while( Nav != NULL )
		{
			DEBUGPATHLOG(FString::Printf(TEXT("     %s"), *Nav->GetName()));

			if( LoopCheck.ContainsItem( Nav ) )
			{
				DEBUGPATHLOG(FString::Printf(TEXT("     LOOP LOOP LOOP %s"), *Nav->GetName()));
				PRINTDEBUGPATHLOG(TRUE);

				Nav = NULL;
			}
			else
			{
				LoopCheck.AddItem( Nav );
				Nav = Nav->previousPath;
			}				
		}
	}
}

static void PrintPathList( ANavigationPoint* Goal )
{
	if( Goal != NULL )
	{
		DEBUGPATHLOG(FString::Printf(TEXT("Print Path List...")));

		TArray<ANavigationPoint*> LoopCheck;
		INT Cnt = 0;
		ANavigationPoint* TestNav = Goal->nextOrdered;
		while( TestNav != NULL )
		{
			Cnt++;

			if( LoopCheck.ContainsItem( TestNav ) )
			{
				DEBUGPATHLOG(FString::Printf(TEXT("LOOP LOOP LOOP %s"), *TestNav->GetName()));
				PRINTDEBUGPATHLOG(TRUE);

				TestNav = NULL;
			}
			else
			{
				DEBUGPATHLOG(FString::Printf(TEXT("Path %d"), Cnt));
				DEBUGPRINTSINGLEPATH( TestNav );

				LoopCheck.AddItem( TestNav );
				TestNav = TestNav->nextOrdered;
			}			
		}

		DEBUGPATHLOG(FString::Printf(TEXT("End Print Path List... Total Count %d"),Cnt));
	}
}

#else
#define DEBUGPATHONLY(x)
#define DEBUGPATHLOG(x)	//{ debugf(*##x); }
#define DEBUGEMPTYPATHLOG		
#define PRINTDEBUGPATHLOG(x)
#define DEBUGPRINTPATHLIST(x)
#define DEBUGPRINTSINGLEPATH(x)
#define DEBUGSTOREPATHSTEP(N,P)
#define DEBUGEMPTYPATHSTEP
#define DEBUGDRAWSTEPPATH(S,C,V)
#define DEBUGREGISTERCOST( N, D, C )
#endif

