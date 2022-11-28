class UTAnnouncer extends Info
	config(Game);

var globalconfig string StatusSoundPackage;
var globalconfig string RewardSoundPackage;
var string FallbackStatusSoundPackage;
var string FallbackRewardSoundPackage;
var globalconfig	byte			AnnouncerLevel;				// 0=none, 1=no possession announcements, 2=all
// FIXMESTEVE var globalconfig	byte			AnnouncerVolume;			// 1 to 4

struct CachedSound
{
	var name CacheName;
	var SoundCue CacheSound;
};

var array<CachedSound> CachedSounds;	// sounds which had to be gotten from backup package

var bool bPrecachedBaseSounds;
var bool bPrecachedGameSounds;
var const bool bEnglishOnly;		// this announcer has not been translated into other languages

var byte PlayingAnnouncementIndex;
var class<UTLocalMessage> PlayingAnnouncementClass; // Announcer Sound

var	UTQueuedAnnouncement	Queue;

var	float				LastTimerCheck;
var	float				GapTime;			// Time between playing 2 announcer sounds
// FIXMESTEVE - need a callback when announcement soundcue completes! - for now, hacked into timer

var UTPlayerController PlayerOwner;

function Destroyed()
{
	local UTQueuedAnnouncement A;

	Super.Destroyed();

	for ( A=Queue; A!=None; A=A.nextAnnouncement )
		A.Destroy();
}

function PostBeginPlay()
{
	Super.PostBeginPlay();

	PlayerOwner = UTPlayerController(Owner);
}

function Timer()
{
	PlayingAnnouncementClass = None;

	if ( Queue == None )
		SetTimer(0, false);
	else
		PlayAnnouncementNow(Queue.AnnouncementClass, Queue.MessageIndex);
}

function PlayAnnouncementNow(class<UTLocalMessage> MessageClass, byte MessageIndex)
{
	local SoundCue ASound;

	ASound = GetSound(MessageClass.Static.AnnouncementSound(MessageIndex), MessageClass.Static.IsRewardAnnouncement(MessageIndex));

	if ( ASound != None )
	{
		PlayerOwner.ClientPlaySound(ASound);
		PlayingAnnouncementClass = MessageClass;
		PlayingAnnouncementIndex = MessageIndex;
	}
	SetTimer(1,false);
}

function PlayAnnouncement(class<UTLocalMessage> MessageClass, byte MessageIndex)
{
	if ( MessageClass.Static.AnnouncementLevel(MessageIndex) > AnnouncerLevel )
	{
		/* FIXMESTEVE
		if ( AnnouncementLevel == 2 )
			PlayerOwner.ClientPlaySound(Sound'GameSounds.DDAverted');
		else if ( AnnouncementLevel == 1 )
			PlayerOwner.PlayBeepSound();
		*/
		return;
	}

	if ( PlayingAnnouncementClass == None )
	{
		// play immediately
		PlayAnnouncementNow(MessageClass, MessageIndex);
		return;
	}
	MessageClass.static.AddAnnouncement(self, MessageIndex);
}

function SoundCue GetSound(name AName, bool bIsReward)
{
	local SoundCue NewSound;
	local string ASoundPackage;
	local int i;

	// check fallback sounds
	for ( i=0; i<CachedSounds.Length; i++ )
		if ( AName == CachedSounds[i].CacheName)
			return CachedSounds[i].CacheSound;

	if  ( bIsReward )
		ASoundPackage = RewardSoundPackage;
	else
		ASoundPackage = StatusSoundPackage;

	// DLO is cheap if already loaded
	NewSound = SoundCue(DynamicLoadObject(ASoundPackage$"."$AName, class'SoundCue', true));

	if ( NewSound == None )
	{
		if ( bIsReward )
			NewSound = PrecacheRewardSound(AName);
		else
			NewSound = PrecacheStatusSound(AName);
	}

	return NewSound;
}

function SoundCue PrecacheRewardSound(name AName)
{
	local SoundCue NewSound;

	NewSound = SoundCue(DynamicLoadObject(RewardSoundPackage$"."$AName, class'SoundCue', true));

	if ( (NewSound == None) && (FallBackRewardSoundPackage != "" ) )
		NewSound = PrecacheFallbackPackage( FallBackRewardSoundPackage, AName );

	if ( NewSound == None )
		warn("Could not find "$AName$" in "$RewardSoundPackage$" nor in fallback package "$FallBackRewardSoundPackage);

	return NewSound;
}

function SoundCue PrecacheStatusSound(name AName)
{
	local SoundCue NewSound;

	NewSound = SoundCue(DynamicLoadObject(StatusSoundPackage$"."$AName, class'SoundCue', true));

	if ( (NewSound == None) && (FallBackStatusSoundPackage != "" ) )
		NewSound = PrecacheFallbackPackage( FallBackStatusSoundPackage, AName );

	if ( NewSound == None )
		warn("Could not find "$AName$" in "$StatusSoundPackage$" nor in fallback package "$FallBackStatusSoundPackage);

	return NewSound;
}

function SoundCue PrecacheFallbackPackage( string Package, name AName )
{
	local SoundCue NewSound;
	local int	i;

	NewSound = SoundCue(DynamicLoadObject(Package$"."$AName, class'SoundCue', true));
	if ( NewSound != None )
	{
		for ( i=0; i<CachedSounds.Length; i++ )
			if ( CachedSounds[i].CacheName == AName )
			{
				CachedSounds[i].CacheSound = NewSound;
				return NewSound;
			}

		CachedSounds.Length = CachedSounds.Length + 1;
		CachedSounds[CachedSounds.Length-1].CacheName	= AName;
		CachedSounds[CachedSounds.Length-1].CacheSound	= NewSound;

		return NewSound;
	}

	return None;
}

function PrecacheAnnouncements()
{
	local class<UTGame> GameClass;

	if ( !bPrecachedGameSounds )
	{
		bPrecachedGameSounds =  ( (Level.GRI != None) && (Level.GRI.GameClass != "") );
		GameClass = class<UTGame>(Level.GetGameClass());
		if ( GameClass != None )
			GameClass.Static.PrecacheGameAnnouncements(self);
	}

	/* FIXMESTEVE
	ForEach DynamicActors(class'Actor', A)
		A.PrecacheAnnouncer(self);
	*/
	if ( !bPrecachedBaseSounds )
	{
		bPrecachedBaseSounds = true;

		PrecacheRewardSound('Headshot_Cue');
		PrecacheRewardSound('Headhunter_Cue');
		PrecacheRewardSound('Berzerk_Cue');
		PrecacheRewardSound('Booster_Cue');
		PrecacheRewardSound('FlackMonkey_Cue');
		PrecacheRewardSound('Combowhore_Cue');
		PrecacheRewardSound('Invisible_Cue');
		PrecacheRewardSound('Speed_Cue');
		PrecacheRewardSound('Camouflaged_Cue');
		PrecacheRewardSound('Pint_sized_Cue');
		PrecacheRewardSound('first_blood_Cue');
		PrecacheRewardSound('adrenalin_Cue');
		PrecacheRewardSound('Double_Kill_Cue');
		PrecacheRewardSound('MultiKill_Cue');
		PrecacheRewardSound('MegaKill_Cue');
		PrecacheRewardSound('UltraKill_Cue');
		PrecacheRewardSound('MonsterKill_Cue');
		PrecacheRewardSound('LudicrousKill_Cue');
		PrecacheRewardSound('HolyShit_Cue');
		PrecacheRewardSound('Killing_Spree_Cue');
		PrecacheRewardSound('Rampage_Cue');
		PrecacheRewardSound('Dominating_Cue');
		PrecacheRewardSound('Unstoppable_Cue');
		PrecacheRewardSound('GodLike_Cue');
		PrecacheRewardSound('WhickedSick_Cue');

		PrecacheStatusSound('one_Cue');
		PrecacheStatusSound('two_Cue');
		PrecacheStatusSound('three_Cue');
		PrecacheStatusSound('four_Cue');
		PrecacheStatusSound('five_Cue');
		PrecacheStatusSound('six_Cue');
		PrecacheStatusSound('seven_Cue');
		PrecacheStatusSound('eight_Cue');
		PrecacheStatusSound('nine_Cue');
		PrecacheStatusSound('ten_Cue');
	}
}

defaultproperties
{
    AnnouncerLevel=2

	StatusSoundPackage="UTAnnouncerFemale"
	RewardSoundPackage="UTAnnouncerMale"
	FallbackStatusSoundPackage="UTAnnouncerFemale"
	FallbackRewardSoundPackage="UTAnnouncerMale"
}
