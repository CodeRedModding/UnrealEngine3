/*=============================================================================
	ReverbVolume.cpp: Used to affect reverb settings in the game and editor.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
 
IMPLEMENT_CLASS( AReverbVolume );

/**
 * Removes the reverb volume to world info's list of reverb volumes.
 */
void AReverbVolume::ClearComponents( void )
{
	// Route clear to super first.
	Super::ClearComponents();

	// GWorld will be NULL during exit purge.
	if( GWorld )
	{
		AReverbVolume* CurrentVolume = GWorld->GetWorldInfo()->HighestPriorityReverbVolume;
		AReverbVolume* PreviousVolume = NULL;

		// Iterate over linked list, removing this volume if found.
		while( CurrentVolume )
		{
			// Found.
			if( CurrentVolume == this )
			{
				// Remove from linked list.
				if( PreviousVolume )
				{
					PreviousVolume->NextLowerPriorityVolume = NextLowerPriorityVolume;
				}
				else
				{
					// Special case removal from first entry.
					GWorld->GetWorldInfo()->HighestPriorityReverbVolume = NextLowerPriorityVolume;
				}

				break;
			}

			// List traversal.
			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityVolume;
		}

		// Reset next pointer to avoid dangling end bits and also for GC.
		NextLowerPriorityVolume = NULL;
	}
}

/**
 * Adds the reverb volume to world info's list of reverb volumes.
 */
void AReverbVolume::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );

	AReverbVolume* CurrentVolume = GWorld->GetWorldInfo()->HighestPriorityReverbVolume;
	AReverbVolume* PreviousVolume = NULL;

	// Find where to insert in sorted linked list.
	if( CurrentVolume )
	{
		// Avoid double insertion!
		while( CurrentVolume && CurrentVolume != this )
		{
			// We use > instead of >= to be sure that we are not inserting twice in the case of multiple volumes having
			// the same priority and the current one already having being inserted after one with the same priority.
			if( Priority > CurrentVolume->Priority )
			{
				if ( PreviousVolume )
				{
					// Insert before current node by fixing up previous to point to current.
					PreviousVolume->NextLowerPriorityVolume = this;
				}
				else
				{
					// Special case for insertion at the beginning.
					GWorld->GetWorldInfo()->HighestPriorityReverbVolume = this;
				}

				// Point to current volume, finalizing insertion.
				NextLowerPriorityVolume = CurrentVolume;
				return;
			}

			// List traversal.
			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityVolume;
		}

		// We're the lowest priority volume, insert at the end.
		if( !CurrentVolume )
		{
			checkSlow( PreviousVolume );
			PreviousVolume->NextLowerPriorityVolume = this;
			NextLowerPriorityVolume = NULL;
		}
	}
	else
	{
		// First volume in the world info.
		GWorld->GetWorldInfo()->HighestPriorityReverbVolume = this;
		NextLowerPriorityVolume	= NULL;
	}
}

/**
 * callback for changed property 
 */
void AReverbVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Settings.Volume = Clamp<FLOAT>( Settings.Volume, 0.0f, 1.0f );
	AmbientZoneSettings.InteriorTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.InteriorTime );
	AmbientZoneSettings.InteriorLPFTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.InteriorLPFTime );
	AmbientZoneSettings.ExteriorTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.ExteriorTime );
	AmbientZoneSettings.ExteriorLPFTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.ExteriorLPFTime );
}
