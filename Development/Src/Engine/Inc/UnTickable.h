/*=============================================================================
	UnTickable.h: Interface for tickable objects.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

/**
 * This class provides common registration for tickable objects. It is an
 * abstract base class requiring you to implement the Tick() method.
 */
class FTickableObject
{
public:
	/** Static array of tickable objects */
	static TArray<FTickableObject*>	TickableObjects;

	/**
	 * Registers this instance with the static array of tickable objects.	
	 */
	FTickableObject()
	{
		TickableObjects.AddItem( this );
	}

	/**
	 * Removes this instance from the static array of tickable objects.
	 */
	virtual ~FTickableObject()
	{
		TickableObjects.RemoveItem( this );
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime ) = 0;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
