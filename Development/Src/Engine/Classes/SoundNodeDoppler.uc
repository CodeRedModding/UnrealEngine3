/**
 * Computes doppler pitch shift
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SoundNodeDoppler extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/** Scales the magnitude of the pitch shift.  1.0 is normal. */
var(Doppler) float DopplerIntensity<ToolTip=How much to scale the doppler shift (1.0 is normal)>;

cpptext
{
public:
	/** 
	 * USoundNode interface. 
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );

protected:
	FLOAT GetDopplerPitchMultiplier(FListener const& InListener, UAudioComponent const& AudioComponent) const;
}

defaultproperties
{
	DopplerIntensity=1.f
}
