/*=============================================================================
	UnObserver.h: Interface for observer objects

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved
=============================================================================*/

/**
 * interface for scene observers
 */
class FObserverInterface
{
public:
	// constructor
	FObserverInterface()
	{
		AddObserver();
	}


	// destructor
	virtual ~FObserverInterface()
	{
		RemoveObserver();
	}

	// FObserver Interface

	virtual FVector	GetObserverViewLocation() = 0;

	virtual void AddObserver();

	virtual void RemoveObserver();
};