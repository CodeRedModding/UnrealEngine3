/*=============================================================================
	UnOutputDevices.h: Collection of FOutputDevice subclasses
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if CONSOLE

// don't support colorized text on consoles
#define SET_WARN_COLOR(Color)
#define SET_WARN_COLOR_AND_BACKGROUND(Color, Bkgrnd)
#define CLEAR_WARN_COLOR() 

#else
/*-----------------------------------------------------------------------------
	Colorized text.

	To use colored text from a commandlet, you use the SET_WARN_COLOR macro with
	one of the following standard colors. Then use CLEAR_WARN_COLOR to return 
	to default.

	To use the standard colors, you simply do this:
	SET_WARN_COLOR(COLOR_YELLOW);
	
	You can specify a background color by appending it to the foreground:
	SET_WARN_COLOR, COLOR_YELLOW COLOR_DARK_RED);

	This will have bright yellow text on a dark red background.

	Or you can make your own in the format:
	
	ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
	where each value is either 0 or 1 (can leave off trailing 0's), so 
	blue on bright yellow is "00101101" and red on black is "1"
	
	An empty string reverts to the normal gray on black.
-----------------------------------------------------------------------------*/
// putting them in a namespace to protect against future name conflicts
namespace OutputDeviceColor
{
const TCHAR* const COLOR_BLACK			= TEXT("0000");

const TCHAR* const COLOR_DARK_RED		= TEXT("1000");
const TCHAR* const COLOR_DARK_GREEN		= TEXT("0100");
const TCHAR* const COLOR_DARK_BLUE		= TEXT("0010");
const TCHAR* const COLOR_DARK_YELLOW	= TEXT("1100");
const TCHAR* const COLOR_DARK_CYAN		= TEXT("0110");
const TCHAR* const COLOR_DARK_PURPLE	= TEXT("1010");
const TCHAR* const COLOR_DARK_WHITE		= TEXT("1110");
const TCHAR* const COLOR_GRAY			= COLOR_DARK_WHITE;

const TCHAR* const COLOR_RED			= TEXT("1001");
const TCHAR* const COLOR_GREEN			= TEXT("0101");
const TCHAR* const COLOR_BLUE			= TEXT("0011");
const TCHAR* const COLOR_YELLOW			= TEXT("1101");
const TCHAR* const COLOR_CYAN			= TEXT("0111");
const TCHAR* const COLOR_PURPLE			= TEXT("1011");
const TCHAR* const COLOR_WHITE			= TEXT("1111");

const TCHAR* const COLOR_NONE			= TEXT("");

}
using namespace OutputDeviceColor;

// let a console or FINAL_RELEASE define it to nothing
#ifndef SET_WARN_COLOR

/**
 * Set the console color with Color or a Color and Background color
 */
#define SET_WARN_COLOR(Color) \
	warnf(NAME_Color, Color);
#define SET_WARN_COLOR_AND_BACKGROUND(Color, Bkgrnd) \
	warnf(NAME_Color, *FString::Printf(TEXT("%s%s"), Color, Bkgrnd));

/**
 * Return color to it's default
 */
#define CLEAR_WARN_COLOR() \
	warnf(NAME_Color, COLOR_NONE);

#endif
#endif

/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

/**
 * Class used for output redirection to allow logs to show
 */
class FOutputDeviceRedirector : public FOutputDeviceRedirectorBase
{
private:

	/** The type of lines buffered by secondary threads. */
	struct FBufferedLine
	{
		const FString Data;
		const EName Event;

		/** Initialization constructor. */
		FBufferedLine(const TCHAR* InData,EName InEvent):
			Data(InData),
			Event(InEvent)
		{}
	};

	/** A FIFO of lines logged by non-master threads. */
	TArray<FBufferedLine> BufferedLines;

	/** A FIFO backlog of messages logged before the editor had a chance to intercept them. */
	TArray<FBufferedLine> BacklogLines;

	/** Array of output devices to redirect to */
	TArray<FOutputDevice*> OutputDevices;

	/** The master thread ID.  Logging from other threads will be buffered for processing by the master thread. */
	DWORD MasterThreadID;

	/** Whether backlogging is enabled. */
	UBOOL bEnableBacklog;

	/** Object used for synchronization via a scoped lock */
	FCriticalSection	SynchronizationObject;

	/**
	 * The unsynchronized version of FlushThreadedLogs.
	 * Assumes that the caller holds a lock on SynchronizationObject.
	 */
	void UnsynchronizedFlushThreadedLogs();

public:

	/** Initialization constructor. */
	FOutputDeviceRedirector();

	/**
	 * Adds an output device to the chain of redirections.	
	 *
	 * @param OutputDevice	output device to add
	 */
	void AddOutputDevice( FOutputDevice* OutputDevice );

	/**
	 * Removes an output device from the chain of redirections.	
	 *
	 * @param OutputDevice	output device to remove
	 */
	void RemoveOutputDevice( FOutputDevice* OutputDevice );

	/**
	 * Returns whether an output device is currently in the list of redirectors.
	 *
	 * @param	OutputDevice	output device to check the list against
	 * @return	TRUE if messages are currently redirected to the the passed in output device, FALSE otherwise
	 */
	UBOOL IsRedirectingTo( FOutputDevice* OutputDevice );

	/** Flushes lines buffered by secondary threads. */
	virtual void FlushThreadedLogs();

	/**
	 * Serializes the current backlog to the specified output device.
	 * @param OutputDevice	- Output device that will receive the current backlog
	 */
	virtual void SerializeBacklog( FOutputDevice* OutputDevice );

	/**
	 * Enables or disables the backlog.
	 * @param bEnable	- Starts saving a backlog if TRUE, disables and discards any backlog if FALSE
	 */
	virtual void EnableBacklog( UBOOL bEnable );

	/**
	 * Sets the current thread to be the master thread that prints directly
	 * (isn't queued up)
	 */
	virtual void SetCurrentThreadAsMasterThread();

	/**
	 * Serializes the passed in data via all current output devices.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	void Serialize( const TCHAR* Data, enum EName Event );

	/**
	 * Passes on the flush request to all current output devices.
	 */
	void Flush();

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown();
};

/*-----------------------------------------------------------------------------
	FOutputDevice subclasses.
-----------------------------------------------------------------------------*/

/** string added to the filename of timestamped backup log files */
#define BACKUP_LOG_FILENAME_POSTFIX TEXT("-backup-")

/**
 * File output device.
 */
class FOutputDeviceFile : public FOutputDevice
{
public:
	/** 
	 * Constructor, initializing member variables.
	 *
	 * @param InFilename	Filename to use, can be NULL
	 * @param bDisableBackup If TRUE, existing files will not be backed up
	 */
	FOutputDeviceFile( const TCHAR* InFilename = NULL, UBOOL bDisableBackup=FALSE, UBOOL bRespectAllowDebugFilesDefine=TRUE );

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown();

	/**
	 * Flush the write cache so the file isn't truncated in case we crash right
	 * after calling this function.
	 */
	void Flush();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	void Serialize( const TCHAR* Data, enum EName Event );

private:
	FArchive*	LogAr;
	TCHAR		Filename[1024];
	UBOOL		Opened;
	UBOOL		Dead;
	UBOOL		bRespectAllowDebugFilesDefine;

	/** If TRUE, existing files will not be backed up */
	UBOOL		bDisableBackup;
	
	void WriteRaw( const TCHAR* C );
};

// Null output device.
class FOutputDeviceNull : public FOutputDevice
{
public:
	/**
	 * NULL implementation of Serialize.
	 *
	 * @param	Data	unused
	 * @param	Event	unused
	 */
	void Serialize( const TCHAR* /*V*/, enum EName /*Event*/ )
	{}
};

class FOutputDeviceDebug : public FOutputDevice
{
public:
	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	void Serialize( const TCHAR* Data, enum EName Event );
};

/*-----------------------------------------------------------------------------
	FOutputDeviceError subclasses.
-----------------------------------------------------------------------------*/

class FOutputDeviceAnsiError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	FOutputDeviceAnsiError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	void Serialize( const TCHAR* Msg, enum EName Event );

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError();

private:
	void LocalPrint( const TCHAR* Str );

	INT		ErrorPos;
	EName	ErrorType;
};

#if !CONSOLE && defined(_MSC_VER)

class FOutputDeviceWindowsError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	FOutputDeviceWindowsError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	void Serialize( const TCHAR* Msg, enum EName Event );

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError();

private:
	INT		ErrorPos;
	EName	ErrorType;
};

#endif // CONSOLE

/*-----------------------------------------------------------------------------
	FOutputDeviceConsole subclasses.
-----------------------------------------------------------------------------*/

#if !CONSOLE && defined(_MSC_VER)

/**
 * Windows implementation of an inherited console log window (if we've been run
 * on the command line from our .com launcher)
 */
class FOutputDeviceConsoleWindowsInherited : public FOutputDeviceConsole
{
private:
	/** Did we inherit a redirected console ? */
	HANDLE ConsoleHandle;

	/** Are we "shown" */
	UBOOL Shown;

	/** Device we should forward to if we don't connect */
	FOutputDeviceConsole &ForwardConsole;

public:
	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleWindowsInherited(FOutputDeviceConsole &forward);

	/**
	 * destructor
	 */
	~FOutputDeviceConsoleWindowsInherited();

	/**
	 * Attempt to connect to the pipes set up by our .com launcher
	 *
	 * @retval TRUE if connection was successful, FALSE otherwise
	 */
	UBOOL Connect();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
	 */
	virtual void Show( UBOOL ShowWindow );

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return TRUE if console is shown, FALSE otherwise
	 */
	virtual UBOOL IsShown();

	/**
	 * Returns whether the console has been inherited (TRUE) or created (FALSE)
	 *
	 * @return TRUE if console is inherited, FALSE if it was created
     */
	virtual UBOOL IsInherited() const;

	/**
	 * Disconnect an inherited console. Default does nothing
	 */
	void DisconnectInherited();

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, enum EName Event );
};


/**
 * Windows implementation of console log window, utilizing the Win32 console API
 */
class FOutputDeviceConsoleWindows : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	HANDLE ConsoleHandle;

	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleWindows();
	~FOutputDeviceConsoleWindows();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
	 */
	virtual void Show( UBOOL ShowWindow );

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return TRUE if console is shown, FALSE otherwise
	 */
	virtual UBOOL IsShown();

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, enum EName Event );
};

#endif // CONSOLE


#if DEDICATED_SERVER && defined(_MSC_VER)
	#define WANTS_WINDOWS_EVENT_LOGGING 1
#endif

#if WANTS_WINDOWS_EVENT_LOGGING
/**
 * Output device that writes to Windows Event Log
 */
class FOutputDeviceEventLog :
	public FOutputDevice
{
	/** Handle to the event log object */
	HANDLE EventLog;

public:
	/**
	 * Constructor, initializing member variables
	 */
	FOutputDeviceEventLog(void);

	/** Destructor that cleans up any remaining resources */
	virtual ~FOutputDeviceEventLog(void);

	/**
	 * Writes a buffer to the event log
	 *
	 * @param Buffer the text to log
	 * @param Event the FName attributed to the log entry
	 */
	virtual void Serialize(const TCHAR* Buffer,EName Event);
	
	/** Does nothing */
	virtual void Flush(void)
	{
	}

	/**
	 * Closes any event log handles that are open
	 */
	virtual void TearDown(void);
};

#endif
