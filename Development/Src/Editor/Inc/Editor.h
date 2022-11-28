/*=============================================================================
	Editor.h: Unreal editor public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_EDITOR
#define _INC_EDITOR

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "..\..\XWindow\inc\XWindow.h"
#include "Engine.h"
#include "UnPrefab.h"	// UnrealEd Prefabs

struct FBuilderPoly
{
	TArray<INT> VertexIndices;
	INT Direction;
	FName ItemName;
	INT PolyFlags;
	FBuilderPoly()
	: VertexIndices(), Direction(0), ItemName(NAME_None)
	{}
};

#include "EditorClasses.h"
#include "EditorCommandlets.h"

#define CAMERA_MOVEMENT_SPEED		3.0f
#define CAMERA_ROTATION_SPEED		32.0f
#define CAMERA_ZOOM_DAMPEN			200.f
#define MOUSE_CLICK_DRAG_DELTA		4.0f
#define CAMERA_ZOOM_DIV				15000.0f
#define CAMERA_SCALE_FACTOR			.01f

#include "UnEdCoordSystem.h"
#include "UnEdDragTools.h"
#include "UnEdViewport.h"
#include "UnEdComponents.h"

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

class FGeomBase;
class FGeomVertex;
class FGeomEdge;
class FGeomPoly;
class FGeomObject;
class WxGeomModifiers;

/*-----------------------------------------------------------------------------
	Editor public.
-----------------------------------------------------------------------------*/

#define MAX_EDCMD 512   // Max Unrealed->Editor Exec command string length.

//
// The editor object.
//

extern class UEditorEngine* GEditor;

// Texture alignment.
enum ETAxis
{
    TAXIS_X                 = 0,
    TAXIS_Y                 = 1,
    TAXIS_Z                 = 2,
    TAXIS_WALLS             = 3,
    TAXIS_AUTO              = 4,
};

//
// Importing object properties.
//

const TCHAR* ImportDefaultProperties(
	UClass*				Class,
	const TCHAR*		Text,
	FFeedbackContext*	Warn,
	INT					Depth
	);

const TCHAR* ImportObjectProperties(
	UStruct*			ObjectStruct,
	BYTE*				Object,
	ULevel*				Level,
	const TCHAR*		Text,
	UObject*			InParent,
	UObject*			SubObjectOuter,
	FFeedbackContext*	Warn,
	INT					Depth,
	UObject*			ComponentOwner
	);

//
// GBuildStaticMeshCollision - Global control for building static mesh collision on import.
//

extern UBOOL GBuildStaticMeshCollision;

//
// Creating a static mesh from an array of triangles.
//
UStaticMesh* CreateStaticMesh(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshMaterial>& Materials,UObject* Outer,FName Name);

//
// Converting models to static meshes.
//
void GetBrushTriangles(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshMaterial>& Materials,AActor* Brush,UModel* Model);
UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer,FName Name,ABrush* Brush,UModel* Model);

//
// Converting static meshes back to brushes.
//
void CreateModelFromStaticMesh(UModel* Model,AStaticMeshActor* StaticMeshActor);

#include "UnEdModeTools.h"
#include "UnWidget.h"

#include "UnEdModes.h"
#include "UnGeom.h"					// Support for the editors "geometry mode"

/*-----------------------------------------------------------------------------
	FScan.
-----------------------------------------------------------------------------*/

typedef void (*POLY_CALLBACK)( UModel* Model, INT iSurf );

/*-----------------------------------------------------------------------------
	FConstraints.
-----------------------------------------------------------------------------*/

//
// General purpose movement/rotation constraints.
//
class FConstraints
{
public:
	// Functions.
	virtual void Snap( FVector& Point, FVector GridBase )=0;
	virtual void Snap( FRotator& Rotation )=0;
	virtual UBOOL Snap( ULevel* Level, FVector& Location, FVector GridBase, FRotator& Rotation )=0;
};

/*-----------------------------------------------------------------------------
	FConstraints.
-----------------------------------------------------------------------------*/

//
// General purpose movement/rotation constraints.
//
class FEditorConstraints : public FConstraints
{
public:
	enum { MAX_GRID_SIZES=11 };

	// Variables.
	BITFIELD	GridEnabled:1;				// Grid on/off.
	BITFIELD	SnapVertices:1;				// Snap to nearest vertex within SnapDist, if any.
	FLOAT		SnapDistance;				// Distance to check for snapping.
	FLOAT		GridSizes[MAX_GRID_SIZES];	// Movement grid size steps.
	INT			CurrentGridSz;				// Index into GridSizes.
	BITFIELD	RotGridEnabled:1;			// Rotation grid on/off.
	FRotator	RotGridSize;				// Rotation grid.

	FLOAT GetGridSize();
	void SetGridSz( INT InIndex );

	// Functions.
	virtual void Snap( FVector& Point, FVector GridBase );
	virtual void Snap( FRotator& Rotation );
	virtual UBOOL Snap( ULevel* Level, FVector& Location, FVector GridBase, FRotator& Rotation );
};

/*-----------------------------------------------------------------------------
	UEditorEngine definition.
-----------------------------------------------------------------------------*/

class UEditorEngine : public UEngine
{
	DECLARE_CLASS(UEditorEngine,UEngine,CLASS_Transient|CLASS_Config,Editor)

	// Objects.
	ULevel*						Level;
	ULevel*						PlayLevel;
	UModel*						TempModel;
	class UTransactor*			Trans;
	class UTextBuffer*			Results;
	TArray<class WxPropertyWindowFrame*>	ActorProperties;
	class WxPropertyWindowFrame*	LevelProperties;
	class WProperties*			UseDest;
	INT							AutosaveCounter;

	// Graphics.
	UTexture2D *Bad;
	UTexture2D *Bkgnd, *BkgndHi, *BadHighlight, *MaterialArrow, *MaterialBackdrop;

	// Static Meshes
	UStaticMesh* TexPropCube;
	UStaticMesh* TexPropSphere;
	UStaticMesh* TexPropPlane;
	UStaticMesh* TexPropCylinder;

	// Toggles.
	BITFIELD				FastRebuild:1;
	BITFIELD				Bootstrapping:1;

	// Variables.
	INT						AutoSaveIndex;
	INT						AutoSaveCount;
	INT						TerrainEditBrush;
	DWORD					ClickFlags;
	FLOAT					MovementSpeed;
	UObject*				ParentContext;
	FVector					ClickLocation;				// Where the user last clicked in the world
	FPlane					ClickPlane;
	FVector					MouseMovement;				// How far the mouse has moved since the last button was pressed down
	TArray<FEditorLevelViewportClient*> ViewportClients;

	// Tools.
	TArray<UObject*>		Tools;
	UClass*					BrowseClass;

	// Constraints.
	FEditorConstraints		Constraints;

	// Advanced.
	BITFIELD				UseSizingBox:1;		// Shows sizing information in the top left corner of the viewports
	BITFIELD				UseAxisIndicator:1;	// Displays an axis indictor in the bottom left corner of the viewports
	FLOAT					FOVAngle;
	BITFIELD				GodMode:1;
	BITFIELD				AutoSave:1;
	BYTE					AutosaveTimeMinutes;
	FStringNoInit			AutoSaveDir;
	BITFIELD				InvertwidgetZAxis;
	FStringNoInit			GameCommandLine;
	TArray<FString>			EditPackages;
	FStringNoInit			EditPackagesInPath;
	FStringNoInit			EditPackagesOutPath;
	BITFIELD				AlwaysShowTerrain:1;			// Always show the terrain in the overhead 2D view?
	BITFIELD				UseActorRotationGizmo:1;		// Use the gizmo for rotating actors?
	BITFIELD				bShowBrushMarkerPolys:1;		// Show translucent marker polygons on the builder brush and volumes?
	
	// Array of actor factories created at editor startup and used by context menu etc.
	TArray<UActorFactory*>	ActorFactories;

	// Constructor.
	void StaticConstructor();
	UEditorEngine();

	// UObject interface.
	void Destroy();
	void Serialize( FArchive& Ar );

	// UEngine interface.
	void Init();
	void InitEditor();

	void Tick( FLOAT DeltaSeconds );
	void Draw( UViewport* Viewport, UBOOL Blit=1, BYTE* HitData=NULL, INT* HitSize=NULL ) { check(0); }
	void MouseDelta( UViewport* Viewport, DWORD Buttons, FLOAT DX, FLOAT DY ) { check(0); }
	void MousePosition( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y ) { check(0); }
	void MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta ) { check(0); }
	void Click( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y ) { check(0); }
	void UnClick( UViewport* Viewport, DWORD Buttons, INT MouseX, INT MouseY ) { check(0); }
	void SetClientTravel( const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType ) {}
	virtual ULevel* GetLevel()
	{
		return Level;
	}
	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor );

	// UnEdSrv.cpp
	virtual UBOOL SafeExec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	UBOOL Exec_StaticMesh( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Brush( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Paths( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_BSP( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Light( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Map( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Select( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Poly( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Texture( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Transaction( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Obj( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Class( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Camera( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Level( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Terrain( const TCHAR* Str, FOutputDevice& Ar );

	// Pivot handling.
	virtual FVector GetPivotLocation() { return FVector(0,0,0); }
	virtual void SetPivot( FVector NewPivot, UBOOL SnapPivotToGrid, UBOOL MoveActors, UBOOL bIgnoreAxis ) {}
	virtual void ResetPivot() {}


	// General functions.
	virtual void Cleanse( UBOOL Redraw, const TCHAR* TransReset );
	virtual void FinishAllSnaps( ULevel* Level ) { check(0); }
	virtual void RedrawLevel( ULevel* Level ) {}
	virtual void NoteSelectionChange( ULevel* Level ) { check(0); }
	virtual AActor* AddActor( ULevel* Level, UClass* Class, FVector V, UBOOL bSilent = 0 );
	virtual void NoteActorMovement( ULevel* Level ) { check(0); }
	virtual UTransactor* CreateTrans();
	void RedrawAllViewports( UBOOL bLevelViewportsOnly );
	UActorFactory* FindActorFactory( UClass* InClass );
	AActor* UseActorFactory( UActorFactory* Factory ); 
	virtual void edactCopySelected( ULevel* Level ) {}
	virtual void edactPasteSelected( ULevel* InLevel, UBOOL InDuplicate, UBOOL InUseOffset ) {}

	// Editor CSG virtuals from UnEdCsg.cpp.
	virtual void csgPrepMovingBrush( ABrush* Actor );
	virtual void csgCopyBrush( ABrush* Dest, ABrush* Src, DWORD PolyFlags, DWORD ResFlags, UBOOL NeedsPrep );
	virtual ABrush*	csgAddOperation( ABrush* Actor, ULevel* Level, DWORD PolyFlags, ECsgOper CSG );
	virtual void csgRebuild( ULevel* Level );
	virtual const TCHAR* csgGetName( ECsgOper CsgOper );

	// Editor EdPoly/BspSurf assocation virtuals from UnEdCsg.cpp.
	virtual INT polyFindMaster( UModel* Model, INT iSurf, FPoly& Poly );
	virtual void polyUpdateMaster( UModel* Model, INT iSurf, INT UpdateTexCoords );
	virtual void polyGetLinkedPolys( ABrush* InBrush, FPoly* InPoly, TArray<FPoly>* InPolyList );
	virtual void polyGetOuterEdgeList( TArray<FPoly>* InPolyList, TArray<FEdge>* InEdgeList );
	virtual void polySplitOverlappingEdges( TArray<FPoly>* InPolyList, TArray<FPoly>* InResult );

	// Bsp Poly search virtuals from UnEdCsg.cpp.
	virtual void polySetAndClearPolyFlags( UModel* Model, DWORD SetBits, DWORD ClearBits, INT SelectedOnly, INT UpdateMaster );

	// Selection.
	virtual void SelectActor( ULevel* Level, AActor* Actor, UBOOL InSelected = 1, UBOOL bNotify = 1 ) {}
	virtual void SelectNone( ULevel* Level, UBOOL Notify, UBOOL BSPSurfs = 1 ) {}
	virtual void SelectBSPSurf( ULevel* Level, INT iSurf, UBOOL InSelected = 1, UBOOL bNotify = 1 ) {}

	// Bsp Poly selection virtuals from UnEdCsg.cpp.
	virtual void polySelectAll ( UModel* Model );
	virtual void polySelectMatchingGroups( UModel* Model );
	virtual void polySelectMatchingItems( UModel* Model );
	virtual void polySelectCoplanars( UModel* Model );
	virtual void polySelectAdjacents( UModel* Model );
	virtual void polySelectAdjacentWalls( UModel* Model );
	virtual void polySelectAdjacentFloors( UModel* Model );
	virtual void polySelectAdjacentSlants( UModel* Model );
	virtual void polySelectMatchingBrush( UModel* Model );
	virtual void polySelectMatchingTexture( UModel* Model );
	virtual void polySelectReverse( UModel* Model );
	virtual void polyMemorizeSet( UModel* Model );
	virtual void polyRememberSet( UModel* Model );
	virtual void polyXorSet( UModel* Model );
	virtual void polyUnionSet( UModel* Model );
	virtual void polyIntersectSet( UModel* Model );
	virtual void polySelectZone( UModel *Model );

	// Poly texturing virtuals from UnEdCsg.cpp.
	virtual void polyTexPan( UModel* Model, INT PanU, INT PanV, INT Absolute );
	virtual void polyTexScale( UModel* Model,FLOAT UU, FLOAT UV, FLOAT VU, FLOAT VV, UBOOL Absolute );

	// Map brush selection virtuals from UnEdCsg.cpp.
	virtual void mapSelectOperation( ULevel* Level, ECsgOper CSGOper );
	virtual void mapSelectFlags(ULevel* Level, DWORD Flags );
	virtual void mapBrushGet( ULevel* Level );
	virtual void mapBrushPut( ULevel* Level );
	virtual void mapSendToFirst( ULevel* Level );
	virtual void mapSendToLast( ULevel* Level );
	virtual void mapSendToSwap( ULevel* Level );
	virtual void mapSetBrush( ULevel* Level, enum EMapSetBrushFlags PropertiesMask, _WORD BrushColor, FName Group, DWORD SetPolyFlags, DWORD ClearPolyFlags, DWORD CSGOper, INT DrawType );

	// Bsp virtuals from UnBsp.cpp.
	virtual void bspRepartition( ULevel* Level, INT iNode, UBOOL Simple );
	virtual INT bspAddVector( UModel* Model, FVector* V, UBOOL Exact );
	virtual INT bspAddPoint( UModel* Model, FVector* V, UBOOL Exact );
	virtual INT bspNodeToFPoly( UModel* Model, INT iNode, FPoly* EdPoly );
	virtual void bspBuild( UModel* Model, enum EBspOptimization Opt, INT Balance, INT PortalBias, INT RebuildSimplePolys, INT iNode );
	virtual void bspRefresh( UModel* Model, UBOOL NoRemapSurfs );
	virtual void bspCleanup( UModel* Model );
	virtual void bspBuildBounds( UModel* Model );
	virtual void bspBuildFPolys( UModel* Model, UBOOL SurfLinks, INT iNode );
	virtual void bspMergeCoplanars( UModel* Model, UBOOL RemapLinks, UBOOL MergeDisparateTextures );
	virtual INT bspBrushCSG( ABrush* Actor, UModel* Model, DWORD PolyFlags, ECsgOper CSGOper, UBOOL RebuildBounds, UBOOL MergePolys = 1 );
	virtual void bspOptGeom( UModel* Model );
	virtual void bspValidateBrush( UModel* Brush, INT ForceValidate, INT DoStatusUpdate );
	virtual void bspUnlinkPolys( UModel* Brush );
	virtual INT	bspAddNode( UModel* Model, INT iParent, enum ENodePlace ENodePlace, DWORD NodeFlags, FPoly* EdPoly );

	// Shadow virtuals (UnShadow.cpp).
	virtual void shadowIlluminateBsp( ULevel* Level );

	// Skeletal animation import
	static void ImportPSAIntoAnimSet( class UAnimSet* AnimSet, const TCHAR* Filename );

	// Visibility.
	virtual void TestVisibility( ULevel* Level, UModel* Model, int A, int B );

	// Scripts.
	virtual int MakeScripts( UClass* BaseClass, FFeedbackContext* Warn, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses );

	// Topics.
	virtual void Get( const TCHAR* Topic, const TCHAR* Item, FOutputDevice& Ar );
	virtual void Set( const TCHAR* Topic, const TCHAR* Item, const TCHAR* Value );

	// Object management.
	virtual void RenameObject(UObject* Object,UObject* NewOuter,const TCHAR* NewName);
	virtual void DeleteObject(UObject* Object);

	// Static mesh management.
	UStaticMesh* CreateStaticMeshFromSelection(const TCHAR* Package,const TCHAR* Name);

	// Level management.
	void AnalyzeLevel(ULevel* Level,FOutputDevice& Ar);
	void PlayMap(FVector* StartLocation = NULL);
	void EndPlayMap();

	/**
	 * Removes all components from the current level's scene.
	 */
	void ClearComponents();

	/**
	 * Updates all components in the current level's scene.
	 */
	void UpdateComponents();

	// Viewport management.
	void DisableRealtimeViewports();

	// Editor main context menu.
	virtual void ShowUnrealEdContextMenu() {}
	virtual void ShowUnrealEdContextSurfaceMenu() {}
};

/*-----------------------------------------------------------------------------
	Parameter parsing functions.
-----------------------------------------------------------------------------*/

UBOOL GetFVECTOR( const TCHAR* Stream, const TCHAR* Match, FVector& Value );
UBOOL GetFVECTOR( const TCHAR* Stream, FVector& Value );
UBOOL GetFROTATOR( const TCHAR* Stream, const TCHAR* Match, FRotator& Rotation, int ScaleFactor );
UBOOL GetFROTATOR( const TCHAR* Stream, FRotator& Rotation, int ScaleFactor );
UBOOL GetBEGIN( const TCHAR** Stream, const TCHAR* Match );
UBOOL GetEND( const TCHAR** Stream, const TCHAR* Match );
TCHAR* SetFVECTOR( TCHAR* Dest, const FVector* Value );
UBOOL GetFSCALE( const TCHAR* Stream, FScale& Scale );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif