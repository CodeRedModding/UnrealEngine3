// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CSourceFile::CSourceFile( const string &code, const string &name )
	: Name( name )
	, Code( code )
{	
	Rewind();
}

void CSourceFile::Rewind()
{
	// Reset line counter
	Line = 0;

	// Initialize string buffer
	CodeBuf.str( Code );
	CodeBuf.seekg( 0 );
}

bool CSourceFile::IsEOF() const
{
	return CodeBuf.eof();
}

int CSourceFile::GetLineNumber() const
{
	return Line;
}

string CSourceFile::ReadLine()
{
	// Advance line counter
	Line++;

	// Read line
	string line;
	getline( CodeBuf, line );

	// Remove end of line chars
	if ( line!="" && line[ line.length()-1 ] < ' ' )
	{
		line = line.substr( 0, line.length()-1 );
	}

	return line;
}