/*=============================================================================
	FOpenAutomate.h: UnrealEngine interface to OpenAutomate.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __OPENAUTOMATEINTERFACE_H__
#define __OPENAUTOMATEINTERFACE_H__

#if WITH_OPEN_AUTOMATE

#include "../../External/OpenAutomateSDK-1.0-7/inc/OpenAutomate.h"

void EngineTick( void );

class FOpenAutomate
{
public:
	FOpenAutomate( void )
		: bIsBenchmarking( FALSE )
	{
		GIsBenchmarking = TRUE;
	}

	~FOpenAutomate( void )
	{
	}

	/** 
	 * Generic Exec handling
	 */
	UBOOL Exec( const TCHAR* Command, FOutputDevice& Ar );

	/*
	 * Register the application with OpenAutomate
	 *
	 * Write a file to the user's home folder that describes the application and how to run it
	 */
	UBOOL RegisterApplication( void );

	/**
	 * Setup the engine to use OpenAutomate
	 */
	UBOOL Init( const TCHAR* CmdLine );

	/**
	 * Main OpenAutomate processing loop
	 */
	UBOOL ProcessLoop( void );

	/**
	 * Pass all the tweakable options to OpenAutomate
	 */
	void GetAllOptions( void );

	/**
	 * Pass the current settings of all the above registered tweakables to OpenAutomate
	 */
	void GetCurrentOptions( void );

	/**
	 * Set any settings that OpenAutomate wishes to apply
	 */
	void SetOptions( void );

	/**
	 * Load in all the benchmarks to run from an ini file, and register them with OpenAutomate
	 */
	void GetBenchmarks( void );

	/**
	 * Run a previously registered named benchmark
	 */
	void RunBenchmark( const oaChar* BenchmarkName );

private:
	// Set to TRUE when a benchmark is in progress, set to FALSE when an exit is requested
	UBOOL bIsBenchmarking;
};

#endif // WITH_OPEN_AUTOMATE

#endif // __OPENAUTOMATEINTERFACE_H__
