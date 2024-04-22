/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __InteropShared_h__
#define __InteropShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app


// gcroot helper that can be used in both managed and native headers
#ifdef __cplusplus_cli

//
// CLR system include: gcroot.h allows us to work with managed objects in native classes.  For example,
// a native class can contain a reference to a managed object that's wrapped by a gcroot<> declaration.
//
// NOTE: Only source files compiled with /clr will be able to compile gcroot.h
//
#include <msclr/gcroot.h>
#include <msclr/auto_gcroot.h>

#define GCRoot( Type ) gcroot< Type >
#define AutoGCRoot( Type ) msclr::auto_gcroot< Type >

#else

#define GCRoot( Type ) PTRINT
#define AutoGCRoot( Type ) PTRINT

#endif



namespace InteropTools
{
	/** Creates an assembly resolve handler to find assemblies that aren't in the same folder */
	void AddResolveHandler();

	/** Initializes the backend interface for the the editor's companion module: UnrealEdCSharp */
	void InitUnrealEdCSharpBackend();

	/** Loads any resource dictionaries needed by our WPF controls (localized text, etc.) */
	void LoadResourceDictionaries();
}


#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...


#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else


#endif	// __InteropShared_h__

