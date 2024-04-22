/*=============================================================================
	SurfaceIterators.h: Model surface iterators.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SURFACE_ITERATORS_H__
#define __SURFACE_ITERATORS_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Level filters
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Level filter that passes all levels.
 */
class FAllSurfaceLevelFilter
{
public:
	static FORCEINLINE UBOOL IsSuitable(const ULevel* Level)
	{
		return TRUE;
	}
};

/**
 * Level filter that passes the current level.
 */
class FCurrentLevelSurfaceLevelFilter
{
public:
	static FORCEINLINE UBOOL IsSuitable(const ULevel* Level)
	{
		return Level == GWorld->CurrentLevel;
	}
};


/** The default level filter. */
typedef FAllSurfaceLevelFilter DefaultSurfaceLevelFilter;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TSurfaceIteratorBase
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Iterates over the selected surfaces of all levels in the specified UWorld.
 */
template< class SurfaceFilter, class LevelFilter=DefaultSurfaceLevelFilter >
class TSurfaceIteratorBase
{
public:
	typedef SurfaceFilter	SurfaceFilterType;
	typedef LevelFilter		LevelFilterType;

	FORCEINLINE FBspSurf* operator*()
	{
		check( CurrentSurface );
		return CurrentSurface;
	}
	FORCEINLINE FBspSurf* operator->()
	{
		check( CurrentSurface );
		return CurrentSurface;
	}
	FORCEINLINE operator UBOOL()
	{
		return !bReachedEnd;
	}

	FORCEINLINE UModel* GetModel()
	{
		check( !bReachedEnd );
		return World->Levels(LevelIndex)->Model;
	}

	FORCEINLINE INT GetSurfaceIndex() const
	{
		check( !bReachedEnd );
		return SurfaceIndex;
	}

	FORCEINLINE INT GetLevelIndex() const
	{
		check( !bReachedEnd );
		return LevelIndex;
	}

	void operator++()
	{
		CurrentSurface = NULL;

		ULevel* Level = World->Levels(LevelIndex);
		while ( !bReachedEnd && !CurrentSurface )
		{
			// Skip over unsuitable levels or levels for whom all surfaces have been visited.
			if ( !LevelFilter::IsSuitable( Level ) || ++SurfaceIndex >= Level->Model->Surfs.Num() )
			{
				if ( ++LevelIndex >= World->Levels.Num() )
				{
					// End of level list.
					bReachedEnd = TRUE;
					LevelIndex = 0;
					SurfaceIndex = 0;
					CurrentSurface = NULL;
					break;
				}
				else
				{
					// Get the next level.
					Level = World->Levels(LevelIndex);
					if ( !LevelFilter::IsSuitable( Level ) )
					{
						continue;
					}

					SurfaceIndex = 0;
					// Gracefully handle levels with no surfaces.
					if ( SurfaceIndex >= Level->Model->Surfs.Num() )
					{
						continue;
					}
				}
			}
			CurrentSurface = &Level->Model->Surfs(SurfaceIndex);
			if ( !SurfaceFilterType::IsSuitable( CurrentSurface ) )
			{
				CurrentSurface = NULL;
			}
		}
	}

protected:
	TSurfaceIteratorBase(UWorld* InWorld)
		:	bReachedEnd( FALSE )
		,	World( InWorld )
		,	LevelIndex( 0 )
		,	SurfaceIndex( -1 )
		,	CurrentSurface( NULL )
	{}

private:
	/** TRUE if the iterator has reached the end. */
	UBOOL		bReachedEnd;

	/** The world whose surfaces we're iterating over. */
	UWorld*		World;

	/** Current index into UWorld's Levels array. */
	INT			LevelIndex;

	/** Current index into the current level's Surfs array. */
	INT			SurfaceIndex;

	/** Current surface pointed at by the iterator. */
	FBspSurf*	CurrentSurface;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TSurfaceIterator
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Surface filter that passes all surfaces.
 */
class FAllSurfaceFilter
{
public:
	static FORCEINLINE UBOOL IsSuitable(const FBspSurf* Surface)
	{
		return TRUE;
	}
};

/**
 * Iterates over selected surfaces of the specified UWorld.
 */
template< class LevelFilter=DefaultSurfaceLevelFilter >
class TSurfaceIterator : public TSurfaceIteratorBase<FAllSurfaceFilter, LevelFilter>
{
public:
	typedef TSurfaceIteratorBase<FAllSurfaceFilter, LevelFilter> Super;

	TSurfaceIterator(UWorld* InWorld=GWorld)
		: Super( InWorld )
	{
		// Initialize members by advancing to the first valid surface.
		++(*this);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TSelectedSurfaceIterator
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Surface filter that passes selected surfaces.
 */
class FSelectedSurfaceFilter
{
public:
	static FORCEINLINE UBOOL IsSuitable(const FBspSurf* Surface)
	{
		return (Surface->PolyFlags & PF_Selected) ? TRUE : FALSE;
	}
};

/**
 * Iterates over selected surfaces of the specified UWorld.
 */
template< class LevelFilter=DefaultSurfaceLevelFilter >
class TSelectedSurfaceIterator : public TSurfaceIteratorBase<FSelectedSurfaceFilter, LevelFilter>
{
public:
	typedef TSurfaceIteratorBase<FSelectedSurfaceFilter, LevelFilter> Super;

	TSelectedSurfaceIterator(UWorld* InWorld=GWorld)
		: Super( InWorld )
	{
		// Initialize members by advancing to the first valid surface.
		++(*this);
	}
};

#define FOR_EACH_UMODEL			for ( INT LevelIndex##__LINE__ = 0 ; LevelIndex##__LINE__ < GWorld->Levels.Num() ; ++LevelIndex##__LINE__ ) { UModel* Model = GWorld->Levels(LevelIndex##__LINE__)->Model;
#define END_FOR_EACH_UMODEL		}

#endif // __SURFACE_ITERATORS_H__
