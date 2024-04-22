/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:	
	/**
	 * Called when the user double clicks on a sound node object, can be used to display node specific
	 * property dialogs.
	 *
	 * @param InEditor	Pointer to the sound cue editor that initiated the callback.
	 * @param InNode	Pointer to the sound node that was clicked on.
	 */
	virtual void  OnDoubleClick( const class WxSoundCueEditor* InEditor, USoundNode* InNode ) const 
	{ 
	}
