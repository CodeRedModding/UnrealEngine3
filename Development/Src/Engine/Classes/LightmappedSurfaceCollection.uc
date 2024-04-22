/**
 *	Collection of surfaces in a single static lighting mapping.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LightmappedSurfaceCollection extends Object
	hidecategories(Object)
	editinlinenew
	native;

/** The UModel these surfaces come from. */
var()	Model		SourceModel;

/** An array of the surface indices grouped into a single static lighting mapping. */
var()	array<int>	Surfaces;

cpptext
{
}
