//=============================================================================
// The brush class.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Brush extends Actor
	hidecategories(Object)
	hidecategories(Movement)
	hidecategories(Display)
	native;

//-----------------------------------------------------------------------------
// Variables.

// CSG operation performed in editor.
var() enum ECsgOper
{
	CSG_Active,			// Active brush.
	CSG_Add,			// Add to world.
	CSG_Subtract,		// Subtract from world.
	CSG_Intersect,		// Form from intersection with world.
	CSG_Deintersect,	// Form from negative intersection with world.
} CsgOper;

// Information.
var() color BrushColor;
var	  int	PolyFlags;
var() bool  bColored;
var bool	bSolidWhenSelected;

/** If TRUE, this brush class can be placed using the class browser like other simple class types */
var bool	bPlaceableFromClassBrowser;

var export const Model	Brush;
var editconst const BrushComponent BrushComponent;

// Selection information for geometry mode

struct native export GeomSelection
{
	var int		Type;			// EGeometrySelectionType_
	var int		Index;			// Index into the geometry data structures
	var int		SelectionIndex;	// The selection index of this item
};

/**
 * Stores selection information from geometry mode.  This is the only information that we can't
 * regenerate by looking at the source brushes following an undo operation.
 */
 
var array<GeomSelection>	SavedSelections;

//-----------------------------------------------------------------------------
// cpptext.

cpptext
{
	// UObject interface.
	virtual void PostLoad();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	
	/** Called after using geom mode to edit thie brush's geometry */
	virtual void PostEditBrush() {}
	
	virtual UBOOL IsABrush() const {return TRUE;}

	/**
	 * Note that the object has been modified.  If we are currently recording into the 
	 * transaction buffer (undo/redo), save a copy of this object into the buffer and 
	 * marks the package as needing to be saved.
	 *
	 * @param	bAlwaysMarkDirty	if TRUE, marks the package dirty even if we aren't
	 *								currently recording an active undo/redo transaction
	 */
	virtual void Modify(UBOOL bAlwaysMarkDirty = FALSE);

	/**
	 * Serialize function
	 *
	 * @param Ar Archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	* Return whether this actor is a builder brush or not.
	*
	* @return TRUE if this actor is a builder brush, FALSE otherwise
	*/
	virtual UBOOL IsABuilderBrush() const;

	/**
	* Return whether this actor is the current builder brush or not
	*
	* @return TRUE if htis actor is the current builder brush, FALSE otherwise
	*/
	virtual UBOOL IsCurrentBuilderBrush() const;

	// ABrush interface.
	virtual void CopyPosRotScaleFrom( ABrush* Other );
	virtual void InitPosRotScale();

#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/**
	* Figures out the best color to use for this brushes wireframe drawing.
	*/

	virtual FColor GetWireColor() const;
}

defaultproperties
{
	Begin Object Class=BrushComponent Name=BrushComponent0
	End Object
	BrushComponent=BrushComponent0
	CollisionComponent=BrushComponent0
	Components.Add(BrushComponent0)

	bStatic=True
	bHidden=True
	bNoDelete=True
	bEdShouldSnap=True
}
