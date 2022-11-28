class AudioComponent extends ActorComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	hidecategories(ActorComponent)
	editinlinenew;

var()					SoundCue			SoundCue;
var		native const	SoundNode			CueFirstNode; // This is just a pointer to the root node in SoundCue.


var						bool				bUseOwnerLocation;
var						bool				bAutoPlay;
var						bool				bAutoDestroy;
var						bool				bFinished;
var						bool				bNonRealtime;
var						bool				bWasPlaying;

/** Is this audio component allowed to be spatialized? */
var						bool				bAllowSpatialization;

var		native const	array<pointer>		WaveInstances;
var		native const	array<byte>			SoundNodeData;
var		native const	map_mirror			SoundNodeOffsetMap;
var		native const	multimap_mirror		SoundNodeWaveMap;
var		native const	multimap_mirror		SoundNodeResetWaveMap;
var		native const	pointer				Listener;

var		native const	float				PlaybackTime;
var		native			vector				Location;
var		native const	vector				ComponentLocation;

// Temporary variables for node traversal.
var		native const	SoundNode			CurrentNotifyFinished;
var		native const	vector				CurrentLocation;
var		native const	float				CurrentDelay;
var		native const	float				CurrentVolume;
var		native const	float				CurrentPitch;
var		native const	int					CurrentUseSpatialization;
var		native const	int					CurrentNodeIndex;

native function Play();
native function Pause();
native function Stop();

defaultproperties
{
	bUseOwnerLocation=true
	bAutoDestroy=false
	bAutoPlay=false
	bAllowSpatialization=true
}