/*=============================================================================
	StaticLightingVertexMapping.cpp: Static lighting vertex mapping implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "StaticLightingPrivate.h"

// Don't compile the static lighting system on consoles.
#if !CONSOLE

UBOOL FStaticLightingVertexMapping::DebugThisMapping() const
{
	// This only works for mappings that have a one to one relationship with the associated component.
	// Other mapping types need to override this function.
	const UBOOL bDebug = GCurrentSelectedLightmapSample.Component 
		&& GCurrentSelectedLightmapSample.Component == Mesh->Component;
	return bDebug;
}

#endif
