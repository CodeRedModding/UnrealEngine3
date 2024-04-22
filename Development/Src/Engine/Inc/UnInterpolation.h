/*=============================================================================
	UnInterpolation.h: Matinee related C++ declarations
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Struct of data that is passed to the input interface. */
struct FInterpEdInputData
{
	FInterpEdInputData()
		: InputType( 0 ),
		  InputData( 0 ),
		  TempData( NULL ),
		  bCtrlDown( FALSE ),
		  bAltDown( FALSE ),
		  bShiftDown( FALSE ),
		  MouseStart( 0, 0 ),
		  MouseCurrent( 0, 0 ),
		  PixelsPerSec( 0.0f )
	{
	}

	FInterpEdInputData( INT InType, INT InData )
		: InputType( InType ),
		  InputData( InData ),
		  TempData( NULL ),
		  bCtrlDown( FALSE ),
		  bAltDown( FALSE ),
		  bShiftDown( FALSE ),
		  MouseStart( 0, 0 ),
		  MouseCurrent( 0, 0 ),
		  PixelsPerSec( 0.0f )
	{
	}

	INT InputType;
	INT InputData;
	void* TempData;	 // Should only be initialized in StartDrag and should only be deleted in EndDrag!

	// Mouse data - Should be filled in automatically.
	UBOOL bCtrlDown;
	UBOOL bAltDown;
	UBOOL bShiftDown;
	FIntPoint MouseStart;
	FIntPoint MouseCurrent;
	FLOAT PixelsPerSec;
};

/** Defines a set of functions that provide drag drop functionality for the interp editor classes. */
class FInterpEdInputInterface
{
public:
	/**
	 * @return Returns the mouse cursor to display when this input interface is moused over.
	 */
	virtual EMouseCursor GetMouseCursor(FInterpEdInputData &InputData) {return MC_NoChange;}

	/**
	 * Lets the interface object know that we are beginning a drag operation.
	 */
	virtual void BeginDrag(FInterpEdInputData &InputData) {}

	/**
	 * Lets the interface object know that we are ending a drag operation.
	 */
	virtual void EndDrag(FInterpEdInputData &InputData) {}

	/** @return Whether or not this object can be dropped on. */
	virtual UBOOL AcceptsDropping(FInterpEdInputData &InputData, FInterpEdInputInterface* DragObject) {return FALSE;}

	/**
	 * Called when an object is dragged.
	 */
	virtual void ObjectDragged(FInterpEdInputData& InputData) {};

	/**
	 * Allows the object being dragged to be draw on the canvas.
	 */
	virtual void DrawDragObject(FInterpEdInputData &InputData, FViewport* Viewport, FCanvas* Canvas) {}

	/**
	 * Allows the object being dropped on to draw on the canvas.
	 */
	virtual void DrawDropObject(FInterpEdInputData &InputData, FViewport* Viewport, FCanvas* Canvas) {}

	/** @return Whether or not the object being dragged can be dropped. */
	virtual UBOOL ShouldDropObject(FInterpEdInputData &InputData) {return FALSE;}

	/** @return Returns a UObject pointer of this instance if it also inherits from UObject. */
	virtual UObject* GetUObject() {return NULL;}
};


class FInterpEdSelKey
{
public:
	FInterpEdSelKey()
	{
		Group = NULL;
		Track = NULL;
		KeyIndex = INDEX_NONE;
		UnsnappedPosition = 0.f;
	}

	FInterpEdSelKey(class UInterpGroup* InGroup, class UInterpTrack* InTrack, INT InKey)
	{
		Group = InGroup;
		Track = InTrack;
		KeyIndex = InKey;
	}

	UBOOL operator==(const FInterpEdSelKey& Other) const
	{
		if(	Group == Other.Group &&
			Track == Other.Track &&
			KeyIndex == Other.KeyIndex )
		{
			return true;
		}
		else
		{
			return false;
		}
	}

private:
	/** 
	 * Recursive function used by GetOwningTrack();  to search through all subtracks
	 */
	class UInterpTrack* GetOwningTrack( UInterpTrack* pTrack );

public:
	/** 
	 * Returns the parent track of this key.  If this track isn't a subtrack, Track is returned (it owns itself)
	 */
	class UInterpTrack* GetOwningTrack();

	/** 
	 * Returns the sub group name of the parent track of this key. If this track isn't a subtrack, nothing is returned
	 */
	FString GetOwningTrackSubGroupName( INT* piSubTrack = NULL );

	class UInterpGroup* Group;
	class UInterpTrack* Track;
	INT KeyIndex;
	FLOAT UnsnappedPosition;
};



/** Parameters for drawing interp tracks, used by Matinee */
class FInterpTrackDrawParams
{

public:

	/** This track's index */
	INT TrackIndex;
	
	/** Track display width */
	INT TrackWidth;
	
	/** Track display height */
	INT TrackHeight;
	
	/** The view range start time (within the sequence) */
	FLOAT StartTime;
	
	/** Scale of the track window in pixels per second */
	FLOAT PixelsPerSec;

	/** Current position of the Matinee time cursor along the timeline */
	FLOAT TimeCursorPosition;

	/** Current snap interval (1.0 / frames per second) */
	FLOAT SnapAmount;

	/** True if we want frame numbers to be rendered instead of time values where appropriate */
	UBOOL bPreferFrameNumbers;

	/** True if we want time cursor positions drawn for all anim tracks */
	UBOOL bShowTimeCursorPosForAllKeys;

	/** True if the user should be allowed to select using "keyframe bars," such as those for audio tracks */
	UBOOL bAllowKeyframeBarSelection;

	/** True if the user should be allowed to select using keyframe text */
	UBOOL bAllowKeyframeTextSelection;

	/** List of keys that are currently selected */
	TArray< FInterpEdSelKey > SelectedKeys;
};

/** Helper struct for creating sub tracks supported by this track */
struct FSupportedSubTrackInfo
{
	/** The sub track class which is supported by this track */
	UClass* SupportedClass;
	/** The name of the subtrack */
	FString SubTrackName;
	/** Index into the any subtrack group this subtrack belongs to (can be -1 for no group) */
	INT GroupIndex;
};

/** A small structure holding data for grouping subtracks. (For UI drawing purposes) */
struct FSubTrackGroup
{
	/** Name of the subtrack  group */
	FString GroupName;
	/** Indices to tracks in the parent track subtrack array. */
	TArray<INT> TrackIndices;
	/** If this group is collapsed */
	BITFIELD bIsCollapsed:1;
	/** If this group is selected */ 
	BITFIELD bIsSelected:1;
};

/** Required condition for this track to be enabled */
enum ETrackActiveCondition
{
	/** Track is always active */
	ETAC_Always,

	/** Track is active when extreme content (gore) is enabled */
	ETAC_GoreEnabled,

	/** Track is active when extreme content (gore) is disabled */
	ETAC_GoreDisabled
};


class UInterpTrack : public UObject, public FCurveEdInterface, public FInterpEdInputInterface
{
public:
	/** A list of subtracks that belong to this track */
	TArrayNoInit<UInterpTrack*> SubTracks;
#if WITH_EDITORONLY_DATA
	/** A list of subtrack groups (for editor UI organization only) */
	TArrayNoInit<FSubTrackGroup> SubTrackGroups;
	/** A list of supported tracks that can be a subtrack of this track. */
	TArrayNoInit<FSupportedSubTrackInfo> SupportedSubTracks;
#endif // WITH_EDITORONLY_DATA
    class UClass* TrackInstClass;
	BYTE ActiveCondition;
    FStringNoInit TrackTitle;
    BITFIELD bOnePerGroup:1;
    BITFIELD bDirGroupOnly:1;
private:
	BITFIELD bDisableTrack:1;
public:
	BITFIELD bIsAnimControlTrack:1;
	BITFIELD bSubTrackOnly:1;
	BITFIELD bVisible:1;
	BITFIELD bIsSelected:1;
	BITFIELD bIsRecording:1;
	BITFIELD bIsCollapsed:1;
	SCRIPT_ALIGN;
    DECLARE_CLASS_NOEXPORT(UInterpTrack,UObject,0,Engine)

	// InterpTrack interface

	/** Total number of keyframes in this track. */
	virtual INT GetNumKeyframes() const { return 0; }

	/** Get first and last time of keyframes in this track. */
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const { StartTime = 0.f; EndTime = 0.f; }

	/** Get the time of the keyframe with the given index. */
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const {return 0.f; }

	/** Get the index of the keyframe with the given time. */
	virtual INT GetKeyframeIndex( FLOAT KeyTime ) const { return -1; }

	/** Add a new keyframe at the speicifed time. Returns index of new keyframe. */
	virtual INT AddKeyframe( FLOAT Time, class UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode ) { return INDEX_NONE; }

	/**
     * Adds a keyframe to a child track 
	 *
	 * @param ChildTrack		The child track where the keyframe should be added
	 * @param Time				What time the keyframe is located at
	 * @param TrackInst			The track instance of the parent track(this track)
	 * @param InitInterpMode	The initial interp mode for the keyframe?	 
	 */
	virtual INT AddChildKeyframe( class UInterpTrack* ChildTrack, FLOAT Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) { return INDEX_NONE; }

	/** Change the value of an existing keyframe. */
	virtual void UpdateKeyframe(INT KeyIndex, class UInterpTrackInst* TrInst) {}

	/**
     * Updates a child track keyframe
	 *
	 * @param ChildTrack		The child track with keyframe to update
	 * @param KeyIndex			The index of the key to be updated
	 * @param TrackInst			The track instance of the parent track(this track)
	 */
	virtual void UpdateChildKeyframe( class UInterpTrack* ChildTrack, INT KeyIndex, UInterpTrackInst* TrackInst ) {};

	/** Change the time position of an existing keyframe. This can change the index of the keyframe - the new index is returned. */
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true) { return INDEX_NONE; }

	/** Remove the keyframe with the given index. */
	virtual void RemoveKeyframe(INT KeyIndex) {}

	/** 
	 *	Duplicate the keyframe with the given index to the specified time. 
	 *	Returns the index of the newly created key.
	 */
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime) { return INDEX_NONE; }

	/** Return the closest time to the time passed in that we might want to snap to. */
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition) { return false; }

	/** 
	 *	Conditionally calls PreviewUpdateTrack depending on whether or not the track is enabled.
	 */
	virtual void ConditionalPreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst);

	/** 
	 *	Conditionally calls UpdateTrack depending on whether or not the track is enabled.
	 */
	virtual void ConditionalUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst, UBOOL bJump);


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
	virtual void UpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst, UBOOL bJump) {} // This is called in the game, when USeqAct_Interp is ticked

	/** Called when playback is stopped in Matinee. Useful for stopping sounds etc. */
	virtual void PreviewStopPlayback(class UInterpTrackInst* TrInst) {}

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	/** Get the icon to draw for this track in Matinee. */
	virtual class UMaterial* GetTrackIcon() const;

	/** Whether or not this track is allowed to be used on static actors. */
	virtual UBOOL AllowStaticActors() { return FALSE; }

	/** Draw this track with the specified parameters */
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );

	/** Return color to draw each keyframe in Matinee. */
	virtual FColor GetKeyframeColor(INT KeyIndex) const;

	/**
	 * @return	The ending time of the track. 
	 */
	virtual FLOAT GetTrackEndTime() const { return 0.0f; }

	/**
	 *	For drawing track information into the 3D scene.
	 *	TimeRes is how often to draw an event (eg. resoltion of spline path) in seconds.
	 */
	virtual void Render3DTrack(class UInterpTrackInst* TrInst, 
		const FSceneView* View, 
		FPrimitiveDrawInterface* PDI, 
		INT TrackIndex, 
		const FColor& TrackColor, 
		TArray<class FInterpEdSelKey>& SelectedKeys) {}

	/** Set this track to sensible default values. Called when track is first created. */
	virtual void SetTrackToSensibleDefault() {}

	// FInterpEdInputInterface Interface

	/** @return Returns a UObject pointer of this instance if it also inherits from UObject. */
	virtual UObject* GetUObject() {return this;}

	/** 
	 * Returns the outer group of this track.  If this track is a subtrack, the group of its parent track is returned
	 */
	UInterpGroup* GetOwningGroup();

	/** 
	 * Enables this track and optionally, all subtracks.
	 * 
	 * @param bInEnable				True if the track should be enabled, false to disable
	 * @param bPropagateToSubTracks	True to propagate the state to all subtracks
	 */
	void EnableTrack( UBOOL bInEnable, UBOOL bPropagateToSubTracks = TRUE );

	/**
	 * Toggle the override flags for a camera actor's post process setting if 
	 * the given track is a property track that references a post process 
	 * setting and the group actor is a camera actor.
	 *
	 * @param	GroupActor	The group actor for the interp track.
	 */
	void DisableCameraPostProcessFlags( AActor* GroupActor );

	/** Returns true if this track has been disabled.  */
	UBOOL IsDisabled() const { return bDisableTrack; }

	/** Returns the property name, if any. */
	virtual UBOOL GetPropertyName( FName& PropertyNameOut ) const { return FALSE; }

	/**
     * Creates and adds subtracks to this track
	 *
	 * @param bCopy	If subtracks are being added as a result of a copy
	 */
	virtual void CreateSubTracks( UBOOL bCopy ) {};

	/** 
	 * Reduce Keys within Tolerance
	 *
	 * @param bIntervalStart	start of the key to reduce
	 * @param bIntervalEnd		end of the key to reduce
	 * @param Tolerance			tolerance
	 */
	virtual void ReduceKeys( FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance ){};
};

namespace MatineeKeyReduction
{
	// For 1D curves, use this structure to allow selected operators on the float.
	class SFLOAT
	{
	public:
		FLOAT f;
		FLOAT& operator[](INT i) { return f; }
		const FLOAT& operator[](INT i) const { return f; }
		SFLOAT operator-(const SFLOAT& g) const { SFLOAT out; out.f = f - g.f; return out; }
		SFLOAT operator+(const SFLOAT& g) const { SFLOAT out; out.f = f + g.f; return out; }
		SFLOAT operator/(const FLOAT& g) const { SFLOAT out; out.f = f / g; return out; }
		SFLOAT operator*(const FLOAT& g) const { SFLOAT out; out.f = f * g; return out; }
		friend SFLOAT operator*(const FLOAT& g, const SFLOAT& f) { SFLOAT out; out.f = f.f * g; return out; }
	};

	// float-float comparison that allows for a certain error in the floating point values
	// due to floating-point operations never being exact.
	static UBOOL IsEquivalent(FLOAT a, FLOAT b, FLOAT Tolerance = KINDA_SMALL_NUMBER)
	{
		return (a - b) > -Tolerance && (a - b) < Tolerance;
	}

	// A key extracted from a track that may be reduced.
	template <class TYPE, int DIM>
	class MKey
	{
	public:
		FLOAT Time;
		TYPE Output;
		BYTE Interpolation;

		UBOOL Smoothness[DIM]; // Only useful for broken Hermite tangents.

		FLOAT Evaluate(FInterpCurve<TYPE>& Curve, TYPE& Tolerance)
		{
			TYPE Invalid;
			TYPE Evaluated = Curve.Eval(Time, Invalid);
			FLOAT Out = 0.0f;
			for (INT D = 0; D < DIM; ++D)
			{
				FLOAT Difference = (Output[D] - Evaluated[D]) * (Output[D] - Evaluated[D]);
				if (Difference > (Tolerance[D] * Tolerance[D]))
				{
					Out += Difference;
				}
			}
			return appSqrt(Out);
		}
	};

	// A temporary curve that is going through the key-reduction process.
	template <class TYPE, int DIM>
	class MCurve
	{
	public:
		typedef MKey<TYPE, DIM> KEY;
		FInterpCurveInit<TYPE> OutputCurve; // The output animation curve.
		TArray<KEY> ControlPoints; // The list of keys to reduce.
		TArray<FIntPoint> SegmentQueue; // The segments to reduce iteratively.
		TYPE Tolerance; // Acceptable tolerance for each of the dimensions.

		FLOAT RelativeTolerance; // Comes from the user: 0.05f is the default.
		FLOAT IntervalStart, IntervalEnd; // Comes from the user: interval in each to apply the reduction.

		void Reduce()
		{
			INT ControlPointCount = ControlPoints.Num();

			// Fill in the output values for the curve key already created because they
			// cannot be reduced.
			INT KeyFillListCount = OutputCurve.Points.Num();
			for ( INT I = 0; I < KeyFillListCount; ++I )
			{
				FLOAT KeyTime = OutputCurve.Points(I).InVal;
				KEY* ControlPoint = NULL;
				for ( INT J = 0; J < ControlPointCount; ++J )
				{
					if ( IsEquivalent( ControlPoints(J).Time, KeyTime, 0.001f ) ) // 1ms tolerance
					{
						ControlPoint = &ControlPoints(J);
					}
				}
				check ( ControlPoint != NULL );

				// Copy the control point value to the curve key.
				OutputCurve.Points(I).OutVal = ControlPoint->Output;
				OutputCurve.Points(I).InterpMode = ControlPoint->Interpolation;
			}

			for ( INT I = 0; I < KeyFillListCount; ++I )
			{
				// Second step: recalculate the tangents.
				// This is done after the above since it requires valid output values on all keys.
				RecalculateTangents(I);
			}

			if ( ControlPointCount < 2 )
			{
				check( ControlPoints.Num() == 1 );
				OutputCurve.AddPoint( ControlPoints(0).Time, ControlPoints(0).Output );
			}
			else
			{
				SegmentQueue.Reserve(ControlPointCount - 1);
				if ( SegmentQueue.Num() == 0 )
				{
					SegmentQueue.AddItem(FIntPoint(0, ControlPointCount - 1));
				}

				// Iteratively reduce the segments.
				while (SegmentQueue.Num() > 0)
				{
					// Dequeue the first segment.
					FIntPoint Segment = SegmentQueue(0);
					SegmentQueue.Remove(0);

					// Reduce this segment.
					ReduceSegment(Segment.X, Segment.Y);
				}
			}
		}

		void ReduceSegment(INT StartIndex, INT EndIndex)
		{
			if (EndIndex - StartIndex < 2) return;

			// Find the segment control point with the largest delta to the current curve segment.
			// Emphasize middle control points, as much as possible.
			INT MiddleIndex = 0; FLOAT MiddleIndexDelta = 0.0f;
			for (INT CPIndex = StartIndex + 1; CPIndex < EndIndex; ++CPIndex)
			{
				FLOAT CPDelta = ControlPoints(CPIndex).Evaluate(OutputCurve, Tolerance);
				if (CPDelta > 0.0f)
				{
					FLOAT TimeDelta[2];
					TimeDelta[0] = ControlPoints(CPIndex).Time - ControlPoints(StartIndex).Time;
					TimeDelta[1] = ControlPoints(EndIndex).Time - ControlPoints(CPIndex).Time;
					if (TimeDelta[1] < TimeDelta[0]) TimeDelta[0] = TimeDelta[1];

					CPDelta *= TimeDelta[0];
					if (CPDelta > MiddleIndexDelta)
					{
						MiddleIndex = CPIndex;
						MiddleIndexDelta = CPDelta;
					}
				}
			}

			if (MiddleIndexDelta > 0.0f)
			{
				// Add this point to the curve and re-calculate the tangents.
				INT PointIndex = OutputCurve.AddPoint(ControlPoints(MiddleIndex).Time, ControlPoints(MiddleIndex).Output);
				OutputCurve.Points(PointIndex).InterpMode = CIM_CurveUser;
				RecalculateTangents(PointIndex);
				if (PointIndex > 0) RecalculateTangents(PointIndex - 1);
				if (PointIndex < OutputCurve.Points.Num() - 1) RecalculateTangents(PointIndex + 1);

				// Schedule the two sub-segments for evaluation.
				if (MiddleIndex - StartIndex >= 2) SegmentQueue.AddItem(FIntPoint(StartIndex, MiddleIndex));
				if (EndIndex - MiddleIndex >= 2) SegmentQueue.AddItem(FIntPoint(MiddleIndex, EndIndex));
			}
		}

		void RecalculateTangents(INT CurvePointIndex)
		{
			// Retrieve the previous and next curve points.
			// Alias the three curve points and the tangents being calculated, for readability.
			INT PreviousIndex = CurvePointIndex > 0 ? CurvePointIndex - 1 : 0;
			INT NextIndex = CurvePointIndex < OutputCurve.Points.Num() - 1 ? CurvePointIndex + 1 : OutputCurve.Points.Num() - 1;
			FInterpCurvePoint<TYPE>& PreviousPoint = OutputCurve.Points(PreviousIndex);
			FInterpCurvePoint<TYPE>& CurrentPoint = OutputCurve.Points(CurvePointIndex);
			FInterpCurvePoint<TYPE>& NextPoint = OutputCurve.Points(NextIndex);
			TYPE& InSlope = CurrentPoint.ArriveTangent;
			TYPE& OutSlope = CurrentPoint.LeaveTangent;

			if ( CurrentPoint.InterpMode != CIM_CurveBreak || CurvePointIndex == 0 || CurvePointIndex == OutputCurve.Points.Num() - 1)
			{
				// Check for local minima/maxima on every dimensions
				for ( INT D = 0; D < DIM; ++ D )
				{
					// Average out the slope.
					if ( ( CurrentPoint.OutVal[D] >= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] >= PreviousPoint.OutVal[D] ) // local maxima
						|| ( CurrentPoint.OutVal[D] <= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] <= PreviousPoint.OutVal[D] ) ) // local minima
					{
						InSlope[D] = OutSlope[D] = 0.0f;
					}
					else
					{
						InSlope[D] = OutSlope[D] = (NextPoint.OutVal[D] - PreviousPoint.OutVal[D]) / (NextPoint.InVal - PreviousPoint.InVal);
					}
				}
			}
			else
			{
				KEY* ControlPoint = FindControlPoint( CurrentPoint.InVal );
				check ( ControlPoint != NULL );

				for ( INT D = 0; D < DIM; ++ D )
				{
					if ( ControlPoint->Smoothness[D] )
					{
						if ( ( CurrentPoint.OutVal[D] >= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] >= PreviousPoint.OutVal[D] ) // local maxima
							|| ( CurrentPoint.OutVal[D] <= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] <= PreviousPoint.OutVal[D] ) ) // local minima
						{
							InSlope[D] = OutSlope[D] = 0.0f;
						}
						else
						{
							InSlope[D] = OutSlope[D] = (NextPoint.OutVal[D] - PreviousPoint.OutVal[D]) / (NextPoint.InVal - PreviousPoint.InVal);
						}
					}
					else
					{
						InSlope[D] = (CurrentPoint.OutVal[D] - PreviousPoint.OutVal[D]) /* / (CurrentPoint.InVal - PreviousPoint.InVal) */;
						OutSlope[D] = (NextPoint.OutVal[D] - CurrentPoint.OutVal[D]) /* / (NextPoint.InVal - CurrentPoint.InVal) */;
					}
				}
			}
		}

		// This badly needs to be optimized out.
		KEY* FindControlPoint(FLOAT Time)
		{
			INT CPCount = ControlPoints.Num();
			if (CPCount < 8)
			{
				// Linear search
				for ( INT I = 0; I < CPCount; ++I )
				{
					if ( IsEquivalent( ControlPoints(I).Time, Time, 0.001f ) ) // 1ms tolerance
					{
						return &ControlPoints(I);
					}
				}
			}
			else
			{
				// Binary search
				INT Start = 0, End = CPCount;
				for ( INT Mid = (End + Start) / 2; Start < End; Mid = (End + Start) / 2 )
				{
					if ( IsEquivalent( ControlPoints(Mid).Time, Time, 0.001f ) ) // 1ms tolerance
					{
						return &ControlPoints(Mid);
					}
					else if ( Time < ControlPoints(Mid).Time ) End = Mid;
					else Start = Mid + 1;
				}
			}
			return NULL;
		}

		// Called by the function below, this one is fairly inefficient
		// and needs to be optimized out, later.
		KEY* SortedAddControlPoint(FLOAT Time)
		{
			INT ControlPointCount = ControlPoints.Num();
			INT InsertionIndex = ControlPointCount;
			for ( INT I = 0; I < ControlPointCount; ++I )
			{
				KEY& ControlPoint = ControlPoints(I);
				if ( IsEquivalent( ControlPoint.Time, Time, 0.001f ) ) return &ControlPoint; // 1ms tolerance
				else if ( ControlPoint.Time > Time ) { InsertionIndex = I; break; }
			}

			ControlPoints.Insert( InsertionIndex, 1 );
			ControlPoints(InsertionIndex).Time = Time;
			ControlPoints(InsertionIndex).Interpolation = CIM_CurveUser;

			// Also look through the segment indices and push them up.
			INT SegmentCount = SegmentQueue.Num();
			for ( INT I = 0; I < SegmentCount; ++I )
			{
				FIntPoint& Segment = SegmentQueue(I);
				if ( Segment.X >= InsertionIndex ) ++Segment.X;
				if ( Segment.Y >= InsertionIndex ) ++Segment.Y;
			}

			return &ControlPoints(InsertionIndex); 
		}

#if 0	// [GLaforte - 26-02-2007] Disabled until I have more time to debug this and implement tangent checking in the key addition code above.

		// Look for the relative maxima and minima within one dimension of a curve's segment.
		template <class TYPE2>
		void FindSegmentExtremes(const FInterpCurve<TYPE2>& OldCurve, INT PointIndex, INT Dimension, FLOAT& Maxima, FLOAT& Minima )
		{
			// Alias the information we are interested in, for readability.
			const FInterpCurvePoint<TYPE2>& StartPoint = OldCurve.Points(PointIndex);
			const FInterpCurvePoint<TYPE2>& EndPoint = OldCurve.Points(PointIndex + 1);
			FLOAT StartTime = StartPoint.InVal;
			FLOAT EndTime = EndPoint.InVal;
			FLOAT SegmentLength = EndTime - StartTime;
			FLOAT StartValue = StartPoint.OutVal[Dimension];
			FLOAT EndValue = EndPoint.OutVal[Dimension];
			FLOAT StartTangent = StartPoint.LeaveTangent[Dimension];
			FLOAT EndTangent = StartPoint.ArriveTangent[Dimension];
			FLOAT Slope = (EndValue - StartValue) / (EndTime - StartTime);

			// Figure out which form we have, as Hermite tangents on one dimension can only have four forms.
			FLOAT MaximaStartRange = StartTime, MaximaEndRange = EndTime;
			FLOAT MinimaStartRange = StartTime, MinimaEndRange = EndTime;
			if ( StartTangent > Slope )
			{
				if ( EndTangent < Slope )
				{
					// Form look like: /\/ .
					Maxima = ( 3.0f * StartTime + EndTime ) / 4.0f;
					MaximaEndRange = ( StartTime + EndTime ) / 2.0f;
					Minima = ( StartTime + 3.0f * EndTime ) / 4.0f;
					MinimaStartRange = ( StartTime + EndTime ) / 2.0f;
				}
				else
				{
					// Form look like: /\ .
					Maxima = ( StartTime + EndTime ) / 2.0f;
					Minima = StartTime; // Minimas at both endpoints.
				}
			}
			else
			{
				if ( EndTangent > Slope )
				{
					// Form look like: \/\ .
					Minima = ( 3.0f * StartTime + EndTime ) / 4.0f;
					MinimaEndRange = ( StartTime + EndTime ) / 2.0f;
					Maxima = ( StartTime + 3.0f * EndTime ) / 4.0f;
					MaximaStartRange = ( StartTime + EndTime ) / 2.0f;
				}
				else
				{
					// Form look like: \/ .
					Minima = ( StartTime + EndTime ) / 2.0f;
					Maxima = StartTime; // Maximas at both endpoints.
				}
			}

#define EVAL_CURVE(Time) ( \
	CubicInterp( StartValue, StartTangent, EndValue, EndTangent, ( Time - StartTime ) / SegmentLength ) \
	- Slope * ( Time - StartTime ) )
#define FIND_POINT(TimeRef, ValueRef, StartRange, EndRange, CompareOperation) \
	FLOAT ValueRef = EVAL_CURVE( TimeRef ); \
	FLOAT StartRangeValue = EVAL_CURVE( StartRange ); \
	FLOAT EndRangeValue = EVAL_CURVE( EndRange ); \
	UBOOL TestLeft = FALSE; /* alternate between reducing on the left and on the right. */ \
	while ( StartRange < EndRange - 0.001f ) { /* 1ms jitter is tolerable. */ \
	if (TestLeft) { \
	FLOAT TestTime = ( TimeRef + StartRange ) / 2.0f; \
	FLOAT TestValue = EVAL_CURVE( TestTime ); \
	if ( TestValue CompareOperation ValueRef ) { \
	EndRange = Minima; EndRangeValue = ValueRef; \
	ValueRef = TestValue; TimeRef = TestTime; } \
						else { \
						StartRange = TestTime; StartRangeValue = TestValue; \
						TestLeft = FALSE; } } \
					else { \
					FLOAT TestTime = ( TimeRef + EndRange ) / 2.0f; \
					FLOAT TestValue = EVAL_CURVE( TestTime ); \
					if ( TestValue CompareOperation ValueRef ) { \
					StartRange = TimeRef; StartRangeValue = ValueRef; \
					ValueRef = TestValue; TimeRef = TestTime; } \
						else { \
						EndRange = TestTime; EndRangeValue = TestValue; \
						TestLeft = TRUE; } } }

			if ( Minima > StartTime )
			{
				FIND_POINT(Minima, MinimaValue, MinimaStartRange, MinimaEndRange, <);
				if ( IsEquivalent( MinimaValue, StartValue ) ) Minima = StartTime; // Close enough to flat.
			}
			if ( Maxima > StartTime )
			{
				FIND_POINT(Maxima, MaximaValue, MaximaStartRange, MaximaEndRange, >);
				if ( IsEquivalent( MaximaValue, StartValue ) ) Maxima = StartTime; // Close enough to flat.
			}
		}
#endif // 0		

		UBOOL HasControlPoints()
		{
			return (ControlPoints.Num() > 0);
		}

		template <class TYPE2>
		void CreateControlPoints(const FInterpCurve<TYPE2>& OldCurve, INT CurveDimensionCount)
		{
			INT OldCurvePointCount = OldCurve.Points.Num();
			if ( OldCurvePointCount > 0 && ControlPoints.Num() == 0 )
			{
				INT ReduceSegmentStart = 0;
				UBOOL ReduceSegmentStarted = FALSE;

				ControlPoints.Reserve(OldCurvePointCount);
				for ( INT I = 0; I < OldCurvePointCount; ++I )
				{
					// Skip points that are not within our interval.
					if (OldCurve.Points(I).InVal < IntervalStart || OldCurve.Points(I).InVal > IntervalEnd)
						continue;

					// Create the control points.
					// Expected value at the control points will be set by the FillControlPoints function.
					INT ControlPointIndex = ControlPoints.Add(1);
					ControlPoints(ControlPointIndex).Time = OldCurve.Points(I).InVal;

					// Check the interpolation values only the first time the keys are processed.
					UBOOL SmoothInterpolation = OldCurve.Points(I).InterpMode == CIM_Linear || OldCurve.Points(I).InterpMode == CIM_CurveAuto || OldCurve.Points(I).InterpMode == CIM_CurveAutoClamped || OldCurve.Points(I).InterpMode == CIM_CurveUser;

					// Possibly that we want to add broken tangents that are equal to the list, but I cannot check for those without having the full 6D data.
					if ( SmoothInterpolation )
					{
						// We only care for STEP and HERMITE interpolations.
						// In the case of HERMITE, we do care whether the tangents are broken or not.
						ControlPoints(ControlPointIndex).Interpolation = CIM_CurveUser;
						ReduceSegmentStarted = TRUE;
					}
					else
					{
						// This control point will be required in the output curve.
						ControlPoints(ControlPointIndex).Interpolation = OldCurve.Points(I).InterpMode;
						if ( ReduceSegmentStarted )
						{
							SegmentQueue.AddItem(FIntPoint(ReduceSegmentStart, ControlPointIndex));
						}
						ReduceSegmentStart = I;
						ReduceSegmentStarted = FALSE;
					}

					if ( !SmoothInterpolation )
					{
						// When adding these points, the output is intentionally bad but will be fixed up later, in the Reduce function.
						OutputCurve.AddPoint( ControlPoints(ControlPointIndex).Time, TYPE() );
					}
				}

				// Add the first and last control points to the output curve: they are always necessary.
				if ( OutputCurve.Points.Num() == 0 || !IsEquivalent(OutputCurve.Points(0).InVal, ControlPoints(0).Time) )
				{
					OutputCurve.AddPoint( ControlPoints(0).Time, TYPE() );
				}
				if ( !IsEquivalent(OutputCurve.Points.Last().InVal, ControlPoints.Last().Time) )
				{
					OutputCurve.AddPoint( ControlPoints.Last().Time, TYPE() );
				}

				if ( ReduceSegmentStarted )
				{
					SegmentQueue.AddItem( FIntPoint(ReduceSegmentStart, ControlPoints.Num() - 1) );
				}
			}


#if 0
			// On smooth interpolation segments, look for extra control points to
			// create when dealing with local minima/maxima on wacky tangents.
			for ( INT Index = 0; Index < OldCurvePointCount - 1; ++Index )
			{
				// Skip segments that do not start within our interval.
				if (OldCurve.Points(I).InVal < IntervalStart || OldCurve.Points(I).InVal > IntervalEnd)
					continue;

				// Only look at curve with tangents
				if ( OldCurve.Points(Index).InterpMode != CIM_Linear && OldCurve.Points(Index).InterpMode != CIM_Constant )
				{
					for ( INT D = 0; D < CurveDimensionCount; ++D )
					{
						// Find the maxima and the minima for each dimension.
						FLOAT Maxima, Minima, StartTime = OldCurve.Points(Index).InVal;
						FindSegmentExtremes( OldCurve, Index, D, Maxima, Minima );

						// If the maxima and minima are valid, attempt to add them to the list of control points.
						// The "SortedAddControlPoint" function will handle duplicates and points that are very close. 
						if ( Minima - StartTime > 0.001f )
						{
							SortedAddControlPoint( Minima );
						}
						if ( Maxima - StartTime > 0.001f )
						{
							SortedAddControlPoint( Maxima );
						}
					}
				}
			}
#endif // 0
		}

		template <class TYPE2>
		void FillControlPoints(const FInterpCurve<TYPE2>& OldCurve, INT OldCurveDimensionCount, INT LocalCurveDimensionOffset)
		{
			check ( OldCurveDimensionCount + LocalCurveDimensionOffset <= DIM );
			check ( LocalCurveDimensionOffset >= 0 );

			// For tolerance calculations, keep track of the maximum and minimum values of the affected dimension.
			TYPE MinValue, MaxValue;
			for (INT I = 0; I < OldCurveDimensionCount; ++I)
			{
				MinValue[I] = BIG_NUMBER;
				MaxValue[I] = -BIG_NUMBER;
			}

			// Skip all points that are before our interval.
			INT OIndex = 0;
			while (OIndex < OldCurve.Points.Num() && OldCurve.Points(OIndex).InVal < ControlPoints(0).Time)
			{
				++OIndex;
			}

			// Fill the control point values with the information from this curve.
			for (INT CPIndex = 0; CPIndex < ControlPoints.Num(); ++CPIndex)
			{
				// Check which is the next key to consider.
				if ( OIndex < OldCurve.Points.Num() && IsEquivalent( OldCurve.Points(OIndex).InVal, ControlPoints(CPIndex).Time, 0.01f ) )
				{
					// Simply copy the key over.
					for (INT I = 0; I < OldCurveDimensionCount; ++I)
					{
						FLOAT Value = OldCurve.Points(OIndex).OutVal[I];
						ControlPoints(CPIndex).Output[LocalCurveDimensionOffset + I] = Value;
						if (Value < MinValue[I]) MinValue[I] = Value;
						if (Value > MaxValue[I]) MaxValue[I] = Value;
					}

					// Also check for broken-tangents interpolation. In this case, check for smoothness on all dimensions.
					if ( ControlPoints(CPIndex).Interpolation == CIM_CurveBreak )
					{
						for (INT I = 0; I < OldCurveDimensionCount; ++I)
						{
							FLOAT Tolerance = OldCurve.Points(OIndex).ArriveTangent[I] * RelativeTolerance; // Keep a pretty large tolerance here.
							if (Tolerance < 0.0f) Tolerance = -Tolerance;
							if (Tolerance < SMALL_NUMBER) Tolerance = SMALL_NUMBER;
							UBOOL Smooth = IsEquivalent( OldCurve.Points(OIndex).LeaveTangent[I], OldCurve.Points(OIndex).ArriveTangent[I], Tolerance );
							ControlPoints(CPIndex).Smoothness[LocalCurveDimensionOffset + I] = Smooth;
						}
					}

					++OIndex;
				}
				else
				{
					// Evaluate the Matinee animation curve at the given time, for all the dimensions.
					TYPE2 DefaultValue;
					TYPE2 EvaluatedPoint = OldCurve.Eval(ControlPoints(CPIndex).Time, DefaultValue);
					for (INT I = 0; I < OldCurveDimensionCount; ++I)
					{
						FLOAT Value = EvaluatedPoint[I];
						ControlPoints(CPIndex).Output[LocalCurveDimensionOffset + I] = Value;
						if (Value < MinValue[I]) MinValue[I] = Value;
						if (Value > MaxValue[I]) MaxValue[I] = Value;
					}
				}
			}

			// Generate the tolerance values.
			// The relative tolerance value now comes from the user.
			for (INT I = 0; I < OldCurveDimensionCount; ++I)
			{
				Tolerance[LocalCurveDimensionOffset + I] = Max(RelativeTolerance * (MaxValue[I] - MinValue[I]), (FLOAT) KINDA_SMALL_NUMBER);
			}
		}

		template <class TYPE2>
		void CopyCurvePoints(TArrayNoInit<TYPE2>& NewCurve, INT NewCurveDimensionCount, INT LocalCurveDimensionOffset)
		{
			INT PointCount = OutputCurve.Points.Num();

			// Remove the points that belong to the interval from the NewCurve.
			INT RemoveStartIndex = -1, RemoveEndIndex = -1;
			for (INT I = 0; I < NewCurve.Num(); ++I)
			{
				if (RemoveStartIndex == -1 && NewCurve(I).InVal >= IntervalStart)
				{
					RemoveStartIndex = I;
				}
				else if (RemoveEndIndex == -1 && NewCurve(I).InVal > IntervalEnd)
				{
					RemoveEndIndex = I;
					break;
				}
			}
			if (RemoveEndIndex == -1) RemoveEndIndex = NewCurve.Num();
			NewCurve.Remove(RemoveStartIndex, RemoveEndIndex - RemoveStartIndex);

			// Add back into the curve, the new control points generated from the key reduction algorithm.
			NewCurve.Insert(RemoveStartIndex, PointCount);
			for (INT I = 0; I < PointCount; ++I)
			{
				NewCurve(RemoveStartIndex + I).InVal = OutputCurve.Points(I).InVal;
				NewCurve(RemoveStartIndex + I).InterpMode = OutputCurve.Points(I).InterpMode;
				for (INT J = 0; J < NewCurveDimensionCount; ++J)
				{
					NewCurve(RemoveStartIndex + I).OutVal[J] = OutputCurve.Points(I).OutVal[LocalCurveDimensionOffset + J];
					NewCurve(RemoveStartIndex + I).ArriveTangent[J] = OutputCurve.Points(I).ArriveTangent[LocalCurveDimensionOffset + J];
					NewCurve(RemoveStartIndex + I).LeaveTangent[J] = OutputCurve.Points(I).LeaveTangent[LocalCurveDimensionOffset + J];
				}
			}
		}
	};
};