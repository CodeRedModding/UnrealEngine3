// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

int main( int argc, const char* argv[] )
{
	// No stuff specified
	if ( argc < 4 )
	{
		printf( "CodePreprocesor <rulefile> <infile> <outfile>\n" );
		printf( "   rulefile - Processing rules\n" );
		printf( "   infile - File to filter\n" );
		printf( "   outfile - Output file\n" );

		return -1;
	}

	// Parse rule file
	CRuleTable rules;
	if ( !rules.LoadFromFile( argv[1] ) )
	{
		printf( "Unable to load rule file\n" );
		return -1;
	}

	// Open source file
	CFileSystem fileSys;
	CSourceFile *file = fileSys.OpenSourceFile( argv[2], false );
	if ( !file )
	{
		printf( "Unable to load source file\n" );
		return -1;
	}

	// Decompose into code blocks
	CSourceFileDecomposer decomposer;
	CCodeRootBlock *root = decomposer.DecomposeFile( file );

	// Filter
	if ( root )
	{
		COutputDevice o;
		CExpressionCache cache( rules );
		root->Filter( true, cache, o );

		// Save results
		if ( !o.WriteToFile( argv[3] ) )
		{
			printf( "Error saving output file.\n" );
			return -1;
		}
	}
	else
	{
		printf( "Error decomposing file. Possible errors in preprocessor directives.\n" );
		return -1;
	}

	// Close file
	delete file;
	return 0;
}
