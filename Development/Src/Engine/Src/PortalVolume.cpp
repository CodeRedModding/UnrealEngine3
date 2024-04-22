/*=============================================================================
	PortalVolume.cpp: Used to define portal areas.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
 
IMPLEMENT_CLASS( APortalVolume );

/**
 * Removes the portal volume from the world info's list of portal volumes.
 */
void APortalVolume::ClearComponents( void )
{
	// Route clear to super first.
	Super::ClearComponents();

	// GWorld will be NULL during exit purge.
	if( GWorld )
	{
		GWorld->GetWorldInfo()->PortalVolumes.RemoveItem( this );
	}
}

/**
 * Adds the portal volume to world info's list of portal volumes.
 */
void APortalVolume::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );

	GWorld->GetWorldInfo()->PortalVolumes.AddItem( this );
}
