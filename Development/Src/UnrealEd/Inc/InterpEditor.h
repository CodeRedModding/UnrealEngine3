/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __INTERPEDITOR_H__
#define __INTERPEDITOR_H__

#include "UnrealEd.h"
#include "CurveEd.h"
#include "TrackableWindow.h"
#include "UnEdTran.h"

/*-----------------------------------------------------------------------------
	Editor-specific hit proxies.
-----------------------------------------------------------------------------*/

struct HInterpEdTrackBkg : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackBkg,HHitProxy);
	HInterpEdTrackBkg(): HHitProxy(HPP_UI) {}
};

struct HInterpEdGroupTitle : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupTitle,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupTitle(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

struct HInterpEdGroupCollapseBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupCollapseBtn,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupCollapseBtn(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

/** Hit proxy for hitting a collapse button widget on a track with subtracks */
struct HInterpEdTrackCollapseBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackCollapseBtn,HHitProxy);
	/* Track that can be collapsed */
	class UInterpTrack* Track;
	/* Index to a subtrack group that was collapsed.  INDEX_NONE if the parent track was collapsed */
	INT SubTrackGroupIndex;

	HInterpEdTrackCollapseBtn(class UInterpTrack* InTrack, INT InSubTrackGroupIndex = INDEX_NONE ) :
	HHitProxy(HPP_UI),
		Track(InTrack),
		SubTrackGroupIndex( InSubTrackGroupIndex )
	{}
};

struct HInterpEdGroupLockCamBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupLockCamBtn,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupLockCamBtn(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

struct HInterpEdTrackTitle : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackTitle,HHitProxy);
	
	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HInterpEdTrackTitle( class UInterpGroup* InGroup, class UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

/** Hit proxy for a subtrack group that was hit */
struct HInterpEdSubGroupTitle : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdSubGroupTitle,HHitProxy);
	
	/* Track owning the group */
	class UInterpTrack* Track;
	/* Index of the group in the owning track */
	INT SubGroupIndex;

	HInterpEdSubGroupTitle( class UInterpTrack* InTrack, INT InSubGroupIndex = INDEX_NONE ) :
	HHitProxy(HPP_UI),
		Track( InTrack ),
		SubGroupIndex( InSubGroupIndex )
	{}
};
/**
 * Represents the space in the track viewport associated to a given track. 
 */
struct HInterpEdTrackTimeline : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackTimeline,HHitProxy);

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HInterpEdTrackTimeline(class UInterpGroup* InGroup, UInterpTrack* InTrack) :
	// Lowering the priority of this hit proxy to wireframe is a bit of a hack. A problem arises 
	// when the white, track position line overlaps with a key. Since the white line has no hit proxy, 
	// the hit proxy detection algorithm with check the area around the click location. The algorithm 
	// chooses the first hit proxy, which is almost always the track viewport hit proxy. Given that the 
	// key proxy was the same priority as the track viewport proxy, the key proxy is never chosen. 
	// I am lowering the priority of the track viewport proxy to compensate. 
		HHitProxy(HPP_Wireframe),
		Group(InGroup),
		Track(InTrack)
	{}
};

struct HInterpEdTrackTrajectoryButton : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackTrajectoryButton,HHitProxy);

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HInterpEdTrackTrajectoryButton(class UInterpGroup* InGroup, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

struct HInterpEdTrackGraphPropBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackGraphPropBtn,HHitProxy);

	class UInterpGroup* Group;
	class UInterpTrack* Track;
	INT SubTrackGroupIndex;

	HInterpEdTrackGraphPropBtn(class UInterpGroup* InGroup, INT InSubTrackGroupIndex, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		SubTrackGroupIndex( InSubTrackGroupIndex ),
		Track(InTrack)
	{}
};

struct HInterpEdTrackDisableTrackBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackDisableTrackBtn,HHitProxy);

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HInterpEdTrackDisableTrackBtn(class UInterpGroup* InGroup, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

enum EInterpEdEventDirection
{
	IED_Forward,
	IED_Backward
};

struct HInterpEdEventDirBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdEventDirBtn,HHitProxy);

	class UInterpGroup* Group;
	INT TrackIndex;
	EInterpEdEventDirection Dir;

	HInterpEdEventDirBtn(class UInterpGroup* InGroup, INT InTrackIndex, EInterpEdEventDirection InDir) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		TrackIndex(InTrackIndex),
		Dir(InDir)
	{}
};

struct HInterpEdTimelineBkg : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTimelineBkg,HHitProxy);
	HInterpEdTimelineBkg(): HHitProxy(HPP_UI) {}
};

struct HInterpEdNavigatorBackground : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdNavigatorBackground,HHitProxy);
	HInterpEdNavigatorBackground(): HHitProxy(HPP_UI) {}
};

struct HInterpEdNavigator : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdNavigator,HHitProxy);
	HInterpEdNavigator(): HHitProxy(HPP_UI) {}
};

enum EInterpEdMarkerType
{
	ISM_SeqStart,
	ISM_SeqEnd,
	ISM_LoopStart,
	ISM_LoopEnd
};

struct HInterpEdMarker : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdMarker,HHitProxy);

	EInterpEdMarkerType Type;

	HInterpEdMarker(EInterpEdMarkerType InType) :
		HHitProxy(HPP_UI),
		Type(InType)
	{}

	/**
	 * Displays the cross mouse cursor when hovering over the timeline markers.
	 */
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

/** Hitproxy for when the user clicks on a filter tab in matinee. */
struct HInterpEdTab : public HHitProxy
{
	/** Pointer to the interp filter for this hit proxy. */
	class UInterpFilter* Filter;

	DECLARE_HIT_PROXY(HInterpEdTab,HHitProxy);
	HInterpEdTab(UInterpFilter* InFilter): HHitProxy(HPP_UI), Filter(InFilter) {}
};

/*-----------------------------------------------------------------------------
	FInterpGroup
-----------------------------------------------------------------------------*/

struct FInterpGroupParentInfo
{
public:

	/** The parented interp group */
	UInterpGroup*	Group;

	/** The group's parent. Can be NULL */
	UInterpGroup*	Parent;

	/** The index of the interp group in the array of all interp groups. */
	INT				GroupIndex;

	/** Does the group have children? */
	UBOOL			bHasChildren;

	/**
	 * Parameterized constructor.
	 *
	 * @param	InGroup	The group to collect parent info.
	 */
	explicit FInterpGroupParentInfo( UInterpGroup* InGroup )
		:	Group(InGroup)
		,	Parent(NULL)
		,	GroupIndex(INDEX_NONE)
		,	bHasChildren(FALSE)
	{
		// As assumption is made that this structure 
		// should contain a non-NULL group.
		check(Group);
	}

	/**
	 * @return	TRUE if the group has a parent.
	 */
	FORCEINLINE UBOOL HasAParent() const
	{
		return (Parent != NULL);
	}

	/**
	 * @return	TRUE if the group is a parent.
	 */
	FORCEINLINE UBOOL IsAParent() const
	{
		return bHasChildren;
	}

	/**
	* @return	TRUE if the group is a parent.
	*/
	UBOOL IsParent( const FInterpGroupParentInfo& ParentCandidate ) const
	{
		// The group assigned to the group parent info should always be valid!
		check(ParentCandidate.Group);
		return (Parent == ParentCandidate.Group);
	}

	/**
	 * @param	Other	The other group parent info to compare against.
	 *
	 * @return	TRUE if the group in this info is pointing to the same group in the other info.
	 */
	FORCEINLINE UBOOL operator==(const FInterpGroupParentInfo& Other) const
	{
		// The group assigned to the group parent info should always be valid!
		check(Group && Other.Group);
		return (Group == Other.Group);
	}

private:

	/**
	 * Default constructor. Intentionally left Un-defined.
	 *
	 * @note	Use parameterized constructor.
	 */
	FInterpGroupParentInfo();
};

/*-----------------------------------------------------------------------------
	UInterpEdTransBuffer / FInterpEdTransaction
-----------------------------------------------------------------------------*/

class UInterpEdTransBuffer : public UTransBuffer
{
	DECLARE_CLASS_INTRINSIC(UInterpEdTransBuffer,UTransBuffer,CLASS_Transient,UnrealEd)
	NO_DEFAULT_CONSTRUCTOR(UInterpEdTransBuffer)
public:

	UInterpEdTransBuffer(SIZE_T InMaxMemory)
		:	UTransBuffer( InMaxMemory )
	{}

	/**
	 * Begins a new undo transaction.  An undo transaction is defined as all actions
	 * which take place when the user selects "undo" a single time.
	 * If there is already an active transaction in progress, increments that transaction's
	 * action counter instead of beginning a new transaction.
	 * 
	 * @param	SessionName		the name for the undo session;  this is the text that 
	 *							will appear in the "Edit" menu next to the Undo item
	 *
	 * @return	Number of active actions when Begin() was called;  values greater than
	 *			0 indicate that there was already an existing undo transaction in progress.
	 */
	virtual INT Begin(const TCHAR* SessionName)
	{
		return 0;
	}

	/**
	 * Attempts to close an undo transaction.  Only successful if the transaction's action
	 * counter is 1.
	 * 
	 * @return	Number of active actions when End() was called; a value of 1 indicates that the
	 *			transaction was successfully closed
	 */
	virtual INT End()
	{
		return 1;
	}

	/**
	 * Cancels the current transaction, no longer capture actions to be placed in the undo buffer.
	 *
	 * @param	StartIndex	the value of ActiveIndex when the transaction to be cancelled was began. 
	 */
	virtual void Cancel(INT StartIndex = 0)
	{}

	virtual void BeginSpecial(const TCHAR* SessionName);
	virtual void EndSpecial();
};

class FInterpEdTransaction : public FTransaction
{
public:
	FInterpEdTransaction( const TCHAR* InTitle=NULL, UBOOL InFlip=0 )
	:	FTransaction(InTitle, InFlip)
	{}

	virtual void SaveObject( UObject* Object );
	virtual void SaveArray( UObject* Object, FScriptArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor );
};

/*-----------------------------------------------------------------------------
	Interp Track Filters
-----------------------------------------------------------------------------*/

/**
 *	Interp track filter that accepts all interp tracks.
 */
class FAllTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	TRUE	always.
	 */
	static FORCEINLINE UBOOL IsSuitable( const UInterpTrack* Track )
	{
		return TRUE;
	}
};

/**
 *	Interp track filter that accepts only selected interp tracks.
 */
class FSelectedTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	TRUE	if the given track is selected.
	 */
	static FORCEINLINE UBOOL IsSuitable( const UInterpTrack* Track )
	{
		return Track->bIsSelected;
	}
};

/**
 * Interp track filter that accepts only tracks of the given template class type.
 */
template <class ClassType>
class FClassTypeTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	TRUE	if the given track is of the template track type class; FALSE, otherwise.
	 */
	static FORCEINLINE UBOOL IsSuitable( const UInterpTrack* Track )
	{
		return Track->IsA(ClassType::StaticClass()) && Track->bIsSelected;
	}
};

/** The default track filter for interp track iterators. */
typedef FAllTrackFilter		DefaultTrackFilter;

/*-----------------------------------------------------------------------------
	TInterpTrackIteratorBase
-----------------------------------------------------------------------------*/

/**
 * Base iterator for all interp track iterators.
 */
template <bool bConst, class Filter=DefaultTrackFilter>
class TInterpTrackIteratorBase
{
private:

	// Typedefs that allow for both const and non-const versions of this iterator without any code duplication.
	typedef typename TChooseClass<bConst,typename const UInterpGroup,typename UInterpGroup>::Result						GroupType;
	typedef typename TChooseClass<bConst,typename const TArray<UInterpGroup*>,typename TArray<UInterpGroup*> >::Result	GroupArrayType;
	typedef typename TChooseClass<bConst,typename TArray<UInterpGroup*>::TConstIterator,typename TArray<UInterpGroup*>::TIterator>::Result	GroupIteratorType;

	typedef typename TChooseClass<bConst,typename const UInterpTrack,typename UInterpTrack>::Result						TrackType;

public:

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is still valid.
	 */
	FORCEINLINE TrackType* operator*()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		return AllTracksInCurrentGroup( TrackIteratorIndex );
	}

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is still valid.
	 */
	FORCEINLINE TrackType* operator->()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		return AllTracksInCurrentGroup( TrackIteratorIndex );
	}

	/**
	 * @return	If the current track is a subtrack, returns the index of the subtrack in its parent track.  
	 *		If the track is not a subtrack, returns the index of the track in its owning group.
	 *  		Index is guaranteed to be valid as long as the iterator is valid.
	 */
	FORCEINLINE INT GetTrackIndex()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );

		// Get the track index for the current interp track.
		INT TrackIndex = INDEX_NONE;
		UInterpTrack* CurrentTrack = AllTracksInCurrentGroup( TrackIteratorIndex );

		UInterpGroup* OwningGroup = Cast<UInterpGroup>( CurrentTrack->GetOuter() );
		if( OwningGroup )
		{
			// Track is not a subtrack, find its index directly from the group
			TrackIndex = OwningGroup->InterpTracks.FindItemIndex( CurrentTrack );
		}
		else
		{
			// Track is a subtrack, find its index from its owning track.
			UInterpTrack* OwningTrack = CastChecked<UInterpTrack>( CurrentTrack->GetOuter() );
			TrackIndex = OwningTrack->SubTracks.FindItemIndex( CurrentTrack );
		}

		return TrackIndex;
	}

	/**
	 * @return	The group the owns the current track. 
	 */
	FORCEINLINE GroupType* GetGroup()
	{
		return *GroupIt;
	}

	/**
	 * @return	TRUE if the iterator has not reached the end. 
	 */
	FORCEINLINE operator UBOOL()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		return GroupIt && IsCurrentTrackValid();
	}

	/**
	 * Increments the iterator to point to the next valid interp track or the end of the iterator.
	 */
	void operator++()
	{
		UBOOL bFoundNextTrack = FALSE;

		// Increment the interp track iterator instead of the group iterator. Incrementing 
		// the group iterator will change the group and list of selected tracks.
		++TrackIteratorIndex;

		// The current track index should never be below zero after incrementing. 
		check( TrackIteratorIndex >= 0 );

		// Keep iterating until we found another suitable track or we reached the end.
		while( !bFoundNextTrack && GroupIt )
		{

			// If we reached the end of the tracks for the current group, 
			// the we need to advance to the first track of the next group.
			if( TrackIteratorIndex == AllTracksInCurrentGroup.Num() )
			{
				// Advance to the next group. If this is the end 
				// of the iterator, the while loop will terminate.
				++GroupIt;
				TrackIteratorIndex = 0;
				if( GroupIt )
				{
					// Generate a new list of tracks in the current group.
					GetAllTracksInCurrentGroup( *GroupIt );
				}
			}
			// We haven't reached the end, so we can now determine the validity of the current track.
			else
			{
				// If the track is valid, then we found the next track.	
				if( Filter::IsSuitable( AllTracksInCurrentGroup( TrackIteratorIndex ) ) )
				{
					bFoundNextTrack = TRUE;
				}
				// Else, keep iterating through the tracks. 
				else
				{
					TrackIteratorIndex++;
				}
			}
		}
	}

protected:

	/**
	 * Constructor for the base interp track iterator. Only provided to 
	 * child classes so that this class isn't instantiated. 
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackIteratorBase( GroupArrayType& InGroupArray )
		:	GroupArray(InGroupArray)
		,	GroupIt(InGroupArray)
		,	TrackIteratorIndex(INDEX_NONE)
	{
		if( GroupIt )
		{
			// Greate a new list of tracks in the current group
			GetAllTracksInCurrentGroup( *GroupIt );
		}
		++(*this);
	}

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is valid.
	 */
	FORCEINLINE TrackType* GetCurrentTrack()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		
		return AllTracksInCurrentGroup( TrackIteratorIndex );
	}

	/**
	 * Removes the current interp track that the iterator is pointing to from the current interp group and updates the iteator.
	 */
	void RemoveCurrentTrack()
	{
		UInterpTrack* RemovedTrack = AllTracksInCurrentGroup( TrackIteratorIndex );

		// Removing it from the current group doesn't really do anything other than maintain our current array.  We need to remove it from the group/track that owns it
		AllTracksInCurrentGroup.Remove( TrackIteratorIndex );

		UInterpGroup* OwningGroup = Cast<UInterpGroup>( RemovedTrack->GetOuter() );
		if( OwningGroup )
		{
			OwningGroup->InterpTracks.RemoveSingleItem( RemovedTrack );
			// Remove subtracks from tracks in current group and empty all subtracks.
			for( INT SubTrackIndex = 0; SubTrackIndex < RemovedTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				AllTracksInCurrentGroup.RemoveSingleItem( RemovedTrack->SubTracks( SubTrackIndex ) );
			}
			RemovedTrack->SubTracks.Empty();
		}
		else
		{
			UInterpTrack* OwningTrack = CastChecked<UInterpTrack>( RemovedTrack->GetOuter() );
			OwningTrack->SubTracks.RemoveSingleItem( RemovedTrack );
		}
		// Move the track iterator back one so that it's pointing to the track before the removed track. 

		// WARNING: In the event that a non-accept-all track filter is set to this iterator, such as a selected 
		// track filter, this iterator could be invalid at this point until the iterator is moved 
		// forward. NEVER try to access the current track after calling this function.
		--TrackIteratorIndex;
	}

	/**
	 * @return TRUE if the interp track that the iterator is currently pointing to is valid. 
	 */
	FORCEINLINE UBOOL IsCurrentTrackValid() const
	{
		return ( AllTracksInCurrentGroup.IsValidIndex( TrackIteratorIndex ) && Filter::IsSuitable( AllTracksInCurrentGroup(TrackIteratorIndex) ) );
	}

	/**
	 * Decrements the interp track iterator to point to the previous valid interp track.
	 *
	 * @note	This function is protected because we currently don't support reverse iteration.
	 *			Specifically, TInterpTrackIterator::RemoveCurrent() assumes that the iterator 
	 *			will only move forward. 
	 */
	void operator--()
	{
		UBOOL bFoundNextTrack = FALSE;

		// Decrement the interp track iterator instead of the group iterator. Incrementing 
		// the group iterator will change the group and list of selected tracks.
		--TrackIteratorIndex;

		// Keep iterating until we found another suitable track or we reached the end.
		while( !bFoundNextTrack && GroupIt )
		{
			// When decrementing, we should never reach the end of the array tracks.
			check( TrackIteratorIndex < AllTracksInCurrentGroup.Num() );

			// If we reached the end of the tracks for the current group, 
			// the we need to advance to the first track of the next group.
			if( TrackIteratorIndex == INDEX_NONE )
			{
				// Advance to the next group. If this is the end 
				// of the iterator, the while loop will terminate.
				--GroupIt;

				if( GroupIt )
				{
					// Generate a new list of tracks in the current group now that the group has changed.
					GetAllTracksInCurrentGroup( *GroupIt );
					TrackIteratorIndex = ( AllTracksInCurrentGroup.Num() - 1);
				}
			}
			// We haven't reached the end, so we can now determine the validity of the current track.
			else
			{
				// If the track is valid, then we found the next track. So, stop iterating.	
				if( Filter::IsSuitable( AllTracksInCurrentGroup( TrackIteratorIndex ) ) )
				{
					bFoundNextTrack = TRUE;
				}
				// Else, keep iterating through the tracks. 
				else
				{
					--TrackIteratorIndex;
				}
			}
		}
	}

private:

	/** The array of all interp groups currently in the interp editor. */
	GroupArrayType& GroupArray;

	/** The iterator that iterates over all interp groups in the interp editor. */
	GroupIteratorType GroupIt;

	/** The index of the interp track that iterator is currently pointing to. */
	INT TrackIteratorIndex;

	/** A list of all tracks in the current group */
	TArray<UInterpTrack*> AllTracksInCurrentGroup;
	/** 
	 * Default construction intentionally left undefined to prevent instantiation! 
	 *
	 * @note	To use this class, you must call the parameterized constructor.
	 */
	TInterpTrackIteratorBase();

	/** Gets all subtracks in the current track (recursive)*/
	void GetAllSubTracksInTrack( UInterpTrack* Track )
	{
		for( INT SubTrackIdx = 0; SubTrackIdx < Track->SubTracks.Num(); ++SubTrackIdx )
		{
			UInterpTrack* SubTrack = Track->SubTracks( SubTrackIdx );
			AllTracksInCurrentGroup.AddItem( SubTrack );
			GetAllSubTracksInTrack( SubTrack );
		}
	}

	/** Gets all tracks that are in the current group. */
	void GetAllTracksInCurrentGroup( GroupType* CurrentGroup )
	{
		AllTracksInCurrentGroup.Empty();
		if( CurrentGroup )
		{
			for( INT TrackIdx = 0; TrackIdx < CurrentGroup->InterpTracks.Num(); ++TrackIdx )
			{
				UInterpTrack* Track = CurrentGroup->InterpTracks( TrackIdx );
				AllTracksInCurrentGroup.AddItem( Track );
				GetAllSubTracksInTrack( Track );
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	TInterpTrackIterator / TInterpTrackConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp track iterator with option to specify the filter type. 
 */
template <class FilterType=DefaultTrackFilter>
class TInterpTrackIterator : public TInterpTrackIteratorBase<false,FilterType>
{
public:

	/**
	 * Constructor to make a modifiable interp track iterator.
	 * 
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackIterator( TArray<UInterpGroup*>& InGroupArray )
		:	TInterpTrackIteratorBase<false,FilterType>(InGroupArray)
	{
	}

	/**
	 * Removes the interp track that the iterator is currently pointing to. 
	 *
	 * @warning Do not dereference this iterator after calling this function until the iterator has been moved forward. 
	 */
	void RemoveCurrent()
	{
		RemoveCurrentTrack();
	}

	/**
	 * Moves the location of the iterator up or down by one.
	 * 
	 * @note	This function needs to be called every time an interp track is moved up or down by one. 
	 * @param	Value	The amount to move the iterator by. Must be either -1 or 1. 
	 */
	void MoveIteratorBy( INT Value )
	{
		check( Abs(Value) == 1 );

		( Value == 1 ) ? ++(*this) : --(*this);
	}

private:

	/** 
	 * Default constructor. 
	 * Intentionally left undefined to prevent instantiation using the default constructor.
	 * 
	 * @note	Must use parameterized constructor when making an instance of this class.
	 */
	TInterpTrackIterator();
};

/**
 * Implements a non-modifiable iterator with option to specify the filter type. 
 */
template <class FilterType=DefaultTrackFilter>
class TInterpTrackConstIterator : public TInterpTrackIteratorBase<true,FilterType>
{
public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	TInterpTrackIteratorBase<true,FilterType>(InGroupArray)
	{
	}

private:

	/** 
	 * Default constructor. 
	 * Intentionally left undefined to prevent instantiation using the default constructor.
	 * 
	 * @note	Must use parameterized constructor when making an instance of this class.
	 */
	TInterpTrackConstIterator();
};

// These iterators will iterate over all tracks. 
typedef TInterpTrackIterator<>							FAllTracksIterator;
typedef TInterpTrackConstIterator<>						FAllTracksConstIterator;

// These iterator will iterate over only selected tracks.
typedef TInterpTrackIterator<FSelectedTrackFilter>		FSelectedTrackIterator;
typedef TInterpTrackConstIterator<FSelectedTrackFilter>	FSelectedTrackConstIterator;


/*-----------------------------------------------------------------------------
	TTrackClassTypeIterator / TTrackClassTypeConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp track iterator that only iterates over interp tracks of the given UClass.
 */
template <class ChildTrackType>
class TTrackClassTypeIterator : public TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> >
{
private:

	typedef TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> > Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TTrackClassTypeIterator( TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
		// If the user didn't pass in a UInterpTrack derived class, then 
		// they probably made a typo or are using the wrong iterator.
		check( ChildTrackType::StaticClass()->IsChildOf(UInterpTrack::StaticClass()) );
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE ChildTrackType* operator*()
	{
		return CastChecked<ChildTrackType>(GetCurrentTrack());
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE ChildTrackType* operator->()
	{
		return CastChecked<ChildTrackType>(GetCurrentTrack());
	}
};


/**
 * Implements a non-modifiable interp track iterator that only iterates over interp tracks of the given UClass.
 */
template <class ChildTrackType>
class TTrackClassTypeConstIterator : public TInterpTrackConstIterator< FClassTypeTrackFilter<ChildTrackType> >
{
private:

	typedef TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> > Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TTrackClassTypeConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
		// If the user didn't pass in a UInterpTrack derived class, then 
		// they probably made a typo or are using the wrong iterator.
		check( ChildTrackType::StaticClass()->IsChildOf(UInterpTrack::StaticClass()) );
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE const ChildTrackType* operator*()
	{
		return CastChecked<ChildTrackType>(GetCurrentTrack());
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE const ChildTrackType* operator->()
	{
		return CastChecked<ChildTrackType>(GetCurrentTrack());
	}
};

/*-----------------------------------------------------------------------------
	Interp Group Filters
-----------------------------------------------------------------------------*/

/**
 * Interp group filter that accepts all interp groups. 
 */
class FAllGroupsFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	TRUE	always.
	 */
	static UBOOL IsSuitable( const UInterpGroup* InGroup )
	{
		return TRUE;
	}
};

/**
 * Interp group filter that accepts only selected interp groups. 
 */
class FSelectedGroupFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	TRUE	if the given group is selected; FALSE otherwise.
	 */
	static UBOOL IsSuitable( const UInterpGroup* InGroup )
	{
		return InGroup->bIsSelected;
	}
};

/**
 * Interp group filter that accepts only selected folders.
 */
class FSelectedFolderFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	TRUE	if the given group is selected and is a folder; FALSE, otherwise.
	 */
	static UBOOL IsSuitable( const UInterpGroup* InGroup )
	{
		return (InGroup->bIsSelected && InGroup->bIsFolder);
	}
};

/** The default group filter. */
typedef FAllGroupsFilter DefaultGroupFilter;


/*-----------------------------------------------------------------------------
	TInterpGroupIteratorBase
-----------------------------------------------------------------------------*/

/**
 * Implements the common behavior for all interp group iterators. 
 */
template <bool bConst, class GroupFilter=DefaultGroupFilter>
class TInterpGroupIteratorBase
{
private:

	// Typedefs that allow the iterator to be const or non-const without duplicating any code. 
	typedef typename TChooseClass<bConst,typename const UInterpGroup,typename UInterpGroup>::Result								GroupType;
	typedef typename TChooseClass<bConst,typename const TArray<UInterpGroup*>,typename TArray<UInterpGroup*> >::Result			GroupArrayType;
	typedef typename TChooseClass<bConst,typename GroupArrayType::TConstIterator,typename GroupArrayType::TIterator>::Result	GroupIteratorType;

	typedef typename TChooseClass<bConst,typename const UInterpTrack,typename UInterpTrack>::Result								TrackType;
	typedef typename TChooseClass<bConst,typename const TArray<UInterpTrack*>,typename TArray<UInterpTrack*> >::Result			TrackArrayType;

public:

	/**
	 * @return	TRUE if the iterator has not reached the end; FALSE, otherwise.
	 */
	FORCEINLINE operator UBOOL()
	{
		return IsCurrentGroupValid();
	}

	/**
	 * @return	A pointer to the current interp group. Guaranteed to be valid (non-NULL) if the iterator is still valid. 
	 */
	FORCEINLINE GroupType* operator*()
	{
		// The current group must be valid to dereference.
		check( IsCurrentGroupValid() );
		return (*GroupIterator);
	}

	/**
	 * @return	A pointer to the current interp group. Guaranteed to be valid (non-NULL) if the iterator is still valid. 
	 */
	FORCEINLINE GroupType* operator->()
	{
		// The current group must be valid to dereference.
		check( IsCurrentGroupValid() );
		return (*GroupIterator);
	}

	/**
	 * @return	The index of current interp group that the iterator is pointing to.
	 */
	FORCEINLINE INT GetGroupIndex() const
	{
		return GroupIterator.GetIndex();
	}

	/**
	 * Increments the iterator to point to the next valid interp group or may reach the end of the iterator. 
	 */
	void operator++()
	{
		UBOOL bFoundNextGroup = FALSE;

		++GroupIterator;

		// Keep iterating until we found a group that is 
		// valid or we reached the end of the iterator.
		while( !bFoundNextGroup && GroupIterator )
		{
			// The next valid group is found if it passes the group filter. 
			if( GroupFilter::IsSuitable(*GroupIterator) )
			{
				bFoundNextGroup = TRUE;
			}
			else
			{
				++GroupIterator;
			}
		}
	}

protected:

	/**
	 * Constructor for the base group iterator. MUST be called by any derived classes.
	 * Intentionally, left protected to prevent instantiation of this type.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupIteratorBase( GroupArrayType& InGroupArray )
		:	GroupArray(InGroupArray)
		,	GroupIterator(InGroupArray)
	{
		// It's possible that the first group in the array isn't suitable based on the group filter. 
		// So, we need to make sure we are pointing to the first suitable group.
		if( GroupIterator && !GroupFilter::IsSuitable( *GroupIterator ) )
		{
			++(*this);
		}
	}

	/**
	 * Removes the current interp groups from the interp group array and updates the iterator.
	 */
	void RemoveCurrentGroup()
	{
		// Remove the group itself from the array of interp groups.
		GroupArray.Remove(GroupIterator.GetIndex());

		// Move the group iterator back one so that it's pointing to the track before the removed track. 

		// WARNING: In the event that a non-accept-all group filter is set to this iterator, such as a selected 
		// group filter, this iterator could be invalid at this point until the iterator is moved 
		// forward. NEVER try to access the current group after calling this function.
		--GroupIterator;
	}

	/**
	 * @return	TRUE if the current interp group that the iterator is pointing to is valid. 
	 */
	FORCEINLINE UBOOL IsCurrentGroupValid() const
	{
		return ( GroupIterator && GroupFilter::IsSuitable(*GroupIterator) );
	}

private:

	/** The array of all interp groups currently in the interp editor */
	GroupArrayType& GroupArray;

	/** Iterator that iterates over the interp groups */
	GroupIteratorType GroupIterator;

	/** 
	 * Default constructor. Intentionally left undefined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupIteratorBase();
};

/*-----------------------------------------------------------------------------
	TInterpGroupIterator / TInterpGroupConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp group iterator that iterates over that groups that pass the provided filter. 
 */
template<class GroupFilter=DefaultGroupFilter>
class TInterpGroupIterator : public TInterpGroupIteratorBase<false, GroupFilter>
{
private:

	typedef TInterpGroupIteratorBase<false, GroupFilter> Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupIterator( TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
	}

	/**
	 * Removes the interp group and associated track selection slots that the iterator is currently pointing to from the selection map.
	 *
	 * @warning Do not dereference this iterator after calling this function until the iterator has been moved forward. 
	 */
	void RemoveCurrent()
	{
		RemoveCurrentGroup();
	}

private:

	/** 
	 * Default constructor. Intentionally left un-defined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupIterator();
};

/**
 * Implements a non-modifiable interp group iterator that iterates over that groups that pass the provided filter. 
 */
template<class GroupFilter=DefaultGroupFilter>
class TInterpGroupConstIterator : public TInterpGroupIteratorBase<true, GroupFilter>
{
private:

	typedef TInterpGroupIteratorBase<true, GroupFilter> Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
	}

private:

	/** 
	 * Default constructor. Intentionally left undefined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupConstIterator();
};

// These iterators can iterate over all interp groups, regardless of selection states. 
typedef TInterpGroupIterator<>								FGroupIterator;
typedef TInterpGroupConstIterator<>							FGroupConstIterator;

// These iterators only iterate over selected interp groups. 
typedef TInterpGroupIterator<FSelectedGroupFilter>			FSelectedGroupIterator;
typedef TInterpGroupConstIterator<FSelectedGroupFilter>		FSelectedGroupConstIterator;

// These iterators only iterate over selected folders. 
typedef TInterpGroupIterator<FSelectedFolderFilter>			FSelectedFolderIterator;
typedef TInterpGroupConstIterator<FSelectedFolderFilter>	FSelectedFolderConstIterator;


/*-----------------------------------------------------------------------------
	FInterpEdViewportClient
-----------------------------------------------------------------------------*/

/** Struct for passing commonly used draw params into draw routines */
struct FInterpTrackLabelDrawParams
{
	/** Materials used to represent buttons */
	UMaterial* CamUnlockedIcon;
	UMaterial* CamLockedIcon;
	UMaterial* ForwardEventOnMat;
	UMaterial* ForwardEventOffMat;
	UMaterial* BackwardEventOnMat;
	UMaterial* BackwardEventOffMat;
	UMaterial* DisableTrackMat;
	UMaterial* GraphOnMat;
	UMaterial* GraphOffMat;
	UMaterial* TrajectoryOnMat;
	/** The color of the group label */
	FColor GroupLabelColor;
	/** The height of the area for drawing tracks */
	INT TrackAreaHeight;
	/** The number of pixels to indent a track by */
	INT IndentPixels;
	/** The size of the view in X */	 
	INT ViewX;
	/** The size of the view in Y */
	INT ViewY;
	/** The offset in Y from the previous track*/
	INT YOffset;
	/** True if a track is selected*/
	UBOOL bTrackSelected;
};

/** Struct for containing all information to draw keyframes */
struct FKeyframeDrawInfo
{
	/** Position of the keyframe in the editor*/
	FVector2D KeyPos;
	/** Time of the keyframe */
	FLOAT KeyTime;
	/** Color of the keyframe */
	FColor KeyColor;
	/** True if the keyframe is selected */
	UBOOL bSelected;

	/** Comparison routines.  
	 * Two keyframes are said to be equal (for drawing purposes) if they are at the same time
	 * Does not do any tolerance comparison for now.
	 */
	UBOOL operator==( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime == Other.KeyTime;
	}

	UBOOL operator<( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime < Other.KeyTime;
	}

	UBOOL operator>( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime > Other.KeyTime;
	}

};

class FInterpEdViewportClient : public FEditorLevelViewportClient
{
public:
	FInterpEdViewportClient( class WxInterpEd* InInterpEd );
	~FInterpEdViewportClient();

	void DrawTimeline(FViewport* Viewport,FCanvas* Canvas);
	void DrawMarkers(FViewport* Viewport,FCanvas* Canvas);
	void DrawGrid(FViewport* Viewport,FCanvas* Canvas, UBOOL bDrawTimeline);
	void DrawTabs(FViewport* Viewport,FCanvas* Canvas);
	/** 
	 * Draws a track in the interp editor
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track to draw
	 * @param Group			Group containing the track to draw
	 * @param TrackDrawParams	Params for drawing the track
	 * @params LabelDrawParams	Params for drawing the track label
	 */
	INT DrawTrack( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams );

	/** 
	 * Creates a "Push Properties Onto Graph" Button
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track owning the group
	 * @param Group			Subgroup to draw
	 * @param GroupIndex		Index of the group in its parent track.
	 * @param LabelDrawParams	Params for drawing the group label
	 * @param bIsSubtrack		Is this a subtrack?
	 */
	void CreatePushPropertiesOntoGraphButton( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, INT GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UBOOL bIsSubTrack );

	/** 
	 * Draws a sub track group in the interp editor
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track owning the group
	 * @param InGroup		Subgroup to draw
	 * @param GroupIndex		Index of the group in its parent track.
	 * @params LabelDrawParams	Params for drawing the group label
	 * @param Group			Group that this subgroup is part of
	 */
	void DrawSubTrackGroup( FCanvas* Canvas, UInterpTrack* Track, const FSubTrackGroup& InGroup, INT GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UInterpGroup* Group );

	/** 
	 * Draws a track label for a track
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track that needs a label drawn for it.
	 * @param Group			Group containing the track to draw
	 * @param TrackDrawParams	Params for drawing the track
	 * @params LabelDrawParams	Params for drawing the track label
	 */
	void DrawTrackLabel( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams );

	FVector2D DrawTab(FViewport* Viewport, FCanvas* Canvas, INT &TabOffset, UInterpFilter* Filter);

	/** 
	 * Draws collapsed keyframes when a group is collapsed
	 * @param Canvas		Canvas to draw on
	 * @param Track			Track with keyframe data
	 * @param TrackPos		Position where the collapsed keys should be drawn
	 * @param TickSize		Draw size of each keyframe.
	 */
	void DrawCollapsedTrackKeys( FCanvas* Canvas, UInterpTrack* Track, const FVector2D& TrackPos, const FVector2D& TickSize );
	
	/** 
	 * Draws keyframes for all subtracks in a subgroup.  The keyframes are drawn directly on the group.
	 * @param Canvas		Canvas to draw on
	 * @param SubGroupOwner		Track that owns the subgroup 			
	 * @param GroupIndex		Index of a subgroup to draw
	 * @param DrawInfos		An array of draw information for each keyframe that needs to be drawn
	 * @param TrackPos		Starting position where the keyframes should be drawn.  
	 * @param KeySize		Draw size of each keyframe
	 */
	void DrawSubTrackGroupKeys( FCanvas* Canvas, UInterpTrack* SubGroupOwner, INT GroupIndex, const TArray<FKeyframeDrawInfo>& KeyDrawInfos, const FVector2D& TrackPos, const FVector2D& KeySize );
	
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual EMouseCursor GetCursor(FViewport* Viewport,INT X,INT Y);

	virtual void Tick(FLOAT DeltaSeconds);

	virtual void Serialize(FArchive& Ar);

	/** Exec handler */
	virtual void Exec(const TCHAR* Cmd);

	/** 
	 * Returns the vertical size of the entire group list for this viewport, in pixels
	 */
	INT ComputeGroupListContentHeight() const;

	/** 
	 * Returns the height of the viewable group list content box in pixels
	 *
	 *  @param ViewportHeight The size of the viewport in pixels
	 *
	 *  @return The height of the viewable content box (may be zero!)
	 */
	INT ComputeGroupListBoxHeight( const INT ViewportHeight ) const;

	FORCEINLINE UBOOL IsGrabbingHandle() const
	{
		return bGrabbingHandle;
	}

	/** Scroll bar thumb position (actually, this is the negated thumb position.) */
	INT	ThumbPos_Vert;

	/** True if this window is the 'director tracks' window and should only draw director track groups */
	UBOOL bIsDirectorTrackWindow;

	/** True if we want filter tabs to be rendered and interactive for this window */
	UBOOL bWantFilterTabs;

	/** True if we want the animation timeline bar to be rendered and interactive for this window */
	UBOOL bWantTimeline;

private:
	/**
	 * Selects a color for the specified group (bound to the given group actor)
	 *
	 * @param Group The group to select a label color for
	 * @param GroupActorOrNull The actor currently bound to the specified, or NULL if none is bounds
	 *
	 * @return The color to use to draw the group label
	 */
	FColor ChooseLabelColorForGroupActor( UInterpGroup* Group, AActor* GroupActorOrNull ) const;

	/**
	 * Adds all keypoints based on the hit proxy
	 */
	void AddKeysFromHitProxy( const HHitProxy* HitProxy, TArray<FInterpEdSelKey>& Selections ) const;

	INT OldMouseX, OldMouseY;
	INT BoxStartX, BoxStartY;
	INT BoxEndX, BoxEndY;
	INT DistanceDragged;

	/** Used to accumulate velocity for autoscrolling when the user is dragging items near the edge of the viewport. */
	FVector2D ScrollAccum;

	class WxInterpEd* InterpEd;

	/** The object and data we are currently dragging, if NULL we are not dragging any objects. */
	FInterpEdInputData DragData;
	FInterpEdInputInterface* DragObject;

	/** Used to keep track of the coordinates the user began the mouse down event */
	FVector2D PressedCoordinates;

	UBOOL	bPanning;
	UBOOL   bMouseDown;
	UBOOL	bGrabbingHandle;
	UBOOL	bNavigating;
	UBOOL	bBoxSelecting;
	UBOOL	bTransactionBegun;
	UBOOL	bGrabbingMarker;
};

/*-----------------------------------------------------------------------------
	WxInterpEdVCHolder
-----------------------------------------------------------------------------*/

class WxInterpEdVCHolder : public wxWindow
{
public:
	WxInterpEdVCHolder( wxWindow* InParent, wxWindowID InID, class WxInterpEd* InInterpEd );
	virtual ~WxInterpEdVCHolder();

	/**
	 * Destroys the viewport held by this viewport holder, disassociating it from the engine, etc.  Rentrant.
	 */
	void DestroyViewport();

	/** Scroll bar */
	wxScrollBar* ScrollBar_Vert;

	/**
	 * Updates the scroll bar for the current state of the window's size and content layout.  This should be called
	 *  when either the window size changes or the vertical size of the content contained in the window changes.
	 */
	void AdjustScrollBar();

	/**
	 * @return	The scroll bar's thumb position, which is the top of the scroll bar. 
	 */
	INT GetThumbPosition() const
	{
		return -InterpEdVC->ThumbPos_Vert;
	}

	/**
	 * Sets the thumb position from the given parameter. 
	 * 
	 * @param	NewPosition	The new thumb position. 
	 */
	void SetThumbPosition( INT NewPosition )
	{
		InterpEdVC->ThumbPos_Vert = -NewPosition;
	}

	/**
	 * Updates layout of the track editor window's content layout.  This is usually called in response to a window size change
	 */
	void UpdateWindowLayout();

	void OnSize( wxSizeEvent& In );

	FInterpEdViewportClient* InterpEdVC;

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	WxInterpEdToolBar
-----------------------------------------------------------------------------*/

class WxInterpEdToolBar : public WxToolBar
{
public:
	WxMaskedBitmap AddB, PlayReverseB, CreateMovieB, PlayB, LoopSectionB, StopB, UndoB, RedoB, CurveEdB, SnapB, FitSequenceB, FitToSelectedB, FitLoopB, FitLoopSequenceB, EndOfTrackB;
	WxMaskedBitmap Speed1B, Speed10B, Speed25B, Speed50B, Speed100B, SnapTimeToFramesB, FixedTimeStepPlaybackB, GorePreviewB, CreateCameraActorAtCurrentCameraLocationB;
	/** Bitmap used for the button to spawn the viewport recording window*/
	WxBitmap		RecordModeViewportBitmap;

	wxComboBox* SnapCombo;

	/** Combo box that allows the user to select the initial curve interpolation mode (EInterpCurveMode) for newly
	  * created key frames. */
	wxComboBox* InitialInterpModeComboBox;

	WxInterpEdToolBar( wxWindow* InParent, wxWindowID InID );
	~WxInterpEdToolBar();
};

/*-----------------------------------------------------------------------------
	WxInterpEdMenuBar
-----------------------------------------------------------------------------*/

class WxInterpEdMenuBar : public wxMenuBar
{
public:
	wxMenu	*FileMenu, *EditMenu, *ViewMenu;

	WxInterpEdMenuBar(WxInterpEd* InEditor);
	~WxInterpEdMenuBar();
};

/*-----------------------------------------------------------------------------
	WxInterpEd
-----------------------------------------------------------------------------*/

static const FLOAT InterpEdSnapSizes[5] = { 0.01f, 0.05f, 0.1f, 0.5f, 1.0f };

static const FLOAT InterpEdFPSSnapSizes[9] =
{
	1.0f / 15.0f,
	1.0f / 24.0f,
	1.0f / 25.0f,
	1.0f / ( 30.0f / 1.001f ),	// 1.0f / 29.97...
	1.0f / 30.0f,
	1.0f / 50.0f,
	1.0f / ( 60.0f / 1.001f ),	// 1.0f / 59.94...
	1.0f / 60.0f,
	1.0f / 120.0f,
};

static const TCHAR* InterpEdFPSSnapSizeLocNames[9] =
{
	TEXT( "InterpEd_FrameRate_15_fps" ),
	TEXT( "InterpEd_FrameRate_24_fps" ),
	TEXT( "InterpEd_FrameRate_25_fps" ),
	TEXT( "InterpEd_FrameRate_29_97_fps" ),
	TEXT( "InterpEd_FrameRate_30_fps" ),
	TEXT( "InterpEd_FrameRate_50_fps" ),
	TEXT( "InterpEd_FrameRate_59_94_fps" ),
	TEXT( "InterpEd_FrameRate_60_fps" ),
	TEXT( "InterpEd_FrameRate_120_fps" )
};

/** Constants Used by Matinee*/
namespace MatineeConstants
{
	/**The state of recording new Matinee Tracks*/
	enum
	{
		RECORDING_GET_READY_PAUSE,
		RECORDING_ACTIVE,
		RECORDING_COMPLETE,

		NUM_RECORDING_STATES
	};

	/**How recording should behave*/
	enum
	{
		//A new camera actor, camera group, and movement/fovangle tracks will be added for each take
		RECORD_MODE_NEW_CAMERA,
		//A new camera actor, camera group, and movement/fovangle tracks will be added AND rooted to the currently selected object.
		RECORD_MODE_NEW_CAMERA_ATTACHED,
		//Duplicate tracks of each selected track will be made for each take
		RECORD_MODE_DUPLICATE_TRACKS,
		//Empties and re-records over the currently selected tracks
		RECORD_MODE_REPLACE_TRACKS,

		NUM_RECORD_MODES
	};

	/**How the camera should move */
	enum
	{
		//camera movement should align with the camera axes
		CAMERA_SCHEME_FREE_CAM,
		//camera movement should planar
		CAMERA_SCHEME_PLANAR_CAM,

		NUM_CAMERA_SCHEMES
	};

	/**Menu Settings for HUD during record mode*/
	enum
	{
		//Default menu item to Start Recording
		RECORD_MENU_RECORD_MODE,
		//Allows camera translation speed adjustments
		RECORD_MENU_TRANSLATION_SPEED,
		//Allows camera rotation speed adjustments
		RECORD_MENU_ROTATION_SPEED,
		//Allows camera zoom speed adjustments
		RECORD_MENU_ZOOM_SPEED,
		//Allows for custom offset for pitch when using gamecaster camera
		RECORD_MENU_TRIM,
		//Invert the x axis
		RECORD_MENU_INVERT_X_AXIS,
		//Invert the x axis
		RECORD_MENU_INVERT_Y_AXIS,
		//Setting for what to do about roll wiggle
		RECORD_MENU_ROLL_SMOOTHING,
		//Setting for what to do about pitch wiggle
		RECORD_MENU_PITCH_SMOOTHING,
		//camera movement scheme
		RECORD_MENU_CAMERA_MOVEMENT_SCHEME,
		//Absolute setting for zoom distance
		RECORD_MENU_ZOOM_DISTANCE,

		NUM_RECORD_MENU_ITEMS
	};

	/**Delay between recording setup and the start of recording*/
	const UINT CountdownDurationInSeconds = 5;

	const UINT MaxSmoothingSamples = 10;
}

class WxInterpEd : public WxTrackableFrame, public FNotifyHook, public FSerializableObject, public FCurveEdNotifyInterface, public FDockingParent, 	
	// Interface for event handling
	public FCallbackEventDevice
{
public:
	WxInterpEd( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp  );
	virtual	~WxInterpEd();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	void OnSize( wxSizeEvent& In );
	virtual void OnClose( wxCloseEvent& In );

	////////////////////////////////
	// FCallbackEventDevice interface

	virtual void Send(ECallbackEventType Event) {};
	virtual void Send(ECallbackEventType Event, UObject* InObject);
	
	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InString the string information associated with this event
	 * @param InObject the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FString& InString, UObject* InObject) {};

	// FNotify interface

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// FCurveEdNotifyInterface
	virtual void PreEditCurve(TArray<UObject*> CurvesAboutToChange);
	virtual void PostEditCurve();
	virtual void MovedKey();
	virtual void DesireUndo();
	virtual void DesireRedo();

	/**
	 * FCurveEdNotifyInterface: Called by the Curve Editor when a Curve Label is clicked on
	 *
	 * @param	CurveObject	The curve object whose label was clicked on
	 */
	void OnCurveLabelClicked( UObject* CurveObject );

	// FSerializableObject
	void Serialize(FArchive& Ar);

	/** 
	 * Starts playing the current sequence. 
	 * @param bPlayLoop		Whether or not we should play the looping section.
	 * @param bPlayForward	TRUE if we should play forwards, or FALSE for reverse
	 */
	void StartPlaying( UBOOL bPlayLoop, UBOOL bPlayForward );

	/** Stops playing the current sequence. */
	void StopPlaying();

	/** Starts recording the current sequence */
	void StartRecordingMovie();

	// Menu handlers
	void OnScroll(wxScrollEvent& In);
	void OnMenuAddKey( wxCommandEvent& In );
	void OnMenuPlay( wxCommandEvent& In );
	void OnMenuCreateMovie( wxCommandEvent& In );
	void OnMenuStop( wxCommandEvent& In );
	void OnChangePlaySpeed( wxCommandEvent& In );
	void OnMenuInsertSpace( wxCommandEvent& In );
	void StretchSection( UBOOL bUseSelectedOnly );
	void OnMenuStretchSection( wxCommandEvent& In );
	void OnMenuStretchSelectedKeyframes( wxCommandEvent& In );
	void OnMenuDeleteSection( wxCommandEvent& In );
	void OnMenuSelectInSection( wxCommandEvent& In );
	void OnMenuDuplicateSelectedKeys( wxCommandEvent& In );
	void OnSavePathTime( wxCommandEvent& In );
	void OnJumpToPathTime( wxCommandEvent& In );
	void OnViewHide3DTracks( wxCommandEvent& In );
	void OnViewZoomToScrubPos( wxCommandEvent& In );

	/**
	 * Shows or hides all movement track trajectories in the Matinee sequence
	 */
	void OnViewShowOrHideAll3DTrajectories( wxCommandEvent& In );

	/** Toggles 'capture mode' for particle replay tracks */
	void OnParticleReplayTrackContext_ToggleCapture( wxCommandEvent& In );

	/** Called when the "Toggle Gore Preview" button is pressed */
	void OnToggleGorePreview( wxCommandEvent& In );

	/** Called when the "Toggle Gore Preview" UI should be updated */
	void OnToggleGorePreview_UpdateUI( wxUpdateUIEvent& In );

	/** Called when the "Create Camera Actor at Current Camera Location" button is pressed */
	void OnCreateCameraActorAtCurrentCameraLocation( wxCommandEvent& In );

	/** Called when the "Launch Custom Preview Viewport" is pressed */
	void OnLaunchRecordingViewport ( wxCommandEvent& In );
	/** Called when the "Launch the Director Window" button is called*/
	void OnLaunchDirectorWindow (wxCommandEvent& In);

	void OnToggleViewportFrameStats( wxCommandEvent& In );
	void OnEnableEditingGrid( wxCommandEvent& In );
	void OnEnableEditingGridUpdateUI( wxUpdateUIEvent& In );
	void OnSetEditingGrid( wxCommandEvent& In );
	void OnEditingGridUpdateUI( wxUpdateUIEvent& In );
	void OnToggleEditingCrosshair( wxCommandEvent& In );

	void OnToggleCurveEd( wxCommandEvent& In );
	void OnGraphSplitChangePos( wxSplitterEvent& In );

	void OnToggleSnap( wxCommandEvent& In );
	void OnToggleSnap_UpdateUI( wxUpdateUIEvent& In );

	/** Called when the 'snap time to frames' command is triggered from the GUI */
	void OnToggleSnapTimeToFrames( wxCommandEvent& In );
	void OnToggleSnapTimeToFrames_UpdateUI( wxUpdateUIEvent& In );


	/** Called when the 'fixed time step playback' command is triggered from the GUI */
	void OnFixedTimeStepPlaybackCommand( wxCommandEvent& In );

	/** Updates UI state for 'fixed time step playback' option */
	void OnFixedTimeStepPlaybackCommand_UpdateUI( wxUpdateUIEvent& In );


	/** Called when the 'prefer frame numbers' command is triggered from the GUI */
	void OnPreferFrameNumbersCommand( wxCommandEvent& In );

	/** Updates UI state for 'prefer frame numbers' option */
	void OnPreferFrameNumbersCommand_UpdateUI( wxUpdateUIEvent& In );


	/** Updates UI state for 'show time cursor pos for all keys' option */
	void OnShowTimeCursorPosForAllKeysCommand( wxCommandEvent& In );

	/** Called when the 'show time cursor pos for all keys' command is triggered from the GUI */
	void OnShowTimeCursorPosForAllKeysCommand_UpdateUI( wxUpdateUIEvent& In );


	void OnChangeSnapSize( wxCommandEvent& In );

	/**
	 * Called when the initial curve interpolation mode for newly created keys is changed
	 */
	void OnChangeInitialInterpMode( wxCommandEvent& In );

	void OnViewFitSequence( wxCommandEvent& In );
	void OnViewFitToSelected( wxCommandEvent& In );
	void OnViewFitLoop( wxCommandEvent& In );
	void OnViewFitLoopSequence( wxCommandEvent& In );
	void OnViewEndOfTrack( wxCommandEvent& In );

	void OnOpenBindKeysDialog( wxCommandEvent &In );

	/**
	 * Called when a docking window state has changed
	 */
	virtual void OnWindowDockingLayoutChanged();

	/**
	 * Called when the user selects the 'Expand All Groups' option from a menu.  Expands every group such that the
	 * entire hierarchy of groups and tracks are displayed.
	 */
	void OnExpandAllGroups( wxCommandEvent& In );

	/**
	 * Called when the user selects the 'Collapse All Groups' option from a menu.  Collapses every group in the group
	 * list such that no tracks are displayed.
	 */
	void OnCollapseAllGroups( wxCommandEvent& In );

	void OnContextNewTrack( wxCommandEvent& In );
	void OnContextNewGroup( wxCommandEvent& In );
	void OnContextTrackRename( wxCommandEvent& In );
	void OnContextTrackDelete( wxCommandEvent& In );
	void OnContextTrackChangeFrame( wxCommandEvent& In );

	/**
	 * Creates a new group track.
	 */
	void NewGroup( INT Id, AActor* GroupActor );

	/**
	 * Toggles visibility of the trajectory for the selected movement track
	 */
	void OnContextTrackShow3DTrajectory( wxCommandEvent& In );

#if WITH_FBX
	/**
	 * Exports the animations in the selected track to FBX
	 */
	void OnContextTrackExportAnimFBX( wxCommandEvent& In );
#endif
	
	void OnContextGroupRename( wxCommandEvent& In );
	void OnContextGroupDelete( wxCommandEvent& In );
	void OnContextGroupCreateTab( wxCommandEvent& In );
	void OnContextGroupSendToTab( wxCommandEvent& In );
	void OnContextGroupRemoveFromTab( wxCommandEvent& In );
	void OnContextGroupExportAnimFBX( wxCommandEvent& In );
	void OnContextDeleteGroupTab( wxCommandEvent& In );

	/** Called when the user selects to move a group to another group folder */
	void OnContextGroupChangeGroupFolder( wxCommandEvent& In );

	void OnContextKeyInterpMode( wxCommandEvent& In );
	void OnContextRenameEventKey( wxCommandEvent& In );
	void OnContextSetKeyTime( wxCommandEvent& In );
	void OnContextSetValue( wxCommandEvent& In );

	/** Pops up a property window to edit the notify for this key */
	void OnContextEditNotifyKey( wxCommandEvent& In );

	/** Pops up a combo box of the pawn's slot nodes to set the track's ParentNodeName property */
	void OnNotifyTrackContext_SetParentNodeName( wxCommandEvent& In );

	/** Pops up a menu and lets you set the color for the selected key. Not all track types are supported. */
	void OnContextSetColor( wxCommandEvent& In );
	
	/**
	 * Flips the value of the selected key for a boolean property.
	 *
	 * @note	Assumes that the user was only given the option of flipping the 
	 *			value in the context menu (i.e. TRUE -> FALSE or FALSE -> TRUE).
	 *
	 * @param	In	The wxWidgets event sent when a user selects the "Set to True" or "Set to False" context menu option.
	 */
	void OnContextSetBool( wxCommandEvent& In );

	/** Pops up menu and lets the user set a group to use to lookup transform info for a movement keyframe. */
	void OnSetMoveKeyLookupGroup( wxCommandEvent& In );

	/** Clears the lookup group for a currently selected movement key. */
	void OnClearMoveKeyLookupGroup( wxCommandEvent& In );

	void OnSetAnimKeyLooping( wxCommandEvent& In );
	void OnSetAnimOffset( wxCommandEvent& In );
	void OnSetAnimPlayRate( wxCommandEvent& In );

	/** Handler for the toggle animation reverse menu item. */
	void OnToggleReverseAnim( wxCommandEvent& In );

	/** Handler for UI update requests for the toggle anim reverse menu item. */
	void OnToggleReverseAnim_UpdateUI( wxUpdateUIEvent& In );

	/** Handler for the save as camera animation menu item. */
	void OnContextSaveAsCameraAnimation( wxCommandEvent& In );

	/**
	 * Handler to move the clicked-marker to the current timeline position.
	 *
	 * @param	In	The event sent by wxWidgets.
	 */
	void OnContextMoveMarkerToCurrentPosition( wxCommandEvent& In );

	/**
	 * Handler to move the clicked-marker to the beginning of the sequence.
	 *
	 * @param	In	The event sent by wxWidgets.
	 */
	void OnContextMoveMarkerToBeginning( wxCommandEvent& In );

	/**
	 * Handler to move the clicked-marker to the end of the sequence.
	 *
	 * @param	In	The event sent by wxWidgets.
	 */
	void OnContextMoveMarkerToEnd( wxCommandEvent& In );

	/**
	 * Handler to move the clicked-marker to the end of the selected track.
	 *
	 * @param	In	The event sent by wxWidgets.
	 */
	void OnContextMoveMarkerToEndOfSelectedTrack( wxCommandEvent& In );

	/**
	 * Handler to move the grabbed marker to the end of the longest track.
	 *
	 * @param	In	The event sent by wxWidgets.
	 */
	void OnContextMoveMarkerToEndOfLongestTrack( wxCommandEvent& In );

	void OnSetSoundVolume(wxCommandEvent& In);
	void OnSetSoundPitch(wxCommandEvent& In);
	void OnContextDirKeyTransitionTime( wxCommandEvent& In );
	void OnContextDirKeyRenameCameraShot( wxCommandEvent& In);
	void OnFlipToggleKey(wxCommandEvent& In);

	/** Called when a new key condition is selected in a track keyframe context menu */
	void OnKeyContext_SetCondition( wxCommandEvent& In );

	/** Syncs the generic browser to the currently selected sound track key */
	void OnKeyContext_SyncGenericBrowserToSoundCue( wxCommandEvent& In );

	/** Called when the user wants to set the master volume on Audio Master track keys */
	void OnKeyContext_SetMasterVolume( wxCommandEvent& In );

	/** Called when the user wants to set the master pitch on Audio Master track keys */
	void OnKeyContext_SetMasterPitch( wxCommandEvent& In );

	/** Called when the user wants to set the clip ID number for Particle Replay track keys */
	void OnParticleReplayKeyContext_SetClipIDNumber( wxCommandEvent& In );

	/** Called when the user wants to set the duration of Particle Replay track keys */
	void OnParticleReplayKeyContext_SetDuration( wxCommandEvent& In );

	/** Called to delete the currently selected keys */
	void OnDeleteSelectedKeys( wxCommandEvent& In );

	void OnMenuUndo( wxCommandEvent& In );
	void OnMenuRedo( wxCommandEvent& In );
	void OnMenuCut( wxCommandEvent& In );
	void OnMenuCopy( wxCommandEvent& In );
	void OnMenuPaste( wxCommandEvent& In );

	void OnMenuEdit_UpdateUI( wxUpdateUIEvent& In );

	void OnMenuImport( wxCommandEvent& In );
	void OnMenuExport( wxCommandEvent& In );
	void OnMenuReduceKeys( wxCommandEvent& In );

	/**
	 * Called when the user toggles the ability to export a key every frame. 
	 *
	 * @param	In	The wxWidgets event sent when the user clicks on the "Bake Transforms On Export" menu item.
	 */
	void OnToggleBakeTransforms( wxCommandEvent& In );

	/**
	 * Updates the checked-menu item for baking transforms
	 *
	 * @param	In	The wxWidgets event sent when the user clicks on the "Bake Transforms On Export" menu item.
	 */
	void OnToggleBakeTransforms_UpdateUI( wxUpdateUIEvent& In );

	/** Called when the 'Export Sound Cue Info' command is issued */
	void OnExportSoundCueInfoCommand( wxCommandEvent& In );
	
	/** Called when 'Export Animation Info' command is issued */
	void OnExportAnimationInfoCommand( wxCommandEvent& In );

	/** Toggles interting of the panning the interp editor left and right */
	void OnToggleInvertPan( wxCommandEvent& In );

	/** Called when a user selects the split translation and rotation option on a movement track */
	void OnSplitTranslationAndRotation( wxCommandEvent& In );

	/** Called when a user selects the normalize velocity option on a movement track */
	void NormalizeVelocity( wxCommandEvent& In );

	// Selection
	void SetSelectedFilter( class UInterpFilter* InFilter );

	/**
	 * Selects the interp track at the given index in the given group's array of interp tracks.
	 * If the track is already selected, this function will do nothing. 
	 *
	 * @param	OwningGroup		The group that stores the interp track to be selected. Cannot be NULL.
	 * @param	TrackToSelect		The interp track to select.
	 * @param	bDeselectPreviousTracks	If TRUE, then all previously-selected tracks will be deselected. Defaults to TRUE.
	 */
	void SelectTrack( UInterpGroup* Group, UInterpTrack* TrackToSelect, UBOOL bDeselectPreviousTracks = TRUE );

	/**
	 * Selects the given group.
	 *
	 * @param	GroupToSelect			The desired group to select. Cannot be NULL.
	 * @param	bDeselectPreviousGroups	If TRUE, then all previously-selected groups will be deselected. Defaults to TRUE.
	 */
	void SelectGroup( UInterpGroup* GroupToSelect, UBOOL bDeselectPreviousGroups = TRUE );

	/**
	 * Deselects the interp track stored in the given group at the given array index.
	 *
	 * @param	OwningGroup		The group holding the interp track to deselect.
	 * @param	TrackToDeselect		The track to deselect
	 * @param	bUpdateVisuals		If TRUE, then all affected visual components related to track selection will be updated. Defaults to TRUE.
	 */
	void DeselectTrack( UInterpGroup* Group, UInterpTrack* TrackToDeselect, UBOOL bUpdateVisuals = TRUE );

	/**
	 * Deselects every selected track. 
	 *
	 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to track selection will be updated. Defaults to TRUE.
	 */
	void DeselectAllTracks( UBOOL bUpdateVisuals = TRUE );

	/**
	 * Deselects the given group.
	 *
	 * @param	GroupToDeselect	The desired group to deselect.
	 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group selection will be updated. Defaults to TRUE.
	 */
	void DeselectGroup( UInterpGroup* GroupToDeselect, UBOOL bUpdateVisuals = TRUE );

	/**
	 * Deselects all selected groups.
	 *
	 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group selection will be updated. Defaults to TRUE.
	 */
	void DeselectAllGroups( UBOOL bUpdateVisuals = TRUE );

	/**
	 * Deselects all selected groups or selected tracks. 
	 *
	 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group and track selection will be updated. Defaults to TRUE.
	 */
	void DeselectAll( UBOOL bUpdateVisuals = TRUE );

	/**
	 * @return	TRUE if there is at least one selected group. FALSE, otherwise.
	 */
	UBOOL HasAGroupSelected() const;

	/**
	 * @param	GroupClass	The class type of interp group.
	 * @return	TRUE if there is at least one selected group. FALSE, otherwise.
	 */
	UBOOL HasAGroupSelected( const UClass* GroupClass ) const;

	/**
	 * @return	TRUE if at least one folder is selected; FALSE, otherwise.
	 */
	UBOOL HasAFolderSelected() const;

	/**
	 * @param	Group	Interp group to check if 
	 * @return	TRUE if at least one interp group is selected; FALSE, otherwise.
	 */
	FORCEINLINE UBOOL IsGroupSelected( const UInterpGroup* Group ) const
	{
		check( Group );
		return Group->bIsSelected;
	}

	/**
	 * @return	TRUE if every single selected group is a folder. 
	 */
	UBOOL AreAllSelectedGroupsFolders() const;

	/**
	 * @return	TRUE if every single selected group is parented.
	 */
	UBOOL AreAllSelectedGroupsParented() const;

	/**
	 * @return	TRUE if there is at least one track in the Matinee; FALSE, otherwise. 
	 */
	UBOOL HasATrack() const;
	
	/**
	 * @return	TRUE if there is at least one selected track. FALSE, otherwise.
	 */
	UBOOL HasATrackSelected() const;

	/**
	 * @param	TrackClass	The class type of interp track.  
	 * @return	TRUE if there is at least one selected track of the given class type. FALSE, otherwise.
	 */
	UBOOL HasATrackSelected( const UClass* TrackClass ) const;
	
	/**
	 * @param	OwningGroup	Interp group to check for selected tracks.
	 * @return	TRUE if at least one interp track selected owned by the given group; FALSE, otherwise.
	 */
	UBOOL HasATrackSelected( const UInterpGroup* OwningGroup ) const;

	/**
	 * @param	TrackClass	The class to check against each selected track.
	 * @return	TRUE if every single selected track is of the given UClass; FALSE, otherwise.
	 */
	UBOOL AreAllSelectedTracksOfClass( const UClass* TrackClass ) const;

	/**
	 * @param	OwningGroup	The group to check against each selected track.
	 * @return	TRUE if every single selected track is of owned by the given group; FALSE, otherwise.
	 */
	UBOOL AreAllSelectedTracksFromGroup( const UInterpGroup* OwningGroup ) const;

	/**
	 * @param	OwningGroup		The group associated to the track. Cannot be NULL.
	 * @param	TrackIndex		The index representing the interp track in an array of interp tracks stored in the given group. Cannot be INDEX_NONE.
	 * @return	TRUE if an interp track at the given index in the given group's interp track array is selected; FALSE, otherwise.
	 */
	/**
	 * @return	The number of the selected groups.
	 */
	INT GetSelectedGroupCount() const;

	/**
	 * @return	The number of selected tracks. 
	 */
	INT GetSelectedTrackCount() const;

	/**
	 * @return	A modifiable iterator that can iterate through all group entries, whether selected or not. 
	 */
	FGroupIterator GetGroupIterator()				{ return FGroupIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iterate through all group entries, whether selected or not. 
	 */
	FGroupConstIterator GetGroupIterator() const	{ return FGroupConstIterator(IData->InterpGroups); }
	
	/**
	 * @return	A modifiable iterator that can iteator over all selected interp groups.
	 */
	FSelectedGroupIterator GetSelectedGroupIterator()				{ return FSelectedGroupIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iteator over all selected interp groups.
	 */
	FSelectedGroupConstIterator GetSelectedGroupIterator() const	{ return FSelectedGroupConstIterator(IData->InterpGroups); }

	/**
	 * @return	A modifiable iterator that can iterate over all selected interp tracks.
	 */
	FSelectedTrackIterator GetSelectedTrackIterator()				{ return FSelectedTrackIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iterate over all selected interp tracks.
	 */
	FSelectedTrackConstIterator GetSelectedTrackIterator() const	{ return FSelectedTrackConstIterator(IData->InterpGroups); }

	/**
	 * @return	A modifiable iterator that can iterator over the selected interp tracks of the given template class.
	 */
	template<class TrackType> TTrackClassTypeIterator<TrackType> GetSelectedTrackIterator()
	{
		return TTrackClassTypeIterator<TrackType>(IData->InterpGroups);
	}

	/**
	 * @return	A non-modifiable iterator that can iterator over the selected interp tracks of the given template class.
	 */
	template<class TrackType> TTrackClassTypeConstIterator<TrackType> GetSelectedTrackIterator() const
	{
		return TTrackClassTypeConstIterator<TrackType>(IData->InterpGroups);
	}

	/**
	 * Locates the director group in our list of groups (if there is one)
	 *
	 * @param OutDirGroupIndex	The index of the director group in the list (if it was found)
	 *
	 * @return Returns true if a director group was found
	 */
	UBOOL FindDirectorGroup( INT& OutDirGroupIndex ) const;

	/**
	 * Remaps the specified group index such that the director's group appears as the first element
	 *
	 * @param DirGroupIndex	The index of the 'director group' in the group list
	 * @param ElementIndex	The original index into the group list
	 *
	 * @return Returns the reordered element index for the specified element index
	 */
	INT RemapGroupIndexForDirGroup( const INT DirGroupIndex, const INT ElementIndex ) const;

	/**
	 * Scrolls the view to the specified group if it is visible, otherwise it scrolls to the top of the screen.
	 *
	 * @param InGroup	Group to scroll the view to.
	 */
	void ScrollToGroup(class UInterpGroup* InGroup);

	/**
	 * Calculates the viewport vertical location for the given track.
	 *
	 * @note	This helper function is useful for determining if a track label is currently viewable.
	 *
	 * @param	InGroup				The group that owns the track.
	 * @param	InTrackIndex		The index of the track in the group's interp track array. 
	 * @param	LabelTopPosition	The viewport vertical location for the track's label top. 
	 * @param	LabelBottomPosition	The viewport vertical location for the track's label bottom. This is not the height of the label.
	 */
	void GetTrackLabelPositions( UInterpGroup* InGroup, INT InTrackIndex, INT& LabelTopPosition, INT& LabelBottomPosition ) const;
	
	/**
	 * Calculates the viewport vertical location for the given group.
	 *
	 * @param	InGroup				The group that owns the track.
	 * @param	LabelTopPosition	The viewport vertical location for the group's label top. 
	 * @param	LabelBottomPosition	The viewport vertical location for the group's label bottom. This is not the height of the label.
	 */
	void GetGroupLabelPosition( UInterpGroup* InGroup, INT& LabelTopPosition, INT& LabelBottomPosition ) const;

	/**
	 * Expands or collapses all visible groups in the track editor
	 *
	 * @param bExpand TRUE to expand all groups, or FALSE to collapse them all
	 */
	void ExpandOrCollapseAllVisibleGroups( const UBOOL bExpand );

	/**
	 * Updates the track window list scroll bar's vertical range to match the height of the window's content
	 */
	void UpdateTrackWindowScrollBars();

	/**
	 * Dirty the contents of the track window viewports
	 */
	void InvalidateTrackWindowViewports();

	/**
	 * Either shows or hides the director track window by splitting/unsplitting the parent window
	 */
	void UpdateDirectorTrackWindowVisibility();

	/**
	 * Updates the contents of the property window based on which groups or tracks are selected if any. 
	 */
	void UpdatePropertyWindow();

	/**
	 * Creates a string with timing/frame information for the specified time value in seconds
	 *
	 * @param InTime The time value to create a timecode for
	 * @param bIncludeMinutes TRUE if the returned string should includes minute information
	 *
	 * @return The timecode string
	 */
	FString MakeTimecodeString( FLOAT InTime, UBOOL bIncludeMinutes = TRUE ) const;

	/**
	 * Locates the specified group's parent group folder, if it has one
	 *
	 * @param ChildGroup The group who's parent we should search for
	 *
	 * @return Returns the parent group pointer or NULL if one wasn't found
	 */
	UInterpGroup* FindParentGroupFolder( UInterpGroup* ChildGroup ) const;

	/**
	 * Counts the number of children that the specified group folder has
	 *
	 * @param GroupFolder The group who's children we should count
	 *
	 * @return Returns the number of child groups
	 */
	INT CountGroupFolderChildren( UInterpGroup* const GroupFolder ) const;

	/**
	 * @param	InGroup	The group to check if its a parent or has a parent. 
	 * @return	A structure containing information about the given group's parent relationship.
	 */
	FInterpGroupParentInfo GetParentInfo( UInterpGroup* InGroup ) const;

	/**
	 * Determines if the child candidate can be parented (or re-parented) by the parent candiddate.
	 *
	 * @param	ChildCandidate	The group that desires to become the child to the parent candidate.
	 * @param	ParentCandidate	The group that, if a folder, desires to parent the child candidate.
	 *
	 * @return	TRUE if the parent candidate can parent the child candidate. 
	 */
	UBOOL CanReparent( const FInterpGroupParentInfo& ChildCandidate, const FInterpGroupParentInfo& ParentCandidate ) const;

	/**
	 * Fixes up any problems in the folder/group hierarchy caused by bad parenting in previous builds
	 */
	void RepairHierarchyProblems();


	UBOOL KeyIsInSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex);
	void AddKeyToSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex, UBOOL bAutoWind);
	void RemoveKeyFromSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex);
	void ClearKeySelection();
	void CalcSelectedKeyRange(FLOAT& OutStartTime, FLOAT& OutEndTime);
	void SelectKeysInLoopSection();

	/**
	 * Clears all selected key of a given track.
	 *
	 * @param	OwningGroup			The group that owns the track containing the keys. 
	 * @param	Track				The track holding the keys to clear. 
	 * @param	bInvalidateDisplay	Sets the Matinee track viewport to refresh (Defaults to TRUE).
	 */
	void ClearKeySelectionForTrack( UInterpGroup* OwningGroup, UInterpTrack* Track, UBOOL bInvalidateDisplay = TRUE );

	//Deletes keys if they are selected, otherwise will deleted selected tracks or groups
	void DeleteSelection (void);

	// Utils
	void DeleteSelectedKeys(UBOOL bDoTransaction=false);
	void DuplicateSelectedKeys();
	void BeginMoveSelectedKeys();
	void EndMoveSelectedKeys();
	void MoveSelectedKeys(FLOAT DeltaTime);

	/**
	 * Adds a keyframe to the selected track.
	 *
	 * There must be one and only one track selected for a keyframe to be added.
	 */
	void AddKey();

	/** 
	 * Call utility to split an animation in the selected AnimControl track. 
	 *
	 * Only one interp track can be selected and it must be a anim control track for the function.
	 */
	void SplitAnimKey();
	void ReduceKeys();
	void ReduceKeysForTrack( UInterpTrack* Track, UInterpTrackInst* TrackInst, FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance );

	void ViewFitSequence();
	void ViewFitToSelected();
	void ViewFitLoop();
	void ViewFitLoopSequence();
	void ViewEndOfTrack();
	void ViewFit(FLOAT StartTime, FLOAT EndTime);

	void ChangeKeyInterpMode(EInterpCurveMode NewInterpMode=CIM_Unknown);

	/**
	 * Copies the currently selected group/group.
	 *
	 * @param bCut	Whether or not we should cut instead of simply copying the group/track.
	 */
	void CopySelectedGroupOrTrack(UBOOL bCut);

	/**
	 * Pastes the previously copied group/track.
	 */
	void PasteSelectedGroupOrTrack();

	/**
	 * @return Whether or not we can paste a group/track.
	 */
	UBOOL CanPasteGroupOrTrack();

	/**
	 * Duplicates the specified group
	 *
	 * @param GroupToDuplicate		Group we are going to duplicate.
	 */
	virtual void DuplicateGroup(UInterpGroup* GroupToDuplicate);

	/** Duplicates selected tracks in their respective groups and clears them to begin real time recording, and selects them */
	void DuplicateSelectedTracksForRecording (const UBOOL bInDeleteSelectedTracks);

	/** Polls state of controller (Game Caster, Game Pad) and adjust each track by that input*/
	void AdjustRecordingTracksByInput(void);

	/**Used during recording to capture a key frame at the current position of the timeline*/
	void RecordKeys(void);

	/**Store off parent positions so we can apply the parents delta of movement to the child*/
	void SaveRecordingParentOffsets(void);

	/**Apply the movement of the parent to child during recording*/
	void ApplyRecordingParentOffsets(void);

	/**
	 * Returns the custom recording viewport if it has been created yet
	 * @return - NULL, if no record viewport exists yet, or the current recording viewport
	 */
	FEditorLevelViewportClient* GetRecordingViewport(void);

	/**
	 * Adds a new track to the specified group.
	 *
	 * @param Group The group to add a track to
	 * @param TrackClass The class of track object we are going to add.
	 * @param TrackToCopy A optional track to copy instead of instantiating a new one.
	 * @param bAllowPrompts TRUE if we should allow a dialog to be summoned to ask for initial information
	 * @param OutNewTrackIndex [Out] The index of the newly created track in its parent group
	 * @param bSelectTrack TRUE if we should select the track after adding it
	 *
	 * @return Returns newly created track (or NULL if failed)
	 */
	UInterpTrack* AddTrackToGroup( UInterpGroup* Group, UClass* TrackClass, UInterpTrack* TrackToCopy, UBOOL bAllowPrompts, INT& OutNewTrackIndex, UBOOL bSelectTrack = TRUE );

	/**
	 * Adds a new track to the selected group.
	 *
	 * @param TrackClass		The class of the track we are adding.
	 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
	 * @param bSelectTrack		If TRUE, select the track after adding it
	 *
	 * @return	The newly-created track if created; NULL, otherwise. 
	 */
	UInterpTrack* AddTrackToSelectedGroup(UClass* TrackClass, UInterpTrack* TrackToCopy=NULL, UBOOL bSelectTrack = TRUE);

	/**
	 * Adds a new track to a group and appropriately updates/refreshes the editor
	 *
	 * @param Group				The group to add this track to
	 * @param NewTrackName		The default name of the new track to add
	 * @param TrackClass		The class of the track we are adding.
	 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
	 * @param bSelectTrack		If TRUE, select the track after adding it
	 * return					New interp track that was created
	 */
	UInterpTrack*  AddTrackToGroupAndRefresh(UInterpGroup* Group, const FString& NewTrackName, UClass* TrackClass, UInterpTrack* TrackToCopy=NULL, UBOOL bSelectTrack = TRUE );

	/**
	 * Disables all tracks of a class type in this group
	 * @param Group - group in which to disable tracks of TrackClass type
	 * @param TrackClass - Type of track to disable
	 */
	void DisableTracksOfClass(UInterpGroup* Group, UClass* TrackClass);

	/** 
	 * Crops the anim key in the currently selected track. 
	 *
	 * @param	bCropBeginning		Whether to crop the section before the position marker or after.
	 */
	void CropAnimKey(UBOOL bCropBeginning);

	/**
	 * Sets the global property name to use for newly created property tracks
	 *
	 * @param NewName The property name
	 */
	static void SetTrackAddPropName( const FName NewName );


	void UpdateMatineeActionConnectors();
	void LockCamToGroup(class UInterpGroup* InGroup, const UBOOL bResetViewports=TRUE);
	class AActor* GetViewedActor();
	virtual void UpdateCameraToGroup(const UBOOL bInUpdateStandardViewports);
	void UpdateCamColours();

	/**
	 * Updates a viewport from a given actor
	 * @param InActor - The actor to track the viewport to
	 * @param InViewportClient - The viewport to update
	 * @param InFadeAmount - Fade amount for camera
	 * @param InColorScale - Color scale for render
	 * @param bInEnableColorScaling - whether to use color scaling or not
	 */
	void UpdateLevelViewport(AActor* InActor, FEditorLevelViewportClient* InViewportClient, const FRenderingPerformanceOverrides& RenderingOverrides, const FLOAT InFadeAmount, const FVector& InColorScale, const UBOOL bInEnableColorScaling);

	/** Restores a viewport's settings that were overridden by UpdateLevelViewport, where necessary. */
	void RestoreLevelViewport(FEditorLevelViewportClient* InViewportClient);

	void SyncCurveEdView();

	/**
	 * Adds this Track to the curve editor
	 * @param GroupName - Name of this group
	 * @param GroupColor - Color to use for the curve
	 * @param InTrack - The track
	 */
	void AddTrackToCurveEd( FString GroupName, FColor GroupColor, UInterpTrack* InTrack, UBOOL bShouldShowTrack );

	void SetInterpPosition(FLOAT NewPosition);

	/** Refresh the Matinee position marker and viewport state */
	void RefreshInterpPosition();

	/** Make sure particle replay tracks have up-to-date editor-only transient state */
	void UpdateParticleReplayTracks();

	void SelectActiveGroupParent();

	/** Increments the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
	void IncrementSelection();

	/** Decrements the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
	void DecrementSelection();

	void SelectNextKey();
	void SelectPreviousKey();

	/**
	 * Zooms the curve editor and track editor in or out by the specified amount
	 *
	 * @param ZoomAmount			Amount to zoom in or out
	 * @param bZoomToTimeCursorPos	True if we should zoom to the time cursor position, otherwise mouse cursor position
	 */
	void ZoomView( FLOAT ZoomAmount, UBOOL bZoomToTimeCursorPos );

	/** Toggles fixed time step playback mode */
	void SetFixedTimeStepPlayback( UBOOL bInValue );

	/** Updates 'fixed time step' mode based on current playback state and user preferences */
	void UpdateFixedTimeStepPlayback();

	/** Toggles 'prefer frame number' setting */
	void SetPreferFrameNumbers( UBOOL bInValue );

	/** Toggles 'show time cursor pos for all keys' setting */
	void SetShowTimeCursorPosForAllKeys( UBOOL bInValue );

	void SetSnapEnabled(UBOOL bInSnapEnabled);
	
	/** Toggles snapping the current timeline position to 'frames' in Matinee. */
	void SetSnapTimeToFrames( UBOOL bInValue );

	/** Snaps the specified time value to the closest frame */
	FLOAT SnapTimeToNearestFrame( FLOAT InTime ) const;

	FLOAT SnapTime(FLOAT InTime, UBOOL bIgnoreSelectedKeys);

	void BeginMoveMarker();
	void EndMoveMarker();
	void SetInterpEnd(FLOAT NewInterpLength);
	void MoveLoopMarker(FLOAT NewMarkerPos, UBOOL bIsStart);

	void BeginDrag3DHandle(UInterpGroup* Group, INT TrackIndex);
	void Move3DHandle(UInterpGroup* Group, INT TrackIndex, INT KeyIndex, UBOOL bArriving, const FVector& Delta);
	void EndDrag3DHandle();
	void MoveInitialPosition(const FVector& Delta, const FRotator& DeltaRot);

	void ActorModified();
	void ActorSelectionChange();
	void CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation);
	UBOOL ProcessKeyPress(FName Key, UBOOL bCtrlDown, UBOOL bAltDown);

	void InterpEdUndo();
	void InterpEdRedo();

	void MoveActiveBy(INT MoveBy);
	void MoveActiveUp();
	void MoveActiveDown();

	void DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawModeHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	void TickInterp(FLOAT DeltaSeconds);

	/** Called from TickInterp, handles the ticking of the camera recording (game caster, xbox controller)*/
	void UpdateCameraRecording (void);

	/** Constrains the maximum frame rate to the fixed time step rate when playing back in that mode */
	void ConstrainFixedTimeStepFrameRate();

	/**
	 * Updates the initial rotation-translation matrix for the 
	 * given track's inst if the track is a movement track.
	 *
	 * @param	OwningGroup	The group that owns the track to update. 
	 * @param	InMoveTrack	The move track to update
	 */
	void UpdateInitialTransformForMoveTrack( UInterpGroup* OwningGroup, UInterpTrackMove* InMoveTrack );

	static void UpdateAttachedLocations(AActor* BaseActor);
	static void InitInterpTrackClasses();

	/** 
	 * Sets the realtime audio override on the perspective viewport in the editor.
	 *
	 * @param bAudioIsRealtime	TRUE if audio should be realtime
	 * @param bSaveExisting	TRUE if we should save the existing settings so we can restore them later
	 */
	void SetAudioRealtimeOverride( UBOOL bAudioIsRealtime, UBOOL bSaveExisting = FALSE ) const;

	/** Restores saved audio realtime settings */
	void RestoreAudioRealtimeOverride() const;

	WxInterpEdToolBar* ToolBar;
	WxInterpEdMenuBar* MenuBar;

	wxSplitterWindow* GraphSplitterWnd; // Divides the graph from the track view.
	INT GraphSplitPos;

	/** The property window (dockable) */
	WxPropertyWindowHost* PropertyWindow;

	/** The curve editor window (dockable) */
	WxCurveEditor* CurveEd;

	/** Director track editor window (upper split of main pane) */
	WxInterpEdVCHolder* DirectorTrackWindow;

	/** Main track editor window (lower split of main pane) */
	WxInterpEdVCHolder* TrackWindow;

	UTexture2D*	BarGradText;
	FColor PosMarkerColor;
	FColor RegionFillColor;
	FColor RegionBorderColor;

	class USeqAct_Interp* Interp;
	class UInterpData* IData;

	// If we are connecting the camera to a particular group, this is it. If not, its NULL;
	class UInterpGroup* CamViewGroup;

	// Editor-specific Object, containing preferences and selection set to be serialised/undone.
	UInterpEdOptions* Opt;

	// If we are looping 
	UBOOL bLoopingSection;

	/** The real-time that we started playback last */
	DOUBLE PlaybackStartRealTime;

	/** Number of continuous fixed time step frames we've played so far without any change in play back state,
	    such as time step, reverse mode, etc. */
	UINT NumContinuousFixedTimeStepFrames;

	// Currently moving a curve handle in the 3D viewport.
	UBOOL bDragging3DHandle;

	// Multiplier for preview playback of sequence
	FLOAT PlaybackSpeed;

	// Whether to draw the 3D version of any tracks.
	UBOOL bHide3DTrackView;

	/** Indicates if zoom should auto-center on the current scrub position. */
	UBOOL bZoomToScrubPos;

	/** Window menu item for toggling the curve editor */
	wxMenuItem* CurveEdToggleMenuItem;

	/** Snap settings. */
	UBOOL bSnapEnabled;
	UBOOL bSnapToKeys;
	UBOOL bSnapToFrames;
	FLOAT SnapAmount;

	/** True if the interp timeline position should be be snapped to the Matinee frame rate */
	UBOOL bSnapTimeToFrames;

	/** True if fixed time step playback is enabled */
	UBOOL bFixedTimeStepPlayback;
	
	/** True if the user prefers frame numbers to be drawn on track key labels (instead of time values) */
	UBOOL bPreferFrameNumbers;

	/** True if we should draw the position of the time cursor relative to the start of each key right
	    next to time cursor in the track view */
	UBOOL bShowTimeCursorPosForAllKeys;

	/** Initial curve interpolation mode for newly created keys.  This is loaded and saved to/from the user's
	  * editor preference file. */
	EInterpCurveMode InitialInterpMode;

	UTransactor* NormalTransactor;
	UInterpEdTransBuffer* InterpEdTrans;

	/** Set to TRUE in OnClose, at which point the editor is no longer ticked. */
	UBOOL	bClosed;

	/** If TRUE, the editor is modifying a CameraAnim, and functionality is tweaked appropriately */
	UBOOL	bEditingCameraAnim;

	// Static list of all InterpTrack subclasses.
	static TArray<UClass*>	InterpTrackClasses;
	static UBOOL			bInterpTrackClassesInitialized;

	// Used to convert between seconds and size on the timeline
	INT		TrackViewSizeX;
	FLOAT	PixelsPerSec;
	FLOAT	NavPixelsPerSecond;

	FLOAT	ViewStartTime;
	FLOAT	ViewEndTime;

	EInterpEdMarkerType	GrabbedMarkerType;

	UBOOL	bDrawSnappingLine;
	FLOAT	SnappingLinePosition;
	FLOAT	UnsnappedMarkerPos;

	/** Width of track editor labels on left hand side */
	INT LabelWidth;

	/** Creates a popup context menu based on the item under the mouse cursor.
	* @param	Viewport	FViewport for the FInterpEdViewportClient.
	* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
	* @return	A new wxMenu with context-appropriate menu options or NULL if there are no appropriate menu options.
	*/
	virtual wxMenu	*CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult );

	/** Returns TRUE if Matinee is fully initialized */
	UBOOL IsInitialized() const
	{
		return bIsInitialized;
	}

	/** Returns TRUE if viewport frame stats are currently enabled */
	UBOOL IsViewportFrameStatsEnabled() const
	{
		return bViewportFrameStatsEnabled;
	}

	/** Returns the size that the editing grid should be based on user settings */
	INT GetEditingGridSize() const
	{
		return EditingGridSize;
	}

	/** Returns TRUE if the crosshair should be visible in matinee preview viewports */
	UBOOL IsEditingCrosshairEnabled() const
	{
		return bEditingCrosshairEnabled;
	}

	/** Returns TRUE if the editing grid should be enabled */
	UBOOL IsEditingGridEnabled() const
	{
		return bEditingGridEnabled;
	}

	/** Toggles whether or not to display the menu*/
	void ToggleRecordMenuDisplay (void) 
	{
		bDisplayRecordingMenu = !bDisplayRecordingMenu;
	}

	/**Preps Matinee to record/stop-recording realtime camera movement*/
	void ToggleRecordInterpValues(void);

	/**If TRUE, real time camera recording mode has been enabled*/
	UBOOL IsRecordingInterpValues (void) const;

	/**Helper function to properly shut down matinee recording*/
	void StopRecordingInterpValues(void);

	/**
	 * Increments or decrements the currently selected recording menu item
	 * @param bInNext - TRUE if going forward in the menu system, FALSE if going backward
	 */
	void ChangeRecordingMenu(const UBOOL bInNext);

	/**
	 * Increases or decreases the recording menu value
	 * @param bInIncrease - TRUE if increasing the value, FALSE if decreasing the value
	 */
	void ChangeRecordingMenuValue(FEditorLevelViewportClient* InClient, const UBOOL bInIncrease);

	/**
	 * Resets the recording menu value to the default
	 */
	void ResetRecordingMenuValue(FEditorLevelViewportClient* InClient);

	/**
	 * Determines whether only the first click event is allowed or all repeat events are allowed
	 * @return - TRUE, if the value should change multiple times.  FALSE, if the user should have to release and reclick
	 */
	UBOOL IsRecordMenuChangeAllowedRepeat (void) const;
	
	/** Sets the record mode for matinee */
	void SetRecordMode(const UINT InNewMode);

	/** Returns The time that sampling should start at */
	const DOUBLE GetRecordingStartTime (void) const;

	/** Returns The time that sampling should end at */
	const DOUBLE GetRecordingEndTime (void) const;

	/**Return the number of samples we're keeping around for roll smoothing*/
	INT GetNumRecordRollSmoothingSamples (void) const { return RecordRollSmoothingSamples; }
	/**Return the number of samples we're keeping around for roll smoothing*/
	INT GetNumRecordPitchSmoothingSamples (void) const { return RecordPitchSmoothingSamples; }
	/**Returns the current movement scheme we're using for the camera*/
	INT GetCameraMovementScheme(void) const { return RecordCameraMovementScheme; }

	/** Save record settings for next run */
	void SaveRecordingSettings(const FCameraControllerConfig& InCameraConfig);
	/** Load record settings for next run */
	void LoadRecordingSettings(FCameraControllerConfig& InCameraConfig);


	/**
	 * Access function to appropriate camera actor
	 * @param InCameraIndex - The index of the camera actor to return
	 * 
	 */
	ACameraActor* GetCameraActor(const INT InCameraIndex);
	/**
	 * Access function to return the number of used camera actors
	 */
	INT GetNumCameraActors(void) const;

	/**
	 * Adds extra non-wx viewport
	 * @param InViewport - The viewport to update
	 * @param InActor - The actor the viewport is supposed to follow
	 */
	void AddExtraViewport(FEditorLevelViewportClient* InViewport, AActor* InActor);

	/**
	 * Simple accessor for the user's preference on whether clicking on a keyframe bar should trigger
	 * a selection or not
	 *
	 * @return	TRUE if a click on a keyframe bar should cause a selection; FALSE if it should not
	 */
	UBOOL IsKeyframeBarSelectionAllowed() const 
	{ 
		return bAllowKeyframeBarSelection; 
	}

	/**
	 * Simple accessor for the user's preference on whether clicking on keyframe text should trigger
	 * a selection or not
	 *
	 * @return	TRUE if a click on keyframe text should cause a selection; FALSE if it should not
	 */
	UBOOL IsKeyframeTextSelectionAllowed() const
	{
		return bAllowKeyframeTextSelection;
	}

protected:
	
	/** TRUE if Matinee is fully initialized */
	UBOOL bIsInitialized;

	/** TRUE if viewport frame stats are currently enabled */
	UBOOL bViewportFrameStatsEnabled;

	/** TRUE if the viewport editing crosshair is enabled */
	UBOOL bEditingCrosshairEnabled;

	/** TRUE if the editing grid is enabled */
	UBOOL bEditingGridEnabled;

	/** When TRUE, a key will be exported every frame instead of just the keys that user created. */
	UBOOL bBakeTransforms;

	/** If TRUE, clicking on a keyframe bar (such as one representing the duration of an audio cue, etc.) will cause a selection */
	UBOOL bAllowKeyframeBarSelection;

	/** If TRUE, clicking on text associated with a keyframe with cause a selection */
	UBOOL bAllowKeyframeTextSelection;

	/** The size of the editing grid (in number of vertical and horizontal sections) when the editing grid is displayed. 0 if no editing grid. */
	INT EditingGridSize;

	/**
	 * Called when the user toggles the preference for allowing clicks on keyframe "bars" to cause a selection
	 *
	 * @param	In	Event generated by wxWidgets when the user toggles the preference
	 */
	void OnToggleKeyframeBarSelection( wxCommandEvent& In );

	/**
	 * Called automatically by wxWidgets to update the UI for the keyframe bar selection option
	 *
	 * @param	In	Event generated by wxWidgets to update the UI	
	 */
	void OnToggleKeyframeBarSelection_UpdateUI( wxUpdateUIEvent& In );

	/**
	 * Called when the user toggles the preference for allowing clicks on keyframe text to cause a selection
	 *
	 * @param	In	Event generated by wxWidgets when the user toggles the preference
	 */
	void OnToggleKeyframeTextSelection( wxCommandEvent& In );

	/**
	 * Called automatically by wxWidgets to update the UI for the keyframe text selection option
	 *
	 * @param	In	Event generated by wxWidgets to update the UI
	 */
	void OnToggleKeyframeTextSelection_UpdateUI( wxUpdateUIEvent& In );

	/**
	 * Utility function for gathering all the selected tracks into a TArray.
	 *
	 * @param	OutSelectedTracks	[out] An array of all interp tracks that are currently selected.
	 */
	void GetSelectedTracks( TArray<UInterpTrack*>& OutSelectedTracks );
	
	/**
	 * Utility function for gathering all the selected groups into a TArray.
	 *
	 * @param	OutSelectedGroups	[out] An array of all interp groups that are currently selected.
	 */
	void GetSelectedGroups( TArray<UInterpGroup*>& OutSelectedGroups );

	/** 
	 * Deletes the currently active track. 
	 */
	void DeleteSelectedTracks();

	/** 
	 * Deletes all selected groups.
	 */
	void DeleteSelectedGroups();

	/**
	 * Moves the marker the user grabbed to the given time on the timeline. 
	 *
	 * @param	MarkerType	The marker to move.
	 * @param	InterpTime	The position on the timeline to move the marker. 
	 */
	void MoveGrabbedMarker( FLOAT InterpTime );

	/**
	 * Calculates The timeline position of the longest track, which includes 
	 *			the duration of any assets such as: sounds or animations.
	 *
	 * @note	Use the template parameter to define which tracks to consider (all, selected only, etc).
	 * 
	 * @return	The timeline position of the longest track.
	 */
	template <class TrackFilterType>
	FLOAT GetLongestTrackTime() const;

	/**
	 * Selects the group actor associated to the given interp group. 
	 *
	 * @param	AssociatedGroup	The group corresponding to the referenced actor to select. 
	 */
	void SelectGroupActor( UInterpGroup* AssociatedGroup );

	/**
	 * Deselects the group actor associated to the given interp group. 
	 *
	 * @param	AssociatedGroup	The group corresponding to the referenced actor to deselect. 
	 */
	void DeselectGroupActor( UInterpGroup* AssociatedGroup );

	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	/**
	 * Helper function to refresh the Kismet window associated with the editor's USeqAct_Interp
	 */
	void RefreshAssociatedKismetWindow() const;

	/**Recording Menu Selection State*/
	INT RecordMenuSelection;

	//whether or not to display the menu during a recording session
	UBOOL bDisplayRecordingMenu;

	/**State of camera recording (countdown, recording, reprep)*/
	UINT RecordingState;

	/**Mode of recording. See MatineeConstants*/
	INT RecordMode;

	/**Number of samples for roll*/
	INT RecordRollSmoothingSamples;
	/**Number of samples for pitch*/
	INT RecordPitchSmoothingSamples;
	/**Camera Movement Scheme (free fly, planar/sky cam)*/
	INT RecordCameraMovementScheme;

	/**The time the current camera recording state got changed (when did the countdown start)*/
	DOUBLE RecordingStateStartTime;

	/**Tracks that are actively listening to controller input and sampling live key frames*/
	TArray <UInterpTrack*> RecordingTracks;

	/**Scratch pad for saving parent offsets for relative movement*/
	TMap<AActor*, FVector> RecordingParentOffsets;

	/**Map of non-wx viewports and the actors that the viewports should follow*/
	TMap<FEditorLevelViewportClient*, AActor*> ExtraViewports;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdTabMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdTabMenu : public wxMenu
{
public:
	WxMBInterpEdTabMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdTabMenu();
};


/*-----------------------------------------------------------------------------
	WxMBInterpEdGroupMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdGroupMenu : public wxMenu
{
public:
	WxMBInterpEdGroupMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdGroupMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdTrackMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdTrackMenu : public wxMenu
{
public:
	WxMBInterpEdTrackMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdTrackMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdBkgMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdBkgMenu : public wxMenu
{
public:
	WxMBInterpEdBkgMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdBkgMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdKeyMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdKeyMenu : public wxMenu
{
public:
	WxMBInterpEdKeyMenu( WxInterpEd* InterpEd );
	~WxMBInterpEdKeyMenu();
};


/*-----------------------------------------------------------------------------
	WxMBInterpEdCollapseExpandMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdCollapseExpandMenu : public wxMenu
{
public:
	WxMBInterpEdCollapseExpandMenu(WxInterpEd* InterpEd);
	virtual ~WxMBInterpEdCollapseExpandMenu();
};



/*-----------------------------------------------------------------------------
	WxCameraAnimEd
-----------------------------------------------------------------------------*/

/** A specialized version of WxInterpEd used for CameraAnim editing.  Tangential features of Matinee are disabled. */
class WxCameraAnimEd : public WxInterpEd
{
public:
	WxCameraAnimEd( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp );
	virtual ~WxCameraAnimEd();

	virtual void	OnClose( wxCloseEvent& In );

	/** Creates a popup context menu based on the item under the mouse cursor.
	* @param	Viewport	FViewport for the FInterpEdViewportClient.
	* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
	* @return	A new wxMenu with context-appropriate menu options or NULL if there are no appropriate menu options.
	*/
	virtual wxMenu	*CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult );

	/**
	 * Duplicates the specified group
	 *
	 * @param GroupToDuplicate		Group we are going to duplicate.
	 */
	virtual void DuplicateGroup(UInterpGroup* GroupToDuplicate);

private:
	/** Initialize preview info for camera anim **/
	void			InitializePreviewSets();
	UInterpGroup*	CreateInterpGroup(FCameraPreviewInfo& PreviewInfo);
	APawn *			CreatePreviewPawn(UClass * PreviewPawnClass, const FVector & Loc, const FRotator & Rot);
	TArray<APawn*>	PreviewPawns;
};


/*-----------------------------------------------------------------------------
	WxMBCameraAnimEdGroupMenu
-----------------------------------------------------------------------------*/

class WxMBCameraAnimEdGroupMenu : public wxMenu
{
public:
	WxMBCameraAnimEdGroupMenu(WxCameraAnimEd* CamAnimEd);
	~WxMBCameraAnimEdGroupMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdMarkerMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdMarkerMenu : public wxMenu
{
public:

	/**
	 * Default constructor.
	 * Create a context menu with menu items based on the type of marker clicked-on.
	 *
	 * @param	InterpEd	The interp editor.
	 * @param	MarkerType	The type of marker right-clicked on.
	 */
	WxMBInterpEdMarkerMenu( WxInterpEd* InterpEd, EInterpEdMarkerType MarkerType );

	/**
	 * Destructor.
	 */
	~WxMBInterpEdMarkerMenu();
};

#endif // __INTERPEDITOR_H__
