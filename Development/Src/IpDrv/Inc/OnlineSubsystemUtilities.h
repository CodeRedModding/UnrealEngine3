/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __ONLINESUBSYSTEMUTILITIES_H__
#define __ONLINESUBSYSTEMUTILITIES_H__

#if WITH_UE3_NETWORKING

#ifndef E_FAIL
#define E_FAIL (DWORD)-1
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL (DWORD)-2
#endif

#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997
#endif

#ifndef S_OK
#define S_OK 0
#endif

#define DEBUG_PROFILE_DATA 0

#if DEBUG_PROFILE_DATA
/**
 * Logs each profile entry so we can verify profile read/writes
 *
 * @param Profile the profile object to dump the settings for
 */
inline void DumpProfile(UOnlineProfileSettings* Profile)
{
	debugf(NAME_DevOnline,TEXT("Profile object %s of type %s"),
		*Profile->GetName(),*Profile->GetClass()->GetName());
	for (INT Index = 0; Index < Profile->ProfileSettings.Num(); Index++)
	{
		FOnlineProfileSetting& Setting = Profile->ProfileSettings(Index);
		// Dump the value as a string
		FString Value = Setting.ProfileSetting.Data.ToString();
		debugf(NAME_DevOnline,TEXT("ProfileSettings(%d) ProfileId(%d) = %s"),
			Index,Setting.ProfileSetting.PropertyId,*Value);
	}
}
#endif

/** Version of the delegate parms that takes a result code and applies it */
struct FAsyncTaskDelegateResults
{
	/** Whether the async task worked or not. Script accessible only! */
    UBOOL bWasSuccessful;

	/**
	 * Constructor that sets the success flag based upon the passed in results
	 *
	 * @param Result the result code to check
	 */
	inline FAsyncTaskDelegateResults(DWORD Result)
	{
		bWasSuccessful = (Result == 0) ? FIRST_BITFIELD : 0;
	}

	/**
	 * Constructor that sets the success flag to false
	 */
	inline FAsyncTaskDelegateResults(void)
	{
		bWasSuccessful = FALSE;
	}
};

/** Version of the delegate parms that takes a result code and a session name and applies it */
struct FAsyncTaskDelegateResultsNamedSession
{
	/** The name of the session this event is for. Script accessible only! */
	FName SessionName;
	/** Whether the async task worked or not. Script accessible only! */
    UBOOL bWasSuccessful;

	/**
	 * Constructor that sets the success flag based upon the passed in results
	 *
	 * @param InSessionName the name of the session to report
	 * @param Result the result code to check
	 */
	inline FAsyncTaskDelegateResultsNamedSession(FName InSessionName,DWORD Result)
	{
		SessionName = InSessionName;
		bWasSuccessful = (Result == 0) ? FIRST_BITFIELD : 0;
	}

	/**
	 * Constructor that sets the success flag to false with no name
	 */
	inline FAsyncTaskDelegateResultsNamedSession(void)
	{
		SessionName = NAME_None;
		bWasSuccessful = FALSE;
	}
};

/**
 * Helper method that fires off a list of delegates on a particular object
 * with the specified set of parameters
 *
 * @param TargetObject the object that the events are triggered against
 * @param Delegates the list of delegates to process (INTENTIONALLY copied to avoid removal while in use)
 * @param Parms optional pointer to a set of delegate parameters
 */
inline void TriggerOnlineDelegates(UObject* TargetObject,TArray<FScriptDelegate> Delegates,void* Parms)
{
	// Iterate through the delegate list
	for (INT Index = 0; Index < Delegates.Num(); Index++)
	{
		// Cache the number of delegates in case one is removed due to the
		// notification processing
		INT CurrentNum = Delegates.Num();
		// Make sure the pointer if valid before processing
		FScriptDelegate* ScriptDelegate = &Delegates(Index);
		if (ScriptDelegate != NULL)
		{
			// Send the notification of completion
			TargetObject->ProcessDelegate(NAME_None,ScriptDelegate,Parms);
		}
		// Check to see if the number of notifications changed during processing
		if (CurrentNum > Delegates.Num())
		{
			if (CurrentNum - Delegates.Num() > 1)
			{
				debugf(NAME_Error,TEXT("TriggerOnlineDelegates(): Too many delegates where removed during processing"));
			}
			// Assume that the notification just processed removed itself
			Index--;
		}
	}
}

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTask
{
public:
	/** Default used for optional MinCompletionTime. Initialized from [OnlineSubsystem].AsyncMinCompletionTime config value */
	static FLOAT DefaultMinCompletionTime;
protected:
	/** The delegate to fire when the overlapped task is complete */
	TArray<FScriptDelegate>* ScriptDelegates;
	/** Method for determining the type of task */
	const TCHAR* AsyncTaskName;
	/** Tracks how long the task took in seconds */
	FLOAT ElapsedTime;
#if !FINAL_RELEASE
	/** Minimum amount of time this task should take to complete. Delayed until time limit is met */
	FLOAT MinCompletionTime;
#endif

	/** Hidden on purpose */
	FOnlineAsyncTask(void) {}

public:
	/**
	 * Initializes the members
	 *
	 * @param InScriptDelegatse the list of delegates to fire off when complete
	 * @param InAsyncTaskName pointer to static data identifying the type of task
	 * @param InMinCompletionTime minimum amount of time this task should take to complete
	 */
	FOnlineAsyncTask(TArray<FScriptDelegate>* InScriptDelegates,const TCHAR* InAsyncTaskName,FLOAT InMinCompletionTime=DefaultMinCompletionTime) :
		ScriptDelegates(InScriptDelegates),
		AsyncTaskName(InAsyncTaskName),
		ElapsedTime(0.f)
	{
#if !FINAL_RELEASE
		// Also used to delay the completion of an async task for debug purposes
		MinCompletionTime = InMinCompletionTime;
#endif
	}
	
	/**
	 * Logs amount of time taken in the async task
	 */
	virtual ~FOnlineAsyncTask(void)
	{
	}

#if !FINAL_RELEASE && !SHIPPING_PC_GAME
	/**
	 * Logs amount of time taken in the async task
	 */
	virtual void LogResults(void)
	{
		debugf(NAME_DevOnline,TEXT("Async task '%s' completed in %f seconds with 0x%08X"),
			AsyncTaskName,
			ElapsedTime,
			GetCompletionCode());
	}
#endif

	/**
	 * Returns the status code of the completed task. Assumes the task
	 * has finished.
	 *
	 * @return ERROR_SUCCESS if ok, an HRESULT error code otherwise
	 */
	virtual DWORD GetCompletionCode(void)
	{
		return E_FAIL;
	}

	/**
	 * Iterates the list of delegates and fires those notifications
	 *
	 * @param Object the object that the notifications are going to be issued on
	 */
	virtual void TriggerDelegates(UObject* Object)
	{
		check(Object);
		// Only fire off the events if there are some registered
		if (ScriptDelegates != NULL)
		{
			// Pass in the data that indicates whether the call worked or not
			FAsyncTaskDelegateResults Parms(GetCompletionCode());
			// Use the common method to do the work
			TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
		}
	}
	
	/**
	* @return TRUE if the completion of the task should be delayed
	*/
	FORCEINLINE UBOOL ShouldDelayCompletion(void) const
	{
#if FINAL_RELEASE || SHIPPING_PC_GAME	
		return FALSE;
#else
		return ElapsedTime > 0.0 && ElapsedTime < MinCompletionTime;
#endif
	}

	/**
	 * Updates the amount of elapsed time this task has taken
	 *
	 * @param DeltaTime the amount of time that has passed since the last update
	 */
	FORCEINLINE void UpdateElapsedTime(FLOAT DeltaTime)
	{
		ElapsedTime += DeltaTime;
	}

	/** @return the amount of elapsed time the task has taken */
	FORCEINLINE FLOAT GetElapsedTime(void) const
	{
		return ElapsedTime;
	}
};

/**
 * Serializes data in network byte order form into a buffer
 */
class FNboSerializeToBuffer
{
	/** Hidden on purpose */
	FNboSerializeToBuffer(void);

protected:
	/**
	 * Holds the data as it is serialized
	 */
	TArray<BYTE> Data;
	/**
	 * Tracks how many bytes have been written in the packet
	 */
	DWORD NumBytes;

public:
	/**
	 * Inits the write tracking
	 */
	FNboSerializeToBuffer(DWORD Size) :
		NumBytes(0)
	{
		Data.Empty(Size);
		Data.AddZeroed(Size);
	}

	/**
	 * Cast operator to get at the formatted buffer data
	 */
	inline operator BYTE*(void) const
	{
		return (BYTE*)Data.GetData();
	}

	/**
	 * Cast operator to get at the formatted buffer data
	 */
	inline const TArray<BYTE>& GetBuffer(void) const
	{
		return Data;
	}

	/**
	 * Returns the size of the buffer we've built up
	 */
	inline DWORD GetByteCount(void) const
	{
		return NumBytes;
	}

	/**
	 * Returns the number of bytes preallocated in the array
	 */
	inline DWORD GetBufferSize(void) const
	{
		return (DWORD)Data.Num();
	}

	/**
	 * Adds a char to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const char Ch)
	{
		check(Ar.NumBytes + 1 <= Ar.GetBufferSize());
		Ar.Data(Ar.NumBytes++) = Ch;
		return Ar;
	}

	/**
	 * Adds a byte to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const BYTE& B)
	{
		check(Ar.NumBytes + 1 <= Ar.GetBufferSize());
		Ar.Data(Ar.NumBytes++) = B;
		return Ar;
	}

	/**
	 * Adds an INT to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const INT& I)
	{
		return Ar << *(DWORD*)&I;
	}

	/**
	 * Adds a DWORD to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const DWORD& D)
	{
		check(Ar.NumBytes + 4 <= Ar.GetBufferSize());
		Ar.Data(Ar.NumBytes + 0) = (D >> 24) & 0xFF;
		Ar.Data(Ar.NumBytes + 1) = (D >> 16) & 0xFF;
		Ar.Data(Ar.NumBytes + 2) = (D >> 8) & 0xFF;
		Ar.Data(Ar.NumBytes + 3) = D & 0xFF;
		Ar.NumBytes += 4;
		return Ar;
	}

	/**
	 * Adds a QWORD to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const QWORD& Q)
	{
		check(Ar.NumBytes + 8 <= Ar.GetBufferSize());
		Ar.Data(Ar.NumBytes + 0) = (Q >> 56) & 0xFF;
		Ar.Data(Ar.NumBytes + 1) = (Q >> 48) & 0xFF;
		Ar.Data(Ar.NumBytes + 2) = (Q >> 40) & 0xFF;
		Ar.Data(Ar.NumBytes + 3) = (Q >> 32) & 0xFF;
		Ar.Data(Ar.NumBytes + 4) = (Q >> 24) & 0xFF;
		Ar.Data(Ar.NumBytes + 5) = (Q >> 16) & 0xFF;
		Ar.Data(Ar.NumBytes + 6) = (Q >> 8) & 0xFF;
		Ar.Data(Ar.NumBytes + 7) = Q & 0xFF;
		Ar.NumBytes += 8;
		return Ar;
	}

	/**
	 * Adds a FLOAT to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FLOAT& F)
	{
		DWORD Value = *(DWORD*)&F;
		return Ar << Value;
	}

	/**
	 * Adds a DOUBLE to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const DOUBLE& Dbl)
	{
		QWORD Value = *(QWORD*)&Dbl;
		return Ar << Value;
	}

	/**
	 * Adds a FString to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FString& String)
	{
		// We send strings length prefixed
		INT Len = String.Len();
		Ar << Len;
		check(Ar.NumBytes + Len <= Ar.GetBufferSize());
		// Handle empty strings
		if (String.Len() > 0)
		{
			// memcpy it into the buffer
			appMemcpy(&Ar.Data(Ar.NumBytes),TCHAR_TO_ANSI(*String),Len);
			Ar.NumBytes += Len;
		}
		return Ar;
	}

	/**
	 * Adds a string to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const TCHAR* String)
	{
		// We send strings length prefixed
		INT Len = String != NULL ? appStrlen(String) : 0;
		Ar << Len;
		check(Ar.NumBytes + Len <= Ar.GetBufferSize());
		// Don't process if null
		if (String)
		{
			// memcpy it into the buffer
			appMemcpy(&Ar.Data(Ar.NumBytes),TCHAR_TO_ANSI(String),Len);
			Ar.NumBytes += Len;
		}
		return Ar;
	}

	/**
	 * Adds a substring to the buffer
	 */
	inline FNboSerializeToBuffer& AddString(const ANSICHAR* String,const INT Length)
	{
		// We send strings length prefixed
		INT Len = Length;
		(*this) << Len;
		check(NumBytes + Len <= GetBufferSize());
		// Don't process if null
		if (String)
		{
			// memcpy it into the buffer
			appMemcpy(&Data(NumBytes),String,Len);
			NumBytes += Len;
		}
		return *this;
	}

	/**
	 * Adds a string to the buffer
	 */
#if !TCHAR_IS_1_BYTE
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const ANSICHAR* String)
	{
		// We send strings length prefixed
		INT Len = String != NULL ? strlen(String) : 0;
		Ar << Len;
		check(Ar.NumBytes + Len <= Ar.GetBufferSize());
		// Don't process if null
		if (String)
		{
			// memcpy it into the buffer
			appMemcpy(&Ar.Data(Ar.NumBytes),String,Len);
			Ar.NumBytes += Len;
		}
		return Ar;
	}
#endif

	/**
	 * Adds an FSettingsData member to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FSettingsData& Data)
	{
		// Write the type
		Ar << Data.Type;
		// Now write out the held data
		switch (Data.Type)
		{
			case SDT_Float:
			{
				Ar << *(FLOAT*)&Data.Value1;
				break;
			}
			case SDT_Int32:
			{
				Ar << Data.Value1;
				break;
			}
			case SDT_Int64:
			{
				Ar << *(QWORD*)&Data.Value1;
				break;
			}
			case SDT_Double:
			{
				Ar << *(DOUBLE*)&Data.Value1;
				break;
			}
			case SDT_Blob:
			{
				// Write the length
				Ar << Data.Value1;
				BYTE* OutData = (BYTE*)Data.Value2;
				// Followed by each byte of data
				for (INT Index = 0; Index < Data.Value1; Index++)
				{
					Ar << OutData[Index];
				}
				break;
			}
			case SDT_String:
			{
				// This will write a length prefixed string
				Ar << (TCHAR*)Data.Value2;
				break;
			}
			case SDT_DateTime:
			{
				INT Val1, Val2;
				Data.GetData(Val1, Val2);
				Ar << Val1;
				Ar << Val2;
				break;
			}
		}
		return Ar;
	}

	/**
	 * Adds an FLocalizedStringSetting member to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FLocalizedStringSetting& Context)
	{
		Ar << Context.Id << Context.ValueIndex << Context.AdvertisementType;
		return Ar;
	}

	/**
	 * Adds an FSettingsProperty member to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FSettingsProperty& Property)
	{
		Ar << Property.PropertyId;
		Ar << Property.Data;
		Ar << Property.AdvertisementType;
		return Ar;
	}

	/**
	 * Writes an FOnlineProfileSetting member from the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FOnlineProfileSetting& Setting)
	{
		Ar << Setting.Owner;
		Ar << Setting.ProfileSetting;
		return Ar;
	}

	/**
	 * Writes an FUniqueNetId member to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FUniqueNetId& Id)
	{
		check(sizeof(FUniqueNetId) == sizeof(QWORD));
		Ar << (QWORD&)Id;
		return Ar;
	}

	/**
	 * Adds an ip adress to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FInternetIpAddr& Addr)
	{
		DWORD OutIp;
		Addr.GetIp(OutIp);
		Ar << OutIp;
		INT OutPort;
		Addr.GetPort(OutPort);
		Ar << OutPort;
		return Ar;
	}

	/**
	 * Adds a FGuid to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FGuid& Guid)
	{
		Ar << Guid.A;
		Ar << Guid.B;
		Ar << Guid.C;
		Ar << Guid.D;
		return Ar;
	}

	/**
	 * Writes a blob of data to the buffer
	 *
	 * @param Buffer the source data to append
	 * @param NumToWrite the size of the blob to write
	 */
	inline void WriteBinary(const BYTE* Buffer,DWORD NumToWrite)
	{
		check(NumBytes + NumToWrite <= GetBufferSize());
		appMemcpy(&Data(NumBytes),Buffer,NumToWrite);
		NumBytes += NumToWrite;
	}

	/**
	 * Gets the buffer at a specific point
	 */
	inline BYTE* GetRawBuffer(DWORD Offset)
	{
		return &Data(Offset);
	}

	/**
	 * Skips forward in the buffer by the specified amount
	 *
	 * @param Amount the number of bytes to skip ahead
	 */
	inline void SkipAheadBy(DWORD Amount)
	{
		check(NumBytes + Amount <= GetBufferSize());
		NumBytes += Amount;
	}
};

#pragma warning( push )
// Disable used without initialization warning because the reads are initializing
#pragma warning( disable : 4700 )

/**
 * Class used to read data from a NBO data buffer
 */
class FNboSerializeFromBuffer
{
protected:
	/** Pointer to the data this reader is attached to */
	const BYTE* Data;
	/** The size of the data in bytes */
	const INT NumBytes;
	/** The current location in the byte stream for reading */
	INT CurrentOffset;
	/** Indicates whether reading from the buffer caused an overflow or not */
	UBOOL bHasOverflowed;

	/** Hidden on purpose */
	FNboSerializeFromBuffer(void);

public:
	/**
	 * Initializes the buffer, size, and zeros the read offset
	 *
	 * @param InData the buffer to attach to
	 * @param Length the size of the buffer we are attaching to
	 */
	FNboSerializeFromBuffer(const BYTE* InData,INT Length) :
		Data(InData),
		NumBytes(Length),
		CurrentOffset(0),
		bHasOverflowed(FALSE)
	{
	}

	/**
	 * Reads an FSettingsData member from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FSettingsData& Data)
	{
		// Read the type
		Ar >> Data.Type;
		// Now read the held data
		switch (Data.Type)
		{
			case SDT_Float:
			{
				FLOAT Val = 0.f;
				Ar >> Val;
				Data.SetData(Val);
				break;
			}
			case SDT_Int32:
			{
				INT Val = 0;
				Ar >> Val;
				Data.SetData(Val);
				break;
			}
			case SDT_Int64:
			{
				QWORD Val = 0;
				Ar >> Val;
				Data.SetData(Val);
				break;
			}
			case SDT_Double:
			{
				DOUBLE Val = 0.0;
				Ar >> Val;
				Data.SetData(Val);
				break;
			}
			case SDT_Blob:
			{
				INT Length = 0;
				// Read the length
				Ar >> Length;
				if (Ar.CurrentOffset + Length <= Ar.NumBytes)
				{
					// Now directly copy the blob data
					Data.SetData(Length,&Ar.Data[Ar.CurrentOffset]);
					Ar.CurrentOffset += Length;
				}
				else
				{
					Ar.bHasOverflowed = TRUE;
				}
				break;
			}
			case SDT_String:
			{
				FString Val;
				Ar >> Val;
				Data.SetData(Val);
				break;
			}
			case SDT_DateTime:
			{
				INT Val1 = 0, Val2 = 0;
				Ar >> Val1;
				Ar >> Val2;
				Data.SetData(Val1, Val2);
				break;
			}
		}
		return Ar;
	}

	/**
	 * Reads an FLocalizedStringSetting member from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FLocalizedStringSetting& Context)
	{
		Ar >> Context.Id >> Context.ValueIndex >> Context.AdvertisementType;
		return Ar;
	}

	/**
	 * Reads an FSettingsProperty member from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FSettingsProperty& Property)
	{
		Ar >> Property.PropertyId;
		Ar >> Property.Data;
		Ar >> Property.AdvertisementType;
		return Ar;
	}

	/**
	 * Reads an FOnlineProfileSetting member from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FOnlineProfileSetting& Setting)
	{
		Ar >> Setting.Owner;
		Ar >> Setting.ProfileSetting;
		return Ar;
	}

	/**
	 * Reads a char from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,char& Ch)
	{
		if (Ar.CurrentOffset + 1 <= Ar.NumBytes)
		{
			Ch = Ar.Data[Ar.CurrentOffset++];
		}
		else
		{
			Ar.bHasOverflowed = TRUE;
		}
		return Ar;
	}

	/**
	 * Reads a byte from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,BYTE& B)
	{
		if (Ar.CurrentOffset + 1 <= Ar.NumBytes)
		{
			B = Ar.Data[Ar.CurrentOffset++];
		}
		else
		{
			Ar.bHasOverflowed = TRUE;
		}
		return Ar;
	}

	/**
	 * Reads an INT from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,INT& I)
	{
		return Ar >> *(DWORD*)&I;
	}

	/**
	 * Reads a DWORD from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,DWORD& D)
	{
		if (Ar.CurrentOffset + 4 <= Ar.NumBytes)
		{
			DWORD D1 = Ar.Data[Ar.CurrentOffset + 0];
			DWORD D2 = Ar.Data[Ar.CurrentOffset + 1];
			DWORD D3 = Ar.Data[Ar.CurrentOffset + 2];
			DWORD D4 = Ar.Data[Ar.CurrentOffset + 3];
			D = D1 << 24 | D2 << 16 | D3 << 8 | D4;
			Ar.CurrentOffset += 4;
		}
		else
		{
			Ar.bHasOverflowed = TRUE;
		}
		return Ar;
	}

	/**
	 * Adds a QWORD to the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,QWORD& Q)
	{
		if (Ar.CurrentOffset + 8 <= Ar.NumBytes)
		{
			Q = ((QWORD)Ar.Data[Ar.CurrentOffset + 0] << 56) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 1] << 48) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 2] << 40) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 3] << 32) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 4] << 24) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 5] << 16) |
				((QWORD)Ar.Data[Ar.CurrentOffset + 6] << 8) |
				(QWORD)Ar.Data[Ar.CurrentOffset + 7];
			Ar.CurrentOffset += 8;
		}
		else
		{
			Ar.bHasOverflowed = TRUE;
		}
		return Ar;
	}

	/**
	 * Reads a FLOAT from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FLOAT& F)
	{
		return Ar >> *(DWORD*)&F;
	}

	/**
	 * Adds a DOUBLE to the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,DOUBLE& Dbl)
	{
		return Ar >> *(QWORD*)&Dbl;
	}

	/**
	 * Reads a FString from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FString& String)
	{
		// We send strings length prefixed
		INT Len = 0;
		Ar >> Len;
		if (Ar.CurrentOffset + Len <= Ar.NumBytes)
		{
			// Handle strings of zero length
			if (Len > 0)
			{
				char* Temp = (char*)appAlloca(Len + 1);
				// memcpy it infrom the buffer
				appMemcpy(Temp,&Ar.Data[Ar.CurrentOffset],Len);
				Ar.CurrentOffset += Len;
				// Terminate and copy
				Temp[Len] = '\0';
				String = Temp;
			}
			else
			{
				String.Empty();
			}
		}
		else
		{
			Ar.bHasOverflowed = TRUE;
		}
		return Ar;
	}

	/**
	 * Reads an FUniqueNetId member from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FUniqueNetId& Id)
	{
		checkSlow(sizeof(FUniqueNetId) == sizeof(QWORD));
		Ar >> (QWORD&)Id;
		return Ar;
	}

	/**
	 * Reads an ip adress from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FInternetIpAddr& Addr)
	{
		DWORD InIp = 0;
		Ar >> InIp;
		Addr.SetIp(InIp);
		INT InPort = 0;
		Ar >> InPort;
		Addr.SetPort(InPort);
		return Ar;
	}

	/**
	 * Reads a FGuid from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FGuid& Guid)
	{
		Ar >> Guid.A;
		Ar >> Guid.B;
		Ar >> Guid.C;
		Ar >> Guid.D;
		return Ar;
	}

	/**
	 * Reads a blob of data from the buffer
	 *
	 * @param OutBuffer the destination buffer
	 * @param NumToRead the size of the blob to read
	 */
	void ReadBinary(BYTE* OutBuffer,DWORD NumToRead)
	{
		if (CurrentOffset + (INT)NumToRead <= NumBytes)
		{
			appMemcpy(OutBuffer,&Data[CurrentOffset],NumToRead);
			CurrentOffset += NumToRead;
		}
		else
		{
			bHasOverflowed = TRUE;
		}
	}

	/**
	 * Seek to the desired position in the buffer
	 *
	 * @param Pos the offset from the start of the buffer
	 */
	void Seek(INT Pos)
	{
		checkSlow(Pos >= 0);
		if (Pos < NumBytes)
		{
			CurrentOffset = Pos;
		}
		else
		{
			bHasOverflowed = TRUE;
		}
	}

	/** @return Current position of the buffer being to be read */
	inline INT Tell(void) const
	{
		return CurrentOffset;
	}

	/** Returns whether the buffer had an overflow when reading from it */
	inline UBOOL HasOverflow(void) const
	{
		return bHasOverflowed;
	}

	/** @return Number of bytes remaining to read from the current offset to the end of the buffer */
	inline INT AvailableToRead(void) const
	{
		return Max<INT>(0,NumBytes - CurrentOffset);
	}

	/**
	 * Returns the number of total bytes in the buffer
	 */
	inline INT GetBufferSize(void) const
	{
		return NumBytes;
	}
};

#pragma warning( pop )

/** Base class for serializing profile data */
class FProfileSettingsStorageBase
{
protected:
	/** The max size a profile can be when saved/read */
	const DWORD MaxProfileSize;
	/** The max size a profile can be uncompressed */
	const DWORD MaxUncompressedProfileSize;
	/** The amount of space used by the hash */
	static const DWORD HashLength = 20;
	/** Whether to enable digital signatures on the stored data */
	UBOOL bUseDigitalSignature;

	/** Hidden on purpose */
	FProfileSettingsStorageBase(void);

public:
	/**
	 * Ctor that copies the signature flag
	 *
	 * @param InMaxProfileSize the max size to allow when writing to the profile buffer
	 * @param bInUseDigitalSignature whether to sign the data or not
	 */
	FProfileSettingsStorageBase(DWORD InMaxProfileSize,UBOOL bInUseDigitalSignature) :
		MaxProfileSize(InMaxProfileSize),
		MaxUncompressedProfileSize(InMaxProfileSize * 2),
		bUseDigitalSignature(bInUseDigitalSignature)
	{
	}
};

/** Handles writing a set of profile data to a buffer */
class FProfileSettingsWriter :
	public FProfileSettingsStorageBase
{
protected:
	/** The final buffer where the data will reside */
	FNboSerializeToBuffer FinalBuffer;

	/** Hidden on purpose */
	FProfileSettingsWriter(void);

public:
	/**
	 * Initializes the profile buffer with the max size allowed
	 *
	 * @param InMaxProfileSize the max size to allow when writing to the profile buffer
	 * @param bInUseDigitalSignature whether to sign the data or not
	 */
	FProfileSettingsWriter(DWORD InMaxProfileSize,UBOOL bInUseDigitalSignature) :
		FProfileSettingsStorageBase(InMaxProfileSize,bInUseDigitalSignature),
		FinalBuffer(InMaxProfileSize)
	{
	}

	/**
	 * Serializes the profile data to the buffer. The data is written to a
	 * temp buffer, compressed, signed, and then copied into the final buffer.
	 *
	 * @param ProfileSettings the settings to write to the buffer
	 */
	UBOOL SerializeToBuffer(TArray<FOnlineProfileSetting>& ProfileSettings)
	{
		// Allow the temp buffer to be larger than our final because compression
		// will shrink it down
		FNboSerializeToBuffer TempBuffer(MaxUncompressedProfileSize);
		// Write the profile data to the temp buffer
		INT NumSettings = ProfileSettings.Num();
		TempBuffer << NumSettings;
		// Write all of the profile data into the buffer
		for (INT Index = 0; Index < NumSettings; Index++)
		{
			TempBuffer << ProfileSettings(Index);
		}
		// Store the uncompressed size
		DWORD UncompressedSize = TempBuffer.GetByteCount();
		// Reserve space for the hash in the final buffer
		if (bUseDigitalSignature)
		{
			FinalBuffer.SkipAheadBy(HashLength);
		}
		// Write the uncompressed size
		FinalBuffer << UncompressedSize;
		INT CompressedSize = FinalBuffer.GetBufferSize() - FinalBuffer.GetByteCount();
		check(TempBuffer.GetByteCount() <= MaxUncompressedProfileSize);
#if !_XBOX
		ECompressionFlags CompressType = GBaseCompressionMethod;
#else
		// LZX isn't robust so always use LZO on Xbox
		ECompressionFlags CompressType = COMPRESS_LZO;
#endif
		// Compress the resulting data
		UBOOL bSuccess = appCompressMemory(CompressType,
			FinalBuffer.GetRawBuffer(FinalBuffer.GetByteCount()),
			CompressedSize,
			TempBuffer.GetRawBuffer(0),
			TempBuffer.GetByteCount());
		// Move to the end of the compressed data
		FinalBuffer.SkipAheadBy(CompressedSize);
		if (bSuccess && bUseDigitalSignature)
		{
			// Sign the buffer
			HashBuffer();
		}
		return bSuccess;
	}

	/**
	 * Returns the raw pointer to the buffer
	 */
	inline const BYTE* GetFinalBuffer(void)
	{
		return FinalBuffer.GetRawBuffer(0);
	}

	/**
	 * Returns the buffer size of the final buffer (after compression/signature)
	 */
	inline DWORD GetFinalBufferLength(void)
	{
		return FinalBuffer.GetByteCount();
	}

	/**
	 * Method for signing the buffer that can be overloaded on a per platform basis
	 * The default is to use a SHA1 on the whole buffer
	 */
	virtual void HashBuffer(void)
	{
		// Now sign the data
		FSHA1::HashBuffer(FinalBuffer.GetRawBuffer(HashLength),
			FinalBuffer.GetByteCount() - HashLength,
			FinalBuffer.GetRawBuffer(0));
	}
};

/** Handles reading a set of profile data from a buffer */
class FProfileSettingsReader :
	public FProfileSettingsStorageBase
{
protected:
	/** Allow for extra space to uncompress */
	BYTE* UncompressedBuffer;
	/** The compressed raw buffer of data */
	const BYTE* Data;
	/** The size of the raw buffer in bytes */
	const DWORD NumBytes;

public:
	/**
	 * Sets the base class information for reading from the buffer
	 *
	 * @param InMaxProfileSize the max size to allow when writing to the profile buffer
	 * @param bInUseDigitalSignature whether to sign the data or not
	 * @param InData the buffer holding the profile data to read
	 * @param InNumBytes the number of bytes in the buffer
	 */
	FProfileSettingsReader(DWORD InMaxProfileSize,UBOOL bInUseDigitalSignature,const BYTE* InData,DWORD InNumBytes) :
		FProfileSettingsStorageBase(InMaxProfileSize,bInUseDigitalSignature),
		UncompressedBuffer(NULL),
		Data(InData),
		NumBytes(InNumBytes)
	{
		UncompressedBuffer = new BYTE[MaxUncompressedProfileSize];
		if (bUseDigitalSignature)
		{
			check(NumBytes >= HashLength && "Buffer is too small for the hash signature");
		}
	}

	/** Clean up the uncompress buffer */
	virtual ~FProfileSettingsReader()
	{
		delete [] UncompressedBuffer;
	}

	/**
	 * Serializes the profile data from the buffer to the object. The data is 
	 * checked for digital signature, decompressed, and then serialized
	 *
	 * @param ProfileSettings the out array that is populated with the settings
	 */
	UBOOL SerializeFromBuffer(TArray<FOnlineProfileSetting>& ProfileSettings)
	{
		UBOOL bSuccess = TRUE;
		DWORD UncompressedSize = 0;
		const BYTE* CompressedBufferStart = 0;
		DWORD CompressedBufferLen = 0;
		// Check the digital signature if requested
		if (bUseDigitalSignature)
		{
			BYTE TempHash[HashLength];
			// Sign the buffer and verify the results
			HashBuffer(TempHash);
			bSuccess = appMemcmp(Data,TempHash,HashLength) == 0;
			// Pull the size from the buffer
			UncompressedSize = Data[HashLength + 0] << 24 |
				Data[HashLength + 1] << 16 |
				Data[HashLength + 2] << 8 |
				Data[HashLength + 3];
			// Handle the compression buffer after the hash
			CompressedBufferStart = &Data[HashLength + sizeof(DWORD)];
			CompressedBufferLen = NumBytes - HashLength - sizeof(DWORD);
		}
		else
		{
			// Pull the size from the buffer
			UncompressedSize = Data[0] << 24 |
				Data[1] << 16 |
				Data[2] << 8 |
				Data[3];
			// There is no hash so the compression buffer is write after the length
			CompressedBufferStart = &Data[sizeof(DWORD)];
			CompressedBufferLen = NumBytes - sizeof(DWORD);
		}

		// Make sure the amount of data we recorded in the file is under the limit we impose
		if (UncompressedSize > MaxUncompressedProfileSize)
		{
			debugf(TEXT("Uncompressed size of profile too large for buffer, tossing it..."));
			bSuccess = FALSE;
		}

		// If the hash matches, then decompress
		if (bSuccess)
		{
#if PS3
			// verify that the profile was compressed with ZLIB
			if (CompressedBufferStart[0] != 0x78)
			{
				debugf(TEXT("Found non-zlib profile, tossing it..."));
				bSuccess = FALSE;
			}
			else
#endif
			{
#if !_XBOX
				ECompressionFlags CompressType = GBaseCompressionMethod;
#else
				// LZX isn't robust so always use LZO on Xbox
				ECompressionFlags CompressType = COMPRESS_LZO;
#endif
				// Decompress the resulting data
				bSuccess = appUncompressMemory(CompressType,
					UncompressedBuffer,
					UncompressedSize,
					(void*)CompressedBufferStart,
					CompressedBufferLen);
				// If everything decompressed fine, then we can serialize
				if (bSuccess)
				{
					// Attach a reader to the uncompressed raw buffer
					FNboSerializeFromBuffer Reader(UncompressedBuffer,UncompressedSize);
					INT NumProfileSettings = 0;
					Reader >> NumProfileSettings;
					// Presize the array
					ProfileSettings.Empty(NumProfileSettings);
					ProfileSettings.AddZeroed(NumProfileSettings);
					// Read all the settings we stored in the buffer
					for (INT Index = 0;
						Index < NumProfileSettings && Reader.HasOverflow() == FALSE;
						Index++)
					{
						Reader >> ProfileSettings(Index);
					}
					// If the reader overflowed, then the data is corrupt
					if (Reader.HasOverflow() == TRUE)
					{
						bSuccess = FALSE;
					}
				}
			}
		}
		return bSuccess;
	}

	/**
	 * Method for signing the buffer that can be overloaded on a per platform basis
	 * The default is to use a SHA1 on the whole buffer
	 *
	 * @param TempHash the buffer to write the signature into
	 */
	virtual void HashBuffer(BYTE* TempHash)
	{
		// Calculate the digital signature
		FSHA1::HashBuffer((void*)&Data[HashLength],NumBytes - HashLength,TempHash);
	}
};

/**
 * This value indicates which packet version the server is sending. Clients with
 * differing versions will ignore these packets. This prevents crashing when
 * changing the packet format and there are existing servers on the network
 * Current format:
 *
 *	<Ver byte><Platform byte><Game unique 4 bytes><packet type 2 bytes><nonce 8 bytes><payload>
 */
#define LAN_BEACON_PACKET_VERSION (BYTE)7

/** The size of the header for validation */
#define LAN_BEACON_PACKET_HEADER_SIZE 16

/** The max size expected in a lan beacon packet */
#define LAN_BEACON_MAX_PACKET_SIZE 512

// Offsets for various fields
#define LAN_BEACON_VER_OFFSET 0
#define LAN_BEACON_PLATFORM_OFFSET 1
#define LAN_BEACON_GAMEID_OFFSET 2
#define LAN_BEACON_PACKETTYPE1_OFFSET 6
#define LAN_BEACON_PACKETTYPE2_OFFSET 7
#define LAN_BEACON_NONCE_OFFSET 8

// Packet types in 2 byte readable form
#define LAN_SERVER_QUERY1 (BYTE)'S'
#define LAN_SERVER_QUERY2 (BYTE)'Q'

#define LAN_SERVER_RESPONSE1 (BYTE)'S'
#define LAN_SERVER_RESPONSE2 (BYTE)'R'


#define DEBUG_LAN_BEACON 0

#if DEBUG_LAN_BEACON
/**
 * Used to build string information of the contexts being placed in a lan beacon
 * packet. Builds the name and value in a human readable form
 *
 * @param Settings the settings object that has the metadata
 * @param Context the context object to build the string for
 */
inline FString BuildContextString(USettings* Settings,const FLocalizedStringSetting& Context)
{
	FName ContextName = Settings->GetStringSettingName(Context.Id);
	FName Value = Settings->GetStringSettingValueName(Context.Id,Context.ValueIndex);
	return FString::Printf(TEXT("Context '%s' = %s"),*ContextName.ToString(),*Value.ToString());
}

/**
 * Used to build string information of the properties being placed in a lan beacon
 * packet. Builds the name and value in a human readable form
 *
 * @param Settings the settings object that has the metadata
 */
inline FString BuildPropertyString(USettings* Settings,const FSettingsProperty& Property)
{
	FName PropName = Settings->GetPropertyName(Property.PropertyId);
	FString Value = Settings->GetPropertyAsString(Property.PropertyId);
	return FString::Printf(TEXT("Property '%s' = %s"),*PropName.ToString(),*Value);
}
#endif

/**
 * Class responsible for sending/receiving UDP broadcasts for lan match
 * discovery
 */
class FLanBeacon
{
	/** Builds the broadcast address and caches it */
	FInternetIpAddr BroadcastAddr;
	/** The socket to listen for requests on */
	FSocket* ListenSocket;
	/** The address in bound requests come in on */
	FInternetIpAddr ListenAddr;

public:
	/** Sets the broadcast address for this object */
	inline FLanBeacon(void) :
		ListenSocket(NULL)
	{
	}

	/** Frees the broadcast socket */
	~FLanBeacon(void)
	{
		delete ListenSocket;
	}

	/** Return TRUE if there is a valid ListenSocket */
	UBOOL IsListenSocketValid() const
	{
		return (ListenSocket ? TRUE : FALSE);
	}

	/**
	 * Initializes the socket
	 *
	 * @param Port the port to listen on
	 *
	 * @return TRUE if both socket was created successfully, FALSE otherwise
	 */
	UBOOL Init(INT Port)
	{
		UBOOL bSuccess = FALSE;
		// Set our broadcast address
		BroadcastAddr.SetIp(INADDR_BROADCAST);
		BroadcastAddr.SetPort(Port);
		// Now the listen address
		ListenAddr.SetPort(Port);
		ListenAddr.SetIp(getlocalbindaddr(*GWarn));
		// Now create and set up our sockets (no VDP)
		ListenSocket = GSocketSubsystem->CreateDGramSocket(TEXT("LAN beacon"),TRUE);
		if (ListenSocket != NULL)
		{
			ListenSocket->SetReuseAddr();
			ListenSocket->SetNonBlocking();
			ListenSocket->SetRecvErr();
			// Bind to our listen port
			if (ListenSocket->Bind(ListenAddr))
			{
				// Set it to broadcast mode, so we can send on it
				// NOTE: You must set this to broadcast mode on Xbox 360 or the
				// secure layer will eat any packets sent
				bSuccess = ListenSocket->SetBroadcast();
			}
			else
			{
				debugf(NAME_Error,TEXT("Failed to bind listen socket to addr (%s) for lan beacon"),
					*ListenAddr.ToString(TRUE));
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to create listen socket for lan beacon"));
		}
		return bSuccess && ListenSocket;
	}

	/**
	 * Called to poll the socket for pending data. Any data received is placed
	 * in the specified packet buffer
	 *
	 * @param PacketData the buffer to get the socket's packet data
	 * @param BufferSize the size of the packet buffer
	 *
	 * @return the number of bytes read (0 if none or an error)
	 */
	INT ReceivePacket(BYTE* PacketData,INT BufferSize)
	{
		check(PacketData && BufferSize);
		// Default to no data being read
		INT BytesRead = 0;
		if (ListenSocket != NULL)
		{
			FInternetIpAddr SockAddr;
			// Read from the socket
			ListenSocket->RecvFrom(PacketData,BufferSize,BytesRead,SockAddr);
		}
		return BytesRead;
	}

	/**
	 * Uses the cached broadcast address to send packet to a subnet
	 *
	 * @param Packet the packet to send
	 * @param Length the size of the packet to send
	 */
	inline UBOOL BroadcastPacket(BYTE* Packet,INT Length)
	{
		INT BytesSent = 0;
		return ListenSocket->SendTo(Packet,Length,BytesSent,BroadcastAddr) && BytesSent == Length;
	}
};

/**
 * Struct that holds the session information needed to connect to this server
 */
struct FSessionInfo
{
	/** The ip & port that the host is listening on */
	FInternetIpAddr HostAddr;

	/** Default ctor that initializes to local settings */
	inline FSessionInfo(void)
	{
		if (GSocketSubsystem)
		{
			// Read the IP from the system
			GSocketSubsystem->GetLocalHostAddr(*GLog,HostAddr);
			// Now set the port that was configured
			HostAddr.SetPort(FURL::DefaultPort);
		}
	}

	/** Ctor that doesn't inialize its members */
	inline FSessionInfo(ENoInit)
	{
	}

	/** Overridden per platform */
	virtual ~FSessionInfo(void)
	{
	}
};


/**
 * Common auth interface structs/functions/globals
 */

/**
 * Stores platform-specific auth data being transmitted over a net connection
 */
struct FAuthTicketData
{
	TArray<FString>	IncomingBlobs;		// Partial (split up) auth ticket data being received over a net connection

	TArray<BYTE>	FinalAuthTicket;	// Once all blobs are received, they're converted back to raw data and the complete ticket is stored here
	UBOOL		bComplete;		// whether or not all ticket data has been received and stored in FinalAuthTicket

	FAuthTicketData()
		: IncomingBlobs()
		, FinalAuthTicket()
		, bComplete(FALSE)
	{
	}
};

/** Maps available/incoming auth ticket data to a UID; auth tickets are referenced by UID, so UScript does not have access to raw ticket data */
extern TMap<DWORD, FAuthTicketData> AuthTicketMap;

/** When a new auth ticket UID is needed, this value is used and then incremented */
extern DWORD NextAuthTicketUID/*=1*/;


/**
 * Creates an empty auth ticket, returns it, and also outputs its UID
 *
 * @param OutAuthTicketUID	Outputs the new auth tickets UID
 * @return			The new auth ticket
 */
static FORCEINLINE FAuthTicketData* CreateAuthTicket(INT& OutAuthTicketUID)
{
	OutAuthTicketUID = NextAuthTicketUID++;
	return &AuthTicketMap.Set(OutAuthTicketUID, FAuthTicketData());
}

/**
 * Retrieves auth ticket data, based upon an auth session
 *
 * @param AuthSession		The auth session who's auth ticket we want to retrieve
 * @param bCreate		If true, creates an auth ticket entry if it was not found
 * @return			The auth ticket data associated with this UID, or NULL
 */
static FORCEINLINE FAuthTicketData* GetAuthTicket(FAuthSession* AuthSession, UBOOL bCreate=FALSE)
{
	FAuthTicketData* FoundData = AuthTicketMap.Find(AuthSession->AuthTicketUID);

	if (FoundData == NULL && bCreate)
	{
		FoundData = CreateAuthTicket(AuthSession->AuthTicketUID);
	}

	return FoundData;
}


#endif	//#if WITH_UE3_NETWORKING

/**
 * Builds the tabbing for an XML string
 * 
 * @param IndentCount the number of tabs to indent
 *
 * @return the string of tabs to use
 */
inline FString BuildIndentString(DWORD IndentCount)
{
	FString Indent;
	for (DWORD Count = 0; Count < IndentCount; Count++)
	{
		Indent += TEXT('\t');
	}
	return Indent;
}

/**
 * Builds an XML string out of an individual property
 *
 * @param OutXmlString the out string
 * @param Property the list of properties to serialize
 * @param IndentCount the number of indents to use
 */
inline void ToXml(FString& OutXmlString,const FSettingsProperty& Property,const FString& InIndent)
{
	// Don't serialize empty properties
	if (Property.Data.Type != SDT_Empty)
	{
		FString Indent(InIndent);
		Indent += TEXT('\t');
		// Create an xml entry from this property
		OutXmlString += FString::Printf(TEXT("%s<%s id=\"%d\" value=\"%s\"/>\r\n"),
			*Indent,
			Property.Data.GetTypeString(),
			Property.PropertyId,
			*Property.Data.ToString());
	}
}

/**
 * Builds an XML string out of an individual property
 *
 * @param OutXmlString the out string
 * @param Property the list of properties to serialize
 * @param Mapping the name of the property based upon the mapping data
 * @param IndentCount the number of indents to use
 */
inline void ToXml(FString& OutXmlString,const FSettingsProperty& Property,const FName Mapping,const FString& InIndent)
{
	// Don't serialize empty properties
	if (Property.Data.Type != SDT_Empty)
	{
		FString Indent(InIndent);
		Indent += TEXT('\t');
		// Create an xml entry from this property
		OutXmlString += FString::Printf(TEXT("%s<%s id=\"%s\" value=\"%s\"/>\r\n"),
			*Indent,
			Property.Data.GetTypeString(),
			*Mapping.ToString(),
			*Property.Data.ToString());
	}
}

/**
 * Builds an XML string out of an array of properties
 *
 * @param OutXmlString the out string
 * @param Properties the list of properties to serialize
 * @param IndentCount the number of indents to use
 */
inline void ToXml(FString& OutXmlString,const TArray<FSettingsProperty>& Properties,DWORD IndentCount = 0)
{
	FString Indent = BuildIndentString(IndentCount);
	OutXmlString += Indent;
	OutXmlString += TEXT("<SettingsProperties>\r\n");
	// Append each property
	for (INT PropIndex = 0; PropIndex < Properties.Num(); PropIndex++)
	{
		ToXml(OutXmlString,Properties(PropIndex),Indent);
	}
	OutXmlString += Indent;
	OutXmlString += TEXT("</SettingsProperties>\r\n");
}

/**
 * Builds an XML string out of an array of profile settings
 *
 * @param OutXmlString the out string
 * @param Settings the list of profile settings to serialize
 * @param IndentCount the number of indents to use
 */
inline void ToXml(FString& OutXmlString,UOnlinePlayerStorage* Settings,DWORD IndentCount = 0)
{
	FString Indent = BuildIndentString(IndentCount);
	OutXmlString += Indent;
	OutXmlString += TEXT("<ProfileSettings>\r\n");
	const TArray<FOnlineProfileSetting>& ProfSettings = Settings->ProfileSettings;
	// Append each property
	for (INT SettingsIndex = 0; SettingsIndex < ProfSettings.Num(); SettingsIndex++)
	{
		const FName Mapping = Settings->GetProfileSettingName(ProfSettings(SettingsIndex).ProfileSetting.PropertyId);
		if (Mapping != NAME_None)
		{
			ToXml(OutXmlString,ProfSettings(SettingsIndex).ProfileSetting,Mapping,Indent);
		}
		else
		{
			ToXml(OutXmlString,ProfSettings(SettingsIndex).ProfileSetting,Indent);
		}
	}
	OutXmlString += Indent;
	OutXmlString += TEXT("</ProfileSettings>\r\n");
}

#endif //__ONLINESUBSYSTEMUTILITIES_H__


