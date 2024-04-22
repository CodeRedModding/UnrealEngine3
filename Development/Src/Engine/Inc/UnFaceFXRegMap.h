/*=============================================================================
	UnFaceFXRegMap.h: FaceFX Face Graph register support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_FACEFXREGMAP
#define _INC_FACEFXREGMAP
// A structure that associates an Unreal FName with a FaceFX FxName.
struct FFaceFXRegMapEntry
{
	FFaceFXRegMapEntry( const FName& InUnrealRegName, 
		const OC3Ent::Face::FxName& InFaceFXRegName )
	{
		UnrealRegName = InUnrealRegName;
		FaceFXRegName = InFaceFXRegName;
	}

	FName UnrealRegName;
	OC3Ent::Face::FxName FaceFXRegName;
};

// Maintains a mapping of Unreal FNames to FaceFX FxNames for FaceFX registers
// resulting in increased performance for the FaceFX register system in Unreal.
class FFaceFXRegMap
{
public:
	// Initialize the register map system.
	static void Startup( void );
	// Shut down the register map system.
	static void Shutdown( void );

	// Returns a pointer to the register map entry associated with the given
	// register FName.  Note that you should not maintain a reference to this 
	// pointer as it is a pointer into a dynamic array.  If the entry does not
	// exist this function returns NULL.
	static FFaceFXRegMapEntry* GetRegisterMapping( const FName& RegName );

	// Adds a register map entry for the given register FName.  This will not
	// add duplicate entries.
	static void AddRegisterMapping( const FName& RegName );

protected:
	// The register map.
	static TArray<FFaceFXRegMapEntry> RegMap;
};

#endif
