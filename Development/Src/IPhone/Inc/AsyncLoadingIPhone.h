/*=============================================================================
	UnAsyncLoadingIPhone.h: Definitions of classes used for content streaming on IPhone.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNASYNCLOADINGIPHONE_H__
#define __UNASYNCLOADINGIPHONE_H__

#include "IPhoneThreading.h"

/**
 * IPhone implementation of an async IO manager.	
 */
struct FAsyncIOSystemIPhone : public FAsyncIOSystemBase
{
	/**
	 * Constructor
	 */
	FAsyncIOSystemIPhone()
	{
	}

	/**
	 * Destructor
	 */
	~FAsyncIOSystemIPhone()
	{
	}

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
	virtual UBOOL PlatformReadDoNotCallDirectly(FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest);

	/** 
	 * Creates a file handle for the passed in file name
	 *
	 * @param	Filename	Pathname to file
	 *
	 * @return	INVALID_HANDLE if failure, handle on success
	 */
	virtual FAsyncIOHandle PlatformCreateHandle(const TCHAR* Filename);

	/**
	 * Closes passed in file handle.
	 */
	virtual void PlatformDestroyHandle(FAsyncIOHandle FileHandle);

	/**
	 * Returns whether the passed in handle is valid or not.
	 *
	 * @param	FileHandle	File hande to check validity
	 *
	 * @return	TRUE if file handle is valid, FALSE otherwise
	 */
	virtual UBOOL PlatformIsHandleValid(FAsyncIOHandle FileHandle);
};

#endif
