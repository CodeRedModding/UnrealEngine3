/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif


#ifndef NAMES_ONLY
#define AUTOGENERATE_NAME(name) extern FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

enum SoundDistanceModel
{
    ATTENUATION_Linear      =0,
    ATTENUATION_MAX         =1,
};

class USoundNodeAttenuation : public USoundNode
{
public:
    BYTE DistanceModel;
    class UDistributionFloat* MinRadius GCC_PACK(PROPERTY_ALIGNMENT);
    class UDistributionFloat* MaxRadius;
    BITFIELD bSpatialize:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bAttenuate:1;
    DECLARE_CLASS(USoundNodeAttenuation,USoundNode,0,Engine)
	// USoundNode interface.
			
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
};


class USoundNodeLooping : public USoundNode
{
public:
    class UDistributionFloat* LoopCount;
    DECLARE_CLASS(USoundNodeLooping,USoundNode,0,Engine)
	// USoundNode interface.

	virtual UBOOL Finished(  class UAudioComponent* AudioComponent );

	/**
	 * Returns whether this sound node will potentially loop leaf nodes.
	 *
	 * @return TRUE
	 */
	virtual UBOOL IsPotentiallyLooping() { return TRUE; }

	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
};


class USoundNodeMixer : public USoundNode
{
public:
    DECLARE_CLASS(USoundNodeMixer,USoundNode,0,Engine)
	// USoundNode interface.

	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
	virtual INT GetMaxChildNodes() { return -1; }
};


class USoundNodeModulator : public USoundNode
{
public:
    class UDistributionFloat* VolumeModulation;
    class UDistributionFloat* PitchModulation;
    DECLARE_CLASS(USoundNodeModulator,USoundNode,0,Engine)
	// USoundNode interface.
		
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
};


class USoundNodeOscillator : public USoundNode
{
public:
    class UDistributionFloat* Amplitude;
    class UDistributionFloat* Frequency;
    class UDistributionFloat* Offset;
    class UDistributionFloat* Center;
    BITFIELD bModulatePitch:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bModulateVolume:1;
    DECLARE_CLASS(USoundNodeOscillator,USoundNode,0,Engine)
	// USoundNode interface.
		
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
};


class USoundNodeRandom : public USoundNode
{
public:
    TArrayNoInit<FLOAT> Weights;
    DECLARE_CLASS(USoundNodeRandom,USoundNode,0,Engine)
	// USoundNode interface.
	
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	virtual INT GetMaxChildNodes() { return -1; }
	
	// Editor interface.
	
	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );
};

#endif


#ifndef NAMES_ONLY
#undef AUTOGENERATE_NAME
#undef AUTOGENERATE_FUNCTION
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

