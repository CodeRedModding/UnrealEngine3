/*=============================================================================
	UnGeom.h: Support for the geometry editing mode in UnrealEd.

	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

class FPoly;

/*-----------------------------------------------------------------------------
	Classes for storing and editing geometry.
-----------------------------------------------------------------------------*/

/**
 * FGeomBase
 *
 * Base class for all geometry classes.
 */

class FGeomBase
{
public:
	FGeomBase();
	virtual ~FGeomBase();

	void Select( UBOOL InSelect = 1 );
	UBOOL IsSelected() { return SelectionIndex > INDEX_NONE; }
	UBOOL IsFullySelected() { return IsSelected() && SelStrength == 1.f; }
	INT GetSelectionIndex() { return SelectionIndex; }

	/**
	 * Allows manual setting of the selection index.  This is dangerous and should
	 * only be called by the FEdModeGeometry::PostUndo() function.
	 */
	void ForceSelectionIndex( INT InSelectionIndex ) { SelectionIndex = InSelectionIndex; }

	/** Returns a location that represents the middle of the object */
	virtual FVector GetMidPoint() = 0;

	/** Returns a valid position for the widget to be drawn at for this object */
	virtual FVector GetWidgetLocation();

	/** Allows polys, edges and vertices to point to the FGeomObject that owns them. */
	INT ParentObjectIndex;
	class FGeomObject* GetParentObject();

	virtual UBOOL IsVertex() { return 0; }

	FVector Normal;			/** The normal vector for this object */
	FVector Mid;			/** The mid point for this object */
	FLOAT SelStrength;		/** Strength of the selection of this object - used for soft selection */

private:
	INT SelectionIndex;		/** If > INDEX_NONE, this object is selected */
};

/**
 * FPolyVertexIndex
 *
 * An index pair denoting a polygon and vertex within the parent objects ABrush.
 */

struct FPolyVertexIndex
{
	FPolyVertexIndex()
	{
		PolyIndex = VertexIndex = INDEX_NONE;
	}
	FPolyVertexIndex( INT InPolyIndex, INT InVertexIndex )
	{
		PolyIndex = InPolyIndex;
		VertexIndex = InVertexIndex;
	}

	INT PolyIndex, VertexIndex;

	UBOOL operator==( const FPolyVertexIndex& In ) const
	{
		if( In.PolyIndex != PolyIndex
				|| In.VertexIndex != VertexIndex )
		{
			return 0;
		}

		return 1;
	}
};

/**
 * FGeomVertex
 *
 * A 3D position.
 */

class FGeomVertex : public FGeomBase, public FVector
{
public:
	FGeomVertex();
	virtual ~FGeomVertex();

	virtual FVector GetWidgetLocation();

	virtual FVector GetMidPoint();

	virtual UBOOL IsVertex() { return 1; }

	/** The list of vertices that this vertex represents. */
	TArray<FPolyVertexIndex> ActualVertexIndices;
	FVector* GetActualVertex( FPolyVertexIndex& InPVI );

	/**
	 * Indices into the parent poly pool. A vertex can belong to more 
	 * than one poly and this keeps track of that relationship.
	 */
	TArray<INT> ParentPolyIndices;

	FGeomVertex& operator=( const FVector& In )
	{
		X = In.X;
		Y = In.Y;
		Z = In.Z;
		return *this;
	}
};

/**
 * FGeomEdge
 *
 * The space between 2 adjacent vertices.
 */

class FGeomEdge : public FGeomBase
{
public:
	FGeomEdge();
	virtual ~FGeomEdge();

	virtual FVector GetWidgetLocation();

	virtual FVector GetMidPoint();

	/** Checks to see if InEdge matches this edge regardless of winding */
	UBOOL IsSame( FGeomEdge* InEdge );

	/**
	 * Indices into the parent poly pool. An edge can belong to more 
	 * than one poly and this keeps track of that relationship.
	 */
	TArray<INT> ParentPolyIndices;

	/** Indices into the parent vertex pool. */
	int VertexIndices[2];
};

/**
 * FGeomPoly
 *
 * An individual polygon.
 */

class FGeomPoly : public FGeomBase
{
public:
	FGeomPoly();
	virtual ~FGeomPoly();

	virtual FVector GetWidgetLocation();

	virtual FVector GetMidPoint();

	/**
	 * The polygon this poly represents.  This is an index into the polygon array inside 
	 * the ABrush that is selected in this objects parent FGeomObject.
	 */
	INT ActualPolyIndex;
	FPoly* GetActualPoly();

	/** Array of indices into the parent objects edge pool. */
	TArray<int> EdgeIndices;

	UBOOL operator==( const FGeomPoly& In ) const
	{
		if( In.ActualPolyIndex != ActualPolyIndex
				|| In.ParentObjectIndex != ParentObjectIndex
				|| In.EdgeIndices.Num() != EdgeIndices.Num() )
		{
			return 0;
		}

		for( INT x = 0 ; x < EdgeIndices.Num() ; ++x )
		{
			if( EdgeIndices(x) != In.EdgeIndices(x) )
			{
				return 0;
			}
		}

		return 1;
	}
};

/**
 * FGeomObject
 *
 * A group of polygons forming an individual object.
 */

class FGeomObject : public FGeomBase
{
public:
	FGeomObject();
	virtual ~FGeomObject();

	virtual FVector GetMidPoint();

	/** Index to the ABrush actor this object represents. */
	INT ActualBrushIndex;
	ABrush* GetActualBrush();

	/**
	 * Used for tracking the order of selections within this object.  This 
	 * is required for certain modifiers to work correctly.
	 *
	 * This list is generated on the fly whenever the array is accessed but
	 * is marked as dirty (see bSelectionOrderDirty).
	 *
	 * NOTE: do not serialize
	 */
	TArray<FGeomBase*> SelectionOrder;

	/** If 1, the selection order array needs to be compiled before being accessed.
	 *
	 * NOTE: do not serialize
	 */
	UBOOL bSelectionOrderDirty;

	void CompileSelectionOrder();

	/** Master lists.  All lower data types refer to the contents of these pools through indices. */
	TArray<FGeomVertex> VertexPool;
	TArray<FGeomEdge> EdgePool;
	TArray<FGeomPoly> PolyPool;

	void CompileUniqueEdgeArray( TArray<FGeomEdge>* InEdges );

	/** Tells the object to recompute all of it's internal data. */
	void ComputeData();

	/** Erases all current data for this object. */
	void ClearData();

	virtual FVector GetWidgetLocation();
	INT AddVertexToPool( INT InObjectIndex, INT InParentPolyIndex, INT InPolyIndex, INT InVertexIndex );
	INT AddEdgeToPool( FGeomPoly* InPoly, INT InParentPolyIndex, INT InVectorIdxA, INT InVectorIdxB );
	void GetFromSource();
	void SendToSource();
	UBOOL FinalizeSourceData();
	INT GetObjectIndex();

	void SelectNone();
	void SoftSelect();
	INT GetNewSelectionIndex();

	/**
	 * Allows manual setting of the last selection index.  This is dangerous and should
	 * only be called by the FEdModeGeometry::PostUndo() function.
	 */
	void ForceLastSelectionIndex( INT InLastSelectionIndex ) { LastSelectionIndex = InLastSelectionIndex; }

private:
	INT LastSelectionIndex;
};

/*-----------------------------------------------------------------------------
	Hit proxies for geometry editing.
-----------------------------------------------------------------------------*/

struct HGeomPolyProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HGeomPolyProxy,HHitProxy);

	FGeomObject*	GeomObject;
	INT				PolyIndex;

	HGeomPolyProxy(FGeomObject* InGeomObject, INT InPolyIndex):
		GeomObject(InGeomObject),
		PolyIndex(InPolyIndex)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

struct HGeomEdgeProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HGeomEdgeProxy,HHitProxy);

	FGeomObject*	GeomObject;
	INT				EdgeIndex;

	HGeomEdgeProxy(FGeomObject* InGeomObject, INT InEdgeIndex):
		GeomObject(InGeomObject),
		EdgeIndex(InEdgeIndex)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

struct HGeomVertexProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HGeomVertexProxy,HHitProxy);

	FGeomObject*	GeomObject;
	INT				VertexIndex;

	HGeomVertexProxy(FGeomObject* InGeomObject, INT InVertexIndex):
		GeomObject(InGeomObject),
		VertexIndex(InVertexIndex)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

/*-----------------------------------------------------------------------------
	WxGeomModifiers

	The floating modifier window that appears during geometry mode.  This window
	contains buttons and controls for using the various geometry modifiers.
-----------------------------------------------------------------------------*/

class WxGeomModifiers : public wxFrame
{
public:
	DECLARE_DYNAMIC_CLASS(WxGeomModifiers)

	WxGeomModifiers()
	{};
	WxGeomModifiers( wxWindow* parent, wxWindowID id );
	virtual ~WxGeomModifiers();

	wxStaticBox* ModifierBox;
	TArray<wxRadioButton*> RadioButtons;
	TArray<wxButton*> PushButtons;
	WxPropertyWindow* PropertyWindow;
	wxButton* ActionButton;

	void OnClose( wxCloseEvent& In );
	void OnSize( wxSizeEvent& In );
	void OnModifierSelected( wxCommandEvent& In );
	void OnModifierClicked( wxCommandEvent& In );
	void OnUpdateUI( wxUpdateUIEvent& In );
	void OnActionClicked( wxCommandEvent& In );

	void InitFromTool();
	void PositionChildControls();

	DECLARE_EVENT_TABLE()
};

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/
