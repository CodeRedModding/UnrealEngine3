/*=============================================================================
	UnIOBase.h: General-purpose file utilities.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNIOBASE_H__
#define __UNIOBASE_H__

/*-----------------------------------------------------------------------------
	FIOManager.
-----------------------------------------------------------------------------*/

enum EIOSystemTag
{
	IOSYSTEM_GenericAsync,
	IOSYSTEM_LastEpicEntry,
};

/**
 * This class servers as a collection of loaders requests get routed through.
 */
struct FIOManager
{
	/**
	 * Constructor, associating self with GIOManager.
	 */
	FIOManager();

	/**
	 * Destructor, removing association with GIOManager and deleting IO systems.
	 */
	virtual ~FIOManager();

	/**
	 * Flushes the IO manager. This means all outstanding IO requests will be fulfilled and
	 * file handles will be closed. This is mainly to ensure that certain operations like 
	 * saving a package can rely on there not being any open file handles.
	 */
	void Flush();

	/**
	 * Returns the IO system matching the passed in tag.
	 *
	 * @return	FIOSystem matching the passed in tag.
	 */
	virtual struct FIOSystem* GetIOSystem( DWORD IOSystemTag = IOSYSTEM_GenericAsync );

protected:
	/** FIOSystems register themselves, so need access to the array.	*/
	friend struct FIOSystem;

	/** Array of managers requests get routed to.						*/
	TArray<FIOSystem*> IOSystems;
};

/**
 * Enum for async IO priorities.
 */
enum EAsyncIOPriority
{
	AIOP_MIN	= 0,
	AIOP_Low,
	AIOP_BelowNormal,
	AIOP_Normal,
	AIOP_High,
	AIOP_MAX
};

/*-----------------------------------------------------------------------------
	FIOSystem.
-----------------------------------------------------------------------------*/

/**
 * Virtual base class of IO systems.
 */
struct FIOSystem
{
	/**
	 * Constructor, registers self with IOManager.
	 */
	FIOSystem()
	{
		check( GIOManager );
		GIOManager->IOSystems.AddItem(this);
	}

	/**
	 * Destructor, removes self from IOManager 
	 */
	virtual ~FIOSystem()
	{
		check( GIOManager );
		GIOManager->IOSystems.RemoveItem(this);
	}
	
	/** 
	 * Returns the unique tag associated with this IO system.
	 * 
	 * @return the unique tag associated with this IO system.
	 */
	virtual DWORD GetTag() = 0;

	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	Filename	Filename to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished, can be NULL
	 * @param	Priority	Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadData( 
		const FString& Filename, 
		INT Offset, 
		INT Size, 
		void* Dest, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) = 0;

	/**
	 * Requests compressed data to be loaded async. Returns immediately.
	 *
	 * @param	Filename			Filename to load
	 * @param	Offset				Offset into file
	 * @param	Size				Size of load request
	 * @param	UncompressedSize	Size of uncompressed data
	 * @param	Dest				Pointer to load data into
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter to decrement when loading has finished, can be NULL
	 * @param	Priority			Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadCompressedData( 
		const FString& Filename, 
		INT Offset, 
		INT Size, 
		INT UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) = 0;

	/**
	 * Removes N outstanding requests from the queue and returns how many were canceled. We can't cancel
	 * requests currently fulfilled and ones that have already been fulfilled.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	The number of requests that were canceled
	 */
	virtual INT CancelRequests( QWORD* RequestIndices, INT NumIndices ) = 0;

	/**
	 * Removes all outstanding requests from the queue
	 */
	virtual void CancelAllOutstandingRequests() = 0;

	/**
	 * Blocks till all requests are finished and also flushes potentially open handles.
	 */
	virtual void BlockTillAllRequestsFinishedAndFlushHandles() = 0;

	/**
	 * Suspend any IO operations (can be called from another thread)
	 */
	virtual void Suspend() = 0;

	/**
	 * Resume IO operations (can be called from another thread)
	 */
	virtual void Resume() = 0;

	/**
	 * Sets the min priority of requests to fulfill. Lower priority requests will still be queued and
	 * start to be fulfilled once the min priority is lowered sufficiently. This is only a hint and
	 * implementations are free to ignore it.
	 *
	 * @param	MinPriority		Min priority of requests to fulfill
	 */
	virtual void SetMinPriority( EAsyncIOPriority MinPriority ) = 0;


	/**
	 * Give the IO system a hint that it is done with the file for now
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void HintDoneWithFile(const FString& Filename) = 0;

};

/** Helper structure encapsulating file and stats handle. */
struct FAsyncIOHandle
{
	/** Default constructor, initializing all member variables. */
	FAsyncIOHandle()
	:	Handle( (void*) INDEX_NONE )
	,	StatsHandle( 0 )
	,	PlatformSortKey( 0 )
	{}

	/** File handle. */
	void*	Handle;
	/** Handle for tracking I/O requests with I/O stats system. */
	INT		StatsHandle;
	/** Optional space for the platform implementation to store a sort key for it's own private use */
	INT		PlatformSortKey;

#if ANDROID
	/** On Android we need to store an offset into the package file as well */
	SQWORD	FileOffset;
#endif
};

/**
 * Base implementation of an async IO system allowing most of the code to be shared across platforms.
 */
struct FAsyncIOSystemBase : public FIOSystem, FRunnable
{
	// FAsyncIOSystem interface.

	/** 
	 * Returns the unique tag associated with this IO system.
	 * 
	 * @return the unique tag associated with this IO system.
	 */
	virtual DWORD GetTag() { return IOSYSTEM_GenericAsync; }

	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	FileName	File name to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished
	 * @param	Priority	Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadData( 
		const FString& FileName, 
		INT Offset, 
		INT Size, 
		void* Dest, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority );

	/**
	 * Requests compressed data to be loaded async. Returns immediately.
	 *
	 * @param	FileName			File name to load
	 * @param	Offset				Offset into file
	 * @param	Size				Size of load request
	 * @param	UncompressedSize	Size of uncompressed data
	 * @param	Dest				Pointer to load data into
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter to decrement when loading has finished, can be NULL
	 * @param	Priority			Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadCompressedData( 
		const FString& FileName, 
		INT Offset, 
		INT Size, 
		INT UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority );

	/**
	 * Removes N outstanding requests from the queue and returns how many were canceled. We can't cancel
	 * requests currently fulfilled and ones that have already been fulfilled.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	The number of requests that were canceled
	 */
	virtual INT CancelRequests( QWORD* RequestIndices, INT NumIndices );
	
	/**
	 * Removes all outstanding requests from the queue
	 */
	virtual void CancelAllOutstandingRequests();
	
	/**
	 * Blocks till all requests are finished and also flushes potentially open handles.
	 */
	virtual void BlockTillAllRequestsFinishedAndFlushHandles();

	// FRunnable interface.

	/**
	 * Initializes critical section, event and other used variables. 
	 *
	 * This is called in the context of the thread object that aggregates 
	 * this, not the thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual UBOOL Init();

	/**
	 * Called in the context of the aggregating thread to perform cleanup.
	 */
	virtual void Exit();

	/**
	 * This is where all the actual loading is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return always 0
	 */
	virtual DWORD Run();

	/**
	 * This is called if a thread is requested to terminate early.
	 */
	virtual void Stop();
	
	/**
	 * This is called if a thread is requested to suspend its' IO activity
	 */
	virtual void Suspend();

	/**
	 * This is called if a thread is requested to resume its' IO activity
	 */
	virtual void Resume();

	/**
	 * Sets the min priority of requests to fulfill. Lower priority requests will still be queued and
	 * start to be fulfilled once the min priority is lowered sufficiently. This is only a hint and
	 * implementations are free to ignore it.
	 *
	 * @param	MinPriority		Min priority of requests to fulfill
	 */
	virtual void SetMinPriority( EAsyncIOPriority MinPriority );

	/**
	 * Give the IO system a hint that it is done with the file for now
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void HintDoneWithFile(const FString& Filename);

protected:

	/**
	 * Helper structure encapsulating all required cached data for an async IO request.
	 */
	struct FAsyncIORequest
	{
		/** Index of request.																		*/
		QWORD				RequestIndex;
		/** File sort key on media, INDEX_NONE if not supported or unknown.							*/
		INT					FileSortKey;

		/** Name of file.																			*/
		FString				FileName;
		/** Offset into file.																		*/
		INT					Offset;
		/** Size in bytes of data to read.															*/
		INT					Size;
		/** Uncompressed size in bytes of original data, 0 if data is not compressed on disc		*/
		INT					UncompressedSize;														
		/** Pointer to memory region used to read data into.										*/
		void*				Dest;
		/** Flags for controlling decompression														*/
		ECompressionFlags	CompressionFlags;
		/** Thread safe counter that is decremented once work is done.								*/
		FThreadSafeCounter* Counter;
		/** Priority of request.																	*/
		EAsyncIOPriority	Priority;
		/** Is this a request to destroy the handle?												*/
		BITFIELD			bIsDestroyHandleRequest : 1;
		/** Whether we already requested the handle to be cached.									*/
		BITFIELD			bHasAlreadyRequestedHandleToBeCached : 1;

		/** Constructor, initializing all member variables. */
		FAsyncIORequest()
		:	RequestIndex(0)
		,	FileSortKey(INDEX_NONE)
		,	Offset(INDEX_NONE)
		,	Size(INDEX_NONE)
		,	UncompressedSize(INDEX_NONE)
		,	Dest(NULL)
		,	CompressionFlags(COMPRESS_None)
		,	Counter(NULL)
		,	Priority(AIOP_MIN)
		,	bIsDestroyHandleRequest(FALSE)
		{}

		/**
		 * @returns human readable string with information about request
		 */
		FString ToString() const
		{
			return FString::Printf(TEXT("%11.1f, %10d, %10d, %10d, %10d, 0x%p, 0x%08x, 0x%08x, %d, %s"),
				(DOUBLE)RequestIndex, FileSortKey, Offset, Size, UncompressedSize, Dest, (DWORD)CompressionFlags,
				(DWORD)Priority, bIsDestroyHandleRequest ? 1 : 0, *FileName);
		}
	};

	/** 
	 * Implements shared stats handling and passes read to PlatformReadDoNotCallDirectly
	 *
	 * @param	FileHandle	Platform specific file handle
	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	TRUE if read was successful, FALSE otherwise
	 */	
	UBOOL InternalRead( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest );

	/**
	 * Allows each platform to allocate memory to be read into (in case the platform has
	 * any special restrictions on IO buffers
	 *
	 * @param	Size		Minimum size of memory to allocate
	 *
	 * @return	Pointer suitable to be passed to InternalRead
	 */
	virtual void* PlatformAllocateIOBuffer(INT Size)
	{
		// by default, just appMalloc the buffer
		return appMalloc(Size);
	}

	/**
	 * Allows each platform to deallocate memory that was allocated by PlatformAllocateIOBuffer
	 *
	 * @param	Buffer		Pointer previously returned from PlatformAllocateIOBuffer
	 */
	virtual void PlatformDeallocateIOBuffer(void* Buffer)
	{
		// by default, just appFree the buffer
		appFree(Buffer);
	}

	/** 
	 * Pure virtual of platform specific read functionality that needs to be implemented by
	 * derived classes. Should only be called from InternalRead.
	 *
	 * @param	FileHandle	Platform specific file handle
	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	TRUE if read was successful, FALSE otherwise
	 */
	virtual UBOOL PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest ) = 0;

	/** 
	 * Pure virtual of platform specific file handle creation functionality that needs to be 
	 * implemented by derived classes.
	 *
	 * @param	FileName	Pathname to file
	 *
	 * @return	Platform specific value/ handle to file that is later on used; use 
	 *			IsHandleValid to check for errors.
	 */
	virtual FAsyncIOHandle PlatformCreateHandle( const TCHAR* FileName ) = 0;

	/**
	 * Pure virtual of platform specific handle destroy functionality that needs to be 
	 * implemented by derived classes.
	 */
	virtual void PlatformDestroyHandle( FAsyncIOHandle FileHandle ) = 0;

	/**
	 * Pure virtual of platform specific code to determine whether the passed in file handle
	 * is valid or not.
	 *
	 * @param	FileHandle	File hande to check validity
	 *
	 * @return	TRUE if file handle is valid, FALSE otherwise
	 */
	virtual UBOOL PlatformIsHandleValid( FAsyncIOHandle FileHandle ) = 0;
	
	/**
	 * This is made platform specific to allow ordering of read requests based on layout of files
	 * on the physical media. The base implementation is FIFO while taking priority into account
	 *
	 * This function is being called while there is a scope lock on the critical section so it
	 * needs to be fast in order to not block QueueIORequest and the likes.
	 *
	 * @return	index of next to be fulfilled request or INDEX_NONE if there is none
	 */
	virtual INT PlatformGetNextRequestIndex();

	/**
	 * Let the platform handle being done with the file
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void PlatformHandleHintDoneWithFile(const FString& Filename)
	{ 
		// by default do nothing
	}

	/**
	 * Fulfills a compressed read request in a blocking fashion by falling back to using
	 * PlatformSeek and various PlatformReads in combination with FAsyncUncompress to allow
	 * decompression while we are waiting for file I/O.
	 *
	 * @param	IORequest	IO requewst to fulfill
	 * @param	FileHandle	File handle to use
	 */
	void FulfillCompressedRead( const FAsyncIORequest& IORequest, const FAsyncIOHandle& FileHandle );

	/**
	 * Retrieves cached file handle or caches it if it hasn't been already
	 *
	 * @param	FileName	file name to retrieve cached handle for
	 * @return	cached file handle
	 */
	FAsyncIOHandle GetCachedFileHandle( const FString& FileName );

	/**
	 * Returns cached file handle if found, or NULL if not. This function does
	 * NOT create any file handles and therefore is not blocking.
	 *
	 * @param	FileName	file name to retrieve cached handle for
	 * @return	cached file handle, NULL if not cached
	 */
	FAsyncIOHandle* FindCachedFileHandle( const FString& FileName );

	/**
	 * Flushes all file handles.
	 */
	void FlushHandles();

	/**
	 * Blocks till all requests are finished.
	 *
	 * @todo streaming: this needs to adjusted to signal the thread to not accept any new requests from other 
	 * @todo streaming: threads while we are blocking till all requests are finished.
	 */
	void BlockTillAllRequestsFinished();

	/**
	 * Constrains bandwidth to value set in .ini
	 *
	 * @param BytesRead		Number of bytes read
	 * @param ReadTime		Time it took to read
	 */
	void ConstrainBandwidth( INT BytesRead, FLOAT ReadTime );

	/**
	 * Packs IO request into a FAsyncIORequest and queues it in OutstandingRequests array
	 *
	 * @param	FileName			File name associated with request
	 * @param	Offset				Offset in bytes from beginning of file to start reading from
	 * @param	Size				Number of bytes to read
	 * @param	UncompressedSize	Uncompressed size in bytes of original data, 0 if data is not compressed on disc	
	 * @param	Dest				Pointer holding to be read data
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter associated with this request; will be decremented when fulfilled
	 * @param	Priority			Priority of request
	 * 
	 * @return	unique ID for request
	 */
	QWORD QueueIORequest( 
		const FString& FileName, 
		INT Offset, 
		INT Size, 
		INT UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority );
	
#if FLASH	
	/**
	 * This is where all the actual loading is done (synchronously) when there's no separate thread
	 */
	void ServiceRequestsSynchronously();
#endif

	/**
	 * Adds a destroy handle request top the OutstandingRequests array
	 * 
	 * @param	FileName			File name associated with request
	 *
	 * @return	unique ID for request
	 */
	QWORD QueueDestroyHandleRequest(const FString& Filename);

	/** 
	 *	Logs out the given file IO information w/ the given message.
	 *	
	 *	@param	Message		The message to prepend
	 *	@param	IORequest	The IORequest to log
	 */
	void LogIORequest(const FString& Message, const FAsyncIORequest& IORequest);

	/** Critical section used to syncronize access to outstanding requests map						*/
	FCriticalSection*				CriticalSection;
	/** TMap of file name string hash to file handles												*/
	TMap<FString,FAsyncIOHandle>	NameToHandleMap;
	/** Array of outstanding requests, processed in FIFO order										*/
	TArray<FAsyncIORequest>			OutstandingRequests;
	/** Event that is signaled if there are outstanding requests									*/
	FEvent*							OutstandingRequestsEvent;
	/** Thread safe counter that is 1 if the thread is currently busy with request, 0 otherwise		*/
	FThreadSafeCounter				BusyWithRequest;
	/** Thread safe counter that is 1 if the thread is available to process requests, 0 otherwise	*/
	FThreadSafeCounter				IsRunning;
	/** Current request index. We don't really worry about wrapping around with a QWORD				*/
	QWORD							RequestIndex;
	/** Counter to indicate that the application requested that IO should be suspended				*/
	FThreadSafeCounter				SuspendCount;
	/** Critical section to sequence IO when needed (in addition to SuspendCount).					*/
	FCriticalSection*				ExclusiveReadCriticalSection;

	/* Min priority of requests to fulfill.															*/
	EAsyncIOPriority				MinPriority;
};

/*-----------------------------------------------------------------------------
	FAsyncIOSystemWindows
-----------------------------------------------------------------------------*/

#if _MSC_VER && !(defined XBOX)
/**
 * Windows specific implementation of async IO system. @todo move to PlatformWindows project
 */
struct FAsyncIOSystemWindows : public FAsyncIOSystemBase
{
	/** 
	 * Reads passed in number of bytes from passed in file handle.
	 *
	 * @param	FileHandle	Handle of file to read from.
	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	TRUE if read was successful, FALSE otherwise
	 */
	virtual UBOOL PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest );

	/** 
	 * Creates a file handle for the passed in file name
	 *
	 * @param	FileName	Pathname to file
	 *
	 * @return	INVALID_HANDLE if failure, handle on success
	 */
	virtual FAsyncIOHandle PlatformCreateHandle( const TCHAR* FileName );

	/**
	 * Closes passed in file handle.
	 */
	virtual void PlatformDestroyHandle( FAsyncIOHandle FileHandle );

	/**
	 * Returns whether the passed in handle is valid or not.
	 *
	 * @param	FileHandle	File hande to check validity
	 *
	 * @return	TRUE if file handle is valid, FALSE otherwise
	 */
	virtual UBOOL PlatformIsHandleValid( FAsyncIOHandle FileHandle );
};
#endif	

/*-----------------------------------------------------------------------------
	FAsyncIOSystemXenon
-----------------------------------------------------------------------------*/

#if XBOX
/**
 * Windows specific implementation of async IO system. @todo move to PlatformWindows project
 */
struct FAsyncIOSystemXenon : public FAsyncIOSystemBase
{
	/** Constructor, initializing member variables and allocating buffer. */
	FAsyncIOSystemXenon();

	/** Virtual destructor, freeing allocated memory. */
	virtual ~FAsyncIOSystemXenon();

	/** 
	 * Reads passed in number of bytes from passed in file handle.
	 *
	 * @param	FileHandle	Handle of file to read from.
 	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	TRUE if read was successful, FALSE otherwise
	 */
	virtual UBOOL PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest );

	/** 
	 * Creates a file handle for the passed in file name
	 *
	 * @param	FileName	Pathname to file
	 *
	 * @return	INVALID_HANDLE if failure, handle on success
	 */
	virtual FAsyncIOHandle PlatformCreateHandle( const TCHAR* FileName );

	/**
	 * Closes passed in file handle.
	 */
	virtual void PlatformDestroyHandle( FAsyncIOHandle FileHandle );

	/**
	 * Returns whether the passed in handle is valid or not.
	 *
	 * @param	FileHandle	File hande to check validity
	 *
	 * @return	TRUE if file handle is valid, FALSE otherwise
	 */
	virtual UBOOL PlatformIsHandleValid( FAsyncIOHandle FileHandle );

	/**
	 * Determines the next request index to be fulfilled by taking into account previous and next read
	 * requests and ordering them to avoid seeking.
	 *
	 * This function is being called while there is a scope lock on the critical section so it
	 * needs to be fast in order to not block QueueIORequest and the likes.
	 *
	 * @return	index of next to be fulfilled request or INDEX_NONE if there is none
	 */
	virtual INT PlatformGetNextRequestIndex();

	/**
	 * Let the platform handle being done with the file
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void PlatformHandleHintDoneWithFile( const FString& Filename );

private:
	/** Last handle used to read from.							*/
	HANDLE	BufferedHandle;
	/** Last offset buffered data was read into from.			*/
	INT		BufferedOffset;
	/** Buffered data at BufferedOffset for BufferedHandle.		*/
	BYTE*	BufferedData;
	/** Current offset into BufferedHandle file.				*/
	INT		CurrentOffset;

	/** Last sort key used.										*/
	INT		LastSortKey;
	/** Last offset used.										*/
	INT		LastOffset;
	/** Last priority used.										*/
	INT		LastPriority;
	/** Last time an I/O request was made						*/
	DOUBLE	LastIORequestTime;
};
#endif



#endif // __UNIOBASE_H__
