/*=============================================================================
	LevelGridVolume.cpp: Level grid volume
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "DebugRenderSceneProxy.h"


IMPLEMENT_CLASS( ALevelGridVolume );



namespace GridVolumeDefs
{
	// Axis-aligned width of a single tapered hexagon edge expressed as a scalar percent of the cell's size
	// Note: This is a scalar percent of a "naturally" subdivided grid cell, not an "adjusted" subdivided grid cell.
	const FLOAT GTaperedHexEdgeWidthPercent = 0.3f;
}


/**
 * Gets the "friendly" name of this grid volume
 *
 * @return	The name of this grid volume
 */
FString ALevelGridVolume::GetLevelGridVolumeName() const
{
	// Select a name for the level.  Usually this is specified by the user as a property of the
	// grid volume.
	if( LevelGridVolumeName.Len() > 0 )
	{
		return LevelGridVolumeName;
	}

	// Use the object name if the user didn't supply a name
	return GetName();
}



/**
 * UObject: Performs operations after the object is loaded
 */
void ALevelGridVolume::PostLoad()
{
	// Call parent implementation
	Super::PostLoad();

	// Fill in the grid volume's convex cell structure
	// @todo perf: Consider serializing this out instead of constructing in game at load time
	UpdateConvexCellVolume();
}


/**
 * UObject: Called when a property value has been changed in the editor.
 *
 * @param	PropertyThatChanged		The property that changed, or NULL
 */
void ALevelGridVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Call parent implementation
	AVolume::PostEditChangeProperty( PropertyChangedEvent );


	if( GIsEditor && !GIsPlayInEditorWorld )
	{
		// Level grid volume has changed, so make sure that all actors get updated if the user
		// has enabled the setting that forces updates to actors immediately
		GCallbackEvent->Send( CALLBACK_UpdateLevelsForAllActors );
								 
		// Make sure the level browser gets refreshed
		GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

		// Update the grid volume's convex cell structure, as the user may have changed the
		// size/shape of a grid cell or the volume's extents
		UpdateConvexCellVolume();
	}
}




/**
 * Computes the world space bounds of the entire grid
 *
 * @return	Bounds of the grid in world space
 */
FBox ALevelGridVolume::GetGridBounds() const
{
	// @todo perf: Should cache these bounds for faster perf in game
	FBox Bounds(0);

	// Also process the editor grid volume associated with this LevelStreaming object, if there is one
	if( BrushComponent != NULL )
	{
		const FMatrix& BrushTM = BrushComponent->LocalToWorld;

		// Iterate over each convex piece that makes up this volumes
		for(INT ConIdx=0; ConIdx < BrushComponent->BrushAggGeom.ConvexElems.Num(); ConIdx++)
		{
			FKConvexElem& ConvElem = BrushComponent->BrushAggGeom.ConvexElems(ConIdx);

			// Expand bounds to include each vertex (in world space) of this convex piece
			for(INT VertIdx=0; VertIdx<ConvElem.VertexData.Num(); VertIdx++)
			{
				Bounds += BrushTM.TransformFVector(ConvElem.VertexData(VertIdx));
			}
		}
	}

	return Bounds;
}


/**
 * Computes the size of a grid subdivision (not necessarily the same as a grid cell's bounds!)
 *
 * @return	Size of a grid subdivision
 */
FVector ALevelGridVolume::GetGridCellSubdivisionSize() const
{
	// Grab the volume's bounding box.  Grid volumes are always treated as axis aligned boxes.
	FBox GridBounds = GetGridBounds();

	// If this is a hex grid, shrink the bounds such that we don't overflow horizontally
	// outside of the grid volume
	if( CellShape == LGCS_Hex )
	{
		const FLOAT NaturalCellWidth = ( GridBounds.Max.X - GridBounds.Min.X ) / (FLOAT)Subdivisions[ 0 ];
		GridBounds.Max.X -= NaturalCellWidth * GridVolumeDefs::GTaperedHexEdgeWidthPercent;
	}
	
	// Compute the dimension of each cell
	const FVector CellSize( ( GridBounds.Max.X - GridBounds.Min.X ) / (FLOAT)Subdivisions[ 0 ],
							( GridBounds.Max.Y - GridBounds.Min.Y ) / (FLOAT)Subdivisions[ 1 ],
							( GridBounds.Max.Z - GridBounds.Min.Z ) / (FLOAT)Subdivisions[ 2 ] );

	return CellSize;
}


/**
 * Computes the size of a single grid cell
 *
 * @return	Size of the cell
 */
FVector ALevelGridVolume::GetGridCellSize() const
{
	// Grab the volume's bounding box.  Grid volumes are always treated as axis aligned boxes.
	const FBox GridBounds = GetGridBounds();
	
	// Compute the dimension of each cell subdivision
	const FVector SubdivSize = GetGridCellSubdivisionSize();

	FVector CellSize = SubdivSize;

	if( CellShape == LGCS_Hex )
	{
		// For hexes, we expand the bounds to cover the cell to the right of us
		const FLOAT NaturalCellWidth = ( GridBounds.Max.X - GridBounds.Min.X ) / (FLOAT)Subdivisions[ 0 ];
		CellSize.X += NaturalCellWidth * GridVolumeDefs::GTaperedHexEdgeWidthPercent;
	}

	return CellSize;
}




/**
 * Computes the world space bounds of a single grid cell
 *
 * @param	InCoords	Coordinate of cell to compute bounds for
 *
 * @return	Bounds of the cell in world space
 */
FBox ALevelGridVolume::GetGridCellBounds( const FLevelGridCellCoordinate& InCoords ) const
{
	check( InCoords.X < Subdivisions[ 0 ] && InCoords.Y < Subdivisions[ 1 ] && InCoords.Z < Subdivisions[ 2 ] );
	check( Subdivisions[ 0 ] > 0 && Subdivisions[ 1 ] > 0 && Subdivisions[ 2 ] > 0 );

	// Grab the volume's bounding box.  Grid volumes are always treated as axis aligned boxes.
	const FBox GridBounds = GetGridBounds();
	
	// Compute the dimension of each cell subdivision
	const FVector SubdivSize = GetGridCellSubdivisionSize();

	// Compute cell's bounds in world space
	FBox CellBounds;
	CellBounds.Min.X = GridBounds.Min.X + SubdivSize.X * InCoords.X;
	CellBounds.Min.Y = GridBounds.Min.Y + SubdivSize.Y * InCoords.Y;
	CellBounds.Min.Z = GridBounds.Min.Z + SubdivSize.Z * InCoords.Z;

	// Compute the dimension of each cell
	const FVector CellSize = GetGridCellSize();
	CellBounds.Max = CellBounds.Min + CellSize;


	if( CellShape == LGCS_Hex )
	{
		// If this volume is made up of hexagonal prisms, then odd columns are shifted forward
		// by half of the depth of a cell
		if( InCoords.X % 2 == 1 )
		{
			const FLOAT HalfCellDepth = CellSize.Y * 0.5f;
			CellBounds.Min.Y += HalfCellDepth;
			CellBounds.Max.Y += HalfCellDepth;
		}
	}

	return CellBounds;
}



/**
 * Computes the center point of a grid cell
 *
 * @param	InCoords	Coordinate of cell to compute bounds for
 *
 * @return	Center point of the cell in world space
 */
FVector ALevelGridVolume::GetGridCellCenterPoint( const FLevelGridCellCoordinate& InCoords ) const
{
	check( InCoords.X < Subdivisions[ 0 ] && InCoords.Y < Subdivisions[ 1 ] && InCoords.Z < Subdivisions[ 2 ] );
	check( Subdivisions[ 0 ] > 0 && Subdivisions[ 1 ] > 0 && Subdivisions[ 2 ] > 0 );

	const FBox GridBounds = GetGridCellBounds( InCoords );
	return GridBounds.GetCenter();
}



/**
 * Computes the 2D shape of a hex cell for this volume
 *
 * @param	OutHexPoints	Array that will be filled in with the 6 hexagonal points
 */
void ALevelGridVolume::ComputeHexCellShape( FVector2D* OutHexPoints ) const
{
	const FBox GridBounds = GetGridBounds();

	const FVector SubdivSize = GetGridCellSubdivisionSize();
	const FVector CellSize = GetGridCellSize();

	// Compute the dimension of each cell subdivision
	const FLOAT NaturalCellWidth = ( GridBounds.Max.X - GridBounds.Min.X ) / (FLOAT)Subdivisions[ 0 ];
	const FLOAT H = NaturalCellWidth * GridVolumeDefs::GTaperedHexEdgeWidthPercent;
	const FLOAT Side = CellSize.X - ( H * 2.0f );
	const FLOAT R = SubdivSize.Y * 0.5f;

	//const FLOAT Side = 0.5f;
	//const FLOAT H = appSin( 30.0f / 180.0f * PI ) * Side;
	//const FLOAT R = appCos( 30.0f / 180.0f * PI ) * Side;

	OutHexPoints[ 0 ].Set( H,					0.0f );
	OutHexPoints[ 1 ].Set( H + Side,			0.0f );
	OutHexPoints[ 2 ].Set( 2.0f * H + Side,		R );
	OutHexPoints[ 3 ].Set( H + Side,			2.0f * R );
	OutHexPoints[ 4 ].Set( H,					2.0f * R );
	OutHexPoints[ 5 ].Set( 0.0f,				R );

	// Center the points within the hex
	for( INT CurPointIndex = 0; CurPointIndex < 6; ++CurPointIndex )
	{
		OutHexPoints[ CurPointIndex ] -= FVector2D( CellSize.X, CellSize.Y ) * 0.5f;
	}
}



/**
 * Updates the convex volume that represents the shape of a single cell within this volume.
 * Important: The convex volume is centered about the origin and not relative to any volume or cell!
 */
void ALevelGridVolume::UpdateConvexCellVolume()
{
	// Compute the dimension of each cell
	const FVector CellSize = GetGridCellSize();
	const FVector HalfCellSize = CellSize * 0.5f;

	// Grab the 2D shape of this volume's hex cells
	FVector2D HexPoints[ 6 ];
	ComputeHexCellShape( HexPoints );

	TArray< FPlane > Planes;
	{
		if( CellShape == LGCS_Box )
		{
			// Box volume
			Planes.AddItem( FPlane( FVector( 1.0f, 0.0f, 0.0f ), -HalfCellSize.X ) );
			Planes.AddItem( FPlane( FVector( 0.0f, 1.0f, 0.0f ), -HalfCellSize.Y ) );
			Planes.AddItem( FPlane( FVector( 0.0f, 0.0f, 1.0f ), -HalfCellSize.Z ) );
			Planes.AddItem( FPlane( FVector( -1.0f, 0.0f, 0.0f ), HalfCellSize.X ) );
			Planes.AddItem( FPlane( FVector( 0.0f, -1.0f, 0.0f ), HalfCellSize.Y ) );
			Planes.AddItem( FPlane( FVector( 0.0f, 0.0f, -1.0f ), HalfCellSize.Z ) );
		}
		else if( CellShape == LGCS_Hex )
		{
			// Hexagonal prism volume
			for( INT CurPointIndex = 0; CurPointIndex < ARRAY_COUNT( HexPoints ); ++CurPointIndex )
			{
				// Grab a single edge of the hex
				const FVector2D& CurHexPoint = HexPoints[ CurPointIndex ];
				const FVector2D& NextHexPoint = HexPoints[ ( CurPointIndex + 1 ) % ARRAY_COUNT( HexPoints ) ];

				// Compute a plane for this 2D edge
				const FVector2D EdgeVector = NextHexPoint - CurHexPoint;
				const FVector2D EdgeDir = EdgeVector.SafeNormal();
				const FVector UpVector( 0.0f, 0.0f, 1.0f );
 				const FVector PlaneNormal = FVector( EdgeDir, 0.0f ) ^ UpVector;

				// Compute distance to plane
				const FVector2D EdgeCenter = CurHexPoint + EdgeVector * 0.5f;
				//const FLOAT PlaneDist = EdgeCenter.Size();

 				Planes.AddItem( FPlane( FVector( EdgeCenter, 0.0f ), PlaneNormal ) );//, PlaneDist ) );
			}

			// Top and bottom
			Planes.AddItem( FPlane( FVector( 0.0f, 0.0f, HalfCellSize.Z ), FVector( 0.0f, 0.0f, 1.0f ) ) ); //, -HalfCellSize.Z ) );
			Planes.AddItem( FPlane( FVector( 0.0f, 0.0f, -HalfCellSize.Z ), FVector( 0.0f, 0.0f, -1.0f ) ) );
		}
	}


	// Init the convex hull from the set of planes
	appMemzero( &CellConvexElem, sizeof( FKConvexElem ) );
	TArray< FVector > DummySnap;
	CellConvexElem.HullFromPlanes( Planes, DummySnap );
}



/**
 * Gets all levels associated with this level grid volume (not including the P level)
 *
 * @param	OutLevels	List of levels (out)
 */
void ALevelGridVolume::GetLevelsForAllCells( TArray< ULevelStreaming* >& OutLevels ) const
{
	OutLevels.Reset();

	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	for( INT CurLevelIndex = 0; CurLevelIndex < WorldInfo->StreamingLevels.Num(); ++CurLevelIndex )
	{
		ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels( CurLevelIndex );
		if( ensure( LevelStreaming != NULL ) )
		{
			// Is this level streaming record associated with this grid volume?  
			if( LevelStreaming->EditorGridVolume == this )
			{
				OutLevels.AddItem( LevelStreaming );
			}
		}
	}	
}



/**
 * Finds the level for the specified cell coordinates
 *
 * @param	InCoords	Grid cell coordinates
 *
 * @return	Level streaming record for level at the specified coordinates, or NULL if not found
 */
ULevelStreaming* ALevelGridVolume::FindLevelForGridCell( const FLevelGridCellCoordinate& InCoords ) const
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	for( INT CurLevelIndex = 0; CurLevelIndex < WorldInfo->StreamingLevels.Num(); ++CurLevelIndex )
	{
		ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels( CurLevelIndex );
		if( ensure( LevelStreaming != NULL ) )
		{
			// Is this level streaming record associated with this grid volume?  And if so, does
			// it match the grid cell that we're looking for?
			if( LevelStreaming->EditorGridVolume == this &&
				LevelStreaming->GridPosition[ 0 ] == InCoords.X &&
				LevelStreaming->GridPosition[ 1 ] == InCoords.Y &&
				LevelStreaming->GridPosition[ 2 ] == InCoords.Z )
			{
				// Found it!
				return LevelStreaming;
			}
		}
	}	

	// Couldn't find a level streaming record for this cell
	return NULL;
}




/**
 * Returns true if the specified actor belongs in this grid network
 *
 * @param	InActor		The actor to check
 *
 * @return	True if the actor belongs in this grid network
 */
UBOOL ALevelGridVolume::IsActorMemberOfGrid( AActor* InActor ) const
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	for( INT CurLevelIndex = 0; CurLevelIndex < WorldInfo->StreamingLevels.Num(); ++CurLevelIndex )
	{
		ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels( CurLevelIndex );
		if( ensure( LevelStreaming != NULL ) )
		{
			if( LevelStreaming->EditorGridVolume == this )
			{
				if( LevelStreaming->LoadedLevel != NULL && LevelStreaming->LoadedLevel == InActor->GetLevel() )
				{
					// Actor's level is one of the grid's levels, so it belongs with us!
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}



/**
 * Returns true if the specified cell is 'usable'.  That is, the bounds of the cell overlaps the actual
 * level grid volume's brush
 *
 * @return	True if the specified cell is 'usable'
 */
UBOOL ALevelGridVolume::IsGridCellUsable( const FLevelGridCellCoordinate& InCellCoord ) const
{
	// Grab the cell's bounding box
	const FBox& GridCellBounds = GetGridCellBounds( InCellCoord );
	
	// Check to see if this grid cell within the volume's brush
	// @todo perf: Should cache this when volume shape changes for much better performance
	UBOOL bIsGridCellUsable = FALSE;
	if( BrushComponent != NULL )
	{
		FCheckResult CheckResult;
		const UBOOL bOverlapsBrush =
			!BrushComponent->PointCheck( CheckResult, GridCellBounds.GetCenter(), GridCellBounds.GetExtent(), TRACE_Accurate );
		bIsGridCellUsable = bOverlapsBrush;
	}

	// If this volume is made up of hexagonal prisms, then odd columns in the last row of
	// cells are outside of the volume's bounds and never usable
	if( CellShape == LGCS_Hex && InCellCoord.Y >= ( Subdivisions[ 1 ] - 1 ) && InCellCoord.X % 2 == 1 )
	{
		bIsGridCellUsable = FALSE;
	}

	return bIsGridCellUsable;
}



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
UBOOL ALevelGridVolume::FindBestGridCellForBox( const FBox& InBox, const UBOOL bMustOverlap, FLevelGridCellCoordinate& OutBestCell ) const
{
	UBOOL bFoundGridCell = FALSE;
	FLevelGridCellCoordinate BestGridCell;

	{
		TArray< FLevelGridCellCoordinate > OverlappingGridCells;
		FLevelGridCellCoordinate ClosestNonOverlappingGridCell;
		FLOAT ClosestNonOverlappingSquaredDistanceSoFar = BIG_NUMBER;

		const FVector AABBCenter = InBox.GetCenter();

		// For each cell in this volume
		// @todo perf: Perf: We could do this much faster by first computing the smallest range of cells
		for( INT CellX = 0; CellX < Subdivisions[ 0 ]; ++CellX )
		{
			for( INT CellY = 0; CellY < Subdivisions[ 1 ]; ++CellY )
			{
				for( INT CellZ = 0; CellZ < Subdivisions[ 2 ]; ++CellZ )
				{
					FLevelGridCellCoordinate CellCoord;
					CellCoord.X = CellX;
					CellCoord.Y = CellY;
					CellCoord.Z = CellZ;

					// Check to see if this grid cell within the volume's brush
					UBOOL bIsGridCellUsable = IsGridCellUsable( CellCoord );

					// Grab the cell's bounding box
					const FBox& GridCellBounds = GetGridCellBounds( CellCoord );
					
					// Only test against grid cells that actually overlap the precise shape of the volume.  This
					// allows level designers to shape their volumes such that levels will never be created for
					// grid cells outside the volume's shape, even if member actors are overlapping those cells
					if( bIsGridCellUsable )
					{
						// Does the box overlap the current grid cell?
						UBOOL bBoxIntersectsGridCell = TestWhetherCellOverlapsBox( CellCoord, InBox );
						if( bBoxIntersectsGridCell )
						{
							// Box overlaps the grid cell, so we'll keep track of that
							check( !OverlappingGridCells.ContainsItem( CellCoord ) );
							OverlappingGridCells.AddItem( CellCoord );
						}
						else
						{
							// Boxes don't overlap.

							// Compute the distance between the box's center and the cell
							const FLOAT SquaredDistanceToVolume = ComputeSquaredDistanceToCell( CellCoord, AABBCenter );

							// The box doesn't overlap the volume, but we'll still check to see if
							// this volume is the closest to the box so far
							if( SquaredDistanceToVolume < ClosestNonOverlappingSquaredDistanceSoFar )
							{
								ClosestNonOverlappingSquaredDistanceSoFar = SquaredDistanceToVolume;
								ClosestNonOverlappingGridCell = CellCoord;
							}
						}
					}
				}
			}
		}



		// Does the box overlap any grid cells?
		if( OverlappingGridCells.Num() > 0 )
		{
			// Is the box overlapping more than one grid cell?  If so, we'll do a more precise test
			// to figure out which volume the box is overlapping the most
			// @todo gridvolume: If the actor's bounds fully overlaps more than one cell, the result here will be ambiguous!
			if( OverlappingGridCells.Num() > 1 )
			{
				FLevelGridCellCoordinate MostOverlappedGridCell;
				FLOAT LargestOverlapArea = 0.0f;
				{
					for( INT CurOverlappingGridCellIndex = 0; CurOverlappingGridCellIndex < OverlappingGridCells.Num(); ++CurOverlappingGridCellIndex )
					{
						const FLevelGridCellCoordinate& OverlappingGridCell = OverlappingGridCells( CurOverlappingGridCellIndex );
						const FBox& CellBounds = GetGridCellBounds( OverlappingGridCell );

						// Compute the intersection of the two boxes
						const FVector IntMin( Max( CellBounds.Min.X, InBox.Min.X ),
											  Max( CellBounds.Min.Y, InBox.Min.Y ),
											  Max( CellBounds.Min.Z, InBox.Min.Z ) );
						const FVector IntMax( Min( CellBounds.Max.X, InBox.Max.X ),
											  Min( CellBounds.Max.Y, InBox.Max.Y ),
											  Min( CellBounds.Max.Z, InBox.Max.Z ) );
						const FBox Intersection( IntMin, IntMax );

						// Do the boxes overlap?
						const FLOAT OverlapArea = Intersection.GetVolume();
						if( OverlapArea > KINDA_SMALL_NUMBER )
						{
							if( LargestOverlapArea == 0.0f || OverlapArea > LargestOverlapArea )
							{
								MostOverlappedGridCell = OverlappingGridCell;
								LargestOverlapArea = OverlapArea;
							}
						}
					}
				}

				// Use the streaming volume that the actor overlaps the most
				BestGridCell = MostOverlappedGridCell;
				bFoundGridCell = TRUE;
			}
			else
			{
				// Only overlapping a single volume.  This will be easy!
				BestGridCell = OverlappingGridCells( 0 );
				bFoundGridCell = TRUE;
			}
		}
		else
		{
			// Actor doesn't overlap any volumes.  Were we asked to go ahead and find the closest
			// non-overlapping volume?
			if( !bMustOverlap )
			{
				// Use the closest non-overlapping volume
				BestGridCell = ClosestNonOverlappingGridCell;
				bFoundGridCell = TRUE;
			}
		}
	}

	if( bFoundGridCell )
	{
		OutBestCell = BestGridCell;
	}
	return bFoundGridCell;
}



/**
 * Checks to see if an AABB overlaps the specified grid cell
 *
 * @param	InCellCoord		The grid cell coordinate to test against
 * @param	InBox			The world space AABB to test
 *
 * @return	True if the box overlaps the grid cell
 */
UBOOL ALevelGridVolume::TestWhetherCellOverlapsBox( const FLevelGridCellCoordinate& InCellCoord, const FBox& InBox ) const
{
	UBOOL bBoxIntersectsGridCell = FALSE;

	if( CellShape == LGCS_Box )
	{
		// Grab the cell's bounding box
		const FBox& GridCellBounds = GetGridCellBounds( InCellCoord );

		// Quickly check to see if the cell bounds overlaps the box at
		// all by measuring the closest distance between the bounds and the volume
		bBoxIntersectsGridCell = GridCellBounds.Intersect( InBox );
	}
	else if( ensure( CellShape == LGCS_Hex ) )
	{
		// Move the box to the coordinate system of the cell volume shape
		const FVector& GridCellCenterPoint = GetGridCellCenterPoint( InCellCoord );
		FBox TestBoxInCellSpace = InBox;
		TestBoxInCellSpace.Min -= GridCellCenterPoint;
		TestBoxInCellSpace.Max -= GridCellCenterPoint;

		// @todo perf: Ideally cache this inside grid volume!  Kind of expensive to construct on stack.
		GJKHelperConvex ConvexHelper( CellConvexElem, FMatrix::Identity );

		// @todo perf: Constructing this oriented box might be slow?
		const FVector& BoxExtent = InBox.GetExtent();
		FOrientedBox OrientedBoxInCellSpace;
		{
			OrientedBoxInCellSpace.Center = InBox.GetCenter() - GridCellCenterPoint;
			OrientedBoxInCellSpace.ExtentX = BoxExtent.X;
			OrientedBoxInCellSpace.ExtentY = BoxExtent.Y;
			OrientedBoxInCellSpace.ExtentZ = BoxExtent.Z;
		}
		GJKHelperBox BoxHelper( OrientedBoxInCellSpace );

		// Quickly test the box against the cell's convex volume
		FVector ClosestPointToBoxInCellSpace;
		FVector ClosestPointToCellInCellSpace;
		GJKResult Result = ClosestPointsBetweenConvexPrimitives( &ConvexHelper, &BoxHelper, ClosestPointToCellInCellSpace /* Out */, ClosestPointToBoxInCellSpace /* Out */ );
		if( Result != GJK_Fail )
		{
			const FLOAT SquaredDistance = ( ClosestPointToCellInCellSpace - ClosestPointToBoxInCellSpace ).SizeSquared();
			bBoxIntersectsGridCell = ( SquaredDistance < KINDA_SMALL_NUMBER );
		}
	}

	return bBoxIntersectsGridCell;
}



/**
 * Computes the minimum distance between the specified point and grid cell in world space
 *
 * @param	InCellCoord		The grid cell coordinate to test against
 * @param	InPoint			The world space location to test
 *
 * @return	Squared distance to the cell
 */
FLOAT ALevelGridVolume::ComputeSquaredDistanceToCell( const FLevelGridCellCoordinate& InCellCoord, const FVector& InPoint ) const
{
	FLOAT SquaredDistanceToGridCell = BIG_NUMBER;
	if( CellShape == LGCS_Box )
	{
		// Grab the bounds of the volume's grid cell that's associated with the current level
		const FBox& GridCellBounds = GetGridCellBounds( InCellCoord );

		// Compute the squared distance between the point and the grid cell
		SquaredDistanceToGridCell = GridCellBounds.ComputeSquaredDistanceToPoint( InPoint );
	}
	else if( ensure( CellShape == LGCS_Hex ) )
	{
		// Translate the point to the cell's position so we can use a single volume for any cell
		const FVector& CellCenterPoint = GetGridCellCenterPoint( InCellCoord );
		FVector PointLocationInCellSpace = InPoint - CellCenterPoint;

		// @todo perf: Ideally cache this inside grid volume!  Kind of expensive to construct on stack.
		GJKHelperConvex ConvexHelper( CellConvexElem, FMatrix::Identity );

		// Figure out how far away the point is from the cell
		FVector ClosestPointToCellInCellSpace;
		GJKResult Result = ClosestPointOnConvexPrimitive( PointLocationInCellSpace, &ConvexHelper, ClosestPointToCellInCellSpace /* Out */ );
		if( Result != GJK_Fail )
		{
			const FVector ClosestPointToCell = ClosestPointToCellInCellSpace + CellCenterPoint;
			SquaredDistanceToGridCell = ( ClosestPointToCell - InPoint ).SizeSquared();
		}
	}

	return SquaredDistanceToGridCell;
}



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
UBOOL ALevelGridVolume::ShouldLevelBeLoaded( const FLevelGridCellCoordinate& InCellCoord, const FVector& InViewLocation, const UBOOL bIsAlreadyLoaded ) const
{
	// Figure out if this level should be streamed in
	UBOOL bShouldBeLoaded = FALSE;


	// Compute distance between the viewer and the cell
	const FLOAT SquaredDistanceToGridCell = ComputeSquaredDistanceToCell( InCellCoord, InViewLocation );


	// Is the viewer close enough to the grid cell to start streaming in the level?
	const FLOAT SquaredStreamingDistance = LoadingDistance * LoadingDistance;
	const UBOOL bViewerWithinLoadingDistance = SquaredDistanceToGridCell <= SquaredStreamingDistance;
	if( bViewerWithinLoadingDistance )
	{
		// Viewer is within the cell's loading distance.  It should be streamed in.
		bShouldBeLoaded = TRUE;
	}

	// Otherwise, we'll check to see if we need to keep the level in memory (if already loaded.)
	else if( KeepLoadedRange > KINDA_SMALL_NUMBER )
	{
		// Is the level already loaded?
		if( bIsAlreadyLoaded )
		{
			// The viewer isn't within the grid cell's loading distance, but we'll check to make
			// sure the viewer is far enough away from the cell to begin unloading the level
			const FLOAT KeepLoadedDistance = LoadingDistance + KeepLoadedRange;
			const FLOAT SquaredKeepLoadedDistance = KeepLoadedDistance * KeepLoadedDistance;
			const UBOOL bViewerWithinKeepLoadedDistance = SquaredDistanceToGridCell <= SquaredKeepLoadedDistance;

			if( bViewerWithinKeepLoadedDistance )
			{
				// Viewer is close enough to the loading distance that we should keep the
				// already-loaded level still loaded
				bShouldBeLoaded = TRUE;
			}
		}
	}

	return bShouldBeLoaded;
}



/**
 * AActor: Checks this actor for errors.  Called during the map check phase in the editor.
 */
#if WITH_EDITOR
void ALevelGridVolume::CheckForErrors()
{
	Super::CheckForErrors();

	// Streaming level volumes are not permitted outside the persistent level.
	if ( GetLevel() != GWorld->PersistentLevel )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString( LocalizeUnrealEd( "MapCheck_Message_LevelGridVolumes" ) ), TEXT( "LevelGridVolume" ) );
	}
}
#endif

/** Represents a LevelGridVolumeRenderingComponent to the scene manager. */
class FLevelGridVolumeRenderingSceneProxy
	: public FDebugRenderSceneProxy
{

public:

	FLevelGridVolumeRenderingSceneProxy( const ULevelGridVolumeRenderingComponent* InComponent )
		: FDebugRenderSceneProxy( InComponent )
	{
		ALevelGridVolume* LevelGridVolume = Cast<ALevelGridVolume>( InComponent->GetOwner() );
		if( ensure( LevelGridVolume != NULL ) )
		{
			// For box grids, draw the volume's grid subdivisions as dashed lines
			if( LevelGridVolume->CellShape == LGCS_Box )
			{
				const FBox GridBounds = LevelGridVolume->GetGridBounds();

				const FColor LineColor( 40, 40, 40 );	// Grey
				const FLOAT DashSize = 32.0f;
				for( INT SpanAxis = 0; SpanAxis < 3; ++SpanAxis )
				{
					const INT CurAxis = SpanAxis == 0 ? 1 : 0;
					const INT DepthAxis = SpanAxis == 2 ? 1 : 2;

					const INT AxisCellCount = LevelGridVolume->Subdivisions[ CurAxis ];
					const FLOAT AxisCellSize = ( GridBounds.Max[ CurAxis ] - GridBounds.Min[ CurAxis ] ) / (FLOAT)AxisCellCount;
					const INT AxisLineCount = AxisCellCount + 1;

					const INT DepthAxisCellCount = LevelGridVolume->Subdivisions[ DepthAxis ];
					const FLOAT DepthAxisCellSize = ( GridBounds.Max[ DepthAxis ] - GridBounds.Min[ DepthAxis ] ) / (FLOAT)DepthAxisCellCount;
					const INT DepthAxisLineCount = DepthAxisCellCount + 1;

					for( INT CurDepthIndex = 0; CurDepthIndex < DepthAxisLineCount; ++CurDepthIndex )
					{
						for( INT CurLineIndex = 0; CurLineIndex < AxisLineCount; ++CurLineIndex )
						{
							const FLOAT LinePosAlongAxis = GridBounds.Min[ CurAxis ] + CurLineIndex * AxisCellSize;
							const FLOAT LinePosAlongDepthAxis = GridBounds.Min[ DepthAxis ] + CurDepthIndex * DepthAxisCellSize;

	// 						// Adjust lines that overlap the volume's bounding box to reduce the chance of z-fighting
	// 						if( ( CurDepthIndex > 0 && CurDepthIndex < ( DepthAxisLineCount - 1 ) ) ||
	// 							( CurLineIndex > 0 && CurLineIndex < ( AxisLineCount - 1 ) ) )
							{
								FVector LineStart;
								LineStart[ CurAxis ] = LinePosAlongAxis;
								LineStart[ DepthAxis ] = LinePosAlongDepthAxis;
								LineStart[ SpanAxis ] = GridBounds.Min[ SpanAxis ];

								FVector LineEnd;
								LineEnd[ CurAxis ] = LinePosAlongAxis;
								LineEnd[ DepthAxis ] = LinePosAlongDepthAxis;
								LineEnd[ SpanAxis ] = GridBounds.Max[ SpanAxis ];

								// new( DashedLines ) FDashedLine( LineStart, LineEnd, LineColor, DashSize );
								new( Lines ) FDebugLine( LineStart, LineEnd, LineColor );
							}
						}
					}
				}
			}




			// Draw grid cells
			{
				const FColor NormalCellColor( 127, 127, 127 );	// Grey
				const FColor LoadedCellColor( 40, 200, 40 );	// Bright green

				// Construct the hexagon vertices
				FVector2D HexPoints[ 6 ];
				LevelGridVolume->ComputeHexCellShape( HexPoints );	// Out
				
				// For each cell in this volume
				for( INT CellX = 0; CellX < LevelGridVolume->Subdivisions[ 0 ]; ++CellX )
				{
					for( INT CellY = 0; CellY < LevelGridVolume->Subdivisions[ 1 ]; ++CellY )
					{
						for( INT CellZ = 0; CellZ < LevelGridVolume->Subdivisions[ 2 ]; ++CellZ )
						{
							FLevelGridCellCoordinate CellCoord;
							CellCoord.X = CellX;
							CellCoord.Y = CellY;
							CellCoord.Z = CellZ;

							// Grab the cell's bounding box
							const FBox& GridCellBounds = LevelGridVolume->GetGridCellBounds( CellCoord );
							
							// @todo: Draw level SCC status (also update regularly)

							// Is a level for this cell currently loaded?  If so, we'll draw the cell
							// bounds in a different color
							UBOOL bLevelExistsForCell = FALSE;
							{
								ULevelStreaming* AssociatedLevelStreaming = LevelGridVolume->FindLevelForGridCell( CellCoord );
								if( AssociatedLevelStreaming != NULL && AssociatedLevelStreaming->LoadedLevel != NULL )
								{
									bLevelExistsForCell = TRUE;
								}
							}

							// Check to see if this grid cell within the volume's brush
							UBOOL bIsGridCellUsable = LevelGridVolume->IsGridCellUsable( CellCoord );

							// Skip drawing the cell if it's outside the brush volume and not already in use
							const UBOOL bDrawCellBounds = FALSE;
							const UBOOL bDrawCellFloor = TRUE;
							const UBOOL bDrawCellCeiling = TRUE;
							if( bLevelExistsForCell || bIsGridCellUsable )
							{
								const FColor CellColor = bLevelExistsForCell != FALSE ? LoadedCellColor : NormalCellColor;
								const FVector CellSize = GridCellBounds.GetExtent() * 2.0f;

								// Shrink the cell's bounding box a bit to avoid z-fighting with volume grid display
								const FLOAT ShrinkPercent = 0.015f;
								const FBox TightenedCellBounds( GridCellBounds.Min + CellSize * ShrinkPercent, GridCellBounds.Max - CellSize * ShrinkPercent );

								if( bDrawCellBounds )
								{
									new( WireBoxes ) FDebugBox( TightenedCellBounds, CellColor );
								}

								if( LevelGridVolume->CellShape == LGCS_Box )
								{
									if( bDrawCellFloor )
									{
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Min.Y, GridCellBounds.Min.Z ), FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Max.Y, GridCellBounds.Min.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Max.Y, GridCellBounds.Min.Z ), FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Max.Y, GridCellBounds.Min.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Max.Y, GridCellBounds.Min.Z ), FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Min.Y, GridCellBounds.Min.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Min.Y, GridCellBounds.Min.Z ), FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Min.Y, GridCellBounds.Min.Z ), CellColor );
									}

									if( bDrawCellCeiling )
									{
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Min.Y, GridCellBounds.Max.Z ), FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Max.Y, GridCellBounds.Max.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Max.Y, GridCellBounds.Max.Z ), FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Max.Y, GridCellBounds.Max.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Max.Y, GridCellBounds.Max.Z ), FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Min.Y, GridCellBounds.Max.Z ), CellColor );
										new( Lines ) FDebugLine( FVector( TightenedCellBounds.Max.X, TightenedCellBounds.Min.Y, GridCellBounds.Max.Z ), FVector( TightenedCellBounds.Min.X, TightenedCellBounds.Min.Y, GridCellBounds.Max.Z ), CellColor );
									}
								}
								else if( ensure( LevelGridVolume->CellShape == LGCS_Hex ) )
								{
									// Grab the center of the grid cell
									const FVector GridCellCenter = LevelGridVolume->GetGridCellCenterPoint( CellCoord );
									const FVector GridCellFloorCenter( GridCellCenter.X, GridCellCenter.Y, GridCellBounds.Min.Z );
									const FVector GridCellCeilingCenter( GridCellCenter.X, GridCellCenter.Y, GridCellBounds.Max.Z );

									for( INT CurPointIndex = 0; CurPointIndex < ARRAY_COUNT( HexPoints ); ++CurPointIndex )
									{
										const FVector2D& CurHexPoint = HexPoints[ CurPointIndex ];
										const FVector2D& NextHexPoint = HexPoints[ ( CurPointIndex + 1 ) % ARRAY_COUNT( HexPoints ) ];

										// Floor
										if( bDrawCellFloor || bDrawCellCeiling )
										{
											// Push the points toward the cell center a bit just to avoid z-fighting
											// between adjacent cells
											FVector VertA = GridCellFloorCenter + FVector( CurHexPoint.X, CurHexPoint.Y, 0.0f );
											FVector VertB = GridCellFloorCenter + FVector( NextHexPoint.X, NextHexPoint.Y, 0.0f );
											VertA = VertA + ( GridCellFloorCenter - VertA ) * ShrinkPercent;
											VertB = VertB + ( GridCellFloorCenter - VertB ) * ShrinkPercent;

											new( Lines ) FDebugLine( VertA, VertB, CellColor );
										}

										// Ceiling
										if( bDrawCellCeiling )
										{
											// Push the points toward the cell center a bit just to avoid z-fighting
											// between adjacent cells
											FVector VertA = GridCellCeilingCenter + FVector( CurHexPoint.X, CurHexPoint.Y, 0.0f );
											FVector VertB = GridCellCeilingCenter + FVector( NextHexPoint.X, NextHexPoint.Y, 0.0f );
											VertA = VertA + ( GridCellCeilingCenter - VertA ) * ShrinkPercent;
											VertB = VertB + ( GridCellCeilingCenter - VertB ) * ShrinkPercent;

											new( Lines ) FDebugLine( VertA, VertB, CellColor );
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance( const FSceneView* View )
	{
		const UBOOL bVisible = ( View->Family->ShowFlags & SHOW_Volumes ) != 0;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && bVisible;
		Result.SetDPG(SDPG_World,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FDebugRenderSceneProxy::GetAllocatedSize() ); }
};




IMPLEMENT_CLASS( ULevelGridVolumeRenderingComponent );


/**
 * Creates a new scene proxy for the rendering component.
 * @return	Pointer to the FLevelGridVolumeRenderingSceneProxy
 */
FPrimitiveSceneProxy* ULevelGridVolumeRenderingComponent::CreateSceneProxy()
{
	return new FLevelGridVolumeRenderingSceneProxy( this );
}



/** Sets the bounds of this primitive */
void ULevelGridVolumeRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);

	ALevelGridVolume* LevelGridVolume = Cast<ALevelGridVolume>( Owner );
	if( ensure( LevelGridVolume != NULL ) )
	{
		BoundingBox = LevelGridVolume->GetGridBounds();
	}

	Bounds = FBoxSphereBounds(BoundingBox);
}


