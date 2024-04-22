// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HSOURCEFILEH
#define HSOURCEFILEH

/*
 * Source file
 */

class CSourceFile
{
private:
	string			Code;		// File source code
	string			Name;		// File name
	stringstream	CodeBuf;	// String buffer with code
	int				Line;		// Current line

public:
	/* Constructor */
	CSourceFile( const string &code, const string &name );

	/* Initialize parsing */
	void Rewind();

	/* Check if end of file was reached */
	bool IsEOF() const;

	/* Get line from source file */
	string ReadLine();

	/* Get line number */
	int GetLineNumber() const;
};

#endif