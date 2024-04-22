/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __THUMBNAILTOOLSCLR_H__
#define __THUMBNAILTOOLSCLR_H__

#ifdef _MSC_VER
	#pragma once
#endif

#include "UnrealEdCLR.h"

using namespace System;
using namespace System::Windows;
using namespace System::Windows::Media;

namespace ThumbnailToolsCLR
{
	/**
	 * Generates a WPF bitmap for the specified thumbnail
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 * 
	 * @return	Returns the BitmapSource for the thumbnail, or null on failure
	 */
	Imaging::BitmapSource^ CreateBitmapSourceForThumbnail( const FObjectThumbnail& InThumbnail );


	/**
	 * Updates a writeable WPF bitmap with the specified thumbnail's data
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 */
	void CopyThumbnailToWriteableBitmap( const FObjectThumbnail& InThumbnail, Imaging::WriteableBitmap^ WriteableBitmap );

	/**
	 * Generates a writeable WPF bitmap for the specified thumbnail
	 * 
	 * @param	InThumbnail		The thumbnail object to create a bitmap for
	 * 
	 * @return	Returns the BitmapSource for the thumbnail, or null on failure
	 */
	Imaging::WriteableBitmap^ CreateWriteableBitmapForThumbnail( const FObjectThumbnail& InThumbnail );


	/**
	 * Attempts to get a thumbnail for the specified object, returning from cache or generating otherwise.
	 * 
	 * @param	Object	the object that needs a thumbnail
	 * 
	 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
	 */
	Imaging::BitmapSource^ GetBitmapSourceForObject( UObject* InObject );
}
#endif // __THUMBNAILTOOLSCLR_H__