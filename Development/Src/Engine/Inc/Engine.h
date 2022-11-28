/*=============================================================================
	Engine.h: Unreal engine public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_ENGINE
#define _INC_ENGINE

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Core.h"

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern class FMemStack			GEngineMem;
extern class FSelectionTools	GSelectionTools;
extern class FStatChart*		GStatChart;
extern class UEngine*			GEngine;

/*-----------------------------------------------------------------------------
	Size of the world.
-----------------------------------------------------------------------------*/

#define WORLD_MAX			524288.0	/* Maximum size of the world */
#define HALF_WORLD_MAX		262144.0	/* Half the maximum size of the world */
#define HALF_WORLD_MAX1		262143.0	/* Half the maximum size of the world - 1*/
#define MIN_ORTHOZOOM		250.0		/* Limit of 2D viewport zoom in */
#define MAX_ORTHOZOOM		16000000.0	/* Limit of 2D viewport zoom out */
#define DEFAULT_ORTHOZOOM	10000		/* Default 2D viewport zoom */

/*-----------------------------------------------------------------------------
	Rigid body physics.
-----------------------------------------------------------------------------*/

#if defined(_XBOX)
#if _XBOX_VER >= 200
	#undef CONSOLE
#endif	//#if _XBOX_VER >= 200
#endif	//#if defined(_XBOX)

#if !CONSOLE

#ifndef WITH_NOVODEX
#		define WITH_NOVODEX
#endif // WITH_NOVODEX

#endif

#if defined(_XBOX)
#if _XBOX_VER >= 200
#define CONSOLE		1
#endif	//#if _XBOX_VER >= 200
#endif	//#if defined(_XBOX)

/*-----------------------------------------------------------------------------
	Editor.
-----------------------------------------------------------------------------*/

enum ERefreshEditor
{
	ERefreshEditor_Misc					= 1,	// All other things besides browsers
	ERefreshEditor_ActorBrowser			= 2,
	ERefreshEditor_LayerBrowser			= 4,
	ERefreshEditor_GroupBrowser			= 8,
	ERefreshEditor_AnimationBrowser		= 16,
	ERefreshEditor_Terrain				= 2048,
	ERefreshEditor_GenericBrowser		= 4096,
};

#define ERefreshEditor_AllBrowsers		ERefreshEditor_GroupBrowser | ERefreshEditor_LayerBrowser | ERefreshEditor_AnimationBrowser | ERefreshEditor_GenericBrowser

/*-----------------------------------------------------------------------------
	UnrealScript struct pointer hacks.
-----------------------------------------------------------------------------*/

typedef struct FRenderInterface*	FRenderInterfacePtr;

/*-----------------------------------------------------------------------------
	Engine public includes.
-----------------------------------------------------------------------------*/

#include "UnTickable.h"				// FTickableObject interface.
#include "UnSelection.h"			// Tools used by UnrealEd for managing resource selections.
#include "UnRebuildTools.h"			// Tools used by UnrealEd for rebuilding the level.
#include "UnContentStreaming.h"		// Content streaming class definitions.
#include "UnResource.h"				// Resource management definitions.
#include "UnClient.h"				// Platform specific client interface definition.
#include "UnStats.h"				// Performance stats.
#include "UnMaterial.h"				// Materials.
#include "UnTex.h"					// Textures.
#include "UnSHM.h"					// Spherical harmonic maps.
#include "Bezier.h"					// Class for computing bezier curves
#include "UnObj.h"					// Standard object definitions.
#include "UnPrim.h"					// Primitive class.
#include "UnShadowVolume.h"			// Shadow volume definitions.
#include "UnAnimTree.h"				// Animation.
#include "UnComponents.h"			// Forward declarations of object components of actors
#include "UnActorComponent.h"		// Actor component definitions.
#include "UnParticleComponent.h"	// Particle component definitions.
#include "UnModel.h"				// Model class.
#include "UnPhysPublic.h"			// Public physics integration types.
#include "UnInteraction.h"			// Interaction definitions.
#include "UnPhysAsset.h"			// Physics Asset.
#include "UnInterpolation.h"		// Matinee.
#include "UnForceFeedbackWaveform.h" // Gamepad vibration support
#include "UnDistributions.h"		// Base classes for UDistribution objects
#include "EngineClasses.h"			// All actor classes.
#include "UnScene.h"				// Scene management.
#include "UnPhysic.h"				// Physics constants
#include "UnURL.h"					// Uniform resource locators.
#include "UnLevel.h"				// Level object.
#include "UnKeys.h"					// Key name definitions.
#include "UnPlayer.h"				// Player definition.
#include "UnEngine.h"				// Unreal engine.
#include "UnGame.h"					// Unreal game engine.
#include "UnSkeletalMesh.h"			// Skeletal animated mesh.
#include "UnSkeletalAnim.h"			// Skeletal animation related.
#include "UnActor.h"				// Actor inlines.
#include "UnAudio.h"				// Audio code.
#include "UnStaticMesh.h"			// Static T&L meshes.
#include "UnCDKey.h"				// CD key validation.
#include "UnCanvas.h"				// Canvas.
#include "UnPNG.h"					// PNG helper code for storing compressed source art.

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit an actor.
struct HActor : public HHitProxy
{
	DECLARE_HIT_PROXY(HActor,HHitProxy)
	AActor* Actor;
	HActor( AActor* InActor ) : Actor( InActor ) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor;
	}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

//
//	HBspSurf
//

struct HBspSurf : public HHitProxy
{
	DECLARE_HIT_PROXY(HBspSurf,HHitProxy);
	UModel*	Model;
	INT		iSurf,
			iNode;
	HBspSurf(UModel* InModel,INT SurfaceIndex,INT NodeIndex):
		Model(InModel),
		iSurf(SurfaceIndex),
		iNode(NodeIndex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Model;
	}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

//
//	HBSPBrushVert
//

struct HBSPBrushVert : public HHitProxy
{
	DECLARE_HIT_PROXY(HBSPBrushVert,HHitProxy);
	ABrush*	Brush;
	FVector* Vertex;
	HBSPBrushVert(ABrush* InBrush,FVector* InVertex):
		Brush(InBrush),
		Vertex(InVertex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Brush;
	}
};

//
//	HStaticMeshVert
//

struct HStaticMeshVert : public HHitProxy
{
	DECLARE_HIT_PROXY(HStaticMeshVert,HHitProxy);
	AActor*	Actor;
	FVector Vertex;
	HStaticMeshVert(AActor* InActor,FVector InVertex):
		Actor(InActor),
		Vertex(InVertex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor << Vertex;
	}
};

/*-----------------------------------------------------------------------------
	Terrain editing brush types.
-----------------------------------------------------------------------------*/

enum ETerrainBrush
{
	TB_None				= -1,
	TB_VertexEdit		= 0,	// Select/Drag Vertices on heightmap
	TB_Paint			= 1,	// Paint on selected layer
	TB_Smooth			= 2,	// Does a filter on the selected vertices
	TB_Noise			= 3,	// Adds random noise into the selected vertices
	TB_Flatten			= 4,	// Flattens the selected vertices to the height of the vertex which was initially clicked
	TB_TexturePan		= 5,	// Pans the texture on selected layer
	TB_TextureRotate	= 6,	// Rotates the texture on selected layer
	TB_TextureScale		= 7,	// Scales the texture on selected layer
	TB_Select			= 8,	// Selects areas of the terrain for copying, generating new terrain, etc
	TB_Visibility		= 9,	// Toggles terrain sectors on/off
	TB_Color			= 10,	// Paints color into the RGB channels of layers
	TB_EdgeTurn			= 11,	// Turns edges of terrain triangulation
};

/*-----------------------------------------------------------------------------
	Iterator for the editor that loops through all selected actors.
-----------------------------------------------------------------------------*/

class TSelectedActorIterator
{
public:
	TSelectedActorIterator( ULevel* InLevel )
		: Level( InLevel ), Index( -1 )
	{
		check(InLevel);
		++*this;
	}
	void operator++()
	{
		while(
			++Index < Level->Actors.Num()
				&& ( Cast<AActor>( Level->Actors(Index) ) == NULL
				|| !GSelectionTools.IsSelected( Level->Actors(Index) ) )
		);
	}
	AActor* operator*()
	{
		return Level->Actors(Index);
	}
	AActor* operator->()
	{
		return Level->Actors(Index);
	}
	INT GetIndex()
	{
		return Index;
	}
	operator UBOOL()
	{
		return Index < Level->Actors.Num();
	}
protected:
	ULevel* Level;
	INT Index;
};

/*-----------------------------------------------------------------------------
	Iterator for the editor that loops through all valid actors in the level.
-----------------------------------------------------------------------------*/

class TActorIterator
{
public:
	TActorIterator( ULevel* InLevel )
		: Level( InLevel ), Index( -1 )
	{
		check(InLevel);
		++*this;
	}
	void operator++()
	{
		while(
			++Index < Level->Actors.Num()
				&& Cast<AActor>( Level->Actors(Index) ) == NULL
		);
	}
	AActor* operator*()
	{
		return Level->Actors(Index);
	}
	AActor* operator->()
	{
		return Level->Actors(Index);
	}
	operator UBOOL()
	{
		return Index < Level->Actors.Num();
	}
protected:
	ULevel* Level;
	INT Index;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

