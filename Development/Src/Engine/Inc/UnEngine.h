/*=============================================================================
	UnEngine.h: Unreal engine definition.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Unreal engine.
-----------------------------------------------------------------------------*/

class UEngine : public USubsystem
{
	DECLARE_ABSTRACT_CLASS(UEngine,USubsystem,CLASS_Config|CLASS_Transient,Engine)

	// Fonts.

	UFont*	TinyFont;
	UFont*	SmallFont;
	UFont*	MediumFont;
	UFont*	LargeFont;

	// Subsystems.
	UClass*				NetworkDriverClass;

	/** The material used when no material is explicitly applied. */
	class UMaterial*	DefaultMaterial;

	/** The material used to render surfaces in the LightingOnly viewmode. */
	class UMaterial*	LightingOnlyMaterial;

	/** A solid colored material with an instance parameter for the color. */
	class UMaterial*	SolidColorMaterial;

	/** A material used to render colored BSP nodes. */
	class UMaterial*	ColoredNodeMaterial;

	/** A translucent material used to render things in geometry mode. */
	class UMaterial*	GeomMaterial;

	/** Material used for drawing a tick mark. */
	class UMaterial*	TickMaterial;

	/** Material used for drawing a cross mark. */
	class UMaterial*	CrossMaterial;

	/** The colors used to rendering light complexity. */
	TArrayNoInit<FColor> LightComplexityColors;

	/** A material used to render the sides of brushes/volumes/etc. */
	class UMaterial*	EditorBrushMaterial;

	// Variables.
	class UClient*			Client;
	TArray<ULocalPlayer*>	Players;

	INT					TickCycles GCC_PACK(PROPERTY_ALIGNMENT);
	INT					GameCycles, ClientCycles;

	BITFIELD			UseSound:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD			UseStreaming:1;

	/** Maximum number of miplevels being streamed in */
	UINT				MaxStreamedInMips GCC_PACK(PROPERTY_ALIGNMENT);
	/** Minimum number of miplevels being streamed in */
	UINT				MinStreamedInMips;

	/** Global debug manager */
	UDebugManager*		DebugManager;

	// Color preferences.
	FColor
		C_WorldBox,
		C_GroundPlane,
		C_GroundHighlight,
		C_BrushWire,
		C_Pivot,
		C_Select,
		C_Current,
		C_AddWire,
		C_SubtractWire,
		C_GreyWire,
		C_BrushVertex,
		C_BrushSnap,
		C_Invalid,
		C_ActorWire,
		C_ActorHiWire,
		C_Black,
		C_White,
		C_Mask,
		C_SemiSolidWire,
		C_NonSolidWire,
		C_WireBackground,
		C_WireGridAxis,
		C_ActorArrow,
		C_ScaleBox,
		C_ScaleBoxHi,
		C_ZoneWire,
		C_OrthoBackground,
		C_Volume,
		C_ConstraintLine,
		C_AnimMesh,
		C_TerrainWire;

	/** Fudge factor for tweaking the distance based miplevel determination */
	FLOAT StreamingDistanceFactor;

	/** Mapping between PhysicalMaterial class name and physics-engine specific MaterialIndex. */
	TMap<FName, UINT>* MaterialMap;

	/** Class name of the scout to use for path building */
	FStringNoInit ScoutClassName;

	// Constructors.
	UEngine();
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void PostEditChange(UProperty* PropertyThatChanged);

	// UEngine interface.
	virtual void Init();
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Out=*GLog );
	virtual void Flush();
	virtual void Tick( FLOAT DeltaSeconds )=0;
	virtual void SetClientTravel( const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType )=0;
	virtual INT ChallengeResponse( INT Challenge );
	virtual FLOAT GetMaxTickRate() { return 0.0f; }
	virtual void SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds );
	virtual ULevel* GetLevel()=0;

	/**
	 * Allows the editor to accept or reject the drawing of wireframe brush shapes based on mode and tool.
	 */
	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor ) { return 1; }

	/**
	 * Looks at all currently loaded packages and prompts the user to save them
	 * if their "bDirty" flag is set.
	 */
	virtual void SaveDirtyPackages() {}

	UINT FindMaterialIndex(UClass* PhysMatClass);
};

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

class UServerCommandlet : public UCommandlet
{
	DECLARE_CLASS(UServerCommandlet,UCommandlet,CLASS_Transient,Engine);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

//
//	FPlayerIterator - Iterates over local players in the game.
//

class FPlayerIterator
{
protected:

	UEngine*	Engine;
	INT			Index;

public:

	// Constructor.

	FPlayerIterator(UEngine* InEngine):
		Engine(InEngine),
		Index(-1)
	{
		++*this;
	}

	void operator++()
	{
		while(++Index < Engine->Players.Num() && !Engine->Players(Index));
	}
	ULocalPlayer* operator*()
	{
		return Engine->Players(Index);
	}
	ULocalPlayer* operator->()
	{
		return Engine->Players(Index);
	}
	operator UBOOL()
	{
		return Index<Engine->Players.Num();
	}
	void RemoveCurrent()
	{
		check(Index < Engine->Players.Num());
		Engine->Players.Remove(Index--);
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

