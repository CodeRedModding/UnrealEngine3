/*=============================================================================
	Mappings.cpp: Static lighting mapping implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "stdafx.h"
#include "Importer.h"


namespace Lightmass
{
/** A static indicating that debug borders should be used around padded mappings. */
UBOOL FStaticLightingMapping::s_bShowLightmapBorders = FALSE;

void FStaticLightingMapping::Import( class FLightmassImporter& Importer )
{
	Importer.ImportData( (FStaticLightingMappingData*) this );
	Mesh = Importer.GetStaticMeshInstances().FindRef(Guid);
}

void FStaticLightingTextureMapping::Import( class FLightmassImporter& Importer )
{
	FStaticLightingMapping::Import( Importer );
	Importer.ImportData( (FStaticLightingTextureMappingData*) this );
	CachedSizeX = SizeX;
	CachedSizeY = SizeY;
	IrradiancePhotonCacheSizeX = 0;
	IrradiancePhotonCacheSizeY = 0;
}

void FStaticLightingVertexMapping::Import( class FLightmassImporter& Importer )
{
	FStaticLightingMapping::Import( Importer );
	Importer.ImportData( (FStaticLightingVertexMappingData*) this );
	SampleToAreaRatio /= Importer.GetLevelScale() * Importer.GetLevelScale();
}

} //namespace Lightmass
