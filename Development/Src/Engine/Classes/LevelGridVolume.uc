/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LevelGridVolume extends Volume
	native
	dependson(KMeshProps)
	hidecategories(Advanced,Attachment,Collision,Volume,Physics,Location)
	autoexpandcategories(LevelGridVolume)
	placeable;



/** Structure contains coordinates for a single grid cell */
struct native LevelGridCellCoordinate
{
	/** Cell X coordinate */
	var int X;

	/** Cell Y coordinate */
	var int Y;

	/** Cell Z coordinate */
	var int Z;

	structcpptext
	{
		/** Constructor */
		FLevelGridCellCoordinate()
			: X( 0 ), Y( 0 ), Z( 0 )
		{
		}

		/** Equality operator */
		UBOOL operator==( const FLevelGridCellCoordinate& RHS ) const
		{
			return ( RHS.X == X && RHS.Y == Y && RHS.Z == Z );
		}
	}
};



/** Possible shapes for grid cells */
enum LevelGridCellShape
{
	/** Axis-aligned boxes */
	LGCS_Box,

	/** Hexagonal prism */
	LGCS_Hex
};


/** Name of this level grid volume, which is also the prefix for level names created for volume.  If empty, the level grid volume actor's name will be used instead.  You should set this name before placing any actors into the level, and never change it afterwards! */
var() const string LevelGridVolumeName;

/** Shape of the cells this grid is composed of */
var() const LevelGridCellShape CellShape;

/** The number of streaming volumes should the grid be subdivided into along each axis.  Be careful when changing this after actors have been added to the level grid volume! */
var() const int Subdivisions[ 3 ];

/*
* Width of each grid cell (X axis size.)  Be careful when changing this after actors have been added to the level grid volume! /
var() const int CellWidth;

* Depth of each grid cell (Y axis size.)  Be careful when changing this after actors have been added to the level grid volume! /
var() const int CellDepth;

* Height of each grid cell (Z axis size.)  Be careful when changing this after actors have been added to the level grid volume! /
var() const int CellHeight;

* Location offset for all grid cells in the map.  Be careful when changing this after actors have been added to the level grid volume! /
var() const vector GridOffset;
*/


/** Minimum distance between a grid cell and the viewer before a cell's level will be queued to stream in */
var() const float LoadingDistance;

/** Extra distance before the LoadingDistance which levels should stay loaded.  This can be used to prevent a level from continuously being loaded and unloaded as the viewer's distance to the cell crosses the LoadingDistance threshold. */
var() const float KeepLoadedRange;

/** Grid cell convex shape, used for fast distance tests */
var const transient KConvexElem CellConvexElem;



cpptext
{
	/**
	 * Gets the "friendly" name of this grid volume
	 *
	 * @return	The name of this grid volume
	 */
	FString GetLevelGridVolumeName() const;


	/**
	 * UObject: Performs operations after the object is loaded
	 */
	virtual void PostLoad();


	/**
	 * UObject: Called when a property value has been changed in the editor.
	 *
	 * @param	PropertyThatChanged		The property that changed, or NULL
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);


	/**
	 * Computes the world space bounds of the entire grid
	 *
	 * @return	Bounds of the grid in world space
	 */
	FBox GetGridBounds() const;


	/**
	 * Computes the size of a grid subdivision (not necessarily the same as a grid cell's bounds!)
	 *
	 * @return	Size of a grid subdivision
	 */
	FVector GetGridCellSubdivisionSize() const;


	/**
	 * Computes the size of a single grid cell
	 *
	 * @return	Size of the cell
	 */
	FVector GetGridCellSize() const;



	/**
	 * Computes the world space bounds of a single grid cell
	 *
	 * @param	InCoords	Coordinate of cell to compute bounds for
	 *
	 * @return	Bounds of the cell in world space
	 */
	FBox GetGridCellBounds( const FLevelGridCellCoordinate& InCoords ) const;


	/**
	 * Updates the convex volume that represents the shape of a single cell within this volume.
	 * Important: The convex volume is centered about the origin and not relative to any volume or cell!
	 */
	void UpdateConvexCellVolume();


	/**
	 * Computes the center point of a grid cell
	 *
	 * @param	InCoords	Coordinate of cell to compute bounds for
	 *
	 * @return	Center point of the cell in world space
	 */
	FVector GetGridCellCenterPoint( const FLevelGridCellCoordinate& InCoords ) const;


	/**
	 * Computes the 2D shape of a hex cell for this volume
	 *
	 * @param	OutHexPoints	Array that will be filled in with the 6 hexagonal points
	 */
	void ComputeHexCellShape( FVector2D* OutHexPoints ) const;


	/**
	 * Gets all levels associated with this level grid volume (not including the P level)
	 *
	 * @param	OutLevels	List of levels (out)
	 */
	void GetLevelsForAllCells( TArray< class ULevelStreaming* >& OutLevels ) const;


	/**
	 * Finds the level for the specified cell coordinates
	 *
	 * @param	InCoords	Grid cell coordinates
	 *
	 * @return	Level streaming record for level at the specified coordinates, or NULL if not found
	 */
	class ULevelStreaming* FindLevelForGridCell( const FLevelGridCellCoordinate& InCoords ) const;


	/**
	 * Returns true if the specified actor belongs in this grid network
	 *
	 * @param	InActor		The actor to check
	 *
	 * @return	True if the actor belongs in this grid network
	 */
	UBOOL IsActorMemberOfGrid( AActor* InActor ) const;


	/**
	 * Returns true if the specified cell is 'usable'.  That is, the bounds of the cell overlaps the actual
	 * level grid volume's brush
	 *
	 * @return	True if the specified cell is 'usable'
	 */
	UBOOL IsGridCellUsable( const FLevelGridCellCoordinate& InCellCoord ) const;


	/**
	 * Computes the grid cell that a box should be associated with based on the cell that it most
	 * overlaps.  If the box doesn't overlap any cells but bMustOverlap is false, then the function
	 * will choose the cell that's closest to the box.
	 *
	 * @param	InBox			The box to test
	 * @param	bMustOverlap	True if the box must overlap a cell for the function to succeed
	 * @param	OutBestCell		(Out) The best cell for the box
	 *
	 * @return	True if a cell was found for the box.  If bMustOverlap is false, the function will always return true.
	 */
	UBOOL FindBestGridCellForBox( const FBox& InBox, const UBOOL bMustOverlap, FLevelGridCellCoordinate& OutBestCell ) const;


	/**
	 * Checks to see if an AABB overlaps the specified grid cell
	 *
	 * @param	InCellCoord		The grid cell coordinate to test against
	 * @param	InBox			The world space AABB to test
	 *
	 * @return	True if the box overlaps the grid cell
	 */
	UBOOL TestWhetherCellOverlapsBox( const FLevelGridCellCoordinate& InCellCoord, const FBox& InBox ) const;


	/**
	 * Computes the minimum distance between the specified point and grid cell in world space
	 *
	 * @param	InCellCoord		The grid cell coordinate to test against
	 * @param	InPoint			The world space location to test
	 *
	 * @return	Squared distance to the cell
	 */
	FLOAT ComputeSquaredDistanceToCell( const FLevelGridCellCoordinate& InCellCoord, const FVector& InPoint ) const;


	/**
	 * Determines whether or not the level associated with the specified grid cell should be loaded based on
	 * distance to the viewer's position
	 *
	 * @param	InCellCoord			The grid cell coordinate associated with the level we're testing
	 * @param	InViewLocation		The viewer's location
	 * @param	bIsAlreadyLoaded	Pass true if the associated level is already loaded, otherwise false.  This is used to determine whether we should keep an already-loaded level in memory based on a configured distance threshold.
	 *
	 * @return	True if level should be loaded (or for already-loaded levels, should stay loaded)
	 */
	UBOOL ShouldLevelBeLoaded( const FLevelGridCellCoordinate& InCellCoord, const FVector& InViewLocation, const UBOOL bIsAlreadyLoaded ) const;


	/**
	 * AActor: Checks this actor for errors.  Called during the map check phase in the editor.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}


defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=False
		BlockActors=False
		BlockZeroExtent=False
		BlockNonZeroExtent=False
		BlockRigidBody=False
	End Object

	Begin Object Class=LevelGridVolumeRenderingComponent Name=LevelGridVolumeRenderer
	End Object
	Components.Add(LevelGridVolumeRenderer)

	bColored=true

	// Grey brush
	BrushColor=(R=80,G=80,B=80,A=255)

	bCollideActors=False
	bBlockActors=False
	bProjTarget=False
	SupportedEvents.Empty

	CellShape=LGCS_Box
	Subdivisions[0]=1
	Subdivisions[1]=1
	Subdivisions[2]=1

	LoadingDistance=20480
	KeepLoadedRange=2048
}
