/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

protected:
	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	/**
	 * Returns a description of this object that can be used as extra information in list views.
	 */
	virtual FString GetDesc( void );

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Called when a property value from a member struct or array has been changed in the editor.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Called after duplication & serialization and before PostLoad.
	 */
	virtual void PostDuplicate( void );

public:
	/** 
	 * Populate the enum using the serialised fname
	 */
	void Fixup( void );
