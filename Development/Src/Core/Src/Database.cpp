/*=============================================================================
	Database.cpp: Database integration
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Precompiled header include. Can't add anything above this line.
#include "CorePrivate.h"
#include "Database.h"
#include "../../IpDrv/Inc/UnIpDrv.h"

#if USE_REMOTE_INTEGRATION

/**
 * Sends a command to the database proxy.
 *
 * @param	Cmd		The command to be sent.
 */
UBOOL ExecuteDBProxyCommand(FSocket *Socket, const TCHAR* Cmd)
{
	check(Socket);
	check(Cmd);

	INT CmdStrLength = appStrlen(Cmd);
	INT BytesSent = 0;

	// add 1 so we also send NULL
	++CmdStrLength;

	TCHAR *SendBuf = (TCHAR*)appMalloc(CmdStrLength * sizeof(TCHAR));

	// convert to network byte ordering. This is important for running on the ps3 and xenon
	for(INT BufIndex = 0; BufIndex < CmdStrLength; ++BufIndex)
	{
		SendBuf[BufIndex] = htons(Cmd[BufIndex]);
	}

	UBOOL bRet = Socket->Send((BYTE*)SendBuf, CmdStrLength * sizeof(TCHAR), BytesSent);

	appFree(SendBuf);

	return bRet;
}

////////////////////////////////////////////// FRemoteDatabaseConnection ///////////////////////////////////////////////////////
/**
 * Constructor.
 */
FRemoteDatabaseConnection::FRemoteDatabaseConnection()
: Socket(NULL)
{
	check(GSocketSubsystem);
	// The socket won't work if secure connections are enabled, so don't try
	if (GSocketSubsystem->RequiresEncryptedPackets() == FALSE)
	{
		Socket = GSocketSubsystem->CreateStreamSocket(TEXT("remote database connection"));
	}
}

/**
 * Destructor.
 */
FRemoteDatabaseConnection::~FRemoteDatabaseConnection()
{
	check(GSocketSubsystem);

	if ( Socket )
	{
		GSocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
	}
}

/**
 * Opens a connection to the database.
 *
 * @param	ConnectionString	Connection string passed to database layer
 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
 *
 * @return	TRUE if connection was successfully established, FALSE otherwise
 */
UBOOL FRemoteDatabaseConnection::Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride )
{
	UBOOL bIsValid = FALSE;
	if ( Socket )
	{
		FInternetIpAddr Address;
		Address.SetIp(RemoteConnectionIP, bIsValid);
		Address.SetPort(10500);

		if(bIsValid)
		{
			bIsValid = Socket->Connect(Address);

			if(bIsValid && RemoteConnectionStringOverride)
			{
				SetConnectionString(RemoteConnectionStringOverride);
			}
		}
	}
	return bIsValid;
}

/**
 * Closes connection to database.
 */
void FRemoteDatabaseConnection::Close()
{
	if ( Socket )
	{
		Socket->Close();
	}
}

/**
 * Executes the passed in command on the database.
 *
 * @param CommandString		Command to execute
 *
 * @return TRUE if execution was successful, FALSE otherwise
 */
UBOOL FRemoteDatabaseConnection::Execute(const TCHAR* CommandString)
{
	FString Cmd = FString::Printf(TEXT("<command results=\"false\">%s</command>"), CommandString);
	return ExecuteDBProxyCommand(Socket, *Cmd);
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
UBOOL FRemoteDatabaseConnection::Execute(const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet)
{
	RecordSet = NULL;
	FString Cmd = FString::Printf(TEXT("<command results=\"true\">%s</command>"), CommandString);
	UBOOL bRetVal = ExecuteDBProxyCommand(Socket, *Cmd);
	INT ResultID = 0;
	INT BytesRead;

	if(bRetVal)
	{
		Socket->Recv((BYTE*)&ResultID, sizeof(INT), BytesRead);
		bRetVal = BytesRead == sizeof(INT);

		if(bRetVal)
		{
			RecordSet = new FRemoteDataBaseRecordSet(ResultID, Socket);
		}
	}

	return bRetVal;
}

/**
 * Sets the connection string to be used for this connection in the DB proxy.
 *
 * @param	ConnectionString	The new connection string to use.
 */
UBOOL FRemoteDatabaseConnection::SetConnectionString(const TCHAR* ConnectionString)
{
	FString Cmd = FString::Printf(TEXT("<connectionString>%s</connectionString>"), ConnectionString);
	return ExecuteDBProxyCommand(Socket, *Cmd);
}

////////////////////////////////////////////// FRemoteDataBaseRecordSet ///////////////////////////////////////////////////////

/** Moves to the first record in the set. */
void FRemoteDataBaseRecordSet::MoveToFirst()
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<movetofirst resultset=\"%s\"/>"), *ID));
}

/** Moves to the next record in the set. */
void FRemoteDataBaseRecordSet::MoveToNext()
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<movetonext resultset=\"%s\"/>"), *ID));
}

/**
 * Returns whether we are at the end.
 *
 * @return TRUE if at the end, FALSE otherwise
 */
UBOOL FRemoteDataBaseRecordSet::IsAtEnd() const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<isatend resultset=\"%s\"/>"), *ID));

	INT BytesRead;
	UBOOL bResult;
	Socket->Recv((BYTE*)&bResult, sizeof(UBOOL), BytesRead);

	if(BytesRead != sizeof(UBOOL))
	{
		bResult = FALSE;
	}
	else
	{
		bResult = ntohl(bResult);
	}

	return bResult;
}

/**
 * Returns a string associated with the passed in field/ column for the current row.
 *
 * @param	Column	Name of column to retrieve data for in current row
 */
FString FRemoteDataBaseRecordSet::GetString( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getstring resultset=\"%s\">%s</getstring>"), *ID, Column));

	const INT BUFSIZE = 2048;
	INT BytesRead;
	INT StrLength;
	TCHAR Buf[BUFSIZE];

	Socket->Recv((BYTE*)&StrLength, sizeof(INT), BytesRead);

	StrLength = ntohl(StrLength);

	if(BytesRead != sizeof(INT) || StrLength <= 0)
	{
		return TEXT("");
	}

	if(StrLength > BUFSIZE - 1)
	{
		StrLength = BUFSIZE - 1;
	}

	Socket->Recv((BYTE*)Buf, StrLength * sizeof(TCHAR), BytesRead);

	// TCHAR is assumed to be wchar_t so if we recv an odd # of bytes something messed up occured. Round down to the nearest wchar_t and then convert to number of TCHAR's.
	BytesRead -= BytesRead & 1; // rounding down
	BytesRead >>= 1; // conversion

	// convert from network to host byte order
	for(int i = 0; i < BytesRead; ++i)
	{
		Buf[i] = ntohs(Buf[i]);
	}

	Buf[BytesRead] = 0;

	return FString(Buf);
}

/**
* Returns an integer associated with the passed in field/ column for the current row.
*
* @param	Column	Name of column to retrieve data for in current row
*/
INT FRemoteDataBaseRecordSet::GetInt( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getint resultset=\"%s\">%s</getint>"), *ID, Column));

	INT BytesRead;
	INT Value;

	Socket->Recv((BYTE*)&Value, sizeof(INT), BytesRead);

	return ntohl(Value);
}

/**
* Returns a float associated with the passed in field/ column for the current row.
*
* @param	Column	Name of column to retrieve data for in current row
*/
FLOAT FRemoteDataBaseRecordSet::GetFloat( const TCHAR* Column ) const
{
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<getfloat resultset=\"%s\">%s</getfloat>"), *ID, Column));

	INT BytesRead;
	INT Temp;

	Socket->Recv((BYTE*)&Temp, sizeof(INT), BytesRead);

	Temp = ntohl(Temp);

	FLOAT Value = *((FLOAT*)&Temp);

	return Value;
}

/** Constructor. */
FRemoteDataBaseRecordSet::FRemoteDataBaseRecordSet(INT ResultSetID, FSocket *Connection) : Socket(NULL)
{
	check(ResultSetID >= 0);
	check(Connection);

	// NOTE: This socket will be deleted by whatever created it (prob an FRemoteDatabaseConnection), not this class.
	Socket = Connection;
	ID = FString::Printf(TEXT("%i"), ResultSetID);
}

/** Virtual destructor as class has virtual functions. */
FRemoteDataBaseRecordSet::~FRemoteDataBaseRecordSet()
{
	// tell the DB proxy to clean up the resources allocated for the result set.
	ExecuteDBProxyCommand(Socket, *FString::Printf(TEXT("<closeresultset resultset=\"%s\"/>"), *ID));
}

#endif

#if USE_ADO_INTEGRATION
#pragma pack( push, 8 )

/*-----------------------------------------------------------------------------
	ADO integration for database connectivity
-----------------------------------------------------------------------------*/

// Using import allows making use of smart pointers easily. Please post to the list if a workaround such as
// using %COMMONFILES% works to hide the localization issues and non default program file folders.
//#import "C:\Program files\Common Files\System\Ado\msado15.dll" rename("EOF", "ADOEOF")
#import "System\ADO\msado15.dll" rename("EOF", "ADOEOF")


/*-----------------------------------------------------------------------------
	FADODataBaseRecordSet implementation.
-----------------------------------------------------------------------------*/

/**
 * ADO implementation of database record set.
 */
class FADODataBaseRecordSet : public FDataBaseRecordSet
{
private:
	ADODB::_RecordsetPtr ADORecordSet;

protected:
	/** Moves to the first record in the set. */
	virtual void MoveToFirst()
	{
		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveFirst();
		}
	}
	/** Moves to the next record in the set. */
	virtual void MoveToNext()
	{
		if( !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveNext();
		}
	}
	/**
	 * Returns whether we are at the end.
	 *
	 * @return TRUE if at the end, FALSE otherwise
	 */
	virtual UBOOL IsAtEnd() const
	{
		return ADORecordSet->ADOEOF;
	}

public:

	/** 
	 *   Returns a count of the number of records in the record set
	 */
	virtual INT GetRecordCount() const
	{
		return ADORecordSet->RecordCount;
	}

	/**
	 * Returns a string associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FString GetString( const TCHAR* Column ) const
	{
		FString ReturnString;
		try
		{
			// Retrieve specified column field value for selected row.
			_variant_t Value = ADORecordSet->GetCollect( Column );
			// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
			if( Value.vt != VT_NULL )
			{
				ReturnString = (TCHAR*)_bstr_t(Value);
			}
			// Unknown column.
			else
			{
				ReturnString = TEXT("Unknown Column");
			}
		}
		// Error handling. Will return string with error message.
		catch( _com_error& Error )
		{
			ReturnString = FString::Printf(TEXT("Failure retrieving string value for column [%s] [%s]"),Column,(TCHAR*) Error.Description());
			debugf(NAME_DevDataBase,TEXT("%s"),*ReturnString);
		}
		return ReturnString;
	}

	/**
	 * Returns an integer associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual INT GetInt( const TCHAR* Column ) const
	{
		INT ReturnValue = 0;
		try
		{
			// Retrieve specified column field value for selected row.
			_variant_t Value = ADORecordSet->GetCollect( Column );
			// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
			if( Value.vt != VT_NULL )
			{
				ReturnValue = (INT)Value;
			}
			// Unknown column.
			else
			{
				debugf(NAME_DevDataBase,TEXT("Failure retrieving INT value for column [%s]"),Column);
			}
		}
		// Error handling. Will return default value of 0.
		catch( _com_error& Error )
		{
			debugf(NAME_DevDataBase,TEXT("Failure retrieving INT value for column [%s] [%s]"),Column,(TCHAR*) Error.Description());	
		}
		return ReturnValue;
	}

	/**
	 * Returns a float associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FLOAT GetFloat( const TCHAR* Column ) const
	{
		FLOAT ReturnValue = 0;
		try
		{
			// Retrieve specified column field value for selected row.
			_variant_t Value = ADORecordSet->GetCollect( Column );
			// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
			if( Value.vt != VT_NULL )
			{
				ReturnValue = (FLOAT)Value;
			}
			// Unknown column.
			else
			{
				debugf(NAME_DevDataBase,TEXT("Failure retrieving FLOAT value for column [%s]"),Column);
			}
		}
		// Error handling. Will return default value of 0.
		catch( _com_error& Error )
		{
			debugf(NAME_DevDataBase,TEXT("Failure retrieving FLOAT value for column [%s] [%s]"),Column,(TCHAR*) Error.Description());	
		}
		return ReturnValue;
	}

	/**
	 * Returns an SQWORD associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual SQWORD GetBigInt( const TCHAR* Column ) const
	{
		SQWORD ReturnValue = 0;
		try
		{
			// Retrieve specified column field value for selected row.
			_variant_t Value = ADORecordSet->GetCollect( Column );
			// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
			if( Value.vt != VT_NULL )
			{
				ReturnValue = (SQWORD)Value;
			}
			// Unknown column.
			else
			{
				debugf(NAME_DevDataBase,TEXT("Failure retrieving BIGINT value for column [%s]"),Column);
			}
		}
		// Error handling. Will return default value of 0.
		catch( _com_error& Error )
		{
			debugf(NAME_DevDataBase,TEXT("Failure retrieving BIGINT value for column [%s] [%s]"),Column,(TCHAR*) Error.Description());	
		}
		return ReturnValue;
	}

	/**
	 * Returns the set of column names for this Recordset.  This is useful for determining  
	 * what you can actually ask the record set for without having to hard code those ahead of time.  
	 */  
	virtual TArray<FDatabaseColumnInfo> GetColumnNames() const
	{  
		TArray<FDatabaseColumnInfo> Retval;  

		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF ) 
		{  
			ADORecordSet->MoveFirst();

			for( SWORD i = 0; i < ADORecordSet->Fields->Count; ++i )  
			{  
				_bstr_t bstrName = ADORecordSet->Fields->Item[i]->Name;  
				_variant_t varValue = ADORecordSet->Fields->Item[i]->Value;  
				ADODB::DataTypeEnum DataType = ADORecordSet->Fields->Item[i]->Type;  

				FDatabaseColumnInfo NewInfo;  
				NewInfo.ColumnName = FString((TCHAR*)_bstr_t(bstrName));  

				// from http://www.w3schools.com/ado/prop_field_type.asp#datatypeenum  
				switch( DataType )  
				{  
				case ADODB::adInteger:  
				case ADODB::adBigInt:
					NewInfo.DataType = DBT_INT;  
					break;  
				case ADODB::adSingle:  
				case ADODB::adDouble:  
					NewInfo.DataType = DBT_FLOAT;  
					break;  

				case ADODB::adWChar:
				case ADODB::adVarWChar:
					NewInfo.DataType = DBT_STRING;
					break;

				default:  
					warnf( NAME_DevDataBase, TEXT("Unable to find a EDataBaseUE3Types (%s) from DODB::DataTypeEnum DataType: %d "), *NewInfo.ColumnName, static_cast<INT>(DataType) );  
					NewInfo.DataType = DBT_UNKOWN;  
					break;  
				}  


				Retval.AddItem( NewInfo );  
			}  
		}  

		// here for debugging as this code is new.
		for( INT i = 0; i < Retval.Num(); ++ i )  
		{  
			warnf( TEXT( "ColumnName %d: Name: %s  Type: %d"), i, *Retval(i).ColumnName, static_cast<INT>(Retval(i).DataType) );  
		}  

		return Retval;  
	}   


	/**
	 * Constructor, used to associate ADO record set with this class.
	 *
	 * @param InADORecordSet	ADO record set to use
	 */
	FADODataBaseRecordSet( ADODB::_RecordsetPtr InADORecordSet )
	:	ADORecordSet( InADORecordSet )
	{
	}

	/** Destructor, cleaning up ADO record set. */
	virtual ~FADODataBaseRecordSet()
	{
		if(ADORecordSet && (ADORecordSet->State & ADODB::adStateOpen))
		{
			// We're using smart pointers so all we need to do is close and assign NULL.
			ADORecordSet->Close();
		}

		ADORecordSet = NULL;
	}
};


/*-----------------------------------------------------------------------------
	FADODataBaseConnection implementation.
-----------------------------------------------------------------------------*/

/**
 * Data base connection class using ADO C++ interface to communicate with SQL server.
 */
class FADODataBaseConnection : public FDataBaseConnection
{
private:
	/** ADO database connection object. */
	ADODB::_ConnectionPtr DataBaseConnection;

public:
	/** Constructor, initializing all member variables. */
	FADODataBaseConnection()
	{
		DataBaseConnection = NULL;
	}

	/** Destructor, tearing down connection. */
	virtual ~FADODataBaseConnection()
	{
		Close();
	}

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
		try
		{
			// Create instance of DB connection object.
			HRESULT hr = DataBaseConnection.CreateInstance(__uuidof(ADODB::Connection));
			if (FAILED(hr))
			{
				throw _com_error(hr);
			}

			// Open the connection. Operation is synchronous.
			DataBaseConnection->Open( ConnectionString, TEXT(""), TEXT(""), ADODB::adConnectUnspecified );
		}      
		catch(_com_error &Error)
		{	
			// Log error and return. This will not log if called before appInit due to use of debugf.
			TCHAR* ErrorDescription = (TCHAR*) Error.Description();
			debugf(NAME_DevDataBase,TEXT("Failure trying to open connection [%s] [%s]"),ConnectionString,ErrorDescription);
			return FALSE;
		}
		return TRUE;
	}

	/**
	 * Closes connection to database.
	 */
	virtual void Close()
	{
		try
		{
			// Close database connection if exists and free smart pointer.
			if( DataBaseConnection && (DataBaseConnection->State & ADODB::adStateOpen))
			{
				DataBaseConnection->Close();
			}

			DataBaseConnection = NULL;
		}
		catch(_com_error &Error)
		{
			// Log error and return. This will not log if called before appInit due to use of debugf.
			TCHAR* ErrorDescription = (TCHAR*) Error.Description();
			debugf(NAME_DevDataBase,TEXT("Failure closing connection [%s]"),ErrorDescription);
		}
	}

	/**
	 * Executes the passed in command on the database.
	 *
	 * @param CommandString		Command to execute
	 *
	 * @return TRUE if execution was successful, FALSE otherwise
	 */
	virtual UBOOL Execute( const TCHAR* CommandString )
	{
		try
		{
			// Execute command, passing in optimization to tell DB to not return records.
			DataBaseConnection->Execute( CommandString, NULL, ADODB::adExecuteNoRecords );
		}
		catch(_com_error &Error)
		{
			// Log error and return. This will not log if called before appInit due to use of debugf.
			TCHAR* ErrorDescription = (TCHAR*) Error.Description();
			warnf(NAME_DevDataBase,TEXT("Failure executing command [%s] [%s]"),CommandString,ErrorDescription);
			return FALSE;
		}
		return TRUE;
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
		// Initialize return value.
		RecordSet = NULL;
		try
		{
			// Create instance of record set.
			ADODB::_RecordsetPtr ADORecordSet = NULL;
			ADORecordSet.CreateInstance(__uuidof(ADODB::Recordset) );
				
			// Execute the passed in command on the record set. The recordset returned will be in open state so you can call Get* on it directly.
			ADORecordSet->Open( CommandString, _variant_t((IDispatch *) DataBaseConnection), ADODB::adOpenStatic, ADODB::adLockReadOnly, ADODB::adCmdText );

			// Create record set from returned data.
			RecordSet = new FADODataBaseRecordSet( ADORecordSet );
		}
		catch(_com_error &Error)
		{
			// Log error and return. This will not log if called before appInit due to use of debugf.
			TCHAR* ErrorDescription = (TCHAR*) Error.Description();
			warnf(NAME_DevDataBase,TEXT("Failure executing command [%s] [%s]"),CommandString,ErrorDescription);
			
			// Delete record set if it has already been created.
			delete RecordSet;
			RecordSet = NULL;
		}

		return RecordSet != NULL;
	}
};
#pragma pack( pop )

#endif // USE_ADO_INTEGRATION

/*-----------------------------------------------------------------------------
	FDataBaseConnection implementation.
-----------------------------------------------------------------------------*/

/**
 * Static function creating appropriate database connection object.
 *
 * @return	instance of platform specific database connection object
 */
FDataBaseConnection* FDataBaseConnection::CreateObject()
{
	if( ParseParam( appCmdLine(), TEXT("NODATABASE") ) )
	{
		debugf(TEXT("DB usage disabled, please ignore failure messages."));
		return NULL;
	}
#if USE_ADO_INTEGRATION
	if( !GIsCOMInitialized )
	{
		// Initialize COM. We only want to do this once and not override settings of previous calls.
		CoInitialize( NULL );
		GIsCOMInitialized = TRUE;
	}
	return new FADODataBaseConnection();
#elif USE_REMOTE_INTEGRATION
	return new FRemoteDatabaseConnection();
#else
	return new FDataBaseConnection();
#endif
}


/*-----------------------------------------------------------------------------
	FTaskDatabase implementation.
-----------------------------------------------------------------------------*/
/**
 * Base Constructor.  Subclasses should set the various values based on their .ini sections
 */
FTaskDatabase::FTaskDatabase()
: Connection(NULL)
{
}

/** Destructor, cleaning up connection if it was in use. */
FTaskDatabase::~FTaskDatabase()
{
	// If we have a connection at this point it was initialized so we need to close it.
	if( Connection != NULL )
	{
		Connection->Close();
		delete Connection;
		Connection = NULL;
	}
}


/*-----------------------------------------------------------------------------
	FTaskPerfTracker implementation.
-----------------------------------------------------------------------------*/

/** Global task perf tracker object. Is initialized after GConfig.	*/
FTaskPerfTracker* GTaskPerfTracker = NULL;

/**
 * Constructor, initializing all member variables. It also reads .ini settings and caches them and creates the
 * database connection object and attempts to connect if bUseTaskPerfTracking is set.
 */
FTaskPerfTracker::FTaskPerfTracker()
: FTaskDatabase()
, bUseTaskPerfTracking(FALSE)
, TimeSpentTalkingWithDB(0)
{
#if !UDK && !DEDICATED_SERVER
	// Read the ini settings and store them in the struct to aid debugging.
	GConfig->GetBool(TEXT("TaskPerfTracking"), TEXT("bUseTaskPerfTracking"	), bUseTaskPerfTracking, GEngineIni);

	// Only do work if task tracking is enabled.
	if( bUseTaskPerfTracking )
	{
		// only attempt to get the data when we want to use the TaskPerfTracking
		verify(GConfig->GetString( TEXT("TaskPerfTracking"), TEXT("ConnectionString"), ConnectionString, GEngineIni ));
		verify(GConfig->GetString( TEXT("TaskPerfTracking"), TEXT("RemoteConnectionIP"), RemoteConnectionIP, GEngineIni ));
		verify(GConfig->GetString( TEXT("TaskPerfTracking"), TEXT("RemoteConnectionStringOverride"), RemoteConnectionStringOverride, GEngineIni ));

		// Track time spent talking with DB to ensure we don't introduce nasty stalls.
		SCOPE_SECONDS_COUNTER(TimeSpentTalkingWithDB);

		// Create the connection object; needs to be deleted via "delete".
		Connection = FDataBaseConnection::CreateObject();

		// Try to open connection to DB - this is a synchronous operation.
		if( Connection && Connection->Open( *ConnectionString, *RemoteConnectionIP, *RemoteConnectionStringOverride ) == TRUE )
		{
			debugf(NAME_DevDataBase,TEXT("Connection to \"%s\" or \"%s\" succeeded"), *ConnectionString, *RemoteConnectionIP);

			// Create format string for calling procedure.
			FormatString = FString(TEXT("EXEC ADDTASK "));
#if _DEBUG
			FormatString += TEXT(" @ConfigName='DEBUG', ");
#elif FINAL_RELEASE_DEBUGCONSOLE
			FormatString += TEXT(" @ConfigName='FINAL_RELEASE_DEBUGCONSOLE', ");
#elif FINAL_RELEASE
			FormatString += TEXT(" @ConfigName='FINAL_RELEASE', ");
#else
			FormatString += TEXT(" @ConfigName='RELEASE', ");
#endif			
			FormatString += FString(TEXT("@PlatformName='")) + appGetPlatformString() + TEXT("', ");
			FormatString += FString(TEXT("@GameName='")) + GGameName + TEXT("', @MachineName='") + appComputerName() + TEXT("', ");
			FormatString += FString(TEXT("@CmdLine='")) + appCmdLine() + TEXT("', @UserName='") + appUserName() + TEXT("', ");		
			FormatString += FString(TEXT("@TaskDescription='%s', @TaskParameter='%s', @Duration='%f', @Changelist=")) + appItoa(GBuiltFromChangeList);
		}
		// Connection failed :(
		else
		{
			warnf(NAME_DevDataBase,TEXT("Connection to \"%s\" or \"%s\" failed"), *ConnectionString, *RemoteConnectionIP);
			// Only delete object - no need to close as connection failed.
			delete Connection;
			Connection = NULL;
		}
	}
#endif	//#if !UDK
}

/** Destructor */
FTaskPerfTracker::~FTaskPerfTracker()
{
	// The only time this time will be 0 is if bUseTracking is FALSE. If the connection failed this will include
	// the time spent waiting for the failure.
	if( TimeSpentTalkingWithDB > 0 )
	{
		debugf(NAME_DevDataBase,TEXT("Spent %f seconds communicating with \"%s\" or \"%s\""),TimeSpentTalkingWithDB, *ConnectionString, *RemoteConnectionIP);
	}
}

/** 
 * Adds a task to track to the DB.
 *
 * @param	Task					Name of task
 * @param	TaskParameter			Additional information, e.g. name of map if task is lighting rebuild
 * @param	DurationInSeconds		Duration of task in seconds
 */
void FTaskPerfTracker::AddTask( const TCHAR* Task, const TCHAR* TaskParameter, FLOAT DurationInSeconds )
{
	// A valid connection means that tracking is enabled.
	if( Connection )
	{
		// Track time spent talking with DB to ensure we don't introduce nasty stalls.
		SCOPE_SECONDS_COUNTER(TimeSpentTalkingWithDB);
		Connection->Execute( *FString::Printf(*FormatString, Task, TaskParameter, DurationInSeconds) );
	}
}


/*-----------------------------------------------------------------------------
	FTaskPerfMemDatabase implementation.
-----------------------------------------------------------------------------*/

/** Global Remote Database tracker object. Is initialized after GConfig.	*/
FTaskPerfMemDatabase* GTaskPerfMemDatabase = NULL;


/**
 * Constructor, initializing all member variables. It also reads .ini settings and caches them and creates the
 * database connection object and attempts to connect if bUseTaskPerfMemDatabase is set.
 */
FTaskPerfMemDatabase::FTaskPerfMemDatabase()
: FTaskDatabase()
{
#if !UDK && !DEDICATED_SERVER
	// Read the ini settings and store them in the struct to aid debugging.
	GConfig->GetBool(TEXT("TaskPerfMemDatabase"), TEXT("bUseTaskPerfMemDatabase"	), bUseTaskPerfMemDatabase, GEngineIni);

	// Only do work if task tracking is enabled.
	if( bUseTaskPerfMemDatabase == TRUE )
	{
		// only attempt to get the data when we want to use the TaskPerfMemDatabase
		verify(GConfig->GetString( TEXT("TaskPerfMemDatabase"), TEXT("ConnectionString"), ConnectionString, GEngineIni ));
		verify(GConfig->GetString( TEXT("TaskPerfMemDatabase"), TEXT("RemoteConnectionIP"), RemoteConnectionIP, GEngineIni ));
		verify(GConfig->GetString( TEXT("TaskPerfMemDatabase"), TEXT("RemoteConnectionStringOverride"), RemoteConnectionStringOverride, GEngineIni ));

		// Create the connection object; needs to be deleted via "delete".
		Connection = FDataBaseConnection::CreateObject();

		// Try to open connection to DB - this is a synchronous operation.
		if( Connection && Connection->Open( *ConnectionString, *RemoteConnectionIP, *RemoteConnectionStringOverride ) )
		{
			debugf(NAME_DevDataBase,TEXT("Connection to \"%s\" or \"%s\" succeeded"), *ConnectionString, *RemoteConnectionIP);
		}
		// Connection failed :(
		else
		{
			warnf(NAME_DevDataBase,TEXT("Connection to \"%s\" or \"%s\" failed"), *ConnectionString, *RemoteConnectionIP);

			// Only delete object - no need to close as connection failed.
			delete Connection;
			Connection = NULL;
		}
	}
#endif	//#if !UDK
}

/** Destructor */
FTaskPerfMemDatabase::~FTaskPerfMemDatabase()
{
}



/** 
 *  This will send the text to be "Exec" 'd to the DB proxy 
 *
 * @param	ExecCommand  Exec command to send to the Proxy DB.
 */
UBOOL FTaskPerfMemDatabase::SendExecCommand( const FString& ExecCommand )
{
	UBOOL Retval = FALSE;

	if( Connection != NULL )
	{
		// Execute SQL command generated. The order of arguments needs to match the format string.
		Retval = Connection->Execute( *ExecCommand );
		//warnf( TEXT( "%s"), *ExecCommand );
	}

	return Retval;
}


/** 
 *  This will send the text to be "Exec" 'd to the DB proxy 
 *
 * @param  ExecCommand  Exec command to send to the Proxy DB.
 * @param RecordSet		Reference to recordset pointer that is going to hold result
 */
UBOOL FTaskPerfMemDatabase::SendExecCommandRecordSet( const FString& ExecCommand, FDataBaseRecordSet*& RecordSet )
{
	UBOOL Retval = FALSE;

	if( Connection != NULL )
	{
		// Execute SQL command generated. The order of arguments needs to match the format string.
		Retval = Connection->Execute( *ExecCommand, RecordSet );
	}

	return Retval;
}




