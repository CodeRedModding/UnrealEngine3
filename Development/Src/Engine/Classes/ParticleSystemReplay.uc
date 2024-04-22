/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleSystemReplay extends Object
	native( Particle )
	hidecategories( Object )
	AutoExpandCategories( ParticleSystemReplay );




struct native ParticleEmitterReplayFrame
{
	/** Type of emitter (EDynamicEmitterType) */
	var native const int EmitterType;

	/** Original index into the particle systems list of particle emitter indices.  This is currently
	    only needed for mesh emitters. */
	var native const int OriginalEmitterIndex;

	/** State for the emitter this frame.  The actual object type  */
	var native const pointer FrameState{ struct FDynamicEmitterReplayDataBase };


	structcpptext
	{
		/** Constructors */
		FParticleEmitterReplayFrame() {}
		FParticleEmitterReplayFrame( EEventParm )
			: EmitterType( DET_Unknown ),
			  OriginalEmitterIndex( INDEX_NONE ),
			  FrameState( NULL )
		{
		}

		/** Destructor */
		~FParticleEmitterReplayFrame()
		{
			// Clean up frame state
			if( FrameState != NULL )
			{
				delete FrameState;
				FrameState = NULL;
			}
		}

		/** Serialization operator */
		friend FArchive& operator<<( FArchive& Ar, FParticleEmitterReplayFrame& Obj );
	}
};
																						


/** A single frame within this replay */
struct native ParticleSystemReplayFrame
{
	/** Emitter frame state data */
	var native const array< ParticleEmitterReplayFrame > Emitters;


	structcpptext
	{
		/** Constructors */
		FParticleSystemReplayFrame() {}
		FParticleSystemReplayFrame( EEventParm )
		{
			appMemzero( this, sizeof( FParticleSystemReplayFrame ) );
		}

		/** Serialization operator */
		friend FArchive& operator<<( FArchive& Ar, FParticleSystemReplayFrame& Obj );
	}
};



/** Unique ID number for this replay clip */
var() native int ClipIDNumber;

/** Ordered list of frames */
var native const array< ParticleSystemReplayFrame > Frames;



cpptext
{
	/** Serialization */
	virtual void Serialize( FArchive& Ar );
}



defaultproperties
{
}
