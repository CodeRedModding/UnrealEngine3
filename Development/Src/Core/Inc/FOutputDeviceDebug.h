/*=============================================================================
	FOutputDeviceDebug.h: Windows debugging text output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
// ANSI file output device.
//
class FOutputDeviceDebug : public FOutputDevice
{
public:
	void Serialize( const TCHAR* Data, enum EName Event )
	{
		static UBOOL Entry=0;
		if( !GIsCriticalError || Entry )
		{
			if( !FName::SafeSuppressed(Event) )
			{
				if( Event!=NAME_Title )
				{
					TCHAR	Temp[1024];
					appSprintf(Temp,TEXT("%s: %s%s"),FName::SafeString(Event),Data,LINE_TERMINATOR);
					OutputDebugString(Temp);
				}
			}
		}
		else
		{
			Entry=1;
			try
			{
				// Ignore errors to prevent infinite-recursive exception reporting.
				Serialize( Data, Event );
			}
			catch( ... )
			{}
			Entry=0;
		}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

