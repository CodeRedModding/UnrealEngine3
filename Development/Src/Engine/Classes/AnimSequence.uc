/** 
 * One animation sequence of keyframes. Contains a number of tracks of data.
 * The Outer of AnimSequence is expected to be its AnimSet.
 */

class AnimSequence extends Object
	native(Anim)
	hidecategories(Object);

struct native AnimNotifyEvent
{
	var()	float						Time;
	var()	editinline AnimNotify		Notify;
	var()	name						Comment;
};

/** Name of sequence. Used in AnimNodeSequence. */
var		name									SequenceName;

/** 
 *	Animation notifies. 
 *	The AnimNotify object in each one has its Notify method called when it is passed in the animation. 
 *	This array is sorted by time, earliest notification first.
 */
var()	editinline array<AnimNotifyEvent>		Notifies;

/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0 */
var		float									SequenceLength;

/** Number of raw frames in this sequence (not used by engine - just for informational purposes). */
var		int										NumFrames;

/** Number for tweaking playback rate of this animation globally. */
var()	float									RateScale;


/** Raw uncompressed keyframe data. */
var		native const noexport LazyArray_Mirror	RawAnimData;


cpptext
{
	/** One element for each track. */
	TLazyArray<FRawAnimSequenceTrack>			RawAnimData;



	// UObject interface

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();


	// AnimSequence interface

	/** Interpolate the given track to the given time and return the bone information (position/orientation). */
	void GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping);

	/** Remove trivial frames from the Raw data (that is, if position or orientation is constant over whole animation). */
	void CompressRawAnimData();

	/** Sort the Notifies array by time, earliest first. */
	void SortNotifies();

	/** Size of total animation data in this AnimSequence in bytes. */
	INT GetAnimSequenceMemory();
}

defaultproperties
{
	RateScale=1.0
}