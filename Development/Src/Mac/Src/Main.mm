/*=============================================================================
	Main.mm: Mac main function.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <Cocoa/Cocoa.h>

extern void appMacSaveCommandLine(int argc, char* argv[]);

int main(int argc, char *argv[])
{
	appMacSaveCommandLine(argc, argv);

	return NSApplicationMain(argc, (const char **)argv);
}
