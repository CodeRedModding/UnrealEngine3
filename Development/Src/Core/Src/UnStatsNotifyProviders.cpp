/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the implementations of the various remote stats facilities.
 */

#include "CorePrivate.h"

#include "UnStatsNotifyProviders.h"
#include "Engine.h"
#include "ProfilingHelpers.h"

#include <stdio.h>

#if STATS

/**
 * Tells the parent class to open the file and then writes the opening
 * XML data to it
 */
UBOOL FStatNotifyProvider_File::Init(void)
{
	if( GIsSilent == TRUE )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Closes the file
 */
void FStatNotifyProvider_File::Destroy(void)
{
	// Close the output file if we have one
	CloseOutputFile();
}



/**
 * Creates the output file
 *
 * @return	TRUE if successful
 */
UBOOL FStatNotifyProvider_File::CreateOutputFile()
{
	const FString PathName = *(appProfilingDir() + TEXT("UE3Stats") PATH_SEPARATOR );
	GFileManager->MakeDirectory( *PathName );
	//warnf( TEXT( "ustats path: %s" ), *PathName );

	FString Filename = CreateProfileFilename( AppendedName, TRUE );
	Filename = PathName + Filename;
	//warnf( TEXT( "ustats Filename: %s" ), *Filename );


#if !CONSOLE
	FString PassedInName;
	// Check for the name passed in on the command line
	if (Parse(appCmdLine(),TEXT("STATFILE="),PassedInName))
	{
		Filename = PathName;
		Filename += PassedInName + AppendedName;
	}
#endif

	//warnf( TEXT( "ustats full: %s" ), *(Filename) );

	// Ask for an asynchronous file writer
	File = GFileManager->CreateFileWriter( *Filename, FILEWRITE_AllowRead | FILEWRITE_Async );
	ArchiveFilename = Filename;

	return File != NULL;
}



/**
 * Flushes and closes the stats file
 */
void FStatNotifyProvider_File::CloseOutputFile()
{
	if( File != NULL )
	{
		// Flushes any pending file writes to disk
		File->Flush();

		// Close the file
		delete File;
		File = NULL;
	}
}



/**
 * Builds the output string from varargs and writes it out
 *
 * @param Format the format string to use with the varargs
 */
void FStatNotifyProvider_File::WriteString(const ANSICHAR* Format,...)
{
	if( File != NULL )
	{
		check(File);
		ANSICHAR Array[1024];
		va_list ArgPtr;
		va_start(ArgPtr,Format);
		// Build the string
		INT Result = appGetVarArgsAnsi(Array,ARRAY_COUNT(Array),ARRAY_COUNT(Array)-1,Format,ArgPtr);
		// Now write that to the file
		File->Serialize((void*)Array,Result);
	}
}



/**
 * Types of data found in a binary stats file
 */
enum EStatsDataChunkTag
{
	/** Unknown or invalid data tag type */
	EStatsDataChunkTag_Invalid = 0,

	/** Frame data */
	EStatsDataChunkTag_FrameData,

	/** Group descriptions */
	EStatsDataChunkTag_GroupDescriptions,

	/** Stat descriptions */
	EStatsDataChunkTag_StatDescriptions
};



/**
 * Starts writing stats data to disk
 */
void FStatNotifyProvider_BinaryFile::StartWritingStatsFile()
{
	// NOTE: Make sure to update BinaryStatFileVersion if you change serialization behavior or data formats!

	if( File != NULL )
	{
		debugf( TEXT( "Stats System: Can't start capturing stats because a capture is already in progress." ) );
		return;
	}

	
	// Create the file
	if( !CreateOutputFile() )
	{
		debugf( TEXT( "Stats System: Couldn't create output file for stat capture." ) );
		return;
	}
	check( File != NULL );

	// We're expecting the files to be written out in big-endian since that's what StatsViewer wants
#if !( NO_BYTE_ORDER_SERIALIZE && !WANTS_XBOX_BYTE_SWAPPING )
	File->SetByteSwapping( TRUE );
#endif

	debugf( TEXT( "Stats System: Capturing stat data to disk." ) );

	FArchive& OutputFile = *File;


	// Write header
	{
		// NOTE: If you change the binary stats file format, update this version number as well
		//       as the version number in StatsViewerMisc.cpp!
		/* const */ DWORD BinaryStatFileVersion = 3;


		// Header tag (ASCII)
		const ANSICHAR* HeaderTagString = "USTATS";
		const INT HeaderTagLength = appStrlen( HeaderTagString );
		for( INT CharIndex = 0; CharIndex < HeaderTagLength; ++CharIndex )
		{
			BYTE CurCharByte = ( BYTE )HeaderTagString[ CharIndex ];
			OutputFile << CurCharByte;
		}

		// Version info
		OutputFile << BinaryStatFileVersion;

		// Number of seconds for each cycle value
		FLOAT SecondsPerCycleAsFloat = ( FLOAT )GSecondsPerCycle;
		OutputFile << SecondsPerCycleAsFloat;
	}

	
	// Force scoped cycle stats to be enabled.
	GForceEnableScopedCycleStats++;
}



/**
 * Stop writing stats data and finalize the file
 */
void FStatNotifyProvider_BinaryFile::StopWritingStatsFile()
{
	if( File != NULL )
	{
		FArchive& OutputFile = *File;

		// Make sure the stats manager has a fresh list of descriptions for us.  Note that
		// this MUST happen after we've finished capturing stat data, because some stat types
		// are added on-demand, such as script cycle stats
		// @todo: This is a bit sketchy right here, we should avoid hitting the other provider types
		GStatManager.SendNotifiersDescriptions();


		// Write group descriptions
		{
			// Write data chunk tag for 'Script profiler data'
			DWORD DataChunkTag = EStatsDataChunkTag_GroupDescriptions;
			OutputFile << DataChunkTag;

			/* const */ WORD GroupDescriptionCount = GroupDescriptions.Num();
			OutputFile << GroupDescriptionCount;

			for( WORD CurGroupIndex = 0; CurGroupIndex < GroupDescriptionCount; ++CurGroupIndex )
			{
				/* const */ FStatGroupDescriptionData& CurGroupDesc = GroupDescriptions( CurGroupIndex );
				
				WORD ShortGroupID = CurGroupDesc.ID;
				OutputFile << ShortGroupID;
				OutputFile << CurGroupDesc.Name;
			}
		}


		// Write stat descriptions
		{
			// Write data chunk tag for 'Script profiler data'
			DWORD DataChunkTag = EStatsDataChunkTag_StatDescriptions;
			OutputFile << DataChunkTag;

			/* const */ WORD StatDescriptionCount = StatDescriptions.Num();
			OutputFile << StatDescriptionCount;

			for( WORD CurStatIndex = 0; CurStatIndex < StatDescriptionCount; ++CurStatIndex )
			{
				/* const */ FStatDescriptionData& CurStatDesc = StatDescriptions( CurStatIndex );
				
				WORD ShortStatID = CurStatDesc.ID;
				OutputFile << ShortStatID;
				OutputFile << CurStatDesc.Name;
				BYTE ShortStatType = CurStatDesc.StatType;
				OutputFile << ShortStatType;
				WORD ShortGroupID = CurStatDesc.GroupID;
				OutputFile << ShortGroupID;
			}
		}

	

		const FString FullFileName = ArchiveFilename;

		// Flush and close the file
		CloseOutputFile();


		warnf(TEXT("UE3Stats: done writing file [%s]"),*(FullFileName));

		SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!UE3STATS:"), FullFileName );

		// Decrease counter forcing scoped cycle stats to be enabled.
		--GForceEnableScopedCycleStats;

		debugf( TEXT( "Stats System: Finished capturing stat data." ) );

	}
	else
	{
		// File not started yet?
		debugf( TEXT( "Stats System: Stats file capture hasn't been started yet." ) );
	}
}



/**
 * Creates the file that is going to be written to
 */
UBOOL FStatNotifyProvider_BinaryFile::Init(void)
{
	if( !FStatNotifyProvider_File::Init() )
	{
		return FALSE;
	}

	// NOTE: Binary stats capture doesn't start capturing until the user enters "Stat StartFile"

	if( ParseParam( appCmdLine(), TEXT( "StartStatsFile" ) ) )
	{
		// Start capturing right away!
		StartWritingStatsFile();
	}

	return TRUE;
}


/**
 * Makes the data well formed and closes the file
 */
void FStatNotifyProvider_BinaryFile::Destroy(void)
{
	// Now close the file
	FStatNotifyProvider_File::Destroy();
}



/**
 * Tells the provider that we are starting to supply it with descriptions
 * for all of the stats/groups.
 */
void FStatNotifyProvider_BinaryFile::StartDescriptions(void)
{
	// Nothing to do here
}



/**
 * Tells the provider that we are finished sending descriptions for all of
 * the stats/groups.
 */
void FStatNotifyProvider_BinaryFile::EndDescriptions(void)
{
	// Nothing to do here
}



/**
 * Tells the provider that we are starting to supply it with group descriptions
 */
void FStatNotifyProvider_BinaryFile::StartGroupDescriptions(void)
{
	// Clear group description list
	GroupDescriptions.Reset();
}



/**
 * Tells the provider that we are finished sending stat descriptions
 */
void FStatNotifyProvider_BinaryFile::EndGroupDescriptions(void)
{
	// Nothing to do here
}



/**
 * Tells the provider that we are starting to supply it with stat descriptions
 */
void FStatNotifyProvider_BinaryFile::StartStatDescriptions(void)
{
	// Clear description list
	StatDescriptions.Reset();
}



/**
 * Tells the provider that we are finished sending group descriptions
 */
void FStatNotifyProvider_BinaryFile::EndStatDescriptions(void)
{
	// Nothing to do here
}



/**
 * Adds a stat to the list of descriptions. Used to allow custom stats to
 * report who they are, parentage, etc. Prevents applications that consume
 * the stats data from having to change when stats information changes
 *
 * @param StatId the id of the stat
 * @param StatName the name of the stat
 * @param StatType the type of stat this is
 * @param GroupId the id of the group this stat belongs to
 */
void FStatNotifyProvider_BinaryFile::AddStatDescription(DWORD StatId,const TCHAR* StatName,
	DWORD StatType,DWORD GroupId)
{
	// Add new stat description
	FStatDescriptionData NewStatDesc;
	NewStatDesc.ID = StatId;
	NewStatDesc.Name = StatName;
	NewStatDesc.StatType = StatType;
	NewStatDesc.GroupID = GroupId;

	StatDescriptions.AddItem( NewStatDesc );
}



/**
 * Adds a group to the list of descriptions
 *
 * @param GroupId the id of the group being added
 * @param GroupName the name of the group
 */
void FStatNotifyProvider_BinaryFile::AddGroupDescription(DWORD GroupId,const TCHAR* GroupName)
{
	// Add new group description
	FStatGroupDescriptionData NewGroupDesc;
	NewGroupDesc.ID = GroupId;
	NewGroupDesc.Name = GroupName;

	GroupDescriptions.AddItem( NewGroupDesc );
}



/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param ParentId the id of the parent stat
 * @param InstanceId the instance id of the stat being written
 * @param ParentInstanceId the instance id of parent stat
 * @param ThreadId the thread this stat is for
 * @param Value the value of the stat to write out
 * @param CallsPerFrame the number of calls for this frame
 */
void FStatNotifyProvider_BinaryFile::WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,
	DWORD InstanceId,DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
	DWORD CallsPerFrame)
{
	// Only record stats if we're currently writing to a file
	if( File != NULL )
	{
		// Add new stat
		const INT NewStatIndex = CycleStatsForCurrentFrame.Add();
		FCycleStatData& NewStat = CycleStatsForCurrentFrame( NewStatIndex );

		NewStat.StatID = StatId;
		// NewStat.GroupID = GroupId;
		// NewStat.ParentID = ParentId;
		NewStat.InstanceID = InstanceId;
		NewStat.ParentInstanceID = ParentInstanceId;
		NewStat.ThreadID = ThreadId;
		NewStat.Value = Value;
		NewStat.CallsPerFrame = CallsPerFrame;
	}
}



/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_BinaryFile::WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value)
{
	// Only record stats if we're currently writing to a file
	if( File != NULL )
	{
		// Add new float stat
		const INT NewStatIndex = FloatStatsForCurrentFrame.Add();
		FFloatCounterStatData& NewStat = FloatStatsForCurrentFrame( NewStatIndex );

		NewStat.StatID = StatId;
		NewStat.Value = Value;
	}
}



/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_BinaryFile::WriteStat(DWORD StatId,DWORD GroupId,DWORD Value)
{
	// Only record stats if we're currently writing to a file
	if( File != NULL )
	{
		// Add new integer stat
		const INT NewStatIndex = IntegerStatsForCurrentFrame.Add();
		FIntegerCounterStatData& NewStat = IntegerStatsForCurrentFrame( NewStatIndex );

		NewStat.StatID = StatId;
		NewStat.Value = Value;
	}
}



/**
 * Creates a new frame element
 *
 * @param FrameNumber the new frame number being processed
 */
void FStatNotifyProvider_BinaryFile::SetFrameNumber(DWORD FrameNumber)
{
	const INT OldFrameNumber = CurrentFrame;

	// Call base class version
	FStatNotifyProvider::SetFrameNumber(FrameNumber);

	if( File != NULL &&
		OldFrameNumber != CurrentFrame )
	{

		// Capture the player's viewpoint
		FVector ViewLocation = FVector( 0.0f, 0.0f, 0.0f );
		FRotator ViewRotation = FRotator( 0, 0, 0 );
		if( GEngine->GamePlayers.Num() > 0 )
		{
		    ULocalPlayer* Player = GEngine->GamePlayers( 0 );
			if( Player != NULL && Player->Actor != NULL )
			{
				// Calculate the player's view information.
				Player->Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );
			}
		}

		FArchive& OutputFile = *File;


		// Write data chunk tag for 'Frame data'
		DWORD FrameDataChunkTag = EStatsDataChunkTag_FrameData;
		OutputFile << FrameDataChunkTag;

		// Frame number
		OutputFile << CurrentFrame;


		// Viewpoint info
		{
			OutputFile << ViewLocation.X;
			OutputFile << ViewLocation.Y;
			OutputFile << ViewLocation.Z;

			OutputFile << ViewRotation.Yaw;
			OutputFile << ViewRotation.Pitch;
			OutputFile << ViewRotation.Roll;
		}


		// Cycle stats
		{
			/* const */ WORD CycleStatCount = CycleStatsForCurrentFrame.Num();
			OutputFile << CycleStatCount;

			for( WORD CurStatIndex = 0; CurStatIndex < CycleStatCount; ++CurStatIndex )
			{
				/* const */ FCycleStatData& CurStat = CycleStatsForCurrentFrame( CurStatIndex );
				
				WORD ShortStatID = CurStat.StatID;
				OutputFile << ShortStatID;
				OutputFile << CurStat.InstanceID;
				OutputFile << CurStat.ParentInstanceID;
				OutputFile << CurStat.ThreadID;
				OutputFile << CurStat.Value;

				WORD ShortCallsPerFrame = CurStat.CallsPerFrame;
				OutputFile << ShortCallsPerFrame;
			}
		}


		// Integer counter stats
		{
			/* const */ WORD IntegerStatCount = IntegerStatsForCurrentFrame.Num();
			OutputFile << IntegerStatCount;

			for( WORD CurStatIndex = 0; CurStatIndex < IntegerStatCount; ++CurStatIndex )
			{
				/* const */ FIntegerCounterStatData& CurStat = IntegerStatsForCurrentFrame( CurStatIndex );
				
				WORD ShortStatID = CurStat.StatID;
				OutputFile << ShortStatID;
				OutputFile << CurStat.Value;
			}
		}


		// Float counter stats
		{
			/* const */ WORD FloatStatCount = FloatStatsForCurrentFrame.Num();
			OutputFile << FloatStatCount;

			for( WORD CurStatIndex = 0; CurStatIndex < FloatStatCount; ++CurStatIndex )
			{
				/* const */ FFloatCounterStatData& CurStat = FloatStatsForCurrentFrame( CurStatIndex );
				
				WORD ShortStatID = CurStat.StatID;
				OutputFile << ShortStatID;
				OutputFile << CurStat.Value;
			}
		}

		
		// Reset our per-frame lists so they're ready for the next frame's data
		CycleStatsForCurrentFrame.Reset();
		FloatStatsForCurrentFrame.Reset();
		IntegerStatsForCurrentFrame.Reset();
	}
}




/**
 * Tells the parent class to open the file and then writes the opening
 * XML data to it
 */
UBOOL FStatNotifyProvider_XML::Init(void)
{
	UBOOL bOk = FStatNotifyProvider_File::Init();
	if (bOk && ParseParam(appCmdLine(),TEXT("XMLStats")))
	{
		// XML stat provider starts capturing immediately!
		bOk = CreateOutputFile();
		if( bOk	)
		{
			// Create the opening element
			WriteString("<StatFile SecondsPerCycle=\"%e\">\r\n",GSecondsPerCycle);
		}
	}
	return bOk;
}

/**
 * Makes the XML data well formed and closes the file
 */
void FStatNotifyProvider_XML::Destroy(void)
{
	// Close the previous frame element if needed
	if (CurrentFrame != (DWORD)-1)
	{
		WriteString("\t\t</Stats>\r\n");
		WriteString("\t</Frame>\r\n");
	}
	WriteString("\t</Frames>\r\n");

	// Close the opening element
	WriteString("</StatFile>\r\n");

	// Now close the file
	FStatNotifyProvider_File::Destroy();
}

/**
 * Writes the opening XML tag out
 */
void FStatNotifyProvider_XML::StartDescriptions(void)
{
	WriteString("\t<Descriptions>\r\n");
}

/**
 * Writes the closing XML tag out
 */
void FStatNotifyProvider_XML::EndDescriptions(void)
{
	WriteString("\t</Descriptions>\r\n");
	WriteString("\t<Frames>\r\n");
}

/**
 * Writes the opening XML tag out
 */
void FStatNotifyProvider_XML::StartGroupDescriptions(void)
{
	WriteString("\t\t<Groups>\r\n");
}

/**
 * Writes the closing XML tag out
 */
void FStatNotifyProvider_XML::EndGroupDescriptions(void)
{
	WriteString("\t\t</Groups>\r\n");
}

/**
 * Writes the opening XML tag out
 */
void FStatNotifyProvider_XML::StartStatDescriptions(void)
{
	WriteString("\t\t<Stats>\r\n");
}

/**
 * Writes the closing XML tag out
 */
void FStatNotifyProvider_XML::EndStatDescriptions(void)
{
	WriteString("\t\t</Stats>\r\n");
}

/**
 * Adds a stat to the list of descriptions. Used to allow custom stats to
 * report who they are, parentage, etc. Prevents applications that consume
 * the stats data from having to change when stats information changes
 *
 * @param StatId the id of the stat
 * @param StatName the name of the stat
 * @param StatType the type of stat this is
 * @param GroupId the id of the group this stat belongs to
 */
void FStatNotifyProvider_XML::AddStatDescription(DWORD StatId,const TCHAR* StatName,
	DWORD StatType,DWORD GroupId)
{
	// Write out the stat description element
	WriteString("\t\t\t<Stat ID=\"%d\" N=\"%S\" ST=\"%d\" GID=\"%d\"/>\r\n",
		StatId,StatName,StatType,GroupId);
}

/**
 * Adds a group to the list of descriptions
 *
 * @param GroupId the id of the group being added
 * @param GroupName the name of the group
 */
void FStatNotifyProvider_XML::AddGroupDescription(DWORD GroupId,const TCHAR* GroupName)
{
	// Write out the group description element
	WriteString("\t\t\t<Group ID=\"%d\" N=\"%S\"/>\r\n",GroupId,GroupName);
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param ParentId the id of the parent stat
 * @param InstanceId the instance id of the stat being written
 * @param ParentInstanceId the instance id of parent stat
 * @param ThreadId the thread this stat is for
 * @param Value the value of the stat to write out
 * @param CallsPerFrame the number of calls for this frame
 */
void FStatNotifyProvider_XML::WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,
	DWORD InstanceId,DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
	DWORD CallsPerFrame)
{
	// Write out the stat element
	WriteString("\t\t\t\t<Stat ID=\"%d\" IID=\"%d\" PID=\"%d\" TID=\"%d\" V=\"%d\" PF=\"%d\"/>\r\n",
		StatId,InstanceId,ParentInstanceId,ThreadId,Value,CallsPerFrame);
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_XML::WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value)
{
	// Write out the stat element
	WriteString("\t\t\t\t<Stat ID=\"%d\" V=\"%f\"/>\r\n",
		StatId,Value);
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_XML::WriteStat(DWORD StatId,DWORD GroupId,DWORD Value)
{
	// Write out the stat element
	WriteString("\t\t\t\t<Stat ID=\"%d\" V=\"%d\"/>\r\n",
		StatId,Value);
}

/**
 * Creates a new frame element
 *
 * @param FrameNumber the new frame number being processed
 */
void FStatNotifyProvider_XML::SetFrameNumber(DWORD FrameNumber)
{
	// Close the previous frame element if needed
	if (FrameNumber != CurrentFrame && CurrentFrame != (DWORD)-1)
	{
		WriteString("\t\t\t</Stats>\r\n");
		WriteString("\t\t</Frame>\r\n");
	}
	// Call base class version
	FStatNotifyProvider::SetFrameNumber(FrameNumber);
	// And create the new frame element
	WriteString("\t\t<Frame N=\"%d\">\r\n",FrameNumber);
	WriteString("\t\t\t<Stats>\r\n");
}


/**
 * Tells the parent class to open the file and then writes the opening data to it
 */
UBOOL FStatNotifyProvider_CSV::Init(void)
{
	UBOOL bOk = FStatNotifyProvider_File::Init();
	if (bOk && ParseParam(appCmdLine(),TEXT("CSVStats")))
	{
		// CSV stat provider starts capturing immediately!
		bOk = CreateOutputFile();
	}
	return bOk;
}



/**
 * Writes out the SecondsPerCycle info and closes the file
 */
void FStatNotifyProvider_CSV::Destroy(void)
{
	WriteString("\r\nSecondsPerCycle,%e\r\n",GSecondsPerCycle);

	// Now close the file
	FStatNotifyProvider_File::Destroy();
}

/**
 * Writes the CSV headers for the column names
 */
void FStatNotifyProvider_CSV::StartDescriptions(void)
{
	// Write out the headers
	WriteString("Frame,InstanceID,ParentInstanceID,StatID,ThreadID,Name,Value,CallsPerFrame\r\n");
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param ParentId the parent id of the stat
 * @param InstanceId the instance id of the stat being written
 * @param ParentInstanceId the instance id of parent stat
 * @param ThreadId the thread this stat is for
 * @param Value the value of the stat to write out
 * @param CallsPerFrame the number of calls for this frame
 */
void FStatNotifyProvider_CSV::WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,
	DWORD InstanceId,DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
	DWORD CallsPerFrame)
{
	WriteString(WRITE_STAT_1,CurrentFrame,InstanceId,ParentInstanceId,StatId,
		ThreadId,GStatManager.GetStatName(StatId),Value,CallsPerFrame);
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_CSV::WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value)
{
	WriteString(WRITE_STAT_2,CurrentFrame,StatId,
		GStatManager.GetStatName(StatId),Value);
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_CSV::WriteStat(DWORD StatId,DWORD GroupId,DWORD Value)
{
	WriteString(WRITE_STAT_3,CurrentFrame,StatId,
		GStatManager.GetStatName(StatId),Value);
}

#endif


