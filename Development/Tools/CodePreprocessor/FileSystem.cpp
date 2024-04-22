// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CFileSystem::CFileSystem()
{
	// Reset base directory
	SetBaseDirectory( "" );

	// Initialize pahs
	ClearPaths();
}

void CFileSystem::ClearPaths()
{
	// Clear list of include paths
	IncludePaths.empty();

	// Add default path
	IncludePaths.push_back( ".\\" );
}

void CFileSystem::SetBaseDirectory( const string &baseDirectory )
{
	BaseDirectory = baseDirectory;

	// Base directory was not specified , get it from system
	if ( BaseDirectory == "" )
	{
		#ifdef WIN32
			CHAR directoryName[ MAX_PATH ];
			GetCurrentDirectoryA( MAX_PATH, directoryName );
			BaseDirectory = directoryName;
		#else
			#error Implement GetCurrentDirectory
		#endif
	}

	// Directory name should end with "\"
	if ( BaseDirectory!="" )
	{
		char pathTerminator = BaseDirectory[ BaseDirectory.length()-1 ];
		if ( pathTerminator != '\\' && pathTerminator != '/' )
		{
			BaseDirectory += "\\";
		}
	}
}

void CFileSystem::AddIncludePath( const string &path )
{
	// Path should end with "\\"
	if ( path != "" )
	{
		char pathTerminator = path[ path.length()-1 ];

		if ( pathTerminator != '\\' && pathTerminator != '/' )
		{
			IncludePaths.push_back( path + "\\" );
		}
		else
		{
			IncludePaths.push_back( path );
		}
	}
}

CSourceFile *CFileSystem::OpenSourceFile( const string &name, bool useGlobalResolve )
{
	CSourceFile *sourceFile = NULL;

	// Try each path
	for ( vector<string>::const_iterator i=IncludePaths.begin(); i!=IncludePaths.end() && !sourceFile; ++i )
	{
		// Assemble full path name
		string filePath = BaseDirectory + *i + name;

		// Open file
		ifstream file;
		file.open( filePath.c_str(), ios::binary | ios::in );

		// If file is valid, read content
		if ( file.good() )
		{
			// Get file size
			file.seekg( 0, ios::end );
			int length = (int)(file.tellg());
			file.seekg( 0, ios::beg );

			// Allocate buffer for file content
			char *code = new char [ length+1 ];
			file.read( code, length );

			// Data was read
			if ( !file.fail() )
			{
				// Terminate string with
				code[ length ] = '\0';

				// Create source file
				sourceFile = new CSourceFile( code, name );
			}

			// Cleanup
			file.close();
			delete [] code;
		}
	}
	
	return sourceFile;
}