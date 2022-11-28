/*=============================================================================
	UnRebuildTools.h: Level rebuild tools
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Quality level for rebuilding Bsp.
enum EBspOptimization
{
	BSP_Lame,
	BSP_Good,
	BSP_Optimal
};

enum ERebuildOptions
{
	REBUILD_Geometry			= 0x00000001, 	// Basic level geometry
	REBUILD_BSP					= 0x00000002, 	// Thorough BSP compile
	REBUILD_Lighting			= 0x00000004, 	// Lighting
	REBUILD_Paths				= 0x00000008, 	// Pathing
	REBUILD_OnlyVisible			= 0x00000010,	// Only rebuild visible actors
	REBUILD_OnlySelectedZones	= 0x00000020,	// Only rebuild zones that contain selected actors
	REBUILD_Fluids			    = 0x00000040,	// Fluid surfaces
	REBUILD_Default				= REBUILD_Geometry | REBUILD_BSP | REBUILD_Lighting | REBUILD_Paths | REBUILD_Fluids,
};

// Represents a set of rebuild options
class FRebuildOptions
{
public:
	FRebuildOptions();
	~FRebuildOptions();

	void Init();

	inline FString GetName() { return Name; }

	FString				Name;
	EBspOptimization	BspOpt;					// BSP_
	DWORD				Options;				// bitfield : REBUILD_
	INT					Balance,				// The balance of the BSP tree (0-100 - 15 default)
						PortalBias,				// Portal cutting strength (0-100 - 70 default)
						LightmapFormat;			// TEXF_?  (enums can't be forward declared in ANSI C++.)
	UBOOL				DitherLightmaps,		// Whether to dither during compression.
						SaveOutLightmaps,		// If true will save out the lightmaps as .BMPs for debugging.
						ReportErrors;			// Shows the "Map Check" dialog when the rebuild is done?
	
	inline FRebuildOptions operator=( FRebuildOptions RO )
	{
		Name			= RO.Name;
		BspOpt			= RO.BspOpt;
		Options			= RO.Options;
		Balance			= RO.Balance;
		PortalBias		= RO.PortalBias;
		LightmapFormat	= RO.LightmapFormat;
		DitherLightmaps = RO.DitherLightmaps;
		SaveOutLightmaps= RO.SaveOutLightmaps;
		ReportErrors	= RO.ReportErrors;
		return *this;
	}
};

// The editor will fill this struct in with the current rebuild options.
class FRebuildTools
{
public:
	FRebuildTools();
	~FRebuildTools();

	void Init();
	void Shutdown();

	inline FRebuildOptions* GetCurrent() { return Current; }
	void SetCurrent( FString InName );
	FRebuildOptions* GetFromName( FString InName );
	FRebuildOptions* Save( FString InName );
	void Delete( FString InName );
	INT GetIdxFromName( FString InName );

	FRebuildOptions* Current;			// A scratch pad representing the current values in the rebuild dialog
	TArray<FRebuildOptions> Options;	// All the saved configs
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

