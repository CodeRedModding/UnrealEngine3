/*=============================================================================
	Importer.cpp: Lightmass importer implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "stdafx.h"
#include "LightmassSwarm.h"
#include "Importer.h"
#include "Scene.h"


namespace Lightmass
{

FLightmassImporter::FLightmassImporter( FLightmassSwarm* InSwarm )
:	Swarm( InSwarm )
,	LevelScale(0.0f)
{
}

FLightmassImporter::~FLightmassImporter()
{
}

/**
 * Imports a scene and all required dependent objects
 *
 * @param Scene Scene object to fill out
 * @param SceneGuid Guid of the scene to load from a swarm channel
 */
UBOOL FLightmassImporter::ImportScene( class FScene& Scene, const FGuid& SceneGuid )
{
	INT ErrorCode = Swarm->OpenChannel( *CreateChannelName( SceneGuid, LM_SCENE_VERSION, LM_SCENE_EXTENSION ), LM_SCENE_CHANNEL_FLAGS, TRUE );
	if( ErrorCode >= 0 )
	{
		Scene.Import( *this );

		Swarm->CloseCurrentChannel( );
		return TRUE;
	}
	else
	{
		Swarm->SendTextMessage( TEXT( "Failed to open scene channel with GUID {%08x}:{%08x}:{%08x}:{%08x}" ), SceneGuid.A, SceneGuid.B, SceneGuid.C, SceneGuid.D );
	}

	return FALSE;
}

UBOOL FLightmassImporter::Read( void* Data, INT NumBytes )
{
	INT NumRead = Swarm->Read(Data, NumBytes);
	return NumRead == NumBytes;
}

}	//Lightmass
