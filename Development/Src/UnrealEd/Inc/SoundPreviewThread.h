/**
 * Sound quality preview helper thread.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __SOUNDPREVIETHREAD_H__
#define __SOUNDPREVIETHREAD_H__

class FSoundPreviewThread : public FRunnable
{
public:
	/**
	 * @param PreviewCount	number of sounds to preview
	 * @param Node			sound node to perform compression/decompression
	 * @param Info			preview info class
	 */
	FSoundPreviewThread( INT PreviewCount, USoundNodeWave *Node, FPreviewInfo *Info );

	virtual UBOOL Init( void );
	virtual DWORD Run( void );
	virtual void Stop( void );
	virtual void Exit( void );

	/**
	 * @return true if job is finished
	 */
	UBOOL IsTaskFinished( void ) const 
	{ 
		return TaskFinished; 
	}

	/**
	 * @return number of sounds to conversion.
	 */
	INT GetCount( void ) const 
	{ 
		return Count; 
	}

	/**
	 * @return index of last converted sound.
	 */
	INT GetIndex( void ) const 
	{ 
		return Index; 
	}

private:
	/** Number of sound to preview.	*/
	INT					Count;					
	/** Last converted sound index, also using to conversion loop. */
	INT  Index;					
	/** Pointer to sound node to perform preview. */
	USoundNodeWave*		SoundNode;
	/** Pointer to preview info table. */
	FPreviewInfo*		PreviewInfo; 
	/** True if job is finished. */
	UBOOL				TaskFinished;
	/** True if cancel calculations and stop thread. */
	UBOOL				CancelCalculations;	
};
	
#endif
