// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HFILESYSTEMH
#define HFILESYSTEMH

/*
 * Compilation file system
 */

class CFileSystem
{
private:
	string				BaseDirectory;		// Base include directory
	vector<string>		IncludePaths;		// List of include paths

public:
	/* Constructor */
	CFileSystem();

	/* Set base directory */
	void SetBaseDirectory( const string &baseDirectory );

	/* Clear list of include paths */
	void ClearPaths();

	/* Add include path */
	void AddIncludePath( const string &path );

	/* Open source file */
	CSourceFile *OpenSourceFile( const string &name, bool useGlobalResolve );
};

#endif
