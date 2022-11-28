class SoundNodeWave extends SoundNode
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;


var()					float				Volume;
var()					float				Pitch;

var		const			float				Duration;

var		native const	name				FileType;
var		native const	LazyArray_Mirror	RawData;

var		const transient	int					ResourceID;

defaultproperties
{
	Volume=0.25
	Pitch=1.0
}