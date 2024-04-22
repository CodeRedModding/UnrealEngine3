// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime );

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const;

	virtual void BeginDestroy();

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 *
	 * @return always TRUE as Facebook login is in pause menu.
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}
