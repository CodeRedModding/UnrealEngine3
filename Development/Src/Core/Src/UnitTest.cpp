/*=============================================================================
	UnitTest.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/**
 * FOutputDevice interface
 *
 * @param	V		String to serialize within the context
 * @param	Event	Event associated with the string
 */
void FUnitTestFramework::FUnitTestFeedbackContext::Serialize( const TCHAR* V, EName Event )
{
	// Ensure there's a valid unit test associated with the context
	if ( CurUnitTest )
	{
		// Warnings
		if ( Event == NAME_Warning || Event==NAME_ExecWarning || Event==NAME_ScriptWarning )
		{
			// If warnings should be treated as errors, log the warnings as such in the current unit test
			if ( TreatWarningsAsErrors )
			{
				CurUnitTest->AddError( FString( V ) );
			}
			else
			{
				CurUnitTest->AddWarning( FString( V ) );
			}
		}
		// Errors
		else if ( Event == NAME_Error )
		{
			CurUnitTest->AddError( FString( V ) );
		}
		// Log items
		else if ( Event == NAME_Log )
		{
			CurUnitTest->AddLogItem( FString( V ) );
		}
	}
}

/**
 * Return the singleton instance of the framework.
 *
 * @return The singleton instance of the framework.
 */
FUnitTestFramework& FUnitTestFramework::GetInstance()
{
	static FUnitTestFramework Framework;
	return Framework;
}

/**
 * Returns the package name used for unit testing.
 *
 * @return	Name of the package to be used for unit testing
 */
FString FUnitTestFramework::GetUnitTestPackageName()
{
	FString PackageName;
	
	check( GConfig );
	GConfig->GetString( TEXT("UnitTesting"), TEXT("UnitTestPackageName"), PackageName, GEngineIni );
	
	return PackageName;
}

/**
 * Returns a string representing the directory housing unit test items.
 *
 * @return	Directory containing unit test items
 */
FString FUnitTestFramework::GetUnitTestDirectory()
{
	FString UnitTestDirectory;

	check( GConfig );
	GConfig->GetString( TEXT("UnitTesting"), TEXT("UnitTestPath"), UnitTestDirectory, GEngineIni );

	return UnitTestDirectory;
}

/**
 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
 *
 * @param	InContext		Context to dump the execution info to
 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
 */
void FUnitTestFramework::DumpUnitTestExecutionInfoToContext( FFeedbackContext* InContext, const TMap<FString, FUnitTestExecutionInfo>& InInfoToDump )
{
	if ( InContext )
	{
		const FString SuccessMessage = LocalizeUnrealEd("UnitTest_Success");
		const FString FailMessage = LocalizeUnrealEd("UnitTest_Fail");
		for ( TMap<FString, FUnitTestExecutionInfo>::TConstIterator MapIter(InInfoToDump); MapIter; ++MapIter )
		{
			const FString& CurTestName = MapIter.Key();
			const FUnitTestExecutionInfo& CurExecutionInfo = MapIter.Value();

			const FString HeaderMessage = FString::Printf( TEXT("%s: %s\n"), *CurTestName, CurExecutionInfo.bSuccessful ? *SuccessMessage : *FailMessage );
			InContext->Logf( NAME_Log, *HeaderMessage );

			if ( CurExecutionInfo.Errors.Num() > 0 )
			{
				SET_WARN_COLOR(COLOR_RED);
				InContext->Logf( NAME_Log, *FString::Printf( TEXT("%s\n"), *LocalizeUnrealEd("UnitTest_Errors") ) );
				CLEAR_WARN_COLOR();
				for ( TArray<FString>::TConstIterator ErrorIter( CurExecutionInfo.Errors ); ErrorIter; ++ErrorIter )
				{
					InContext->Logf( NAME_Error, *FString::Printf( TEXT("%s\n"), **ErrorIter ) );
				}
			}

			if ( CurExecutionInfo.Warnings.Num() > 0 )
			{
				SET_WARN_COLOR(COLOR_YELLOW);
				InContext->Logf( NAME_Log, *FString::Printf( TEXT("%s\n"), *LocalizeUnrealEd("UnitTest_Warnings") ) );
				CLEAR_WARN_COLOR();
				for ( TArray<FString>::TConstIterator WarningIter( CurExecutionInfo.Warnings ); WarningIter; ++WarningIter )
				{
					InContext->Logf( NAME_Warning, *FString::Printf( TEXT("%s\n"), **WarningIter ) );
				}
			}

			if ( CurExecutionInfo.LogItems.Num() > 0 )
			{
				InContext->Logf( NAME_Log, *FString::Printf( TEXT("%s\n"), *LocalizeUnrealEd("UnitTest_LogItems") ) );
				for ( TArray<FString>::TConstIterator LogItemIter( CurExecutionInfo.LogItems ); LogItemIter; ++LogItemIter )
				{
					InContext->Logf( NAME_Log, *FString::Printf( TEXT("%s\n"), **LogItemIter ) );
				}
			}
			InContext->Logf( NAME_Log, TEXT("\n") );
		}
	}
}

/**
 * Register a unit test into the framework. The unit test may or may not be necessarily valid
 * for the particular application configuration, but that will be determined when tests are attempted
 * to be run.
 *
 * @param	InTestNameToRegister	Name of the test being registered
 * @param	InTestToRegister		Actual test to register
 *
 * @return	TRUE if the test was successfully registered; FALSE if a test was already registered under the same
 *			name as before
 */
UBOOL FUnitTestFramework::RegisterUnitTest( const FString& InTestNameToRegister, class FUnitTestBase* InTestToRegister )
{
	const UBOOL bAlreadyRegistered = UnitTestClassNameToInstanceMap.HasKey( InTestNameToRegister );
	if ( !bAlreadyRegistered )
	{
		UnitTestClassNameToInstanceMap.Set( InTestNameToRegister, InTestToRegister );
	}
	return !bAlreadyRegistered;
}

/**
 * Unregister a unit test with the provided name from the framework.
 *
 * @return TRUE if the test was successfully unregistered; FALSE if a test with that name was not found in the framework.
 */
UBOOL FUnitTestFramework::UnregisterUnitTest( const FString& InTestNameToUnregister )
{
	const UBOOL bRegistered = UnitTestClassNameToInstanceMap.HasKey( InTestNameToUnregister );
	if ( bRegistered )
	{
		UnitTestClassNameToInstanceMap.Remove( InTestNameToUnregister );
	}
	return bRegistered;
}

/**
 * Checks if a provided test is contained within the framework.
 *
 * @param InTestName	Name of the test to check
 *
 * @return	TRUE if the provided test is within the framework; FALSE otherwise
 */
UBOOL FUnitTestFramework::ContainsTest( const FString& InTestName ) const
{
	return UnitTestClassNameToInstanceMap.HasKey( InTestName );
}

/**
 * Attempt to run all unit tests that are valid for the current application configuration.
 *
 * @param	OutExecutionMap	Map of test name to execution results for each test that was run
 *
 * @return	TRUE if all tests run were successful, FALSE if any failed
 */
UBOOL FUnitTestFramework::RunAllValidTests( TMap<FString, FUnitTestExecutionInfo>& OutExecutionInfoMap )
{
	UBOOL bAllSuccessful = TRUE;

	// Ensure there isn't another slow task in progress when trying to run unit tests
	if ( !GIsSlowTask && !GIsPlayInEditorWorld )
	{
		TArray<FString> ValidTests;
		GetValidTestNames( ValidTests );
		if ( ValidTests.Num() > 0 )
		{
			// Make any setting changes that have to occur to support unit testing
			PrepForUnitTests();

			// Run each valid test
			for ( TArray<FString>::TConstIterator TestIter( ValidTests ); TestIter; ++TestIter )
			{
				FUnitTestExecutionInfo& CurExecutionInfo = OutExecutionInfoMap.Set( *TestIter, FUnitTestExecutionInfo() );
				const UBOOL CurTestSuccessful = InternalRunTest( *TestIter, CurExecutionInfo );
				bAllSuccessful = bAllSuccessful && CurTestSuccessful;
			}

			// Restore any changed settings now that unit testing has completed
			ConcludeUnitTests();
		}
	}
	else
	{
		GWarn->Logf( NAME_Error, *LocalizeUnrealEd("UnitTest_TestsNotRunDueToSlowTask") );
		bAllSuccessful = FALSE;
	}
	return bAllSuccessful;
}

/**
 * Attempt to run the specified test.
 *
 * @param	InTestToRun			Name of the test that should be run
 * @param	OutExecutionInfo	Execution results of running the test
 *
 * @return	TRUE if the test ran successfully, FALSE if it did not (or the test could not be found/was invalid)
 */
UBOOL FUnitTestFramework::RunTestByName( const FString& InTestToRun, FUnitTestExecutionInfo& OutExecutionInfo )
{
	UBOOL bTestSuccessful = FALSE;

	// Ensure there isn't another slow task in progress when trying to run unit tests
	if ( !GIsSlowTask && !GIsPlayInEditorWorld )
	{
		// Ensure the test exists in the framework and is valid to run
		if ( ContainsTest( InTestToRun ) )
		{
			if ( IsTestValid( InTestToRun ) )
			{
				// Make any setting changes that have to occur to support unit testing
				PrepForUnitTests();

				bTestSuccessful = InternalRunTest( InTestToRun, OutExecutionInfo );

				// Restore any changed settings now that unit testing has completed
				ConcludeUnitTests();
			}
			else
			{
				GWarn->Logf( NAME_Error, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_TestNotValid"), *InTestToRun ) ) );
			}
		}
		else
		{
			GWarn->Logf( NAME_Error, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_TestNotFound"), *InTestToRun ) ) );
		}
	}
	else
	{
		GWarn->Logf( NAME_Error, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_TestNotRunDueToSlowTask"), *InTestToRun ) ) );
	}
	return bTestSuccessful;
}

/** Helper method called to prepare settings for unit testing to follow */
void FUnitTestFramework::PrepForUnitTests()
{
	// Fire off callback signifying that unit testing is about to begin. This allows
	// other systems to prepare themselves as necessary without the unit testing framework having to know
	// about them.
	if ( GCallbackEvent )
	{
		GCallbackEvent->Send( CALLBACK_PreUnitTesting );
	}

	// Cache if the engine was running unattended or not, as unit testing is going to force into unattended mode to
	// reduce the chance of pop-ups, etc. coming up during testing
	bWasRunningUnattended = GIsUnattended;
	GIsUnattended = TRUE;

	// Cache the contents of GWarn, as unit testing is going to forcibly replace GWarn with a specialized feedback context
	// designed for unit testing
	CachedContext = GWarn;
	UnitTestFeedbackContext.TreatWarningsAsErrors = GWarn->TreatWarningsAsErrors;
	GWarn = &UnitTestFeedbackContext;

	// Mark that unit testing has begun
	GIsUnitTesting = TRUE;
}

/** Helper method called after unit testing is complete to restore settings to how they should be */
void FUnitTestFramework::ConcludeUnitTests()
{
	// Mark that unit testing is over
	GIsUnitTesting = FALSE;

	// Restore cached values to whatever they were before unit testing began
	GIsUnattended = bWasRunningUnattended;
	GWarn = CachedContext;
	CachedContext = NULL;

	// Fire off callback signifying that unit testing has concluded.
	if ( GCallbackEvent )
	{
		GCallbackEvent->Send( CALLBACK_PostUnitTesting );
	}
}

/**
 * Internal helper method designed to simply run the provided test name.
 *
 * @param	InTestToRun			Name of the test that should be run
 * @param	OutExecutionInfo	Results of executing the test
 *
 * @return	TRUE if the test was successfully run; FALSE if it was not, could not be found, or is invalid for
 *			the current application settings
 */
UBOOL FUnitTestFramework::InternalRunTest( const FString& InTestToRun, FUnitTestExecutionInfo& OutExecutionInfo )
{
	UBOOL bTestSuccessful = FALSE;
	if ( ContainsTest( InTestToRun ) )
	{
		FUnitTestBase* CurUnitTest = *( UnitTestClassNameToInstanceMap.Find( InTestToRun ) );
		check( CurUnitTest );
		
		// Clear any execution info from the test in case it has been run before
		CurUnitTest->ClearExecutionInfo();

		// Associate the test that is about to be run with the special unit test feedback context
		UnitTestFeedbackContext.SetCurrentUnitTest( CurUnitTest );

		// Run the test!
		bTestSuccessful = CurUnitTest->RunTest();

		// Disassociate the test from the feedback context
		UnitTestFeedbackContext.SetCurrentUnitTest( NULL );

		// Determine if the test was successful based on two criteria:
		// 1) Did the test itself report success?
		// 2) Did any errors occur and were logged by the feedback context during execution?
		bTestSuccessful = bTestSuccessful && !CurUnitTest->HasAnyErrors();

		// Set the success state of the test based on the above criteria
		CurUnitTest->SetSuccessState( bTestSuccessful );

		// Fill out the provided execution info with the info from the test
		CurUnitTest->GetExecutionInfo( OutExecutionInfo );
	}
	return bTestSuccessful;
}

/**
 * Populates the provided array with the names of all tests in the framework that are valid to run for the current
 * application settings.
 * 
 * @param	OutValidTestNames	Array to populate with valid test names
 */
void FUnitTestFramework::GetValidTestNames( TArray<FString>& OutValidTestNames ) const
{
	OutValidTestNames.Empty();

	DWORD RequiredFlags = 0;

	// Determine required application type (Editor, Game, or Commandlet)
	const UBOOL bRunningEditor = GIsEditor && !GIsUCC;
	const UBOOL bRunningGame = GIsGame && !GIsPlayInEditorWorld;
	const UBOOL bRunningCommandlet = GIsUCC;
	if ( bRunningEditor )
	{
		RequiredFlags |= UTF_Editor;
	}
	else if ( bRunningGame )
	{
		RequiredFlags |= UTF_Game;
	}
	else if ( bRunningCommandlet )
	{
		RequiredFlags |= UTF_Commandlet;
	}

	// Determine required platform flags
	UE3::EPlatformType CurPlatform = appGetPlatformType();
	if ( CurPlatform & UE3::PLATFORM_PC )
	{
		RequiredFlags |= UTF_PC;
	}
	else if ( CurPlatform & UE3::PLATFORM_Console )
	{
		RequiredFlags |= UTF_Console;
	}
	else if ( CurPlatform & UE3::PLATFORM_Mobile )
	{
		RequiredFlags |= UTF_Mobile;
	}

	// @todo: Handle this correctly. GIsUsingNullRHI is defined at Engine-level, so it can't be used directly here in Core.
	// For now, assume Null RHI is only used for commandlets, servers, and when the command line specifies to use it.
#if CONSOLE
	const UBOOL bUsingNullRHI = FALSE;
#else
	const UBOOL bUsingNullRHI = ParseParam( appCmdLine(), TEXT("nullrhi") ) || ParseParam( appCmdLine(), TEXT("SERVER") ) || GIsUCC;
#endif // #if CONSOLE

	for ( TMap<FString, FUnitTestBase*>::TConstIterator UnitTestIter( UnitTestClassNameToInstanceMap ); UnitTestIter; ++UnitTestIter )
	{
		const FString& CurTestName = UnitTestIter.Key();
		const FUnitTestBase* CurTest = UnitTestIter.Value();
		check( CurTest );

		const DWORD CurTestFlags = CurTest->GetUnitTestFlags();
		if ( ( ( CurTestFlags & RequiredFlags ) == RequiredFlags ) && ( !bUsingNullRHI || ( ( CurTestFlags & UTF_RequiresNonNullRHI ) == 0 ) ) )
		{
			OutValidTestNames.AddItem( CurTestName );
		}
	}
}

/**
 * Determines if a given test is valid for the current application settings or not.
 *
 * @param	InTestName	Name of the test to check the validity of
 *
 * @return	TRUE if the test is valid for the current application settings; FALSE if not
 */
UBOOL FUnitTestFramework::IsTestValid( const FString& InTestName ) const
{
	TArray<FString> ValidTests;
	GetValidTestNames( ValidTests );
	return ValidTests.ContainsItem( InTestName );
}

/** Constructor */
FUnitTestFramework::FUnitTestFramework()
:	bWasRunningUnattended( FALSE ),
	CachedContext( NULL )
{}

/** Destructor */
FUnitTestFramework::~FUnitTestFramework()
{
	CachedContext = NULL;
	UnitTestClassNameToInstanceMap.Empty();
}

/** Clear any execution info/results from a prior running of this test */
void FUnitTestBase::ClearExecutionInfo()
{
	ExecutionInfo.Clear();
}

/**
 * Adds an error message to this test
 *
 * @param	InError	Error message to add to the test
 */
void FUnitTestBase::AddError( const FString& InError )
{
	ExecutionInfo.Errors.AddItem( InError );
}

/**
 * Adds a warning to this test
 *
 * @param	InWarning	Warning message to add to this test
 */
void FUnitTestBase::AddWarning( const FString& InWarning )
{
	ExecutionInfo.Warnings.AddItem( InWarning );
}

/**
 * Adds a log item to this test
 *
 * @param	InLogItem	Log item to add to this test
 */
void FUnitTestBase::AddLogItem( const FString& InLogItem )
{
	ExecutionInfo.LogItems.AddItem( InLogItem );
}

/**
 * Returns whether this test has any errors associated with it or not
 *
 * @return TRUE if this test has at least one error associated with it; FALSE if not
 */
UBOOL FUnitTestBase::HasAnyErrors() const
{
	return ExecutionInfo.Errors.Num() > 0;
}

/**
 * Forcibly sets whether the test has succeeded or not
 *
 * @param	bSuccessful	TRUE to mark the test successful, FALSE to mark the test as failed
 */
void FUnitTestBase::SetSuccessState( UBOOL bSuccessful )
{
	ExecutionInfo.bSuccessful = bSuccessful;
}

/**
 * Populate the provided execution info object with the execution info contained within the test. Not particularly efficient,
 * but providing direct access to the test's private execution info could result in errors.
 *
 * @param	OutInfo	Execution info to be populated with the same data contained within this test's execution info
 */
void FUnitTestBase::GetExecutionInfo( FUnitTestExecutionInfo& OutInfo ) const
{
	OutInfo = ExecutionInfo;
}
