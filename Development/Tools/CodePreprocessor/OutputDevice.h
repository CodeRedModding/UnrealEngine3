// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HOUTPUTDEVICEH
#define HOUTPUTDEVICEH

/*
 * Output device
 */

class COutputDevice
{
private:
	string		Content;			// File content

public:
	/* Constructor */
	COutputDevice();

	/* Clear content */
	void Clear();

	/* Write single line */
	void WriteLine( const string &line );

	/* Write results to file, returns true on success */
	bool WriteToFile( const string &fileName );
};

#endif