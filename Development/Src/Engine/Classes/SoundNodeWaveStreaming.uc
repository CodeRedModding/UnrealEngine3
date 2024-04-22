/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class provides a simple adapter between the audio device and code that wants to produce
 *  an ongoing stream of audio data.
 * You feed data to this class through its QueueAudio() method, and it will slowly feed it to 
 *  the audio device as more sound data is needed for playback.
 */

class SoundNodeWaveStreaming extends SoundNodeWave
	native( Sound );

/** FIFO data to give to the audio device in GeneratePCMData(). */
var private array<byte> QueuedAudio;

/** The length of time this sound node has been inactive */
var const float InactiveDuration;


/** Add data to the FIFO that feeds the audio device. */
native event QueueAudio(Array<byte> Data);

/** Adds blank-data/silence to the FIFO */
native event QueueSilence(float Seconds);

/** Remove all queued data from the FIFO. This is only necessary if you want to start over, or GeneratePCMData() isn't going to be called, since that will eventually drain it. */
native event ResetAudio();

/** Query bytes queued for playback */
native event int AvailableAudioBytes();

/** overridden from SoundNodeWave superclass. */
native event GeneratePCMData(out Array<byte> Buffer, int SamplesNeeded);

defaultproperties
{
	bProcedural=true
	bLoopingSound=false
}
