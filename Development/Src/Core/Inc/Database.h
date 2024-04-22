/*=============================================================================
	Database.h: Database integration
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UNREAL_DATABASE_H_
#define _UNREAL_DATABASE_H_

// Only use ADO on windows, if support is enabled and not for shipping games.
#define USE_ADO_INTEGRATION (WIN32 && (!SHIPPING_PC_GAME || DEDICATED_SERVER) && WITH_DATABASE_SUPPORT)
#define USE_REMOTE_INTEGRATION (!USE_ADO_INTEGRATION && !SHIPPING_PC_GAME && WITH_DATABASE_SUPPORT && WITH_UE3_NETWORKING)


/**  
 * UE3 Enums for Database types.  Each Database has their own set of DB types and  
 */  
enum EDataBaseUE3Types  
{  
	DBT_UNKOWN,  
	DBT_FLOAT,  
	DBT_INT,  
	DBT_STRING,  
};  


/**   
 * This struct holds info relating to a column.  Specifically, we need to get back  
 * certain meta info from a RecordSet so we can "Get" data from it.  
 */  
struct FDatabaseColumnInfo  
{  
	/** Default constructor **/  
	FDatabaseColumnInfo(): DataType(DBT_UNKOWN) {}  

	/** The name of the column **/  
	FString ColumnName;  

	/** This is the type of data in this column.  (e.g. so you can do GetFloat or GetInt on the column **/  
	EDataBaseUE3Types DataType;  


	UBOOL operator==(const FDatabaseColumnInfo& OtherFDatabaseColumnInfo) const  
	{  
		return (ColumnName==ColumnName) && (DataType==DataType);  
	}  

};   

/**
 * Empty base class for iterating over database records returned via query. Used on platforms not supporting
 * a direct database connection.
 */
class FDataBaseRecordSet
{
	// Protected functions used internally for iteration.

protected:
	/** Moves to the first record in the set. */
	virtual void MoveToFirst()
	{}
	/** Moves to the next record in the set. */
	virtual void MoveToNext()
	{}
	/**
	 * Returns whether we are at the end.
	 *
	 * @return TRUE if at the end, FALSE otherwise
	 */
	virtual UBOOL IsAtEnd() const
	{
		return TRUE;
	}

public:

	/** 
	 *   Returns a count of the number of records in the record set
	 */
	virtual INT GetRecordCount() const
	{ 
		return 0;
	}

	/**
	 * Returns a string associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FString GetString( const TCHAR* Column ) const
	{
		return TEXT("No database connection compiled in.");
	}

	/**
	 * Returns an integer associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual INT GetInt( const TCHAR* Column ) const
	{
		return 0;
	}

	/**
	 * Returns a float associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FLOAT GetFloat( const TCHAR* Column ) const
	{
		return 0;
	}

	/**
	 * Returns a SQWORD associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual SQWORD GetBigInt( const TCHAR* Column ) const
	{
		return 0;
	}

	/**  
      * Returns the set of column names for this Recordset.  This is useful for determining  
      * what you can actually ask the record set for without having to hard code those ahead of time.  
      */  
     virtual TArray<FDatabaseColumnInfo> GetColumnNames() const  
     {  
          TArray<FDatabaseColumnInfo> Retval;  
          return Retval;  
     }

	/** Virtual destructor as class has virtual functions. */
	virtual ~FDataBaseRecordSet()
	{}

	/**
	 * Iterator helper class based on FObjectIterator.
	 */
	class TIterator
	{
	private:
		// Private class for safe bool conversion.
		struct PrivateBooleanHelper { INT Value; };

	public:
		/** 
		 * Initialization constructor.
		 *
		 * @param	InRecordSet		RecordSet to iterate over
		 */
		TIterator( FDataBaseRecordSet* InRecordSet )
		: RecordSet( InRecordSet )
		{
			RecordSet->MoveToFirst();
		}

		/** 
		 * operator++ used to iterate to next element.
		 */
		void operator++()
		{ 
			RecordSet->MoveToNext();	
		}

		/** Conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		FORCEINLINE operator PrivateBooleanType() const 
		{ 
			return RecordSet->IsAtEnd() ? NULL : &PrivateBooleanHelper::Value; 
		}

		// Access operators
		FORCEINLINE FDataBaseRecordSet* operator*() const
		{
			return RecordSet;
		}
		FORCEINLINE FDataBaseRecordSet* operator->() const
		{
			return RecordSet;
		}

	protected:
		/** Database record set being iterated over. */
		FDataBaseRecordSet* RecordSet;
	};
};



/**
 * Empty base class for database access via executing SQL commands. Used on platforms not supporting
 * a direct database connection.
 */
class FDataBaseConnection
{
public:
	/** Virtual destructor as we have virtual functions. */
	virtual ~FDataBaseConnection() 
	{}

	/**
	 * Opens a connection to the database.
	 *
	 * @param	ConnectionString	Connection string passed to database layer
	 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
	 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
	 *
	 * @return	TRUE if connection was successfully established, FALSE otherwise
	 */
	virtual UBOOL Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride )
	{
		return FALSE;
	}
	
	/**
	 * Closes connection to database.
	 */
	virtual void Close()
	{}

	/**
	 * Executes the passed in command on the database.
	 *
	 * @param CommandString		Command to execute
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	virtual UBOOL Execute( const TCHAR* CommandString )
	{
		return FALSE;
	}

	/**
	 * Executes the passed in command on the database. The caller is responsible for deleting
	 * the created RecordSet.
	 *
	 * @param CommandString		Command to execute
	 * @param RecordSet			Reference to recordset pointer that is going to hold result
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	virtual UBOOL Execute( const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet )
	{
		RecordSet = NULL;
		return FALSE;
	}

	/**
	 * Static function creating appropriate database connection object.
	 *
	 * @return	instance of platform specific database connection object
	 */
	static FDataBaseConnection* CreateObject();
};

// since this relies on FSocket we have to make sure networking is enabled
#if USE_REMOTE_INTEGRATION

//forward declarations
class FSocket;

/**
* This class allows for connections to a remote database proxy that allows any platform, regardless of native DB support, to talk to a DB.
*/
class FRemoteDatabaseConnection : public FDataBaseConnection
{
private:
	FSocket *Socket;

public:
	/**
	 * Constructor.
	 */
	FRemoteDatabaseConnection();

	/**
	 * Destructor.
	 */
	virtual ~FRemoteDatabaseConnection();

	/**
	 * Opens a connection to the database.
	 *
	 * @param	ConnectionString	Connection string passed to database layer
	 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
	 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
	 *
	 * @return	TRUE if connection was successfully established, FALSE otherwise
	 */
	virtual UBOOL Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride );

	/**
	* Closes connection to database.
	*/
	virtual void Close();

	/**
	* Executes the passed in command on the database.
	*
	* @param CommandString		Command to execute
	*
	* @return TRUE if execution was successful, FALSE otherwise
	*/
	virtual UBOOL Execute(const TCHAR* CommandString);

	/**
	 * Executes the passed in command on the database. The caller is responsible for deleting
	 * the created RecordSet.
	 *
	 * @param CommandString		Command to execute
	 * @param RecordSet			Reference to recordset pointer that is going to hold result
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	virtual UBOOL Execute(const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet);

	/**
	 * Sets the connection string to be used for this connection in the DB proxy.
	 *
	 * @param	ConnectionString	The new connection string to use.
	 */
	UBOOL SetConnectionString(const TCHAR* ConnectionString);
};

/**
 * A record set that is accessed from a DB proxy.
 */
class FRemoteDataBaseRecordSet : public FDataBaseRecordSet
{
private:
	/** The record set's ID within the DB proxy. */
	FString ID;

	/** The connection to the proxy DB */
	FSocket *Socket;

	// Protected functions used internally for iteration.
protected:
	/** Moves to the first record in the set. */
	virtual void MoveToFirst();

	/** Moves to the next record in the set. */
	virtual void MoveToNext();

	/**
	 * Returns whether we are at the end.
	 *
	 * @return TRUE if at the end, FALSE otherwise
	 */
	virtual UBOOL IsAtEnd() const;

public:
	/**
	 * Returns a string associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FString GetString( const TCHAR* Column ) const;

	/**
	 * Returns an integer associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual INT GetInt( const TCHAR* Column ) const;

	/**
	 * Returns a float associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FLOAT GetFloat( const TCHAR* Column ) const;

	/** Constructor. */
	FRemoteDataBaseRecordSet(INT ResultSetID, FSocket *Connection);

	/** Virtual destructor as class has virtual functions. */
	virtual ~FRemoteDataBaseRecordSet();
};

#endif

/**
 * This is a base class which all Database task types should subclass.
 * The ideas is that each of those types will need to have a base set of data / functionality.
 * Then they will set that data based off .ini sections (e.g. the Server to connect to, the Database to send data to)
 **/
struct FTaskDatabase
{
protected:
	/** Database connection object. Created in subclass's constructor.	*/
	FDataBaseConnection*	Connection;
	
	/** Config variables read from .ini	(per FTaskDatasbase type 	*/

	/** Name of SQL server to use.									*/
	FString					ConnectionString;
	/** Name of database to use on server.							*/
	FString					RemoteConnectionIP;
	/** Name of database to use on server.							*/
	FString					RemoteConnectionStringOverride;

	/**
	 * Base Constructor.  Subclasses should set the various values based on their .ini sections
	 */
	FTaskDatabase();

	/** Destructor, cleaning up connection if it was in use. */
	virtual ~FTaskDatabase();

public:
	/**
	 * Returns the database connection used. This can be used to perform queries in captured data.
	 *
	 * @return	Database connection used, NULL if not connected
	 */
	FDataBaseConnection* GetConnection() const
	{
		return Connection;
	}
};


/**
 * Task perf tracker. Used to track lengthy tasks via a database to facilitate global persistent tracking.
 */
struct FTaskPerfTracker : FTaskDatabase
{
private:

	/** Config variables read from .ini								*/

	/** Whether to use task tracking reports to DB.					*/
	UBOOL bUseTaskPerfTracking;
	/** Name of stored procedure to call to add data.				*/
	FString	Procedure;


	/** Format string used for calling procedure.					*/
	FString	FormatString;

	/** 
	 * This is the IP/DNS address of the DatabaseProxy.  The proxy allows us to send SQL commands from a non PC (e.g. console) 
	 * which then routes the commands to the DB.
	 **/
	FString	DatabaseProxyAddress;

	/**
	 * This is the connection string the remote connection in the DB proxy will use to connect to the DB.
	 */
	FString DatabseProxyConnectionString;

	/** Time spent communicating with DB (in seconds).				*/
	DOUBLE TimeSpentTalkingWithDB;

public:
	/**
	 * Constructor, initializing all member variables. It also reads .ini settings and caches them and creates the
	 * database connection object and attempts to connect if bUseTracking is set.
	 */
	FTaskPerfTracker();

	/** Destructor, cleaning up connection if it was in use. */
	virtual ~FTaskPerfTracker();

	/** 
	 * Adds a task to track to the DB.
	 *
	 * @param	Task					Name of task
	 * @param	TaskParameter			Additional information, e.g. name of map if task is lighting rebuild
	 * @param	DurationInSeconds		Duration of task in seconds
	 */
	void AddTask( const TCHAR* Task, const TCHAR* TaskParameter, FLOAT DurationInSeconds );
};

/** Global task perf tracker object. Is initialized after GConfig.	*/
extern FTaskPerfTracker* GTaskPerfTracker;

/**
 * Helper structure for scoped task perf tracking.
 */
struct FScopedTaskPerfTracker
{
	/**
	 * Constructor, taking same parameters as FTaskPerfTracker::AddTask
	 */
	FScopedTaskPerfTracker( const TCHAR* InTask, const TCHAR* InTaskParameter )
	:	StartTime(appSeconds())
	,	Task(InTask)
	,	TaskParameter(InTaskParameter)
	{}

	/**
	 * Destructor, passing parameters passed to constructor to task perf tracker.
	 */
	~FScopedTaskPerfTracker()
	{
		GTaskPerfTracker->AddTask( *Task, *TaskParameter, appSeconds() - StartTime );
	}

private:
	/** Start time of task tracked. */
	DOUBLE	StartTime;
	/** Name of task.				*/
	FString Task;
	/** Additional info about task. */
	FString TaskParameter;
};


/**
 * FTaskPerfMemDatabase.  Which we use to wrap up sending data from the PerfMem worldstate to a DB. 
 */
struct FTaskPerfMemDatabase : FTaskDatabase
{
private:
	/** Whether to use task the perfmem database					*/
	UBOOL					bUseTaskPerfMemDatabase;
	/** 
	 * This is the IP/DNS address of the DatabaseProxy.  The proxy allows us to send SQL commands from a non PC (e.g. console) 
	 * which then routes the commands to the DB.
	 **/
	FString DatabaseProxyAddress;

	/**
	 * This is the connection string the remote connection in the DB proxy will use to connect to the DB.
	 */
	FString DatabseProxyConnectionString;


public:
	FTaskPerfMemDatabase();

	virtual ~FTaskPerfMemDatabase();

	/** 
	 *  This will send the text to be "Exec" 'd to the DB proxy 
	 *
	 * @param  ExecCommand  Exec command to send to the Proxy DB.
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	UBOOL SendExecCommand( const FString& ExecCommand );


	/** 
	 *  This will send the text to be "Exec" 'd to the DB proxy 
	 *
	 * @param  ExecCommand  Exec command to send to the Proxy DB.
	 * @param RecordSet		Reference to recordset pointer that is going to hold result
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	UBOOL SendExecCommandRecordSet( const FString& ExecCommand, FDataBaseRecordSet*& RecordSet );

};


/** Global Remote Database tracker object. Is initialized after GConfig.	*/
extern FTaskPerfMemDatabase* GTaskPerfMemDatabase;




#endif // _UNREAL_DATABASE_H_
