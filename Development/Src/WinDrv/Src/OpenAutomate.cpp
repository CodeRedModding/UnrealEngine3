/*=============================================================================
	FOpenAutoMate.cpp: UnrealEngine interface to OpenAutomate.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrvPrivate.h"
#if WITH_OPEN_AUTOMATE
#include "OpenAutomate.h"
#include "SystemSettings.h"

/** 
 * Generic Exec handling
 */
UBOOL FOpenAutomate::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand( &Cmd, TEXT( "ENDBENCHMARK" ) ) )
	{
		bIsBenchmarking = FALSE;
		return TRUE;
	}

	return FALSE;
}

/*
 * Register the application with OpenAutomate
 *
 * Write a file to the user's home folder that describes the application and how to run it
 */
UBOOL FOpenAutomate::RegisterApplication( void )
{
	TCHAR HomeDir[MAX_PATH] = { 0 };
	appGetEnvironmentVariable( TEXT( "USERPROFILE" ), HomeDir, MAX_PATH );

	if( appStrlen( HomeDir ) < 1 )
	{
		return FALSE;
	}

	FString ApplicationRegistrationFileName = FString::Printf( TEXT( "%s\\OpenAutomate\\RegisteredApps\\EpicGames\\%s\\%d" ), HomeDir, GGameName, GEngineVersion );
	FArchive* ApplicationRegistrationFile = GFileManager->CreateFileWriter( *ApplicationRegistrationFileName );
	if( ApplicationRegistrationFile == NULL )
	{
		return FALSE;
	}

	FString Magic = TEXT( "OAREG 1.0" ) LINE_TERMINATOR;
	ApplicationRegistrationFile->Serialize( TCHAR_TO_UTF8( *Magic ), Magic.Len() );

	FString InstallRootPath = FString::Printf( TEXT( "INSTALL_ROOT_PATH: %s%s" ), *appRootDir(), LINE_TERMINATOR );
	ApplicationRegistrationFile->Serialize( TCHAR_TO_UTF8( *InstallRootPath ), InstallRootPath.Len() );

#if _WIN64
	FString ExecutableName = FString::Printf( TEXT( "ENTRY_EXE: %sBinaries\\Win64\\%s.exe%s" ), *appRootDir(), appExecutableName(), LINE_TERMINATOR );
#else
	FString ExecutableName = FString::Printf( TEXT( "ENTRY_EXE: %sBinaries\\Win32\\%s.exe%s" ), *appRootDir(), appExecutableName(), LINE_TERMINATOR );
#endif
	ApplicationRegistrationFile->Serialize( TCHAR_TO_UTF8( *ExecutableName ), ExecutableName.Len() );

	INT Year = 0, Month = 0, DayOfWeek = 0, Day = 0, Hour = 0, Minute = 0, Second = 0, MSec = 0;
	appUtcTime( Year, Month, DayOfWeek, Day, Hour, Minute, Second, MSec );
	FString InstallDateTime = FString::Printf( TEXT( "INSTALL_DATETIME: %04d-%02d-%02d %02d:%02d:%02d%s" ), Year, Month, Day, Hour, Minute, Second, LINE_TERMINATOR );
	ApplicationRegistrationFile->Serialize( TCHAR_TO_UTF8( *InstallDateTime ), InstallDateTime.Len() );

	FString Region = TEXT( "REGION: en_US" ) LINE_TERMINATOR;
	ApplicationRegistrationFile->Serialize( TCHAR_TO_UTF8( *Region ), Region.Len() );

	ApplicationRegistrationFile->Close();

	return TRUE;
}

/**
 * Pass all the tweakable options to OpenAutomate
 */
void FOpenAutomate::GetAllOptions( void )
{
	// Eventually pass everything from FSystemSettings::SystemSettings[]
	oaNamedOption Option = { 0 };

	for( INT SettingIndex = 0; SettingIndex < GSystemSettings.NumberOfSystemSettings; SettingIndex++ )
	{
		FSystemSetting* Setting = GSystemSettings.SystemSettings + SettingIndex;
		if( ( Setting->SettingIntent == SSI_SCALABILITY || Setting->SettingIntent == SSI_PREFERENCE ) && Setting->SettingUpdate != NULL )
		{
			ANSICHAR OptionName[256] = { 0 };

			oaInitOption( &Option );

			appStrncpyANSI( OptionName, TCHAR_TO_UTF8( Setting->SettingName ), 256 );
			Option.Name = OptionName;
			FVSS* Update = Setting->SettingUpdate;

			switch( Setting->SettingType )
			{
			case SST_BOOL:
				Option.DataType = OA_TYPE_BOOL;
				break;

			case SST_INT:
				Option.DataType = OA_TYPE_INT;
				Option.MinValue.Int = Update->GetMinIntSetting();
				Option.MaxValue.Int = Update->GetMaxIntSetting();
				Option.NumSteps = Update->GetSettingCount();
				break;

			case SST_FLOAT:
				Option.DataType = OA_TYPE_INT;
				Option.MinValue.Int = ( oaInt )( Update->GetMinFloatSetting() * 1000 );
				Option.MaxValue.Int = ( oaInt )( Update->GetMaxFloatSetting() * 1000 );
				Option.NumSteps = Update->GetSettingCount();
				break;

			case SST_ENUM:
			default:
				Option.DataType = OA_TYPE_INVALID;
				break;
			}

			oaAddOption( &Option );
		}
	}
}

/**
 * Pass the current settings of all the above registered tweakables to OpenAutomate
 */
void FOpenAutomate::GetCurrentOptions( void )
{
	oaNamedOption Option = { 0 };
	oaInitOption( &Option );

	for( INT SettingIndex = 0; SettingIndex < GSystemSettings.NumberOfSystemSettings; SettingIndex++ )
	{
		FSystemSetting* Setting = GSystemSettings.SystemSettings + SettingIndex;
		if( ( Setting->SettingIntent == SSI_SCALABILITY || Setting->SettingIntent == SSI_PREFERENCE ) && Setting->SettingUpdate != NULL )
		{
			ANSICHAR OptionName[256] = { 0 };

			oaInitOption( &Option );

			appStrncpyANSI( OptionName, TCHAR_TO_UTF8( Setting->SettingName ), 256 );
			Option.Name = OptionName;

			switch( Setting->SettingType )
			{
			case SST_BOOL:
				Option.DataType = OA_TYPE_BOOL;
				Option.Value.Bool = ( oaBool )*( UBOOL* )Setting->SettingAddress;
				break;

			case SST_INT:
				Option.DataType = OA_TYPE_INT;
				Option.Value.Int = ( oaInt )*( INT* )Setting->SettingAddress;
				break;

			case SST_FLOAT:
				Option.DataType = OA_TYPE_INT;
				Option.Value.Int = ( oaInt )( ( *( FLOAT* )Setting->SettingAddress ) * 1000.0f );
				break;

			case SST_ENUM:
			default:
				Option.DataType = OA_TYPE_INVALID;
				break;
			}

			oaAddOptionValue( Option.Name, Option.DataType, &Option.Value );
		}
	}
}

/**
 * Set any settings that OpenAutomate wishes to apply
 */
void FOpenAutomate::SetOptions( void )
{
	oaNamedOption* Option;
	FString Command;

	FSystemSettings OldSystemSettings = GSystemSettings;

	while( ( Option = oaGetNextOption() ) != NULL )
	{
		Command = TEXT( "" );

		FString OptionName = FString( ANSI_TO_TCHAR( Option->Name ) );
		FSystemSetting* Setting = GSystemSettings.FindSystemSetting( OptionName, SST_ANY );
		if( Setting != NULL )
		{
			switch( Setting->SettingType )
			{
			case SST_BOOL:
				*( UBOOL* )Setting->SettingAddress = Option->Value.Bool;
				break;

			case SST_INT:
				*( INT* )Setting->SettingAddress = Option->Value.Int;
				break;

			case SST_FLOAT:
				*( FLOAT* )Setting->SettingAddress = ( FLOAT )Option->Value.Int / 1000.0f;
				break;
			}
		}
	}

	// Save to the ini file
	GSystemSettings.SaveToIni();

	// Apply the settings
	GSystemSettings.ApplySettings( OldSystemSettings );
}

/**
 * Load in all the benchmarks to run from an ini file, and register them with OpenAutomate
 */
void FOpenAutomate::GetBenchmarks( void )
{
	FConfigSection* BenchmarkList = GConfig->GetSectionPrivate( TEXT( "OpenAutomateBenchmarks" ), FALSE, TRUE, GSystemSettingsIni );
	if( BenchmarkList )
	{
		for( FConfigSectionMap::TConstIterator It( *BenchmarkList ); It; ++It )
		{
			FName EntryType = It.Key();
			const FString& EntryValue = It.Value();

			if( EntryType == NAME_Benchmark )
			{
				oaAddBenchmark( TCHAR_TO_UTF8( *EntryValue ) );
			}
		}
	}
	else
	{
		appErrorf( TEXT( "OpenAutomate: Failed to find any benchmarks" ) );
	}
}

/**
 * Run a previously registered named benchmark
 */
void FOpenAutomate::RunBenchmark( const oaChar* BenchmarkName )
{
	/** Load in map */
	GEngine->Exec( *FString::Printf( TEXT( "open %s" ), ANSI_TO_TCHAR( BenchmarkName ) ) );
	
	/* oaStartBenchmark() must be called right before the first frame */ 
	DOUBLE StartTime = GCurrentTime;
	bIsBenchmarking = TRUE;

	oaStartBenchmark();

	while( bIsBenchmarking && GCurrentTime - StartTime < 300.0 )
	{
		EngineTick();
		
		oaDisplayFrame( ( oaFloat )( GCurrentTime - StartTime ) );
	}

	/* oaStartBenchmark() must be called right after the last frame */ 
	oaEndBenchmark();
}

/**
 * Setup the engine to use OpenAutomate
 */
UBOOL FOpenAutomate::Init( const TCHAR* CmdLine )
{
	// Initialise the system
	oaVersion Version;

	// Strip out any double quotes
	FString CleanCmdLine = FString( CmdLine ).Replace( TEXT( "\"" ), TEXT( "" ) );
	CmdLine = *CleanCmdLine;

	ParseCommand( &CmdLine, TEXT( "-OPENAUTOMATE" ) );

	// Pass the remainder into OpenAutomate
	if( !oaInit( ( const oaString )TCHAR_TO_UTF8( CmdLine ), &Version ) )
	{
		appErrorf( TEXT( "OpenAutomate: Failed to initialize properly." ) );
		return FALSE;
	}

	// Register the application with OpenAutomate
	if( !RegisterApplication() )
	{
		appErrorf( TEXT( "OpenAutomate: Failed to register application" ) );
		return FALSE;
	}

	return TRUE;
}

/**
 * Main OpenAutomate processing loop
 */
UBOOL FOpenAutomate::ProcessLoop( void )
{
	oaCommand Command;

	// Enter the command loop to process OpenAutomate commands
	while( TRUE )
	{
		oaInitCommand( &Command );

		switch( oaGetNextCommand( &Command ) )
		{
		// No more commands, exit program
		case OA_CMD_EXIT:
			return TRUE;

		// Run as normal
		case OA_CMD_RUN:
			return FALSE;

		// Enumerate all in-game options
		case OA_CMD_GET_ALL_OPTIONS:
			GetAllOptions();
			break;

		// Return the option values currently set
		case OA_CMD_GET_CURRENT_OPTIONS:
			GetCurrentOptions();
			break;

		// Set all in-game options
		case OA_CMD_SET_OPTIONS:
			SetOptions();
			break;

		// Enumerate all known benchmarks
		case OA_CMD_GET_BENCHMARKS:
			GetBenchmarks();
			break;

		// Run benchmark
		case OA_CMD_RUN_BENCHMARK:
			RunBenchmark( Command.BenchmarkName );
			break;
		}
	}

	return TRUE;
}

#endif // WITH_OPEN_AUTOMATE