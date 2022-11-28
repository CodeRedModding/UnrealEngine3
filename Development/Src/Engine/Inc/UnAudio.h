/*=============================================================================
	UnAudio.h: Unreal base audio.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Wave modification code by Erik de Neve
		* Complete rewrite by Daniel Vogel
=============================================================================*/

#ifdef PlaySound
#undef PlaySound
#endif

class UAudioComponent;
class FSoundSource;
struct FSampleLoop;

/*-----------------------------------------------------------------------------
	UAudioDevice.
-----------------------------------------------------------------------------*/

// Listener.
struct FListener
{
	FVector		Location,
				Up,
				Right,
				Front;
};

//     2 UU == 1"
// <=> 1 UU == 0.0127 m
#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

class UAudioDevice : public USubsystem
{
	DECLARE_CLASS(UAudioDevice,USubsystem,CLASS_Config,Engine)

	// Constructor.
	UAudioDevice() {}
	void StaticConstructor();

	// UAudioDevice interface.
	virtual UBOOL Init();
	virtual void Flush();
	virtual void Update( UBOOL Realtime );
	void SetListener( const FVector& Location, const FVector& Up, const FVector& Right, const FVector& Front );

	// Audio components.
	void AddComponent( UAudioComponent* AudioComponent );
	void RemoveComponent( UAudioComponent* AudioComponent );
	static UAudioComponent* CreateComponent( USoundCue* SoundCue, FScene* Scene, AActor* Actor = NULL, UBOOL Play = 1 );

	// UObject interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) { return false; }
	void Serialize(FArchive& Ar);
	virtual void SerializeHack(FArchive& Ar) { appErrorf(TEXT("UAudioDevice::SerializeHack shouldn never be called")); }

protected:
	// Internal.
	void SortWaveInstances( INT MaxChannels );


	// Configuration.
	FLOAT										SoundVolume;
	INT											MaxChannels;					

	// Variables.		
	TArray<UAudioComponent*>					AudioComponents;
	TArray<FSoundSource*>						Sources;	
	TArray<FSoundSource*>						FreeSources;
	TDynamicMap<FWaveInstance*,FSoundSource*>	WaveInstanceSourceMap;
	UBOOL										LastRealtime;
	FListener									Listener;
	QWORD										CurrentTick;

	friend class FSoundSource;
};

/*-----------------------------------------------------------------------------
	FSoundSource.
-----------------------------------------------------------------------------*/

class FSoundSource
{
public:
	// Constructor/ Destructor.
	FSoundSource( UAudioDevice* InAudioDevice )
	:	AudioDevice( InAudioDevice ),
		WaveInstance( NULL ),
		LastUpdate( 0 )
	{}

	// Initialization & update.
	virtual UBOOL Init( FWaveInstance* WaveInstance ) = 0;
	virtual void Update() = 0;

	// Playback.
	virtual void Play() = 0;
	virtual void Stop();
	virtual void Pause() = 0;

	// Query.
	virtual	UBOOL IsFinished() = 0;
	
protected:
	// Variables.	
	UAudioDevice*		AudioDevice;
	FWaveInstance*		WaveInstance;
	INT					LastUpdate;

	friend class UAudioDevice;
};

/*-----------------------------------------------------------------------------
	USoundCue. 
-----------------------------------------------------------------------------*/

struct FSoundNodeEditorData
{
	INT	NodePosX;
	INT	NodePosY;

	friend FArchive& operator<<( FArchive& Ar, FSoundNodeEditorData* EditorData );
};

class USoundCue : public UObject
{
	DECLARE_CLASS(USoundCue,UObject,0,Engine)

	class USoundNode*						FirstNode;
	TMap<USoundNode*,FSoundNodeEditorData>	EditorData; // Editor-specific info (node position etc)


	// UObject interface.

	virtual void Serialize( FArchive& Ar );

	// for Browser window
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );


	// USoundCue interface

	// Tool drawing
	void DrawCue(FRenderInterface* RI, TArray<USoundNode*>& SelectedNodes);
};

/*-----------------------------------------------------------------------------
	USoundNodeWave. 
-----------------------------------------------------------------------------*/

class USoundNodeWave : public USoundNode
{
	DECLARE_CLASS(USoundNodeWave,USoundNode,0,Engine)

    FLOAT				Volume;
    FLOAT				Pitch;
	FLOAT				Duration;
    FName				FileType;
    TLazyArray<BYTE>	RawData;
	INT					ResourceID;

	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	// USoundNode interface.
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
	virtual INT GetMaxChildNodes() { return 0; }

	// Browser window.
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
};

/*-----------------------------------------------------------------------------
	FWaveModInfo. 
-----------------------------------------------------------------------------*/

//
// Structure for in-memory interpretation and modification of WAVE sound structures.
//
class FWaveModInfo
{
public:

	// Pointers to variables in the in-memory WAVE file.
	DWORD* pSamplesPerSec;
	DWORD* pAvgBytesPerSec;
	_WORD* pBlockAlign;
	_WORD* pBitsPerSample;
	_WORD* pChannels;

	DWORD  OldBitsPerSample;

	DWORD* pWaveDataSize;
	DWORD* pMasterSize;
	BYTE*  SampleDataStart;
	BYTE*  SampleDataEnd;
	DWORD  SampleDataSize;
	BYTE*  WaveDataEnd;

	INT	   SampleLoopsNum;
	FSampleLoop*  pSampleLoop;

	DWORD  NewDataSize;
	UBOOL  NoiseGate;

	// Constructor.
	FWaveModInfo()
	{
		NoiseGate   = false;
		SampleLoopsNum = 0;
	}
	
	// 16-bit padding.
	DWORD Pad16Bit( DWORD InDW )
	{
		return ((InDW + 1)& ~1);
	}

	// Read headers and load all info pointers in WaveModInfo. 
	// Returns 0 if invalid data encountered.
	// UBOOL ReadWaveInfo( TArray<BYTE>& WavData );
	UBOOL ReadWaveInfo( TArray<BYTE>& WavData );
};

/*-----------------------------------------------------------------------------
	USoundNode helper macros. 
-----------------------------------------------------------------------------*/

#define DECLARE_SOUNDNODE_ELEMENT(Type,Name)													\
	Type& ##Name = *((Type*)(Payload));															\
	Payload += sizeof(Type);														

#define DECLARE_SOUNDNODE_ELEMENT_PTR(Type,Name)												\
	Type* ##Name = (Type*)(Payload);															\
	Payload += sizeof(Type);														

#define	RETRIEVE_SOUNDNODE_PAYLOAD( Size )														\
		BYTE*	Payload					= NULL;													\
		UBOOL*	RequiresInitialization	= NULL;													\
		{																						\
			UINT* TempOffset = AudioComponent->SoundNodeOffsetMap.Find( this );					\
			UINT Offset;																		\
			if( !TempOffset )																	\
			{																					\
				Offset = AudioComponent->SoundNodeData.AddZeroed( Size + sizeof(UBOOL));		\
				AudioComponent->SoundNodeOffsetMap.Set( this, Offset );							\
				RequiresInitialization = (UBOOL*) &AudioComponent->SoundNodeData(Offset);		\
				*RequiresInitialization = 1;													\
				Offset += sizeof(UBOOL);														\
			}																					\
			else																				\
			{																					\
				RequiresInitialization = (UBOOL*) &AudioComponent->SoundNodeData(*TempOffset);	\
				Offset = *TempOffset + sizeof(UBOOL);											\
			}																					\
			Payload = &AudioComponent->SoundNodeData(Offset);									\
		}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

