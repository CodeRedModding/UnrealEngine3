/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __EDITOR_H__
#define __EDITOR_H__

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Engine.h"

#define ENUMS_ONLY 1
#include "EngineAnimClasses.h"
#undef ENUMS_ONLY

#include "EngineAIClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "PersistentMapInfo.h"
#include "UnContentCookers.h"
#include "UnGameCookerHelper.h"
#include "AnalyzeReferencedContent.h"
#include "EditorCommandlets.h"
#include "EditorClassHierarchy.h"

#define CAMERA_ROTATION_SPEED		32.0f
#define CAMERA_ZOOM_DAMPEN			200.f
//Needs to be the same for the view matrix AND the translation code
#define CAMERA_ZOOM_DIV				(Viewport->GetSizeX()*15.0f)

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

class FGeomBase;
class FGeomVertex;
class FGeomEdge;
class FGeomPoly;
class FGeomObject;

/*-----------------------------------------------------------------------------
	Editor public.
-----------------------------------------------------------------------------*/

/** Max Unrealed->Editor Exec command string length. */
#define MAX_EDCMD 512

/** The editor object. */
extern class UEditorEngine* GEditor;

/**
 * Returns the path to the engine's editor resources directory (e.g. "/../../Engine/Editor/")
 */
const FString GetEditorResourcesDir();



/** Texture alignment. */
enum ETAxis
{
    TAXIS_X                 = 0,
    TAXIS_Y                 = 1,
    TAXIS_Z                 = 2,
    TAXIS_WALLS             = 3,
    TAXIS_AUTO              = 4,
};

/** Angle snapping. */
enum EAngleSnapType
{
	EST_ANGLE,
	EST_PER90,
	EST_PER360,
};

/** Device targets for mobile previewer. */
enum EBuildPlayDevice
{
	BPD_DEFAULT,
	BPD_CUSTOM,

	BPD_MOBILE_FIRST_DEVICE,
		BPD_IPHONE_3GS = BPD_MOBILE_FIRST_DEVICE,
		BPD_IPHONE_4,
		BPD_IPHONE_4S,
		BPD_IPHONE_5,
		BPD_IPOD_TOUCH_4,
		BPD_IPOD_TOUCH_5,
		BPD_IPAD,
		BPD_IPAD2,
		BPD_IPAD3,
		BPD_IPAD4,
		BPD_IPAD_MINI,
	BPD_NON_MOBILE_FIRST_DEVICE,
#if !UDK
		BPD_FLASH = BPD_NON_MOBILE_FIRST_DEVICE,
		BPD_XBOX_360,
		BPD_PS3,
		BPD_PSVITA,
#endif

	BPD_RESOLUTION_MAX,
	BPD_NUM_MOBILE_DEVICES = BPD_NON_MOBILE_FIRST_DEVICE - BPD_MOBILE_FIRST_DEVICE,

	BPD_FIRST_VIEWPORT_RESIZE_DEVICE = BPD_MOBILE_FIRST_DEVICE,
	BPD_LAST_VIEWPORT_RESIZE_DEVICE = BPD_RESOLUTION_MAX,
	BPD_NUM_VIEWPORT_RESIZE_DEVICES = (BPD_LAST_VIEWPORT_RESIZE_DEVICE+1) - BPD_FIRST_VIEWPORT_RESIZE_DEVICE
};

// Object propagation destination
const INT OPD_None				= 0;
const INT OPD_LocalStandalone	= 1;
const INT OPD_ConsoleStart		= 2; // more after this are okay, means which console in the GConsoleSupportContainer


/**
 * MapChangeEventFlags defines flags passed to CALLBACK_MapChange global events
 */
namespace MapChangeEventFlags
{
	/** MapChangeEventFlags::Type */
	typedef DWORD Type;

	/** Default flags */
	const Type Default = 0;

	/** Set when a new map is created, loaded from disk, imported, etc. */
	const Type NewMap = 1 << 0;

	/** Set when a map rebuild occurred */
	const Type MapRebuild = 1 << 1;

	/** Set when a world was destroyed (torn down) */
	const Type WorldTornDown = 1 << 2;
}


/**
 * Import the entire default properties block for the class specified
 * 
 * @param	Class		the class to import defaults for
 * @param	Text		buffer containing the text to be imported
 * @param	Warn		output device for log messages
 * @param	Depth		current nested subobject depth
 * @param	LineNumber	the starting line number for the defaultproperties block (used for log messages)
 *
 * @return	NULL if the default values couldn't be imported
 */
const TCHAR* ImportDefaultProperties(
	UClass*				Class,
	const TCHAR*		Text,
	FFeedbackContext*	Warn,
	INT					Depth,
	INT					LineNumber
	);


/**
 * Parameters for ImportObjectProperties
 */
struct FImportObjectParams
{
	/** the location to import the property values to */
	BYTE*				DestData;

	/** pointer to a buffer containing the values that should be parsed and imported */
	const TCHAR*		SourceText;

	/** the struct for the data we're importing */
	UStruct*			ObjectStruct;

	/** the original object that ImportObjectProperties was called for.
	    if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
		if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter */
	UObject*			SubobjectRoot;

	/** the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText */
	UObject*			SubobjectOuter;

	/** output device to use for log messages */
	FFeedbackContext*	Warn;

	/** current nesting level */
	INT					Depth;

	/** used when importing defaults during script compilation for tracking which line we're currently for the purposes of printing compile errors */
	INT					LineNumber;

	/** contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
		not necessary to specify a value when calling this function from other code */
	FObjectInstancingGraph* InInstanceGraph;


	/** True if we should call PreEditChange/PostEditChange on the object as it's imported.  Pass false here
	    if you're going to do that on your own. */
	UBOOL				bShouldCallEditChange;


	/** Constructor */
	FImportObjectParams()
		: DestData( NULL ),
		  SourceText( NULL ),
		  ObjectStruct( NULL ),
		  SubobjectRoot( NULL ),
		  SubobjectOuter( NULL ),
		  Warn( NULL ),
		  Depth( 0 ),
		  LineNumber( INDEX_NONE ),
		  InInstanceGraph( NULL ),
		  bShouldCallEditChange( TRUE )
	{
	}
};


/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	InParams	Parameters for object import; see declaration of FImportObjectParams.
 *
 * @return	NULL if the default values couldn't be imported
 */

const TCHAR* ImportObjectProperties( FImportObjectParams& InParams );


/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	DestData			the location to import the property values to
 * @param	SourceText			pointer to a buffer containing the values that should be parsed and imported
 * @param	ObjectStruct		the struct for the data we're importing
 * @param	SubobjectRoot		the original object that ImportObjectProperties was called for.
 *								if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
 *								if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter
 * @param	SubobjectOuter		the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText
 * @param	Warn				ouptut device to use for log messages
 * @param	Depth				current nesting level
 * @param	LineNumber			used when importing defaults during script compilation for tracking which line the defaultproperties block begins on
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
 *								not necessary to specify a value when calling this function from other code
 *
 * @return	NULL if the default values couldn't be imported
 */
const TCHAR* ImportObjectProperties(
	BYTE*				DestData,
	const TCHAR*		SourceText,
	UStruct*			ObjectStruct,
	UObject*			SubobjectRoot,
	UObject*			SubobjectOuter,
	FFeedbackContext*	Warn,
	INT					Depth,
	INT					LineNumber = INDEX_NONE,
	FObjectInstancingGraph* InstanceGraph=NULL
	);

//
// GBuildStaticMeshCollision - Global control for building static mesh collision on import.
//

extern UBOOL GBuildStaticMeshCollision;

//
// Creating a static mesh from an array of triangles.
//
UStaticMesh* CreateStaticMesh(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshElement>& Materials,UObject* Outer,FName Name);

struct FMergeStaticMeshParams
{
	/**
	 *Constructor, setting all values to usable defaults 
	 */
	FMergeStaticMeshParams();

	/** A translation to apply to the verts in SourceMesh */
	FVector Offset;
	/** A rotation to apply to the verts in SourceMesh */
	FRotator Rotation;
	/** A uniform scale to apply to the verts in SourceMesh */
	FLOAT ScaleFactor;
	/** A non-uniform scale to apply to the verts in SourceMesh */
	FVector ScaleFactor3D;
	
	/** If TRUE, DestMesh will not be rebuilt */
	UBOOL bDeferBuild;
	
	/** If set, all triangles in SourceMesh will be set to this element index, instead of duplicating SourceMesh's elements into DestMesh's elements */
	INT OverrideElement;

	/** If TRUE, UVChannelRemap will be used to reroute UV channel values from one channel to another */
	UBOOL bUseUVChannelRemapping;
	/** An array that can remap UV values from one channel to another */
	INT UVChannelRemap[8];
	
	/* If TRUE, UVScaleBias will be used to modify the UVs (AFTER UVChannelRemap has been applied) */
	UBOOL bUseUVScaleBias;
	/* Scales/Bias's to apply to each UV channel in SourceMesh */
	FVector4 UVScaleBias[8];
};

/**
 * Merges SourceMesh into DestMesh, applying transforms along the way
 *
 * @param DestMesh The static mesh that will have SourceMesh merged into
 * @param SourceMesh The static mesh to merge in to DestMesh
 * @param Params Settings for the merge
 */
void MergeStaticMesh(UStaticMesh* DestMesh, UStaticMesh* SourceMesh, const FMergeStaticMeshParams& Params);

//
// Converting models to static meshes.
//
void GetBrushTriangles(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshElement>& Materials,AActor* Brush,UModel* Model );

/**
 * CreateStaticMeshFromBrush - Creates a static mesh from the triangles in a model.
 * 
 * @param Outer - Outer for the new Static Mesh.
 * @param Name  - Name for the new Static Mesh.
 * @param Brush - Used to orientate the new static mesh (NULL will result in the origin being assumed to be the pivot).
 * @param Model - Model the new static mesh is build from.
 * @param MoveToLocalSpace	- If true and no brush is provided the function will assume that the current selections pivot is the new meshes pivot.
 *
 * @return Created static mesh.
 */
UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer, FName Name, ABrush* Brush, UModel* Model, UBOOL MoveToLocalSpace = FALSE);

/**
 * Converts a static mesh to a brush.
 *
 * @param	Model					[out] The target brush.  Must be non-NULL.
 * @param	StaticMeshActor			The source static mesh.  Must be non-NULL.
 */
void CreateModelFromStaticMesh(UModel* Model,AStaticMeshActor* StaticMeshActor);

#include "UnEdModeTools.h"
#include "UnWidget.h"

#include "UnEdModes.h"
#include "UnGeom.h"					// Support for the editors "geometry mode"

/**
 * Sets GWorld to the passed in PlayWorld and sets a global flag indicating that we are playing
 * in the Editor.
 *
 * @param	PlayInEditorWorld		PlayWorld
 * @return	the original GWorld
 */
UWorld* SetPlayInEditorWorld( UWorld* PlayInEditorWorld );

/**
 * Restores GWorld to the passed in one and reset the global flag indicating whether we are a PIE
 * world or not.
 *
 * @param EditorWorld	original world being edited
 */
void RestoreEditorWorld( UWorld* EditorWorld );

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
	virtual void Snap( FVector& Point, const FVector& GridBase )=0;
	virtual void SnapScale( FVector& Point, const FVector& GridBase )=0;
	virtual void Snap( FRotator& Rotation )=0;
	virtual UBOOL Snap( FVector& Location, FVector GridBase, FRotator& Rotation )=0;
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
	/* @todo: The functionality here for changing Drag Grid and Rotation Grid snap sizes is duplicated in
	EditorFrame menu events for the same functionality (via UI). */

	enum { MAX_GRID_SIZES=11 };

	// Variables.
	BITFIELD	GridEnabled:1;				// Grid on/off.
	BITFIELD	SnapScaleEnabled:1;			// Snap Scaling to Grid on/off.
	BITFIELD	SnapVertices:1;				// Snap to nearest vertex within SnapDist, if any.
	INT			ScaleGridSize;				// Integer percentage amount to snap scaling to.
	FLOAT		SnapDistance;				// Distance to check for snapping.
	FLOAT		GridSizes[MAX_GRID_SIZES];	// Movement grid size steps.
	INT			CurrentGridSz;				// Index into GridSizes.
	BITFIELD	RotGridEnabled:1;			// Rotation grid on/off.
	FRotator	RotGridSize;				// Rotation grid.
	INT			AngleSnapType;			// Angle snap type

	FLOAT GetGridSize();
	void SetGridSz( INT InIndex );
	
	void RotGridSizeIncrement();
	void RotGridSizeDecrement();

	// Functions.
	virtual void Snap( FVector& Point, const FVector& GridBase );
	virtual void SnapScale( FVector& Point, const FVector& GridBase );
	virtual void Snap( FRotator& Rotation );
	virtual UBOOL Snap( FVector& Location, FVector GridBase, FRotator& Rotation );
	
private:
	static const INT MIN_ROT_ANGLE = 512;
	static const INT MAX_ROT_ANGLE = 16384;
	void SetRotGridSize( INT Angle );
};

/*-----------------------------------------------------------------------------
	UEditorEngine definition.
-----------------------------------------------------------------------------*/

class UEditorEngine : public UEngine, public IInterface_PylonGeometryProvider, public FCallbackEventDevice
{
	DECLARE_CLASS_NOEXPORT(UEditorEngine,UEngine,CLASS_Transient|CLASS_Config|0,UnrealEd)
	NO_DEFAULT_CONSTRUCTOR(UEditorEngine)

	// Objects.
	UModel*						TempModel;
	UModel*						ConversionTempModel;
	class UTransactor*			Trans;
	class UTextBuffer*			Results;
	TArray<class WxPropertyWindowFrame*>	ActorProperties;
	class WxPropertyWindowFrame*	LevelProperties;

	// Graphics.
	UTexture2D *Bad;
	UTexture2D *Bkgnd, *BkgndHi, *BadHighlight, *MaterialArrow, *MaterialBackdrop;

	// Font used by Canvas-based editors
	UFont *EditorFont;

	// Audio
	USoundCue *				PreviewSoundCue;
	UAudioComponent	*		PreviewAudioComponent;

	// Static Meshes
	UStaticMesh* TexPropCube;
	UStaticMesh* TexPropSphere;
	UStaticMesh* TexPropPlane;
	UStaticMesh* TexPropCylinder;

	// Toggles.
	BITFIELD				FastRebuild:1;
	BITFIELD				Bootstrapping:1;
	BITFIELD				IsImportingT3D:1;

	// Variables.
	INT						TerrainEditBrush;
	DWORD					ClickFlags;
	UPackage*				ParentContext;
	FVector					ClickLocation;				// Where the user last clicked in the world
	FPlane					ClickPlane;
	FVector					MouseMovement;				// How far the mouse has moved since the last button was pressed down
	TArray<FEditorLevelViewportClient*> ViewportClients;

	/** Distance to far clipping plane for perspective viewports.  If <= GNearClippingPlane, far plane is at infinity. */
	FLOAT					FarClippingPlane;

	// Setting for the detail mode to show in the editor viewports
	EDetailMode				DetailMode;

	// Constraints.
	FEditorConstraints		Constraints;

	// Advanced.
	BITFIELD				UseSizingBox:1;		// Shows sizing information in the top left corner of the viewports
	BITFIELD				UseAxisIndicator:1;	// Displays an axis indictor in the bottom left corner of the viewports
	FLOAT					FOVAngle;
	BITFIELD				GodMode:1;
	/** The location to autosave to. */
	FStringNoInit			AutoSaveDir;
	BITFIELD				InvertwidgetZAxis;
	FStringNoInit			GameCommandLine;
	/** the list of package names to compile when building scripts */
	TArrayNoInit<FString>	EditPackages;
	/** the base directory to use for finding .uc files to compile*/
	FStringNoInit			EditPackagesInPath;
	/** the directory to save compiled .u files to */
	FStringNoInit			EditPackagesOutPath;
	/** the directory to save compiled .u files to when script is compiled with the -FINAL_RELEASE switch */
	FStringNoInit			FRScriptOutputPath;
	/** If TRUE, always show the terrain in the overhead 2D view. */
	BITFIELD				AlwaysShowTerrain:1;
	/** If TRUE, use the gizmo for rotating actors. */
	BITFIELD				UseActorRotationGizmo:1;
	/** If TRUE, show translucent marker polygons on the builder brush and volumes. */
	BITFIELD				bShowBrushMarkerPolys:1;
	/** If TRUE, use Maya camera controls. */
	BITFIELD				bUseMayaCameraControls:1;
	/** If TRUE, parts of prefabs cannot be individually selected/edited. */
	BITFIELD				bPrefabsLocked:1;
	/** If TRUE, socket snapping is enabled in the main level viewports. */
	BITFIELD				bEnableSocketSnapping:1;
	/** If TRUE, socket names are enabled in the main level viewports. */
	BITFIELD				bEnableSocketNames:1;
	/** If TRUE, determines if reachspecs should be built for this level's pathnodes (may not be necessary if using navmesh) */
	BITFIELD				bBuildReachSpecs:1;
	/** If TRUE, same type views will be camera tied, and ortho views will use perspective view for LOD parenting */
	BITFIELD				bEnableLODLocking:1;
	/** If TRUE, actors can be grouped and grouping rules will be maintained. When deactivated, any currently existing groups will still be preserved.*/
	BITFIELD				bGroupingActive:1;
	
	FString					HeightMapExportClassName;

	/** array of actor factory classes to ignore for the global list (i.e. because they're not relevant to this game) */
	TArrayNoInit<FName> HiddenActorFactoryNames;
	/** Array of actor factories created at editor startup and used by context menu etc. */
	TArray<UActorFactory*>	ActorFactories;

	/**Actors that are being deleted and should processed in the global re-attach*/
	TArray <AActor*> ActorsForGlobalReattach;

	/** String that maps one class name to another, used to create hook for game-specific actors created through shortcuts etc 
	 *  Pairing is "ORIGINALCLASS;DESIREDCLASS
	 *  (ie APylon;AMyGamePylon)
	 */
	TArrayNoInit<FString> ClassMapPair;

	/** The name of the file currently being opened in the editor. "" if no file is being opened.										*/
	FStringNoInit			UserOpenedFile;

	/** Additional per-user/per-game options set in the .ini file. Should be in the form "?option1=X?option2?option3=Y"					*/
	FStringNoInit			InEditorGameURLOptions;
	/** A pointer to a UWorld that is the duplicated/saved-loaded to be played in with "Play From Here" 								*/
	UWorld*					PlayWorld;
	/** An optional location for the starting location for "Play From Here"																*/
	FVector					PlayWorldLocation;
	/** An optional rotation for the starting location for "Play From Here"																*/
	FRotator				PlayWorldRotation;
	/** Has a request for "Play From Here" been made?													 								*/
	BITFIELD				bIsPlayWorldQueued:1;
	/** Has a request to spectate the map been made?													 								*/
	BITFIELD				bStartInSpectatorMode:1;
	/** Did the request include the optional location and rotation?										 								*/
	BITFIELD				bHasPlayWorldPlacement:1;
	/** True to enable mobile preview mode when launching the game from the editor on PC platform */
	BITFIELD				bUseMobilePreviewForPlayWorld:1;
	/** True to start movie capturing right away when launching the game from the editor on PC platform */
	BITFIELD				bStartMovieCapture:1;
	/** Where did the person want to play? Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer	*/
	INT						PlayWorldDestination;
	/** The current play world destination (I.E console).  -1 means no current play world destination, 0 or more is an index into the GConsoleSupportContainer	*/
	INT						CurrentPlayWorldDestination;

	/** Mobile/PC preview settings for what features/resolution to use */
	INT PlayInEditorWidth;
	/** Mobile/PC preview settings for what features/resolution to use */
	INT PlayInEditorHeight;

	/** Mobile preview settings for what orientation to default to */
	BITFIELD bMobilePreviewPortrait:1;

	/** Currently targeted device. */
	EBuildPlayDevice		BuildPlayDevice;

	/** Folders in which the editor looks for template map files */
	TArrayNoInit<FString>	TemplateMapFolders;

	/** When set to anything other than -1, indicates a specific In-Editor viewport index that PIE should use */
	INT						PlayInEditorViewportIndex;

	/** Play world url string edited by a user. */
	FStringNoInit			UserEditedPlayWorldURL;

	/** The width resolution that we want to use for the matinee capture */
	INT MatineeCaptureResolutionX;

	/** The height resolution that we want to use for the matinee capture */
	INT MatineeCaptureResolutionY;

	/** Contains a list of breakpoints that were hit while PlayWorld was active */
	TArrayNoInit<FString> KismetDebuggerBreakpointQueue;
	/** If true, will cause a Kismet debugger window to be opened after the editor world has been restored */
	UBOOL					bIsKismetDebuggerRequested;

	/** The pointer to the propagator to use for sending to the PIE window																*/
	FObjectPropagator*		InEditorPropagator;
	/** The pointer to the propagator to use for sending to a remote target (console or standalone game on this PC						*/
	FObjectPropagator*		RemotePropagator;

	/** Are we currently pushing the perspective view to the object propagator? */
	BITFIELD				bIsPushingView:1;

	/** Issued by code requesting that decals be reattached. */
	BITFIELD				bDecalUpdateRequested:1;

	/** Temporary render targets that can be used by the editor. */
	UTextureRenderTarget2D*	ScratchRenderTarget2048;
	UTextureRenderTarget2D*	ScratchRenderTarget1024;
	UTextureRenderTarget2D*	ScratchRenderTarget512;
	UTextureRenderTarget2D*	ScratchRenderTarget256;
	
	/** Display StreamingBounds for textures */
	UTexture2D*				StreamingBoundsTexture;

	/** Global instance of the editor user settings class. */
	UEditorUserSettings* UserSettings;

	/** Stores the class hierarchy generated from the make commandlet*/
	FEditorClassHierarchy* EditorClassHierarchy;

	/** The paths of the meshes used to preview in editor. */
	TArrayNoInit<FString>	PreviewMeshNames;

	/** A mesh component used to preview in editor without spawning a static mesh actor. */
	class UStaticMeshComponent*	PreviewMeshComp;

	/** The index of the mesh to use from the list of preview meshes. */
	INT						PreviewMeshIndex;

	/** When TRUE, the preview mesh mode is activated. */
	BITFIELD				bShowPreviewMesh:1;

	/** If "Camera Align" emitter handling uses a custom zoom or not */
	BITFIELD bCustomCameraAlignEmitter:1;
	/** The distance to place the camera from an emitter actor when custom zooming is enabled */
	FLOAT CustomCameraAlignEmitterDistance;

	/** If true, then draw sockets when socket snapping is enabled in 'g' mode */
	BITFIELD bDrawSocketsInGMode:1;

	/** If true, then draw particle debug helpers in editor viewports */
	BITFIELD bDrawParticleHelpers:1;

	TArrayNoInit<AGroupActor*> ActiveGroupActors;
	
	/** Actor list for the intermediary buffer level used for moving actors between levels */
	TArrayNoInit<AActor*> BufferLevelActors;

	/** Do we want to force PIE to start in exact place suppressing kismet. It forces all levels to be streamed in, skips all level begin events and sets all matinees to be skipable */
	BITFIELD bForcePlayFromHere:1;

	/** Keeps track of the last actor that had the camera aligned to it in Exec_Camera() */
	AActor* LastCameraAlignTarget;

	/** If true, then do slow reference checks during map check */
	BITFIELD bDoReferenceChecks:1;

	/** 
	* A mapping of all startup packages to whether or not we have warned the user about editing them
	*/
	TMap<UPackage*, UBOOL> StartupPackageToWarnState;

	// Constructor.
	void StaticConstructor();

	// UObject interface.
	virtual void FinishDestroy();
	/** Serializes this object to an archive. */
	virtual void Serialize( FArchive& Ar );

	// UEngine interface.
	virtual void Init();

	/** Get tick rate limitor. */
	virtual FLOAT GetMaxTickRate( FLOAT DeltaTime, UBOOL bAllowFrameRateSmoothing = TRUE );

	/**
	 * Issued by code requesting that decals be reattached.
	 */
	virtual void IssueDecalUpdateRequest();

	/**
	 * Initializes the Editor.
	 */
	void InitEditor();
	
	/**
	 * Constructs a default cube builder brush, this function MUST be called at the AFTER UEditorEngine::Init in order to guarantee builder brush and other required subsystems exist.
	 */
	void InitBuilderBrush();

	virtual void Tick( FLOAT DeltaSeconds );
	void SetClientTravel( const TCHAR* NextURL, ETravelType TravelType ) {}

	/** Get some viewport. Will be GameViewport in game, and one of the editor viewport windows in editor. */
	virtual FViewport* GetAViewport();

	/** Returns the global instance of the editor user settings class. */
	const UEditorUserSettings& GetUserSettings()
	{
		if( UserSettings == NULL )
		{
			UserSettings = ConstructObject<UEditorUserSettings>( UEditorUserSettings::StaticClass() );
		}

		check( UserSettings != NULL );
		return *UserSettings;
	}

	/** Returns the global instance of the editor user settings class. */
	UEditorUserSettings& AccessUserSettings()
	{
		if( UserSettings == NULL )
		{
			UserSettings = ConstructObject<UEditorUserSettings>( UEditorUserSettings::StaticClass() );
		}

		check( UserSettings != NULL );
		return *UserSettings;
	}

	/** Saves the user settings to disk. */
	void SaveUserSettings()
	{
		AccessUserSettings().SaveConfig();
	}

	/**
	 * Updates a single viewport
	 * @param Viewport - the viewport that we're trying to draw
	 * @param bInAllowNonRealtimeViewportToDraw - whether or not to allow non-realtime viewports to update
	 * @param bLinkedOrthoMovement	True if orthographic viewport movement is linked
	 * @return - Whether a NON-realtime viewport has updated in this call.  Used to help time-slice canvas redraws
	 */
	UBOOL UpdateSingleViewportClient(FEditorLevelViewportClient* InViewportClient, const UBOOL bInAllowNonRealtimeViewportToDraw, UBOOL bLinkedOrthoMovement );

	/**
	 * Callback for when a editor property changed.
	 *
	 * @param	PropertyThatChanged	Property that changed and initiated the callback.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Callback for when a package gets saved.
	 *
	 * @param	Package - The current package being saved.
	 * @param	bIsSilent - The caller wants non-critical messages suppressed.
	 */
	virtual void PreparePackageForSaving(UPackage* Package, TCHAR* Path, UBOOL bIsSilent) {}

	/** Used for generating status bar text */
	enum EMousePositionType
	{
		MP_None,
		MP_WorldspacePosition,
		MP_Translate,
		MP_Rotate,
		MP_Scale,
		MP_CameraSpeed,
		MP_NoChange
	};

	/**
	* Updates the mouse position status bar field.
	*
	* @param PositionType	Mouse position type, used to decide how to generate the status bar string.
	* @param Position		Position vector, has the values we need to generate the string.  These values are dependent on the position type.
	*/
	virtual void UpdateMousePositionText( EMousePositionType PositionType, const FVector &Position ) { check(0); }

	/**
	 * Returns whether or not the map build in progressed was cancelled by the user. 
	 */
	virtual UBOOL GetMapBuildCancelled() const
	{
		return FALSE;
	}

	/**
	 * Sets the flag that states whether or not the map build was cancelled.
	 *
	 * @param InCancelled	New state for the cancelled flag.
	 */
	virtual void SetMapBuildCancelled( UBOOL InCancelled )
	{
		// Intentionally empty.
	}

	/**
	 * Returns whether or not the actor passed in should draw as wireframe.
	 *
	 * @param InActor	Actor that is being drawn.
	 */
	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor );

	// UnEdSrv.cpp
	virtual UBOOL SafeExec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	UBOOL Exec_StaticMesh( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Brush( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Paths( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Poly( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Obj( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Class( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Camera( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Transaction(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Exec_Particle(const TCHAR* Str, FOutputDevice& Ar);

	/** 
	 *	Gather the status of BakeAndPrune settings for the referenced AnimSets in the given level
	 *	
	 *	@param	InLevel							The source level
	 *	@param	OutAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
	 *											This is filled in during the bake, and used during the prune
	 *											If NULL, bake all possible animsets
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	UBOOL GatherBakeAndPruneStatus(ULevel* InLevel, TMap<FString,UBOOL>* OutAnimSetSkipBakeAndPruneMap);

	/** 
	 *	Clear the BakeAndPrune status arrays in the given level
	 *	
	 *	@param	InLevel							The source level
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	UBOOL ClearBakeAndPruneStatus(ULevel* InLevel);

	/** 
	 *	Bake the anim sets in the given level
	 *	
	 *	@param	InLevel							The source level
	 *	@param	InAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
	 *											If NULL, bake all possible animsets
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	UBOOL BakeAnimSetsInLevel(ULevel* InLevel, TMap<FString,UBOOL>* InAnimSetSkipBakeAndPruneMap);

	/** 
	 *	Prune the anim sets in the given level
	 *	
	 *	@param	InLevel							The source level
	 *	@param	InAnimSetSkipBakeAndPruneMap	List of anim sets to not touch during the operation
	 *											If NULL, prune all possible animsets
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	UBOOL PruneAnimSetsInLevel(ULevel* InLevel, TMap<FString,UBOOL>* InAnimSetSkipBakeAndPruneMap);

	/**
	 *	Clear unreferenced AnimSets from InterpData Groups
	 *
	 *	@param	InLevel		The source level to clean up
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not
	 */
	UBOOL ClearUnreferenceAnimSetsFromGroups(ULevel* InLevel);

	/**
	 * Executes each line of text in a file sequentially, as if each were a separate command
	 *
	 * @param InFilename The name of the file to load and execute
	 * @param Ar Output device
	 */
	void ExecFile( const TCHAR* InFilename, FOutputDevice& Ar );

	// Transaction interfaces.
	INT BeginTransaction(const TCHAR* SessionName);
	INT EndTransaction();
	void ResetTransaction(const TCHAR* Action);
	void CancelTransaction(INT Index);
	UBOOL UndoTransaction();
	UBOOL RedoTransaction();
	UBOOL IsTransactionActive();
	INT GetFreeTransactionBufferSpace();
	INT GetLastTransactionSize();
	UBOOL IsTransactionBufferBreeched();

	/**
	 * Rebuilds the map.
	 *
	 * @param bBuildAllVisibleMaps	Whether or not to rebuild all visible maps, if FALSE only the current level will be built.
	 */
	enum EMapRebuildType
	{
		MRT_Current				= 0,
		MRT_AllVisible			= 1,
		MRT_AllDirtyForLighting	= 2,
	};
	void RebuildMap(EMapRebuildType RebuildType);

	/**
	 * Quickly rebuilds a single level (no bounds build, visibility testing or Bsp tree optimization).
	 *
	 * @param Level	The level to be rebuilt.
	 */
	void RebuildLevel(ULevel& Level);

	/**
	 * Builds up a model from a set of brushes. Used by RebuildLevel.
	 *
	 * @param Model					The model to be rebuilt.
	 * @param bSelectedBrushesOnly	Use all brushes in the current level or just the selected ones?.
	 * @param bExcludeBuilderBrush	Ignore the current builder brush? Defaults to ignore it.
	 * @param bStaticBrushesOnly	Use only static brushes? Dynamic brushes like volumes are ignored. Defaults to ignore dynamic brushes.
	 * @param bAddActiveBrushes		Treat brushes of type CSG_Active as CGS_Add for model building purposes? Defaults to FALSE. Used by the 'convert to mesh' code.
	 *
	 * @return Number of brushes used.
	 */
	UINT RebuildModelFromBrushes(UModel* Model, UBOOL bSelectedBrushesOnly, UBOOL bExcludeBuilderBrush = TRUE, UBOOL bStaticBrushesOnly = TRUE, UBOOL bAddActiveBrushes = FALSE);

	/**
	 * Rebuilds levels containing currently selected brushes and should be invoked after a brush has been modified
	 */
	void RebuildAlteredBSP();

	/**
	 * @return	A pointer to the named actor or NULL if not found.
	 */
	AActor* SelectNamedActor(const TCHAR *TargetActorName);

	/**
	 * Moves all viewport cameras to the target actor.
	 * @param	Actor					Target actor.
	 * @param	bActiveViewportOnly		If TRUE, move/reorient only the active viewport.
	 */
	void MoveViewportCamerasToActor(AActor& Actor,  UBOOL bActiveViewportOnly);

	/** 
	 * Moves an actor to the floor.  Optionally will align with the trace normal.
	 * @param InActor			Actor to move to the floor.
	 * @param InAlign			Whether or not to rotate the actor to align with the trace normal.
	 * @return					Whether or not the actor was moved.
	 */
	UBOOL MoveActorToFloor( AActor* InActor, UBOOL InAlign, UBOOL UsePivot );

	/**
	 * Moves an actor in front of a camera specified by the camera's origin and direction.
	 * The distance the actor is in front of the camera depends on the actors bounding cylinder
	 * or a default value if no bounding cylinder exists.
	 * 
	 * @param InActor			The actor to move
	 * @param InCameraOrigin	The location of the camera in world space
	 * @param InCameraDirection	The normalized direction vector of the camera
	 */
	void MoveActorInFrontOfCamera( AActor& InActor, const FVector& InCameraOrigin, const FVector& InCameraDirection );

	/**
	* Moves all viewport cameras to focus on the provided array of actors.
	* @param	Actors					Target actors.
	* @param	bActiveViewportOnly		If TRUE, move/reorient only the active viewport.
	*/
	void MoveViewportCamerasToActor(const TArray<AActor*> &Actors, UBOOL bActiveViewportOnly);

	/**
	 * Snaps the view of the camera to that of the provided actor.
	 *
	 * @param	Actor	The actor the camera is going to be snapped to.
	 */
	void SnapViewToActor(const AActor &Actor);


	// Pivot handling.
	virtual FVector GetPivotLocation() { return FVector(0,0,0); }
	/** Sets the editor's pivot location, and optionally the pre-pivots of actors.
	 *
	 * @param	NewPivot				The new pivot location
	 * @param	bSnapPivotToGrid		If TRUE, snap the new pivot location to the grid.
	 * @param	bIgnoreAxis				If TRUE, leave the existing pivot unaffected for components of NewPivot that are 0.
	 * @param	bAssignPivot			If TRUE, assign the given pivot to any valid actors that retain it (defaults to FALSE)
	 */
	virtual void SetPivot(FVector NewPivot, UBOOL bSnapPivotToGrid, UBOOL bIgnoreAxis, UBOOL bAssignPivot=FALSE) {}
	virtual void ResetPivot() {}


	// General functions.
	/**
	 * Cleans up after major events like e.g. map changes.
	 *
	 * @param	ClearSelection	Whether to clear selection
	 * @param	Redraw			Whether to redraw viewports
	 * @param	TransReset		Human readable reason for resetting the transaction system
	 */
	virtual void Cleanse( UBOOL ClearSelection, UBOOL Redraw, const TCHAR* TransReset );
	virtual void FinishAllSnaps() { }

	/**
	 * Redraws all level editing viewport clients.
	 *
	 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
	 */
	virtual void RedrawLevelEditingViewports(UBOOL bInvalidateHitProxies=TRUE) {}
	virtual void NoteSelectionChange() { check(0); }

	/**
	 * Adds an actor to the world at the specified location.
	 *
	 * @param	Class		A non-abstract, non-transient, placeable class.  Must be non-NULL.
	 * @param	Location	The world-space location to spawn the actor.
	 * @param	bSilent		If TRUE, suppress logging (optional, defaults to FALSE).
	 * @result				A pointer to the newly added actor, or NULL if add failed.
	 */
	virtual AActor* AddActor(UClass* Class, const FVector& Location, UBOOL bSilent = FALSE);

	virtual void NoteActorMovement() { check(0); }
	virtual UTransactor* CreateTrans();

	/** 
	 * Returns an audio component linked to the current scene that it is safe to play a sound on
	 *
	 * @param	SoundCue	A sound cue to attach to the audio component
	 * @param	SoundNode	A sound node that is attached to the audio component when the sound cue is NULL
	 */
	UAudioComponent* GetPreviewAudioComponent( USoundCue* SoundCue, USoundNode* SoundNode );

	/** 
	 * Stop any sounds playing on the preview audio component and allowed it to be garbage collected
	 */
	void ClearPreviewAudioComponents( void );

	/**
	 * Redraws all editor viewport clients.
	 *
	 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
	 */
	void RedrawAllViewports(UBOOL bInvalidateHitProxies=TRUE);

	/**
	 * Invalidates all viewports parented to the specified view.
	 *
	 * @param	InParentView				The parent view whose child views should be invalidated.
	 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
	 */
	void InvalidateChildViewports(FSceneViewStateInterface* InParentView, UBOOL bInvalidateHitProxies=TRUE);

	/**
	 * Looks for an appropriate actor factory for the specified UClass.
	 *
	 * @param	InClass		The class to find the factory for.
	 * @return				A pointer to the factory to use.  NULL if no factories support this class.
	 */
	UActorFactory* FindActorFactory( const UClass* InClass );

	/**
	 * Uses the supplied factory to create an actor at the clicked location ands add to level.
	 *
	 * @param	Factory					The factor to create the actor from.  Must be non-NULL.
	 * @param	bIgnoreCanCreate		[opt] If TRUE, don't call UActorFactory::CanCreateActor.  Default is FALSE.
	 * @param	bUseSurfaceOrientation	[opt] If TRUE, align new actor's orientation to the underlying surface normal.  Default is FALSE.
	 * @param	bUseCurrentSelection	[opt] If TRUE, fills in the factory properties using the currently selected object(s)
	 * @return							A pointer to the new actor, or NULL on fail.
	 */
	AActor* UseActorFactory( UActorFactory* Factory, UBOOL bIgnoreCanCreate=FALSE, UBOOL bUseSurfaceOrientation=FALSE, UBOOL bUseCurrentSelection=TRUE ); 

	/** replaces the selected Actors with the same number of a different kind of Actor
	 * if a Factory is specified, it is used to spawn the requested Actors, otherwise NewActorClass is used (one or the other must be specified)
	 * note that only Location, Rotation, Tag, and Group are copied from the old Actors
	 * @param Factory - the Factory to use to create Actors
	 */
	void ReplaceSelectedActors(UActorFactory* Factory, UClass* NewActorClass);

	/** 
	 * Converts passed in light actors into new actors of another type.
	 * Note: This replaces the old actor with the new actor.
	 * Most properties of the old actor that can be copied are copied to the new actor during this process.
	 * Properties that can be copied are ones found in a common superclass between the actor to convert and the new class. 
	 * Common light component properties between the two classes are also copied
	 *
	 * @param	ActorsToConvert	A list of actors to convert
	 * @param	ConvertToClass	The light class we are going to convert to. 
	 */
	void ConvertLightActors( const TArray< AActor* >& ActorsToConvert, UClass* ConvertToClass );

	/**
	 * Converts passed in actors into new actors of the specified type.
	 * Note: This replaces the old actors with brand new actors while attempting to preserve as many properties as possible.
	 * Properties of the actors components are also attempted to be copied for any component names supplied in the third parameter.
	 * If a component name is specified, it is only copied if a component of the specified name exists in the source and destination actors,
	 * as well as in the class default object of the class of the source actor, and that all three of those components share a common base class.
	 * This approach is used instead of simply accepting component classes to copy because some actors could potentially have multiple of the same
	 * component type.
	 *
	 * @param	ActorsToConvert			Array of actors which should be converted to the new class type
	 * @param	ConvertToClass			Class to convert the provided actors to
	 * @param	ComponentsToConsider	Names of components to consider for property copying as well
	 * @param	bIgnoreKismetRefActors	If TRUE, actors which are referenced by Kismet will not be converted
	 */
	void ConvertActors( const TArray<AActor*>& ActorsToConvert, UClass* ConvertToClass, const TSet<FString>& ComponentsToConsider, UBOOL bIgnoreKismetRefActors );

	/**
	 * Changes the state of preview mesh mode to on or off. 
	 *
	 * @param	bState	Enables the preview mesh mode if TRUE; Disables the preview mesh mode if FALSE. 
	 */
	void SetPreviewMeshMode( UBOOL bState );

	/**
	 * Updates the position of the preview mesh in the level. 
	 */
	void UpdatePreviewMesh();

	/**
	 * Changes the preview mesh to the next one. 
	 */
	void CyclePreviewMesh();

	/**
	 * Copy selected actors to the clipboard.  Does not copy PrefabInstance actors or parts of Prefabs.
	 *
	 * @param	bReselectPrefabActors	If TRUE, reselect any actors that were deselected prior to export as belonging to prefabs.
	 * @param	bClipPadCanBeUsed		If TRUE, the clip pad is available for use if the user is holding down SHIFT.
	 * @param	DestinationData			If != NULL, additionally copy data to string
	 */
	virtual void edactCopySelected(UBOOL bReselectPrefabActors, UBOOL bClipPadCanBeUsed, FString* DestinationData = NULL) {}

	/**
	 * Paste selected actors from the clipboard.
	 *
	 * @param	bDuplicate			Is this a duplicate operation (as opposed to a real paste)?
	 * @param	bOffsetLocations	Should the actor locations be offset after they are created?
	 * @param	bClipPadCanBeUsed	If TRUE, the clip pad is available for use if the user is holding down SHIFT.
	 * @param	SourceData			If != NULL, use instead of clipboard data
	 */
	virtual void edactPasteSelected(UBOOL bDuplicate, UBOOL bOffsetLocations, UBOOL bClipPadCanBeUsed, FString* SourceData = NULL) {}

	/** 
	 * Duplicates selected actors.  Handles the case where you are trying to duplicate PrefabInstance actors.
	 *
	 * @param	bUseOffset		Should the actor locations be offset after they are created?
	 */
	virtual void edactDuplicateSelected(UBOOL bOffsetLocations) {}

	/**
	 * Deletes all selected actors.  bIgnoreKismetReferenced is ignored when bVerifyDeletionCanHappen is TRUE.
	 *
	 * @param		bVerifyDeletionCanHappen	[opt] If TRUE (default), verify that deletion can be performed.
	 * @param		bIgnoreKismetReferenced		[opt] If TRUE, don't delete actors referenced by Kismet.
	 * @return									TRUE unless the delete operation was aborted.
	 */
	virtual UBOOL edactDeleteSelected(UBOOL bVerifyDeletionCanHappen=TRUE, UBOOL bIgnoreKismetReferenced=FALSE) { return TRUE; }

	/**
	 * Checks the state of the selected actors and notifies the user of any potentially unknown destructive actions which may occur as
	 * the result of deleting the selected actors.  In some cases, displays a prompt to the user to allow the user to choose whether to
	 * abort the deletion.
	 *
	 * @param	bOutIgnoreKismetReferenced	[out] Set only if it's okay to delete actors; specifies if the user wants Kismet-refernced actors not deleted.
	 * @return								FALSE to allow the selected actors to be deleted, TRUE if the selected actors should not be deleted.
	 */
	virtual UBOOL ShouldAbortActorDeletion(UBOOL& bOutIgnoreKismetReferenced) const { return FALSE; }

	/**
	 * Create archetypes of the selected actors, and replace those actors with instances of
	 * the archetype class.
	 */
	virtual void edactArchetypeSelected() {};

	/**
	 *  Update archetype of the selected actor
	 */
	virtual void edactUpdateArchetypeSelected() {};

	/** Create a prefab from the selected actors, and replace those actors with an instance of that prefab. */
	virtual void edactPrefabSelected() {};

	/** Add the selected prefab at the clicked location. */
	virtual void edactAddPrefab() {};

	/** Select all Actors that make up the selected PrefabInstance. */
	virtual void edactSelectPrefabActors() {};

	// Editor CSG virtuals from UnEdCsg.cpp.
	virtual void csgRebuild();

	// Editor EdPoly/BspSurf assocation virtuals from UnEdCsg.cpp.
	virtual INT polyFindMaster( UModel* Model, INT iSurf, FPoly& Poly );
	virtual void polyUpdateMaster( UModel* Model, INT iSurf, INT UpdateTexCoords );
	virtual void polyGetLinkedPolys( ABrush* InBrush, FPoly* InPoly, TArray<FPoly>* InPolyList );
	virtual void polyGetOuterEdgeList( TArray<FPoly>* InPolyList, TArray<FEdge>* InEdgeList );
	virtual void polySplitOverlappingEdges( TArray<FPoly>* InPolyList, TArray<FPoly>* InResult );

	// Bsp Poly search virtuals from UnEdCsg.cpp.
	virtual void polySetAndClearPolyFlags( UModel* Model, DWORD SetBits, DWORD ClearBits, INT SelectedOnly, INT UpdateMaster );

	// Selection.
	virtual void SelectActor(AActor* Actor, UBOOL InSelected, FViewportClient* InViewportClient, UBOOL bNotify, UBOOL bSelectEvenIfHidden=FALSE) {}
	virtual void SelectGroup(AGroupActor* InGroupActor, UBOOL bForceSelection=FALSE, UBOOL bInSelected=TRUE) {}

	/**
	 * Replaces the components in ActorsToReplace with an primitive component in Replacement
	 *
	 * @param ActorsToReplace Primitive components in the actors in this array will have their ReplacementPrimitive set to a component in Replacement
	 * @param Replacement The first usable component in Replacement will be the ReplacementPrimitive for the actors
	 * @param ClassToReplace If this is set, only components will of this class will be used/replaced
	 */
	virtual void AssignReplacementComponentsByActors(TArray<AActor*>& ActorsToReplace, AActor* Replacement, UClass* ClassToReplace=NULL);

	/**
	 * Selects or deselects a BSP surface in the persistent level's UModel.  Does nothing if GEdSelectionLock is TRUE.
	 *
	 * @param	InModel					The model of the surface to select.
	 * @param	iSurf					The index of the surface in the persistent level's UModel to select/deselect.
	 * @param	bSelected				If TRUE, select the surface; if FALSE, deselect the surface.
	 * @param	bNoteSelectionChange	If TRUE, call NoteSelectionChange().
	 */
	virtual void SelectBSPSurf(UModel* InModel, INT iSurf, UBOOL bSelected, UBOOL bNoteSelectionChange) {}

	/**
	 * Deselect all actors.  Does nothing if GEdSelectionLock is TRUE.
	 *
	 * @param	bNoteSelectionChange		If TRUE, call NoteSelectionChange().
	 * @param	bDeselectBSPSurfs			If TRUE, also deselect all BSP surfaces.
	 */
	virtual void SelectNone(UBOOL bNoteSelectionChange, UBOOL bDeselectBSPSurfs, UBOOL WarnAboutManyActors=TRUE) {}

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

	/**
	 * Selects surfaces whose material matches that of any selected surfaces.
	 *
	 * @param	bCurrentLevelOnly		If TRUE, select
	 */
	virtual void polySelectMatchingMaterial(UBOOL bCurrentLevelOnly);
	/**
	 * Selects surfaces whose lightmap resolution matches that of any selected surfaces.
	 *
	 * @param	bCurrentLevelOnly		If TRUE, select
	 */
	virtual void polySelectMatchingResolution(UBOOL bCurrentLevelOnly);

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
	virtual void mapSelectOperation( ECsgOper CSGOper );
	virtual void mapSelectFlags( DWORD Flags );
	virtual void mapBrushGet();
	virtual void mapBrushPut();
	virtual void mapSendToFirst();
	virtual void mapSendToLast();
	virtual void mapSendToSwap();
	virtual void mapSetBrush(enum EMapSetBrushFlags PropertiesMask, WORD BrushColor, FName Group, DWORD SetPolyFlags, DWORD ClearPolyFlags, DWORD CSGOper, INT DrawType );

	// Bsp virtuals from UnBsp.cpp.
	virtual void bspRepartition( INT iNode, UBOOL Simple );
	virtual INT bspNodeToFPoly( UModel* Model, INT iNode, FPoly* EdPoly );
	virtual void bspCleanup( UModel* Model );
	virtual void bspBuildFPolys( UModel* Model, UBOOL SurfLinks, INT iNode );
	virtual void bspMergeCoplanars( UModel* Model, UBOOL RemapLinks, UBOOL MergeDisparateTextures );
	/**
	 * Performs any CSG operation between the brush and the world.
	 *
	 * @param	Actor							The brush actor to apply.
	 * @param	Model							The model to apply the CSG operation to; typically the world's model.
	 * @param	PolyFlags						PolyFlags to set on brush's polys.
	 * @param	CSGOper							The CSG operation to perform.
	 * @param	bBuildBounds					If TRUE, updates bounding volumes on Model for CSG_Add or CSG_Subtract operations.
	 * @param	bMergePolys						If TRUE, coplanar polygons are merged for CSG_Intersect or CSG_Deintersect operations.
	 * @param	bReplaceNULLMaterialRefs		If TRUE, replace NULL material references with a reference to the GB-selected material.
	 * @param	bShowProgressBar				If TRUE, display progress bar for complex brushes
	 * @return									0 if nothing happened, 1 if the operation was error-free, or 1+N if N CSG errors occurred.
	 */
	virtual INT bspBrushCSG( ABrush* Actor, UModel* Model, DWORD PolyFlags, ECsgOper CSGOper, UBOOL bBuildBounds, UBOOL bMergePolys, UBOOL bReplaceNULLMaterialRefs, UBOOL bShowProgressBar=TRUE );
	virtual void bspOptGeom( UModel* Model );

	/** Break all connections to all selected SplineActors */
	virtual void SplineBreakAll();
	/** Create connection between first 2 selected SplineActors (or flip connection if already connected) */
	virtual void SplineConnect();
	/** Break any connections between selected SplineActors */
	virtual void SplineBreak();
	/** Util that reverses direction of all splines between selected SplineActors */	
	virtual void SplineReverseAllDirections();
	/** Util to test a route from one selected spline node to another */
	virtual void SplineTestRoute();
	/** Select all nodes on the same splines as selected set */
	virtual void SplineSelectAllNodes();
	/** Set tangents on the selected two points to be straight and even. */
	virtual void SplineStraightTangents();

	/**
	 * Builds lighting information depending on passed in options.
	 *
	 * @param	Options		Options determining on what and how lighting is built
	 */
	void BuildLighting(const class FLightingBuildOptions& Options);

	/**
	 * Builds fluid surface data
	 */
	void BuildFluidSurfaces();

	/**
	 * Assembles a list of worlds whose PIE packages need resaving.
	 *
	 * @param	FilenamePrefix				The PIE filename prefix.
	 * @param	OutWorldsNeedingPIESave		[out] The list worlds that need to be saved for PIE.
	 */
	void GetWorldsNeedingPIESave(const TCHAR* FilenamePrefix, TArray<UWorld*>& OutWorldsNeedingPIESave) const;

	/**
	 * Creates a fully qualified PIE package file name, given an original package file name
	 */
	FString MakePIEFileName( const TCHAR* FilenamePrefix, const FFilename& PackageFileName ) const;

	/**
	 * Checks to see if we need to delete any PIE files from disk, usually because we're closing a map (or shutting
	 * down) and the in memory map data has been modified since PIE packages were generated last
	 */
	void PurgePIEDataForDirtyPackagesIfNeeded();


	/**
	 * Open a PSA file with the given name, and import each sequence into the supplied AnimSet.
	 * This is only possible if each track expected in the AnimSet is found in the target file. If not the case, a warning is given and import is aborted.
	 * If AnimSet is empty (ie. TrackBoneNames is empty) then we use this PSA file to form the track names array.
	 */
	static void ImportPSAIntoAnimSet( class UAnimSet* AnimSet, const TCHAR* Filename, class USkeletalMesh* FillInMesh, UBOOL bSilence=FALSE );


	/**
	 * Performs any updates and recalculations on notifiers attached to an animset.  This is useful to recalculate data
	 * (such as AnimTrail info) in the event that an animation or mesh has been reimported.
	 */
	 static void UpdateAnimSetNotifiers(class UAnimSet* AnimSet, class UAnimNodeSequence* NodeSequence);

#if WITH_FBX
	/**
	 * Open a Fbx file with the given name, and import each sequence into the supplied AnimSet.
	 * This is only possible if each track expected in the AnimSet is found in the target file. If not the case, a warning is given and import is aborted.
	 * If AnimSet is empty (ie. TrackBoneNames is empty) then we use this Fbx file to form the track names array.
	 *
	 * @param AnimSet	The animset to import into
	 * @param Filename	The FBX filename
	 * @param FillInMesh	The skeletal mesh to fill in
	 * @param bImportMorphTracks	TRUE to import any morph curve data.
	 * @param bHideMissingTrackError TRUE to bypass missing track messages.
	 */
	static void ImportFbxANIMIntoAnimSet( UAnimSet* AnimSet, const TCHAR* Filename, class USkeletalMesh* FillInMesh, UBOOL bImportMorphTracks, UBOOL& bHideMissingTrackError );
#endif // WITH_FBX


	// Visibility.
	virtual void TestVisibility( UModel* Model, int A, int B );

	// Scripts.
	virtual UBOOL MakeScripts( UClass* BaseClass, FFeedbackContext* Warn, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses, UPackage* LimitOuter = NULL, UBOOL bParseOnly=0, UBOOL bHeaders=FALSE );

	// Object management.
	virtual void RenameObject(UObject* Object,UObject* NewOuter,const TCHAR* NewName, ERenameFlags Flags=REN_None);

	// Level management.
	void AnalyzeLevel(ULevel* Level,FOutputDevice& Ar);

	/**
	 * Pastes clipboard text into a clippad entry
	 */
	virtual void PasteClipboardIntoClipPad() {}

	/**
	 * Removes all components from the current level's scene.
	 */
	void ClearComponents();

	/**
	 * Updates all components in the current level's scene.
	 */
	void UpdateComponents();

	/**
	 * Check for any PrefabInstances which are out of date.  For any PrefabInstances which have a TemplateVersion less than its PrefabTemplate's
	 * PrefabVersion, propagates the changes to the source Prefab to the PrefabInstance.
	 */
	void UpdatePrefabs();

	/** Util that looks for and fixes any incorrect ParentSequence pointers in Kismet objects in memory. */
	void FixKismetParentSequences();
	/**	Runs ConvertObject() and UpdateObject() on all out-of-date SequenceObjects */
	void UpdateKismetObjects();
	/** Since the Kismet window cannot be modified while PlayInEditor world is active, this method can be used to defer a call to WxKismet::OpenKismetDebugger() when the editor world is active */
	void RequestKismetDebuggerOpen(const TCHAR* SequenceName);


	/**
	 * FCallbackEventDevice interface
	 */
	virtual void Send( ECallbackEventType Event );
	virtual void Send( ECallbackEventType Event, DWORD Param );


	/**
	 * Makes a request to start a play from editor session (in editor or on a remote platform)
	 * @param	StartLocation			If specified, this is the location to play from (via a Teleporter - Play From Here)
	 * @param	StartRotation			If specified, this is the rotation to start playing at
	 * @param	DestinationConsole		Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer
	 * @param	InPlayInViewportIndex	Viewport index to play the game in, or -1 to spawn a standalone PIE window
	 * @param	bUseMobilePreview		True to enable mobile preview mode (PC platform only)
	 * @param	bMovieCapture			True to start with movie capture recording (PC platform only)
	 */
	virtual void PlayMap( FVector* StartLocation = NULL, FRotator* StartRotation = NULL, INT DestinationConsole = -1, INT InPlayInViewportIndex = -1, UBOOL bUseMobilePreview = FALSE, UBOOL bMovieCapture = FALSE );

	/**
	 * Kicks off a "Play From Here" request that was most likely made during a transaction
	 */
	virtual void StartQueuedPlayMapRequest();

	/**
	 * Saves play in editor levels and also fixes up references in AWorldInfo to other levels.
	 *
	 * @param	Prefix				Prefix used to save files to disk.
	 * @param	bSaveAllPackages	Do we save all non-map packages as well? Useful for PlayOnXenon which needs to copy updated packages to the Xenon
	 *
	 * @return	False if the save failed and the user wants to abort what they were doing
	 */
	virtual UBOOL SavePlayWorldPackages( const TCHAR* Prefix, UBOOL bSaveAllPackages );

	/**
	 * Builds a URL for game spawned by the editor (not including map name!). Has stuff like the teleporter, spectating, etc.
	 * @param	MapName			The name of the map to put into the URL
	 * @param	bSpecatorMode	If true, the player starts in spectator mode
	 *
	 * @return	The URL for the game
	 */
	virtual FString BuildPlayWorldURL(const TCHAR* MapName, UBOOL bSpectatorMode = FALSE);

	/**
	 * Spawns a teleporter in the given world
	 * @param	World		The World to spawn in (for PIE this may not be GWorld)
	 * @param	Teleporter	A reference to the resulting Teleporter actor
	 *
	 * @return	If the spawn failed
	 */
	virtual UBOOL SpawnPlayFromHereTeleporter(UWorld* World, ATeleporter*& Teleporter);

	/**
	 * Starts a Play In Editor session
	 */
	virtual void PlayInEditor();

	/**
	 * Sends the level over to a platform (one of the platorms in the GConsoleSupportContainer)
	 * @param	ConsoleIndex		The index into the GConsoleSupportContainer of which platform to play on
	 * @param	bUseMobilePreview	True to enable mobile preview mode (PC platform only)
	 * @param	bStartMovieCapture	True to start with movie capture recording (PC platform only)
	 */
	virtual void PlayOnConsole(INT ConsoleIndex, UBOOL bUseMobilePreview, UBOOL bStartMovieCapture);

	/**
	 * Kills the Play From Here session
	 */
	virtual void EndPlayMap();

	/**
	 * Ends the current play on console session.
	 */
	virtual void EndPlayOnConsole();

	/**
	 * Creates an embedded Play In Editor viewport window (if possible)
	 *
	 * @param ViewportClient The viewport client the new viewport will be associated with
	 * @param InPlayInViewportIndex Viewport index to play in, or -1 for "don't care"
	 *
	 * @return TRUE if successful
	 */
	virtual UBOOL CreateEmbeddedPIEViewport( UGameViewportClient* ViewportClient, INT InPlayInViewportIndex )
	{
		// Override this in derived classes
		return FALSE;
	}

	/**
	 * Sets where the object propagation in the editor goes (PIE, a console, etc)
	 * @param	Destination		The enum of where to send propagations (see top of this file for values
	 */
	void SetObjectPropagationDestination(INT Destination);

	/**
	 * The support DLL may have changed the IP address to propagate, so update the IP address
	 */
	void UpdateObjectPropagationIPAddress();

	/**
	 * Disables any realtime viewports that are currently viewing the level.  This will not disable
	 * things like preview viewports in Cascade, etc. Typically called before running the game.
	 */
	void DisableRealtimeViewports();

	/**
	 * Restores any realtime viewports that have been disabled by DisableRealtimeViewports. This won't
	 * disable viewporst that were realtime when DisableRealtimeViewports has been called and got
	 * latter toggled to be realtime.
	 */
	void RestoreRealtimeViewports();

	/**
	 *	Returns pointer to a temporary render target.
	 *	If it has not already been created, does so here.
	 */
	UTextureRenderTarget2D* GetScratchRenderTarget( const UINT MinSize );

	/**
	 * Resets the autosave timer.
	 */
	virtual void ResetAutosaveTimer() {}

	/**
	 * Pauses the autosave timer.
	 */
	virtual void PauseAutosaveTimer(UBOOL bPaused) {}

	/**
	 * Handles freezing/unfreezing of rendering
	 */
	virtual void ProcessToggleFreezeCommand();

	/**
	 * Handles frezing/unfreezing of streaming
	 */
	virtual void ProcessToggleFreezeStreamingCommand();

	// Editor specific

	/**
	* Closes the main editor frame.
	*/ 
	virtual void CloseEditor() {}
	virtual void ShowUnrealEdContextMenu() {}
	virtual void ShowUnrealEdContextSurfaceMenu() {}
	virtual void ShowUnrealEdContextCoverSlotMenu(class ACoverLink *Link, FCoverSlot &Slot) {}
	virtual void GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass ) {}

	/**
	 *	Get/Set the ParticleSystemRealTime flag in the thumbnail manager
	 */
	virtual UBOOL GetPSysRealTimeFlag()	{ return FALSE;	}
	virtual void SetPSysRealTimeFlag(UBOOL bPSysRealTime)	{};

	/**
	 * Returns the number of currently selected actors.
	 *
	 */
	UBOOL GetSelectedActorCount() const;

	/**
	 * Returns the set of selected actors.
	 */
	class USelection* GetSelectedActors() const;

	/**
	 * Returns an FSelectionIterator that iterates over the set of selected actors.
	 */
	class FSelectionIterator GetSelectedActorIterator() const;

	/**
	 * Returns the set of selected non-actor objects.
	 */
	class USelection* GetSelectedObjects() const;

	/**
	 * Returns the appropriate selection set for the specified object class.
	 */
	class USelection* GetSelectedSet( const UClass* Class ) const;

	FString GetMobileDeviceSystemSettingsSection() const;

	/**
	 * Clears out the current map, if any, and creates a new blank map.
	 */
	void NewMap();

	/**
	 * Exports the current map to the specified filename.
	 *
	 * @param	InFilename					Filename to export the map to.
	 * @param	bExportSelectedActorsOnly	If TRUE, export only the selected actors.
	 */
	void ExportMap(const TCHAR* InFilename, UBOOL bExportSelectedActorsOnly);

	/**
	 * Iterates over objects belonging to the specified packages and reports
	 * direct references to objects in the trashcan packages.  Only looks
	 * at loaded objects.  Output is to the specified arrays -- the i'th
	 * element of OutObjects refers to the i'th element of OutTrashcanObjects.
	 *
	 * @param	Packages			Only objects in these packages are considered when looking for trashcan references.
	 * @param	OutObjects			[out] Receives objects that directly reference objects in the trashcan.
	 * @param	OutTrashcanObject	[out] Receives the list of referenced trashcan objects.
	 */
	void CheckForTrashcanReferences(const TArray<UPackage*>& SelectedPackages, TArray<UObject*>& OutObjects, TArray<UObject*>& OutTrashcanObjects);

	/**
	 * Checks loaded levels for references to objects in the trashcan and
	 * reports to the Map Check dialog.
	 */
	void CheckLoadedLevelsForTrashcanReferences();

	/**
	 * Deselects all selected prefab instances or actors belonging to prefab instances.  If a prefab
	 * instance is selected, or if all actors in the prefab are selected, record the prefab.
	 *
	 * @param	OutPrefabInstances		[out] The set of prefab instances that were selected.
	 * @param	bNotify					If TRUE, call NoteSelectionChange if any actors were deselected.
	 */
	void DeselectActorsBelongingToPrefabs(TArray<APrefabInstance*>& OutPrefabInstances, UBOOL bNotify);

	/**
	 * Moves selected actors to the current level.
	 *
	 * @param	bUseCurrentLevelGridVolume	If true, moves actors to an appropriately level associated with the current level grid volume (if there is one.)  If there is no current level grid volume, the actors will be moved to the current level.
	 */
	void MoveSelectedActorsToCurrentLevel( const UBOOL bUseCurrentLevelGridVolume );

	/**
	 * Copies selected actors to the clipboard.  Supports copying actors from multiple levels.
	 * NOTE: Doesn't support copying prefab instance actors!
	 *
	 * @param bShouldCut If TRUE, deletes the selected actors after copying them to the clipboard
	 * @param bShouldClipPadCanBeUsed If TRUE and the SHIFT key is held down, the actors will be copied to the clip pad
	 */
	void CopySelectedActorsToClipboard( UBOOL bShouldCut, UBOOL bClipPadCanBeUsed );

	/**
	 * Computes a color to use for property coloration for the given object.
	 *
	 * @param	Object		The object for which to compute a property color.
	 * @param	OutColor	[out] The returned color.
	 * @return				TRUE if a color was successfully set on OutColor, FALSE otherwise.
	 */
	virtual UBOOL GetPropertyColorationColor(class UObject* Object, FColor& OutColor);

	/**
	 * Sets property value and property chain to be used for property-based coloration.
	 *
	 * @param	PropertyValue		The property value to color.
	 * @param	Property			The property to color.
	 * @param	CommonBaseClass		The class of object to color.
	 * @param	PropertyChain		The chain of properties from member to lowest property.
	 */
	virtual void SetPropertyColorationTarget(const FString& PropertyValue, class UProperty* Property, class UClass* CommonBaseClass, class FEditPropertyChain* PropertyChain);

	/**
	 * Accessor for current property-based coloration settings.
	 *
	 * @param	OutPropertyValue	[out] The property value to color.
	 * @param	OutProperty			[out] The property to color.
	 * @param	OutCommonBaseClass	[out] The class of object to color.
	 * @param	OutPropertyChain	[out] The chain of properties from member to lowest property.
	 */
	virtual void GetPropertyColorationTarget(FString& OutPropertyValue, UProperty*& OutProperty, UClass*& OutCommonBaseClass, FEditPropertyChain*& OutPropertyChain);

	/**
	 * Selects actors that match the property coloration settings.
	 */
	void SelectByPropertyColoration();

	/**
	 *	Sets the texture to use for displaying StreamingBounds.
	 *
	 *	@param	InTexture	The source texture for displaying StreamingBounds.
	 *						Pass in NULL to disable displaying them.
	 */
	void SetStreamingBoundsTexture(UTexture2D* InTexture);

	/** 
	 *	Create a new instance of a prefab in the level. 
	 *
	 *	@param	Prefab		The prefab to create an instance of.
	 *	@param	Location	Location to create the new prefab at.
	 *	@param	Rotation	Rotation to create the new prefab at.
	 *	@return				Pointer to new PrefabInstance actor in the level, or NULL if it fails.
	 */
	class APrefabInstance* Prefab_InstancePrefab(class UPrefab* Prefab, const FVector& Location, const FRotator& Rotation) const;

	/**
	 * Warns the user of any hidden levels, and prompts them with a Yes/No dialog
	 * for whether they wish to continue with the operation.  No dialog is presented if all
	 * levels are visible.  The return value is TRUE if no levels are hidden or
	 * the user selects "Yes", or FALSE if the user selects "No".
	 *
	 * @param	bIncludePersistentLvl	If TRUE, the persistent level will also be checked for visibility
	 * @param	AdditionalMessage		An additional message to include in the dialog.  Can be NULL.
	 * @return							FALSE if the user selects "No", TRUE otherwise.
	 */
	UBOOL WarnAboutHiddenLevels(UBOOL bIncludePersistentLvl, const TCHAR* AdditionalMessage) const;

	void ApplyDeltaToActor(AActor* InActor, UBOOL bDelta, const FVector* InTranslation, const FRotator* InRotation, const FVector* InScaling, UBOOL bAltDown=FALSE, UBOOL bShiftDown=FALSE, UBOOL bControlDown=FALSE) const;

	/** called after script compilation to allow for game specific post-compilation steps */
	virtual void PostScriptCompile() {}

	/** Hook for game stats tool to render things in viewport. */
	virtual void GameStatsRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType) {}
	/** Hook for game stats tool to render things in 3D viewport. */
	virtual void GameStatsRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType) {}
	/** Hook for game stats to be informed about mouse movements (for tool tip) */
	virtual void GameStatsMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y) {}
	/** Hook for game to be informed about key input */
	virtual void GameStatsInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event) {}

	/** Hook for sentinel stats tool to render things in viewport. */
	virtual void SentinelStatRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas) {}
	/** Hook for sentinel stats tool to render things in 3D viewport. */
	virtual void SentinelStatRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI) {}
	/** Hook for sentinel to be informed about mouse movements (for tool tip) */
	virtual void SentinelMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y) {}
	/** Hook for sentinel to be informed about key input */
	virtual void SentinelInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event) {}


	/** This allows the indiv game to return the set of decal materials it uses based off its internal objects (e.g. PhysicalMaterials)**/
	virtual class TArray<UMaterialInterface*> GetDecalMaterialsFromGame( UObject* InObject ) const { TArray<UMaterialInterface*> Retval; return Retval; }

	/**
	 * Create a hierarchical menu of sound classes
	 */
	INT RecursiveAddSoundClass( UAudioDevice* AudioDevice, wxMenu* Parent, class USoundClass* SoundClass, FName ClassName, INT wxID );
	void CreateSoundClassMenu( wxMenu* Parent );

	INT RecursiveAddSoundClassForContentBrowser( UAudioDevice* AudioDevice, TArray<FObjectSupportedCommandType>& OutCommands, INT ParentIndex, USoundClass* SoundClass, FName ClassName, INT wxID );
	
	/** Create SoundClass menu commands for each sound class
	 * 
	 * @param OutCommands	The returned list of commands generated
	 * @param ParentIndex	OptionalParameter to set the entire command structure to a different parent
	 */
	void CreateSoundClassMenuForContentBrowser( TArray<FObjectSupportedCommandType>& OutCommands, INT ParentIndex = INDEX_NONE );

	/** 
	 *	Game-specific function called by Map_Check BEFORE iterating over all actors.
	 *
	 *	@param	Str						The exec command parameters
	 *	@param	Ar						The output archive for logging (?)
	 *	@param	bCheckDeprecatedOnly	If TRUE, only check for deprecated classes
	 */
	virtual UBOOL Game_Map_Check(const TCHAR* Str, FOutputDevice& Ar, UBOOL bCheckDeprecatedOnly) { return TRUE; }

	/** 
	 *	Game-specific function called per-actor by Map_Check 
	 *
	 *	@param	Str						The exec command parameters
	 *	@param	Ar						The output archive for logging (?)
	 *	@param	bCheckDeprecatedOnly	If TRUE, only check for deprecated classes
	 */
	virtual UBOOL Game_Map_Check_Actor(const TCHAR* Str, FOutputDevice& Ar, UBOOL bCheckDeprecatedOnly, AActor* InActor) { return TRUE; }

	/** Map given class name key to value in ClassMapPair array - if no key match just return the class of given name */
	class UClass* GetClassFromPairMap( FString ClassName );

	/**
	 * Auto merge all staticmeshes that are able to be merged
	*/
	void AutoMergeStaticMeshes();

	/**
	 *	Check the InCmdParams for "MAPINISECTION=<name of section>".
	 *	If found, fill in OutMapList with the proper map names.
	 *
	 *	@param	InCmdParams		The cmd line parameters for the application
	 *	@param	OutMapList		The list of maps from the ini section, empty if not found
	 */
	void ParseMapSectionIni(const TCHAR* InCmdParams, TArray<FString>& OutMapList);

	/**
	 *	Load the list of maps from the given section of the Editor.ini file
	 *
	 *	@param	InSectionName		The name of the section to load
	 *	@param	OutMapList			The list of maps from that section
	 */
	void LoadMapListFromIni(const FString& InSectionName, TArray<FString>& OutMapList);

	/**
	 *	Check whether the specified package file is a map
	 */
	UBOOL PackageIsAMapFile( const TCHAR* PackageFilename );

	/**
	 *	Searches through the given ULevel for any external references. Prints any found UObjects to the log and informs the user
	 *
	 *	@param	LevelToCheck		ULevel to search through for external objects
	 *	@param	bAddForMapCheck		Optional flag to add any found references to the Map Check dialog (defaults to FALSE)
	 *	@return	TRUE if the given package has external references and the user does not want to ignore the warning (via prompt)
	 */
	UBOOL PackageUsingExternalObjects(ULevel* LevelToCheck, UBOOL bAddForMapCheck=FALSE);

	//////////////////////
	// Interface_PylonGeometryProvider
	virtual UObject* GetUObjectInterfaceInterface_PylonGeometryProvider(){return this;}

	/**
	 * Exports all path colliding geometry within pylon's bounds
	 * @param Pylon - bounding pylon
	 * @param Verts - list of exported vertices 
	 * @param Faces - list of exported triangles, 3 indices to Verts array for each item
	 */
	virtual void GetPathCollidingGeometry(APylon* Pylon, TArray<FVector>& Verts, TArray<INT>& Faces);

	// Interface_PylonGeometryProvider
	//////////////////////

private:
	//////////////////////
	// Map execs

	UBOOL Map_Rotgrid(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Select(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Brush(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Sendto(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Rebuild(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Load(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Import(const TCHAR* Str, FOutputDevice& Ar, UBOOL bNewMap);
	UBOOL Map_Check(const TCHAR* Str, FOutputDevice& Ar, UBOOL bCheckDeprecatedOnly, UBOOL bClearExistingMessages, UBOOL bDisplayResultDialog=TRUE );
	UBOOL Map_Scale(const TCHAR* Str, FOutputDevice& Ar);
	UBOOL Map_Setbrush(const TCHAR* Str, FOutputDevice& Ar);

	/**
	 * Attempts to load a preview static mesh from the array preview static meshes at the given index.
	 *
	 * @param	Index	The index of the name in the PlayerPreviewMeshNames array from the editor user settings.
	 * @return	TRUE if a static mesh was loaded; FALSE, otherwise. 
	 */
	UBOOL LoadPreviewMesh( INT Index );	

	/**
	 * Moves selected actors to the current level - NB. This is only intended to be called from MoveSelectedActorsToCurrentLevel()
	 *
	 * @param	bUseCurrentLevelGridVolume	If true, moves actors to an appropriately level associated with the current level grid volume (if there is one.)  If there is no current level grid volume, the actors will be moved to the current level.
	 */
	void DoMoveSelectedActorsToCurrentLevel( const UBOOL bUseCurrentLevelGridVolume );
};

/*-----------------------------------------------------------------------------
	Parameter parsing functions.
-----------------------------------------------------------------------------*/

UBOOL GetFVECTOR( const TCHAR* Stream, const TCHAR* Match, FVector& Value );
UBOOL GetFVECTOR( const TCHAR* Stream, FVector& Value );
const TCHAR* GetFVECTORSpaceDelimited( const TCHAR* Stream, FVector& Value );
UBOOL GetFROTATOR( const TCHAR* Stream, const TCHAR* Match, FRotator& Rotation, INT ScaleFactor );
UBOOL GetFROTATOR( const TCHAR* Stream, FRotator& Rotation, int ScaleFactor );
const TCHAR* GetFROTATORSpaceDelimited( const TCHAR* Stream, FRotator& Rotation, INT ScaleFactor );
UBOOL GetBEGIN( const TCHAR** Stream, const TCHAR* Match );
UBOOL GetEND( const TCHAR** Stream, const TCHAR* Match );
UBOOL GetSUBSTRING(const TCHAR*	Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen);
TCHAR* SetFVECTOR( TCHAR* Dest, const FVector* Value );

class FReimportHandler;

/** Reimport manager for package resources with associated source files on disk. */
class FReimportManager
{
public:
	/**
	 * Singleton function, provides access to the only instance of the class
	 *
	 * @return	Singleton instance of the manager
	 */
	static FReimportManager* Instance();

	/**
	 * Register a reimport handler with the manager
	 *
	 * @param	InHandler	Handler to register with the manager
	 */
	void RegisterHandler( FReimportHandler& InHandler );

	/**
	 * Unregister a reimport handler from the manager
	 *
	 * @param	InHandler	Handler to unregister from the manager
	 */
	void UnregisterHandler( FReimportHandler& InHandler );

	/**
	 * Attempt to reimport the specified object from its source by giving registered reimport
	 * handlers a chance to try to reimport the object
	 *
	 * @param	Obj	Object to try reimporting
	 *
	 * @return	TRUE if the object was handled by one of the reimport handlers; FALSE otherwise
	 */
	virtual UBOOL Reimport( UObject* Obj );

private:
	/** Reimport handlers registered with this manager */
	TArray<FReimportHandler*> Handlers;

	/** Constructor */
	FReimportManager();

	/** Destructor */
	~FReimportManager();

	/** Copy constructor; intentionally left unimplemented */
	FReimportManager( const FReimportManager& );

	/** Assignment operator; intentionally left unimplemented */
	FReimportManager& operator=( const FReimportManager& );
};

/** 
* Reimport handler for package resources with associated source files on disk.
*/
class FReimportHandler
{
public:
	/** Constructor. Add self to manager */
	FReimportHandler(){ FReimportManager::Instance()->RegisterHandler( *this ); }
	/** Destructor. Remove self from manager */
	virtual ~FReimportHandler(){ FReimportManager::Instance()->UnregisterHandler( *this ); }
	
	/**
	 * Attempt to reimport the specified object from its source
	 *
	 * @param	Obj	Object to attempt to reimport
	 *
	 * @return	TRUE if this handler was able to handle reimporting the provided object
	 */
	virtual UBOOL Reimport( UObject* Obj ) = 0;
};

/*-----------------------------------------------------------------------------
	Cooking helpers.
-----------------------------------------------------------------------------*/

/** 
 * Info used to setup the rows of the sound quality previewer
 */
class FPreviewInfo
{
public:
	FPreviewInfo( INT Quality );
	~FPreviewInfo( void ) 
	{ 
		Cleanup(); 
	}

	void Cleanup( void );

	INT			QualitySetting;

	INT			OriginalSize;

	INT			OggVorbisSize;
	INT			XMASize;
	INT			PS3Size;

	BYTE*		DecompressedOggVorbis;
	BYTE*		DecompressedXMA;
	BYTE*		DecompressedPS3;
};

/**
 * Cooks SoundNodeWave to a specific platform
 *
 * @param	SoundNodeWave			Wave file to cook
 * @param	SoundCooker				Platform specific cooker object to cook with
 * @param	DestinationData			Destination bulk data
 */
UBOOL CookSoundNodeWave( USoundNodeWave* SoundNodeWave, class FConsoleSoundCooker* SoundCooker, FByteBulkData& DestinationData );

/**
 * Cooks SoundNodeWave to all available platforms
 *
 * @param	SoundNodeWave			Wave file to cook
 * @param	Platform				Platform to cook for, PLATFORM_Unknown for all platforms
 */
UBOOL CookSoundNodeWave( USoundNodeWave* SoundNodeWave, UE3::EPlatformType Platform = UE3::PLATFORM_Unknown );

/**
 * Compresses SoundNodeWave for all available platforms, and then decompresses to PCM 
 *
 * @param	SoundNodeWave			Wave file to compress
 * @param	PreviewInfo				Compressed stats and decompressed data
 */
void SoundNodeWaveQualityPreview( USoundNodeWave* SoundNode, FPreviewInfo * PreviewInfo );

/** Initializes mobile global variables, must be called once before the rendering thread has been started. */
extern void InitializeMobileSettings();

/**
 * Sets which mobile emulation features are enabled.  This is only intended to be used in Unreal Editor.
 * Note that this function can sometimes take awhile to complete as graphics resource may need updating.
 *
 * @param	bNewEmulateMobileRendering					Enables or disables mobile rendering emulation
 * @param	bNewUseGammaCorrectionForMobileEmulation	Enables or disables gamma correction when emulation mobile
 * @param	bReattachComponents							Reattaches components after setting the new emulate mobile rendering setting
 */
extern void SetMobileRenderingEmulation( const UBOOL bNewEmulateMobileRendering, const UBOOL bNewUseGammaCorrectionForMobileEmulation, const UBOOL bReattachComponents = TRUE );

/**
 * Specifies how a material should be flattened.
 */
enum EFlattenType
{
	FLATTEN_BaseTexture,
	FLATTEN_NormalTexture,
	FLATTEN_DiffuseTexture,
	FLATTEN_MAX
};

/**
 * Flatten the given material to a texture.
 *
 * @param MaterialInterface The material to flatten.
 * @param FlattenType How to flatten the material.
 * @param TextureName The name of the flattened texture.
 * @param TextureOuter The outer of the flattened texture.
 * @param TextureObjectFlags Object flags with which to create the flattened texture.
 * @returns The texture to which the material has been flattened.
 */
UTexture2D* FlattenMaterialToTexture(UMaterialInterface* MaterialInterface, EFlattenType FlattenType, const FString& TextureName, UObject* TextureOuter, EObjectFlags TextureObjectFlags);

/**
 * Flatten materials to textures, and cook any uncompressed textures that don't have cached cooked data 
 * if needed for mobile platforms
 *
 * @param Package Package to prepare
 * @param bIsSilent If TRUE, don't show the slow task dialog
 * @param WorldBeingSaved	The world currently being saved.  NULL if there is no world being saved.
 */
void PreparePackageForMobile(UPackage* Package, UBOOL bIsSilent, UWorld* WorldBeingSaved = NULL );

/**
 * Converts textures to PVRTC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @param bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCachePVRTCTextures(UTexture2D* Texture, UBOOL bUseFastCompression = FALSE, UBOOL bForceCompression = FALSE, TArray<FColor>* SourceArtOverride = NULL);

/**
 * Converts textures to ATITC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCacheATITCTextures(UTexture2D* Texture, UBOOL bUseFastCompression = FALSE, UBOOL bForceCompression = FALSE, TArray<FColor>* SourceArtOverride = NULL);

/**
* Converts textures to ETC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
*
* @param Texture Texture to convert
* @bForceCompression Forces compression of the texture
*/
UBOOL ConditionalCacheETCTextures(UTexture2D* Texture, UBOOL bForceCompression = FALSE, TArray<FColor>* SourceArtOverride = NULL);

/**
 * Converts textures to Flash, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCacheFlashTextures(UTexture2D* Texture, UBOOL bUseFastCompression = FALSE, UBOOL bForceCompression = FALSE, TArray<FColor>* SourceArtOverride = NULL);

//
// Things to set in mapSetBrush.
//
enum EMapSetBrushFlags				
{
	MSB_BrushColor	= 1,			// Set brush color.
	MSB_Group		= 2,			// Set group.
	MSB_PolyFlags	= 4,			// Set poly flags.
	MSB_CSGOper		= 8,			// Set CSG operation.
};

// Byte describing effects for a mesh triangle.
enum EJSMeshTriType
{
	// Triangle types. Mutually exclusive.
	MTT_Normal				= 0,	// Normal one-sided.
	MTT_NormalTwoSided      = 1,    // Normal but two-sided.
	MTT_Translucent			= 2,	// Translucent two-sided.
	MTT_Masked				= 3,	// Masked two-sided.
	MTT_Modulate			= 4,	// Modulation blended two-sided.
	MTT_Placeholder			= 8,	// Placeholder triangle for positioning weapon. Invisible.
	// Bit flags. 
	MTT_Unlit				= 16,	// Full brightness, no lighting.
	MTT_Flat				= 32,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Environment			= 64,	// Environment mapped.
	MTT_NoSmooth			= 128,	// No bilinear filtering on this poly's texture.
};

/**
 * Provides access to the FEditorModeTools singleton.
 */
class FEditorModeTools& GEditorModeTools();

extern struct FEditorLevelViewportClient* GCurrentLevelEditingViewportClient;

/** Tracks the last level editing viewport client that received a key press. */
extern struct FEditorLevelViewportClient* GLastKeyLevelEditingViewportClient;

/*-----------------------------------------------------------------------------
	Exporters.
-----------------------------------------------------------------------------*/

class UTextBufferExporterTXT : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UTextBufferExporterTXT,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Out, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

#if 1
class USoundExporterWAV : public UExporter
{
	DECLARE_CLASS_INTRINSIC(USoundExporterWAV,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

#else
class USoundExporterOGG : public UExporter
{
	DECLARE_CLASS_INTRINSIC(USoundExporterOGG,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};
#endif

class USoundSurroundExporterWAV : public UExporter
{
	DECLARE_CLASS_INTRINSIC(USoundSurroundExporterWAV,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();

	/** 
	* Number of binary files to export for this object. Should be 1 in the vast majority of cases. Noted exception would be multichannel sounds
	* which have upto 8 raw waves stored within them.
	*/
	virtual INT GetFileCount( void ) const;

	/** 
	 * Differentiates the filename for objects with multiple files to export. Only needs to be overridden if GetFileCount() returns > 1.
	 */
	virtual FString GetUniqueFilename( const TCHAR* Filename, INT FileIndex );

	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

class UClassExporterUC : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UClassExporterUC,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

class UObjectExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UObjectExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

class UPolysExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UPolysExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

class UModelExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UModelExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

class ULevelExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(ULevelExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
	virtual void ExportPackageObject(FExportPackageParams& ExpPackageParams);
	virtual void ExportPackageInners(FExportPackageParams& ExpPackageParams);
	virtual void ExportComponentExtra( const FExportObjectInnerContext* Context, const TArray<UComponent*>& Components, FOutputDevice& Ar, DWORD PortFlags);
};

class ULevelExporterSTL : public UExporter
{
	DECLARE_CLASS_INTRINSIC(ULevelExporterSTL,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

class UPackageExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UPackageExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
	virtual void ExportPackageObject(FExportPackageParams& ExpPackageParams);
	virtual void ExportPackageInners(FExportPackageParams& ExpPackageParams);
};

class UTextureExporterPCX : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UTextureExporterPCX,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

class UTextureExporterBMP : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UTextureExporterBMP,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
	UBOOL ExportBinary(const BYTE* Data, EPixelFormat Format, INT SizeX, INT SizeY, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, DWORD PortFlags = 0);
};
class UTextureExporterTGA : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UTextureExporterTGA,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};
class UStaticMeshExporterOBJ : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UStaticMeshExporterOBJ,UExporter,0,UnrealEd)
	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};
class UStaticMeshExporterFBX : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UStaticMeshExporterFBX,UExporter,0,UnrealEd)
	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};
class USkeletalMeshExporterFBX : public UExporter
{
	DECLARE_CLASS_INTRINSIC(USkeletalMeshExporterFBX,UExporter,0,UnrealEd)
	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

/** exports a render target's surface to a targa formatted file or archive */
class URenderTargetExporterTGA : public UExporter
{
	DECLARE_CLASS_INTRINSIC(URenderTargetExporterTGA,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

/** exports a single face of the cube render target's surfaces to a targa formatted file or archive */
class URenderTargetCubeExporterTGA : public UExporter
{
	DECLARE_CLASS_INTRINSIC(URenderTargetCubeExporterTGA,UExporter,0,UnrealEd)

	/** cube map face to export - see ECubeTargetFace */
	BYTE	CubeFace;

	void StaticConstructor();
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

class ULevelExporterLOD : public UExporter
{
	DECLARE_CLASS_INTRINSIC(ULevelExporterLOD,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};
class ULevelExporterOBJ : public UExporter
{
	DECLARE_CLASS_INTRINSIC(ULevelExporterOBJ,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};
class ULevelExporterFBX : public UExporter
{
	DECLARE_CLASS_INTRINSIC(ULevelExporterFBX,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};
class UPolysExporterOBJ : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UPolysExporterOBJ,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};
class USequenceExporterT3D : public UExporter
{
	DECLARE_CLASS_INTRINSIC(USequenceExporterT3D,UExporter,0,UnrealEd)

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 );
};

/*-----------------------------------------------------------------------------
	Terrain-related exporters
-----------------------------------------------------------------------------*/
class UTerrainHeightMapExporter;
struct FFilterLimit;
class UTerrainLayerSetup;
struct FTerrainDecoration;
struct FTerrainDecorationInstance;

class UTerrainExporterT3D : public UExporter
{
	static UBOOL			s_bHeightMapExporterArrayFilled;
	static TArray<UClass*>	s_HeightMapExporterArray;
	UObjectExporterT3D*		ObjectExporter;
	UBOOL					bExportingTerrainOnly;

	DECLARE_CLASS_INTRINSIC(UTerrainExporterT3D,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportHeightMapData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportInfoData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportLayerData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportFilterLimit(FFilterLimit& FilterLimit, const TCHAR* Name, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportLayerSetup(UTerrainLayerSetup* Setup, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportDecoLayerData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportDecoration(FTerrainDecoration& Decoration, INT Index, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportDecorationInstance(FTerrainDecorationInstance& DecorationInst, INT Index, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	UBOOL ExportAlphaMapData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);

public:
	const UBOOL	IsExportingTerrainOnly() const								{	return bExportingTerrainOnly;						}
	void		SetIsExportingTerrainOnly(UBOOL bExportingTerrainOnlyIn)	{	bExportingTerrainOnly = bExportingTerrainOnlyIn;	}

private:
	UBOOL FindHeightMapExporters(FFeedbackContext* Warn);
};

struct FTerrainLayer;

class UTerrainHeightMapExporter : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UTerrainHeightMapExporter,UExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	virtual UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
};

class UTerrainHeightMapExporterTextT3D : public UTerrainHeightMapExporter
{
	DECLARE_CLASS_INTRINSIC(UTerrainHeightMapExporterTextT3D,UTerrainHeightMapExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	virtual UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
};

class UTerrainHeightMapExporterG16BMPT3D : public UTerrainHeightMapExporter
{
	DECLARE_CLASS_INTRINSIC(UTerrainHeightMapExporterG16BMPT3D,UTerrainHeightMapExporter,0,UnrealEd)
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	virtual UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
	virtual UBOOL ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags=0);
};

/*-----------------------------------------------------------------------------
	'Extended' T3D exporters...
-----------------------------------------------------------------------------*/
class UExporterT3DX : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UExporterT3DX,UExporter,0,UnrealEd)

	void ExportObjectStructProperty(const FString& PropertyName, const FString& ScriptStructName, BYTE* DataValue, UObject* ParentObject, FOutputDevice& Ar, DWORD PortFlags);
	void ExportBooleanProperty(const FString& PropertyName, UBOOL BooleanValue, FOutputDevice& Ar, DWORD PortFlags);
	void ExportFloatProperty(const FString& PropertyName, FLOAT FloatValue, FOutputDevice& Ar, DWORD PortFlags);
	void ExportIntProperty(const FString& PropertyName, INT IntValue, FOutputDevice& Ar, DWORD PortFlags);
	void ExportStringProperty(const FString& PropertyName, const FString& StringValue, FOutputDevice& Ar, DWORD PortFlags);
	void ExportObjectProperty(const FString& PropertyName, const UObject* InExportingObject, const UObject* InObject, FOutputDevice& Ar, DWORD PortFlags);

	void ExportUntypedBulkData(FUntypedBulkData& BulkData, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags);
	void ExportBinaryBlob(INT BlobSize, BYTE* BlobData, FOutputDevice& Ar, DWORD PortFlags);

	virtual UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 ) { return FALSE; };
};

class UMaterialExporterT3D : public UExporterT3DX
{
	DECLARE_CLASS_INTRINSIC(UMaterialExporterT3D,UExporterT3DX,0,UnrealEd)

	static const int VersionMax = 0;
	static const int VersionMin = 0;

	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
};

class UStaticMeshExporterT3D : public UExporterT3DX
{
	DECLARE_CLASS_INTRINSIC(UStaticMeshExporterT3D,UExporterT3DX,0,UnrealEd)

	static const int VersionMax = 0;
	static const int VersionMin = 0;

	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);

	void ExportStaticMeshElement(const FString& Name, const FStaticMeshElement& SMElement, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags);
	void ExportStaticMeshTriangleBulkData(FStaticMeshTriangleBulkData& SMTBulkData, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags);
	UBOOL ExportRenderData(FStaticMeshRenderData& Model, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);
};

class UTextureExporterT3D : public UExporterT3DX
{
	DECLARE_CLASS_INTRINSIC(UTextureExporterT3D,UExporterT3DX,0,UnrealEd)

	static const int VersionMax = 0;
	static const int VersionMin = 0;

	/**
	* Initializes property values for intrinsic classes.  It is called immediately after the class default object
	* is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0);

	UBOOL ExportText_Texture2D(const FExportObjectInnerContext* Context, UTexture2D* InTexture2D, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags);
	UBOOL ExportText_TextureCube(const FExportObjectInnerContext* Context, UTextureCube* InTextureCube, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags);
};

class UPhysXExporterAsset : public UExporter
{
	DECLARE_CLASS_INTRINSIC(UPhysXExporterAsset,UExporter,0,UnrealEd)

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 );
};

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

// Accepts a triangle (XYZ and UV values for each point) and returns a poly base and UV vectors
// NOTE : the UV coords should be scaled by the texture size
static inline void FTexCoordsToVectors(const FVector& V0, const FVector& UV0,
									   const FVector& V1, const FVector& InUV1,
									   const FVector& V2, const FVector& InUV2,
									   FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.SafeNormal();

	FVector UV1( InUV1 );
	FVector UV2( InUV2 );

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( UV0.X == UV1.X ) || ( UV2.X == UV1.X ) || ( UV2.X == UV0.X ) ||
		( UV0.Y == UV1.Y ) || ( UV2.Y == UV1.Y ) || ( UV2.Y == UV0.Y ) )
	{
		UV1 += FVector(0.004173f,0.004123f,0.0f);
		UV2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them 
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... , 
	// then the third assumes we're perpendicular to the normal. 
	//
	FMatrix TexEqu = FMatrix::Identity;
	TexEqu.SetAxis( 0, FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z ) );
	TexEqu.SetAxis( 1, FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z ) );
	TexEqu.SetAxis( 2, FVector( PN.X,        PN.Y,        PN.Z        ) );
	TexEqu = TexEqu.Inverse();

	const FVector UResult( UV1.X-UV0.X, UV2.X-UV0.X, 0.0f );
	const FVector TUResult = TexEqu.TransformNormal( UResult );

	const FVector VResult( UV1.Y-UV0.Y, UV2.Y-UV0.Y, 0.0f );
	const FVector TVResult = TexEqu.TransformNormal( VResult );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//				
	FMatrix BaseEqu = FMatrix::Identity;
	BaseEqu.SetAxis( 0, TUResult );
	BaseEqu.SetAxis( 1, TVResult ); 
	BaseEqu.SetAxis( 2, FVector( PN.X, PN.Y, PN.Z ) );
	BaseEqu = BaseEqu.Inverse();

	const FVector BResult = FVector( UV0.X - ( TUResult|V0 ), UV0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BaseEqu.TransformNormal( BResult );
	*InUResult = TUResult;
	*InVResult = TVResult;

}

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

class UEditorPlayer : public ULocalPlayer
{
	DECLARE_CLASS_INTRINSIC(UEditorPlayer,ULocalPlayer,0,UnrealEd);

	// FExec interface.
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
};


#endif // __EDITOR_H__
