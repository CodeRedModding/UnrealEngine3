/*=============================================================================
	FOutputDeviceRedirector.h: Redirecting output device.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/


class FOutputDeviceRedirector : public FOutputDeviceRedirectorBase
{
private:
	TArray<FOutputDevice*> OutputDevices;

public:

	/**
	 * Adds an output device to the chain of redirections.	
	 *
	 * @param OutputDevice	output device to add
	 */
	void AddOutputDevice( FOutputDevice* OutputDevice )
	{
		if( OutputDevice )
		{
			OutputDevices.AddUniqueItem( OutputDevice );
		}
	}

	/**
	 * Removes an output device from the chain of redirections.	
	 *
	 * @param OutputDevice	output device to remove
	 */
	void RemoveOutputDevice( FOutputDevice* OutputDevice )
	{
		OutputDevices.RemoveItem( OutputDevice );
	}

	/**
	 * Returns whether an output device is currently in the list of redirectors.
	 *
	 * @param	OutputDevice	output device to check the list against
	 * @return	TRUE if messages are currently redirected to the the passed in output device, FALSE otherwise
	 */
	UBOOL IsRedirectingTo( FOutputDevice* OutputDevice )
	{
		return OutputDevices.FindItemIndex( OutputDevice ) == INDEX_NONE ? FALSE : TRUE;
	}

	void Serialize( const TCHAR* Data, enum EName Event )
	{
        for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->Serialize( Data, Event );
		}
	}

	void Flush()
	{
        for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->Flush();
		}
	}

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown()
	{
		for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->TearDown();
		}

		OutputDevices.Empty();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

