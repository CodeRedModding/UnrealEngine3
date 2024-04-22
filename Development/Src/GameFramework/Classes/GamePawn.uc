/**
 * GamePawn
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GamePawn extends Pawn
	config(Game)
	native
	abstract
	notplaceable
	nativereplication;


/** Was the last hit considered a head shot?  Used to see if we need to pop off helmet/head */ // @TODO FIXMESTEVE SHOULD NOT BE HERE!
var transient bool bLastHitWasHeadShot;

/** Whether pawn responds to explosions or not (ie knocked down from explosions) */
var bool bRespondToExplosions;

cpptext
{
		// Networking
	INT* GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}


replication
{
	// Replicated to ALL
	if( Role == Role_Authority )
		bLastHitWasHeadShot;
}

/** This will update the shadow settings for this pawn's mesh **/
simulated event UpdateShadowSettings( bool bInWantShadow )
{
	local bool bNewCastShadow;
	local bool bNewCastDynamicShadow;

	if( Mesh != None )
	{
		bNewCastShadow = default.Mesh.CastShadow && bInWantShadow;
		bNewCastDynamicShadow = default.Mesh.bCastDynamicShadow && bInWantShadow;

		if( (bNewCastShadow != Mesh.CastShadow) || (bNewCastDynamicShadow != Mesh.bCastDynamicShadow) )
		{
			// if there is a pending Attach then this will set the shadow immediately as the flags have changed an a reattached has occurred
			Mesh.CastShadow = bNewCastShadow;
			Mesh.bCastDynamicShadow = bNewCastDynamicShadow;

			// if we are in a poor framerate situation just change the settings even if people are looking at it
			if( WorldInfo.bAggressiveLOD == TRUE )
			{
				ReattachMesh();
			}
			else
			{
				ReattachMeshWithoutBeingSeen();
			}
		}
	}
}

/** reattaches the mesh component **/
simulated function ReattachMesh()
{
	ClearTimer( nameof(ReattachMeshWithoutBeingSeen) );
	ReattachComponent(Mesh);
}

/** reattaches the mesh component without being seen **/
simulated function ReattachMeshWithoutBeingSeen()
{
	// defer so we do not pop from any settings we have changed (e.g. shadow settings)
	if( LastRenderTime > WorldInfo.TimeSeconds - 1.0 )
	{
		SetTimer( 0.5 + FRand() * 0.5, FALSE, nameof(ReattachMeshWithoutBeingSeen) );
	}
	// we have not been rendered for a bit so go ahead and reattach
	else
	{
		ReattachMesh();
	}
}


defaultproperties
{
	bCanBeAdheredTo=TRUE
	bCanBeFrictionedTo=TRUE
}
