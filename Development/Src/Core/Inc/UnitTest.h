/*=============================================================================
	UnitTest.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __UNITTEST_H__
#define __UNITTEST_H__

/** Flags for specifying unit test requirements/behavior */
enum EUnitTestFlags
{
	UTF_Editor						= 0x00000001,	// Test is suitable for running within the editor
	UTF_Game						= 0x00000002,	// Test is suitable for running within the game
	UTF_Commandlet					= 0x00000004,	// Test is suitable for running within a commandlet
	UTF_PC							= 0x00000008,	// Test is suitable for running on the PC
	UTF_Console						= 0x00000010,	// Test is suitable for running on a console
	UTF_Mobile						= 0x00000020,	// Test is suitable for running on a mobile device
	UTF_RequiresNonNullRHI			= 0x00000040	// Test requires a non-null RHI to run correctly
};

/** Simple class to store the results of the execution of a unit test */
class FUnitTestExecutionInfo
{
public:
	/** Constructor */
	FUnitTestExecutionInfo() : bSuccessful( FALSE ) {}

	/** Destructor */
	~FUnitTestExecutionInfo()
	{
		Clear();
	}

	/** Helper method to clear out the results from a previous execution */
	void Clear()
	{
		Errors.Empty();
		Warnings.Empty();
		LogItems.Empty();
	}

	/** Whether the unit test completed successfully or not */
	UBOOL bSuccessful;

	/** Any errors that occurred during execution */
	TArray<FString> Errors;

	/** Any warnings that occurred during execution */
	TArray<FString> Warnings;

	/** Any log items that occurred during execution */
	TArray<FString> LogItems;
};

/** Class representing the main framework for running unit tests */
class FUnitTestFramework
{
public:
	/**
	 * Return the singleton instance of the framework.
	 *
	 * @return The singleton instance of the framework.
	 */
	static FUnitTestFramework& GetInstance();

	/**
	 * Returns the package name used for unit testing.
	 *
	 * @return	Name of the package to be used for unit testing
	 */
	static FString GetUnitTestPackageName();

	/**
	 * Returns a string representing the directory housing unit test items.
	 *
	 * @return	Directory containing unit test items
	 */
	static FString GetUnitTestDirectory();

	/**
	 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
	 *
	 * @param	InContext		Context to dump the execution info to
	 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
	 */
	static void DumpUnitTestExecutionInfoToContext( FFeedbackContext* InContext, const TMap<FString, FUnitTestExecutionInfo>& InInfoToDump );

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
	UBOOL RegisterUnitTest( const FString& InTestNameToRegister, class FUnitTestBase* InTestToRegister );

	/**
	 * Unregister a unit test with the provided name from the framework.
	 *
	 * @return TRUE if the test was successfully unregistered; FALSE if a test with that name was not found in the framework.
	 */
	UBOOL UnregisterUnitTest( const FString& InTestNameToUnregister );

	/**
	 * Checks if a provided test is contained within the framework.
	 *
	 * @param InTestName	Name of the test to check
	 *
	 * @return	TRUE if the provided test is within the framework; FALSE otherwise
	 */
	UBOOL ContainsTest( const FString& InTestName ) const;

	/**
	 * Attempt to run all unit tests that are valid for the current application configuration.
	 *
	 * @param	OutExecutionMap	Map of test name to execution results for each test that was run
	 *
	 * @return	TRUE if all tests run were successful, FALSE if any failed
	 */
	UBOOL RunAllValidTests( TMap<FString, FUnitTestExecutionInfo>& OutExecutionInfoMap );
	
	/**
	 * Attempt to run the specified test.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	OutExecutionInfo	Execution results of running the test
	 *
	 * @return	TRUE if the test ran successfully, FALSE if it did not (or the test could not be found/was invalid)
	 */
	UBOOL RunTestByName( const FString& InTestToRun, FUnitTestExecutionInfo& OutExecutionInfo );
	
	/**
	 * Populates the provided array with the names of all tests in the framework that are valid to run for the current
	 * application settings.
	 *
	 * @param	OutValidTestNames	Array to populate with valid test names
	 */
	void GetValidTestNames( TArray<FString>& OutValidTestNames ) const;

	/**
	 * Determines if a given test is valid for the current application settings or not.
	 *
	 * @param	InTestName	Name of the test to check the validity of
	 *
	 * @return	TRUE if the test is valid for the current application settings; FALSE if not
	 */
	UBOOL IsTestValid( const FString& InTestName ) const;

private:

	/** Special feedback context used exclusively while unit testing */
	class FUnitTestFeedbackContext : public FFeedbackContext
	{
	public:

		/** Constructor */
		FUnitTestFeedbackContext() 
			: SlowTaskCount( 0 ), CurUnitTest( NULL ) {}

		/** Destructor */
		~FUnitTestFeedbackContext()
		{
			CurUnitTest = NULL;
		}

		/** Unneeded by the context, just return FALSE */
		VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
		{
			return FALSE;
		}

		/** Signify the a slow task is beginning (parameters unused in this implementation) */
		void BeginSlowTask( const TCHAR* Task, UBOOL ShowProgressDialog, UBOOL bShowCancelButton=FALSE )
		{
			GIsSlowTask = ++SlowTaskCount > 0;
		}

		/** Signify that a slow task is ending (parameters unused in this implementation) */
		void EndSlowTask()
		{
			check( SlowTaskCount > 0 );
			GIsSlowTask = --SlowTaskCount > 0;
		}

		/** Unneeded by the context, just return TRUE */
		VARARG_BODY( UBOOL VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) )
		{
			return TRUE;
		}

		/**
		 * FOutputDevice interface
		 *
		 * @param	V		String to serialize within the context
		 * @param	Event	Event associated with the string
		 */
		virtual void Serialize( const TCHAR* V, EName Event );

		/**
		 * Set the unit test associated with the feedback context. The unit test is where all warnings, errors, etc.
		 * will be routed to.
		 *
		 * @param	InUnitTest	Unit test to associate with the feedback context.
		 */
		void SetCurrentUnitTest( class FUnitTestBase* InUnitTest )
		{
			CurUnitTest = InUnitTest;
		}

	private:
		/** Number of slow tasks currently occurring */
		INT SlowTaskCount;

		/** Associated unit test; all warnings, errors, etc. are routed to the unit test to track */
		class FUnitTestBase* CurUnitTest;
	};

	/** Helper method called to prepare settings for unit testing to follow */
	void PrepForUnitTests();

	/** Helper method called after unit testing is complete to restore settings to how they should be */
	void ConcludeUnitTests();

	/**
	 * Internal helper method designed to simply run the provided test name.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	OutExecutionInfo	Results of executing the test
	 *
	 * @return	TRUE if the test was successfully run; FALSE if it was not, could not be found, or is invalid for
	 *			the current application settings
	 */
	UBOOL InternalRunTest( const FString& InTestToRun, FUnitTestExecutionInfo& OutExecutionInfo );

	/** Constructor */
	FUnitTestFramework();

	/** Destructor */
	~FUnitTestFramework();

	// Copy constructor and assignment operator intentionally left unimplemented
	FUnitTestFramework( const FUnitTestFramework& );
	FUnitTestFramework& operator=( const FUnitTestFramework& );

	/** Cached value specifying whether the engine was running unattended or not prior to unit testing */
	UBOOL bWasRunningUnattended;

	/** Cached feedback context, contains the contents of GWarn at the time of unit testing, restored to GWarn when unit testing is complete */
	FFeedbackContext* CachedContext;

	/** Specialized feedback context used for unit testing */
	FUnitTestFeedbackContext UnitTestFeedbackContext;

	/** Mapping of unit test names to their respective object instances */
	TMap<FString, class FUnitTestBase*> UnitTestClassNameToInstanceMap;
};

/** Simple abstract base class for all unit tests */
class FUnitTestBase
{
public:
	/**
	 * Constructor
	 *
	 * @param	InName	Name of the test
	 */
	FUnitTestBase( const FString& InName )
		: TestName( InName )
	{
		// Register the newly created unit test into the unit testing framework
		FUnitTestFramework::GetInstance().RegisterUnitTest( InName, this );
	}

	/** Destructor */
	virtual ~FUnitTestBase() 
	{ 
		// Unregister the unit test from the unit testing framework
		FUnitTestFramework::GetInstance().UnregisterUnitTest( TestName ); 
	}

	/**
	 * Pure virtual method; should be implemented by subclasses with logic necessary to run the unit test
	 *
	 * @return TRUE if the test was run successfully; FALSE otherwise
	 */
	virtual UBOOL RunTest() = 0;

	/**
	 * Pure virtual method; returns the flags associated with the given unit test
	 *
	 * @return	Unit test flags associated with the test
	 */
	virtual DWORD GetUnitTestFlags() const = 0;

	/** Clear any execution info/results from a prior running of this test */
	void ClearExecutionInfo();

	/**
	 * Adds an error message to this test
	 *
	 * @param	InError	Error message to add to this test
	 */
	void AddError( const FString& InError );

	/**
	 * Adds a warning to this test
	 *
	 * @param	InWarning	Warning message to add to this test
	 */
	void AddWarning( const FString& InWarning );

	/**
	 * Adds a log item to this test
	 *
	 * @param	InLogItem	Log item to add to this test
	 */
	void AddLogItem( const FString& InLogItem );

	/**
	 * Returns whether this test has any errors associated with it or not
	 *
	 * @return TRUE if this test has at least one error associated with it; FALSE if not
 	 */
	UBOOL HasAnyErrors() const;

	/**
	 * Forcibly sets whether the test has succeeded or not
	 *
	 * @param	bSuccessful	TRUE to mark the test successful, FALSE to mark the test as failed
	 */
	void SetSuccessState( UBOOL bSuccessful );

	/**
	 * Populate the provided execution info object with the execution info contained within the test. Not particularly efficient,
	 * but providing direct access to the test's private execution info could result in errors.
	 *
	 * @param	OutInfo	Execution info to be populated with the same data contained within this test's execution info
	 */
	void GetExecutionInfo( FUnitTestExecutionInfo& OutInfo ) const;

protected:
	/** Name of the test */
	FString TestName;

	/** Info related to the last execution of this test */
	FUnitTestExecutionInfo ExecutionInfo;
};

/**
 * Macro to simplify the creation of new unit tests. To create a new test one simply must put
 * IMPLEMENT_UNIT_TEST( NewUnitClassName, UnitClassFlags )
 * in their cpp file, and then proceed to write an implementation for:
 * UBOOL NewUnitTestClassName::RunTest() {}
 * While the macro could also have allowed the code to be specified, leaving it out of the macro allows
 * the code to be debugged more easily.
 *
 * Builds supporting unit tests will automatically create and register an instance of the unit test within
 * the unit test framework as a result of the macro.
 */
#if USE_UNIT_TESTS
#define IMPLEMENT_UNIT_TEST( TClass, TFlags ) \
namespace\
{\
	class TClass : public FUnitTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FUnitTestBase( InName ) {} \
		virtual UBOOL RunTest(); \
		virtual DWORD GetUnitTestFlags() const { return TFlags; } \
	};\
	TClass TClass##UnitTestInstance( TEXT(#TClass) );\
}
#else
#define IMPLEMENT_UNIT_TEST( TClass, TFlags ) \
namespace\
{\
	class TClass : public FUnitTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FUnitTestBase( InName ) {} \
		virtual UBOOL RunTest(); \
		virtual DWORD GetUnitTestFlags() const { return TFlags; } \
	};\
}
#endif // #if USE_UNIT_TESTS

#endif // #define __UNITTEST_H__

