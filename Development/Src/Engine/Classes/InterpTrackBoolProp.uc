/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackBoolProp extends InterpTrack
	native(Interpolation);

/** Information for one event in the track. */
struct native BoolTrackKey
{
	var		float	Time;
	var()	bool	Value;
};	

/** Array of booleans to set. */
var	array<BoolTrackKey>	BoolTrack;

/** Name of property in Group Actor which this track will modify over time. */
var()	editconst	name		PropertyName;

cpptext
{
	/** Returns the property name */
	virtual UBOOL GetPropertyName( FName& PropertyNameOut ) const { PropertyNameOut = PropertyName; return TRUE; }

	/**
	 * @return  The number of keyframes currently in this track.
	 */
	virtual INT GetNumKeyframes() const;

	/**
	 * Gathers the range that spans all keyframes.
	 * 
	 * @param   StartTime   [out] The time of the first keyframe on this track.
	 * @param   EndTime     [out] The time of the last keyframe on this track. 
	 */
	virtual void GetTimeRange( FLOAT& StartTime, FLOAT& EndTime ) const;
	
	/**
	 * @return	The ending time of the track. 
	 */
	virtual FLOAT GetTrackEndTime() const;

	/**
	 * @param   KeyIndex    The index of the key to retrieve the time in the track's key array. 
	 * 
	 * @return  The time of the given key in the track on the timeline. 
	 */
	virtual FLOAT GetKeyframeTime( INT KeyIndex ) const;

	/**
	 * Changes the time of the given key with the new given time.
	 * 
	 * @param   KeyIndex        The index key to change in the track's key array.
	 * @param   NewKeyTime      The new time for the given key in the timeline.
	 * @param   bUpdateOrder    When TRUE, moves the key to be in chronological order in the array. 
	 * 
	 * @return  The new index for the given key. 
	 */
	virtual INT SetKeyframeTime( INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder = TRUE );

	/**
	 * Removes the given key from the array of keys in the track.
	 * 
	 * @param   KeyIndex    The index of the key to remove in this track's array of keys. 
	 */
	virtual void RemoveKeyframe( INT KeyIndex );

	/**
	 * Duplicates the given key.
	 * 
	 * @param   KeyIndex    The index of the key to duplicate in this track's array of keys.
	 * @param   NewKeyTime  The time to assign to the duplicated key.
	 * 
	 * @return  The new index for the given key.
	 */
	virtual INT DuplicateKeyframe( INT KeyIndex, FLOAT NewKeyTime );

	/**
	 * Gets the position of the closest key with snapping incorporated.
	 * 
	 * @param   InPosition  The current position in the timeline.
	 * @param   IgnoreKeys  The set of keys to ignore when searching for the closest key.
	 * @param   OutPosition The position of the closest key with respect to snapping and ignoring the given set of keys.
	 * 
	 * @return  TRUE if a keyframe was found; FALSE if no keyframe was found. 
	 */
	virtual UBOOL GetClosestSnapPosition( FLOAT InPosition, TArray<INT>& IgnoreKeys, FLOAT& OutPosition );

	/**
	 * Adds a keyframe at the given time to the track.
	 * 
	 * @param	Time			The time to place the key in the track timeline.
	 * @param	TrackInst		The instance of this track. 
	 * @param	InitInterpMode	The interp mode of the newly-added keyframe.
	 */
	virtual INT AddKeyframe( FLOAT Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode );

	/**
	 * Changes the value of an existing keyframe.
	 *
	 * @param	KeyIndex	The index of the key to update in the track's key array. 
	 * @param	TrackInst	The instance of this track to update. 
	 */
	virtual void UpdateKeyframe( INT KeyIndex, UInterpTrackInst* TrackInst );

	/**
	 * Updates the instance of this track based on the new position. This is for editor preview.
	 *
	 * @param	NewPosition	The position of the track in the timeline. 
	 * @param	TrackInst	The instance of this track to update. 
	 */
	virtual void PreviewUpdateTrack( FLOAT NewPosition, UInterpTrackInst* TrackInst );

	/** 
	 * Updates the instance of this track based on the new position. This is called in the game, when USeqAct_Interp is ticked.
	 *
	 * @param	NewPosition	The position of the track in the timeline. 
	 * @param	TrackInst	The instance of this track to update. 
	 * @param	bJump		Indicates if this is a sudden jump instead of a smooth move to the new position.
	 */
	virtual void UpdateTrack( FLOAT NewPosition, UInterpTrackInst* TrackInst, UBOOL bJump );

	/** 
	 * @return  TRUE if this track type works with static actors; FALSE, otherwise.
	 */
	virtual UBOOL AllowStaticActors() { return TRUE; }
	
	/** 
	 * Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	 * 
	 * @return	String name of the helper class.
	 */
	virtual const FString GetEdHelperClassName() const;

	/** 
	 * @return	The icon to draw for this track in Matinee. 
	 */
	virtual class UMaterial* GetTrackIcon() const;
}


defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstBoolProp'
	TrackTitle="Bool Property"
}
