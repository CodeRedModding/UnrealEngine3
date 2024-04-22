// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

COutputDevice::COutputDevice()
{

}

void COutputDevice::Clear()
{
	Content = "";
}

void COutputDevice::WriteLine( const string &line )
{
	//printf( "Out: '%s'\n", line.c_str() );

	Content += line;
	Content += "\r\n";
}

bool COutputDevice::WriteToFile( const string &fileName )
{
	ofstream file;
	file.open( fileName.c_str(), ios::binary | ios::out );

	if ( file.good() )
	{
		file.write( Content.c_str(), (streamsize)Content.length() + 1 );
		file.close();

		return true;
	}
	else
	{
		return false;
	}
}