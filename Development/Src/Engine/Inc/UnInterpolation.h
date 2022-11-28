/*=============================================================================
	UnInterpolation.h: Matinee related C++ declarations
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by James Golding
=============================================================================*/

class UInterpTrack : public UObject, public FCurveEdInterface
{
public:
    class UClass* TrackInstClass;
    FStringNoInit TrackTitle;
    BITFIELD bOnePerGroup:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bDirGroupOnly:1;
    DECLARE_CLASS(UInterpTrack,UObject,0,Engine)

	// InterpTrack interface

	/** Total number of keyframes in this track. */
	virtual INT GetNumKeyframes() { return 0; }

	/** Get first and last time of keyframes in this track. */
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) { StartTime = 0.f; EndTime = 0.f; }

	/** Get the time of the keyframe with the given index. */
	virtual FLOAT GetKeyframeTime(INT KeyIndex) {return 0.f; }

	/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
	virtual INT AddKeyframe(FLOAT Time, class UInterpTrackInst* TrInst) { return INDEX_NONE; }

	/** Change the value of an existing keyframe. */
	virtual void UpdateKeyframe(INT KeyIndex, class UInterpTrackInst* TrInst) {}

	/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
	virtual int SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true) { return INDEX_NONE; }

	/** Remove the keyframe with the given index. */
	virtual void RemoveKeyframe(INT KeyIndex) {}

	/** 
	 *	Duplicate the keyframe with the given index to the specified time. 
	 *	Returns the index of the newly created key.
	 */
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime) { return INDEX_NONE; }

	/** 
	 *	Function which actually updates things based on the new position in the track. 
	 *  This is called in the editor, when scrubbing/previewing etc.
	 */
	virtual void PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst) {} // This is called in the editor, when scrubbing/previewing etc

	/** 
	 *	Function which actually updates things based on the new position in the track. 
	 *  This is called in the game, when USeqAct_Interp is ticked
	 *  @param bJump	Indicates if this is a sudden jump instead of a smooth move to the new position.
	 */
	virtual void UpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst, UBOOL bJump=false) {} // This is called in the game, when USeqAct_Interp is ticked


	/** 
	 *	Assumes RI->Origin is in the correct place to start drawing this track.
	 *	SelectedKeyIndex is INDEX_NONE if no key is selected.
	 */
	virtual void DrawTrack(FRenderInterface* RI, INT TrackIndex, INT TrackWidth, INT TrackHeight, FLOAT StartTime, FLOAT PixelsPerSec, TArray<class FInterpEdSelKey>& SelectedKeys);

	/** Return color to draw each keyframe in Matinee. */
	virtual FColor GetKeyframeColor(INT KeyIndex);

	/**
	 *	For drawing track information into the 3D scene.
	 *	TimeRes is how often to draw an event (eg. resoltion of spline path) in seconds.
	 */
	virtual void Render3DTrack(class UInterpTrackInst* TrInst, 
		const FSceneContext& Context, 
		FPrimitiveRenderInterface* PRI, 
		INT TrackIndex, 
		const FColor& TrackColor, 
		TArray<class FInterpEdSelKey>& SelectedKeys) {}

	/** Set this track to sensible default values. Called when track is first created. */
	virtual void SetTrackToSensibleDefault() {}
};