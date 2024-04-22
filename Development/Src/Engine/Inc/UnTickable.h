/*=============================================================================
	UnTickable.h: Interface for tickable objects.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * This class provides common registration for gamethread tickable objects. It is an
 * abstract base class requiring you to implement the Tick() method.
 */
class FTickableObject
{
public:
	/** Static array of tickable objects */
	static TArrayNoInit<FTickableObject*> TickableObjects;

	/**
	 * Registers this instance with the static array of tickable objects.	
	 *
	 */
	FTickableObject()
	{
		checkf(IsInGameThread(), TEXT("Rendering thread attempted to register an object in the TickableObjects array."));
		if ( !GIsAffectingClassDefaultObject )
		{
			check(!TickableObjects.ContainsItem(this));
			TickableObjects.AddItem( this );
		}
	}

	/**
	 * Removes this instance from the static array of tickable objects.
	 */
	virtual ~FTickableObject()
	{
		checkf(!GIsGuarded || IsInGameThread(), TEXT("Rendering thread attempted to unregister an object in the TickableObjects array."));
		if ( !GIsAffectingClassDefaultObject )
		{
			// make sure this tickable object was registered from the game thread
			const INT Pos = TickableObjects.FindItemIndex(this);
			check(Pos!=INDEX_NONE);
			TickableObjects.Remove(Pos);
		}
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime ) = 0;

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const = 0;

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 * Defaults to false, as that mimics old behavior.
	 *
	 * @return TRUE if it should be ticked when paused, FALSE otherwise
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return FALSE;
	}

	/**
	 * Used to determine whether the object should be ticked in the editor.  Defaults to FALSE since
	 * that is the previous behavior.
	 *
	 * @return	TRUE if this tickable object can be ticked in the editor
	 */
	virtual UBOOL IsTickableInEditor() const
	{
		return FALSE;
	}

};

/**
 * This class provides common registration for render thread tickable objects. It is an
 * abstract base class requiring you to implement the Tick() method.
 */
class FTickableObjectRenderThread
{
public:

	/** Static array of tickable objects that are ticked from rendering thread*/
	static TArrayNoInit<FTickableObjectRenderThread*> RenderingThreadTickableObjects;

	/**
	 * Registers this instance with the static array of tickable objects.	
	 *
	 * @param bRegisterImmediately TRUE if the object should be registered immediately.
	 */
	FTickableObjectRenderThread(UBOOL bRegisterImmediately=TRUE)
	{
		if(bRegisterImmediately)
		{
			Register();
		}
	}

	/**
	 * Removes this instance from the static array of tickable objects.
	 */
	virtual ~FTickableObjectRenderThread()
	{
		// make sure this tickable object was registered from the rendering thread
		checkf(IsInRenderingThread(), TEXT("Game thread attempted to unregister an object in the RenderingThreadTickableObjects array."));
		const INT Pos=RenderingThreadTickableObjects.FindItemIndex(this);
		check(Pos!=INDEX_NONE);
		RenderingThreadTickableObjects.Remove(Pos);
	}

	/**
	 * Registers the object for ticking.
	 * @param bIsRenderingThreadObject TRUE if this object is owned by the rendering thread.
	 */
	void Register(UBOOL bIsRenderingThreadObject = FALSE)
	{
		// make sure that only the rendering thread is attempting to add items to the RenderingThreadTickableObjects list
		checkf(IsInRenderingThread(), TEXT("Game thread attempted to register an object in the RenderingThreadTickableObjects array."));
		check(!RenderingThreadTickableObjects.ContainsItem(this));
		RenderingThreadTickableObjects.AddItem( this );
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime ) = 0;

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const = 0;

	/**
	 * Used to determine if a rendering thread tickable object must have rendering in a non-suspended
	 * state during it's Tick function.
	 *
	 * @return TRUE if the RHIResumeRendering should be called before tick if rendering has been suspended
	 */
	virtual UBOOL NeedsRenderingResumedForRenderingThreadTick() const
	{
		return FALSE;
	}
};

