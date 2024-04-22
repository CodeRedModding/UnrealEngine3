/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


//=============================================================================
// EditorEngine: The UnrealEd subsystem.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class EditorEngine extends Engine
	native
	config(Engine)
	noexport
	transient
	implements(Interface_PylonGeometryProvider)
	inherits(FCallbackEventDevice);

// Objects.
var const model       TempModel;
var const model       ConversionTempModel;
var const transbuffer Trans;
var const textbuffer  Results;
var const array<pointer>  ActorProperties;
var const pointer     LevelProperties;

// Textures.
var const texture2D Bad, Bkgnd, BkgndHi, BadHighlight, MaterialArrow, MaterialBackdrop;

// Font used by Canvas-based editors
var const Font EditorFont;

// Audio
var const transient SoundCue			PreviewSoundCue;
var const transient AudioComponent		PreviewAudioComponent;

// Used in UnrealEd for showing materials
var staticmesh	TexPropCube;
var staticmesh	TexPropSphere;
var staticmesh	TexPropPlane;
var staticmesh	TexPropCylinder;

// Toggles.
var const bool bFastRebuild, bBootstrapping, bIsImportingT3D;

// Other variables.
var const int TerrainEditBrush, ClickFlags;
var const package ParentContext;
var const vector ClickLocation;
var const plane ClickPlane;
var const vector MouseMovement;
var const native array<pointer> ViewportClients;

/** Distance to far clipping plane for perspective viewports.  If <= GNearClippingPlane, far plane is at infinity. */
var config float FarClippingPlane;

// Setting for the detail mode to show in the editor viewports
var EDetailMode				DetailMode;

// BEGIN FEditorConstraints
var					noexport const	pointer	ConstraintsVtbl;

// Grid.
var(Grid)			noexport config bool	GridEnabled;
var(Grid)			noexport config bool	SnapScaleEnabled;
var(Grid)			noexport config bool	SnapVertices;
var(Grid)			noexport config int		ScaleGridSize;		// Integer percentage amount to snap scaling to.
var(Grid)			noexport config float	SnapDistance;
var(Grid)			noexport config float	GridSizes[11];		// FEditorConstraints::MAX_GRID_SIZES = 11 in native code
var(Grid)			noexport config int		CurrentGridSz;		// Index into GridSizes
// Rotation grid.
var(RotationGrid)	noexport config bool	RotGridEnabled;
var(RotationGrid)	noexport config rotator RotGridSize;
var(RotationGrid)	noexport config int		AngleSnapType;
// END FEditorConstraints


// Advanced.
var(Advanced) config bool UseSizingBox;
var(Advanced) config bool UseAxisIndicator;
var(Advanced) config float FOVAngle;
var(Advanced) config bool GodMode;

/** The location to autosave to. */
var(Advanced) config string AutoSaveDir;

var(Advanced) config bool InvertwidgetZAxis;
var(Advanced) config string GameCommandLine;

/** the list of package names to compile when building scripts */
var(Advanced) globalconfig array<string> EditPackages;

/** the base directory to use for finding .uc files to compile*/
var(Advanced) config string EditPackagesInPath;

/** the directory to save compiled .u files to */
var(Advanced) config string EditPackagesOutPath;

/** the directory to save compiled .u files to when script is compiled with the -FINAL_RELEASE switch */
var(Advanced) config string FRScriptOutputPath;

/** If TRUE, always show the terrain in the overhead 2D view. */
var(Advanced) config bool AlwaysShowTerrain;

/** If TRUE, use the gizmo for rotating actors. */
var(Advanced) config bool UseActorRotationGizmo;

/** If TRUE, show translucent marker polygons on the builder brush and volumes. */
var(Advanced) config bool bShowBrushMarkerPolys;

/** If TRUE, use Maya camera controls. */
var(Advanced) config bool bUseMayaCameraControls;

/** If TRUE, parts of prefabs cannot be individually selected/edited. */
var(Advanced) config bool bPrefabsLocked;

/** If TRUE, socket snapping is enabled in the main level viewports. */
var(Advanced) config bool bEnableSocketSnapping;

	/** If TRUE, socket names are enabled in the main level viewports. */
var(Advanced) config bool bEnableSocketNames;

/** If TRUE, determines if reachspecs should be built for this level's pathnodes (may not be necessary if using navmesh) */
var(Advanced) config bool bBuildReachSpecs;

/** If TRUE, same type views will be camera tied, and ortho views will use perspective view for LOD parenting */
var	bool bEnableLODLocking;

/** If TRUE, actors can be grouped and grouping rules will be maintained. When deactivated, any currently existing groups will still be preserved.*/
var(Advanced) config bool bGroupingActive;

var config string HeightMapExportClassName;

/** array of actor factory classes to ignore for the global list (i.e. because they're not relevant to this game) */
var config array<name> HiddenActorFactoryNames;
/** Array of actor factories created at editor startup and used by context menu etc. */
var const array<ActorFactory> ActorFactories;
/** Actors that are being deleted and should processed in the global re-attach*/
var array <Actor> ActorsForGlobalReattach;

/** String that maps one class name to another, used to create hook for game-specific actors created through shortcuts etc 
 *  Pairing is "ORIGINALCLASS;DESIREDCLASS
 *  (ie APylon;AMyGamePylon)
 */
var config array<String> ClassMapPair;

/** The name of the file currently being opened in the editor. "" if no file is being opened. */
var string	UserOpenedFile;

///////////////////////////////
// "Play From Here" properties

/** Additional per-user/per-game options set in the .ini file. Should be in the form "?option1=X?option2?option3=Y"					*/
var(Advanced) config string InEditorGameURLOptions;
/** A pointer to a UWorld that is the duplicated/saved-loaded to be played in with "Play From Here" 								*/
var const World PlayWorld;
/** An optional location for the starting location for "Play From Here"																*/
var const vector PlayWorldLocation;
/** An optional rotation for the starting location for "Play From Here"																*/
var const rotator PlayWorldRotation;
/** Has a request for "Play From Here" been made?													 								*/
var const bool bIsPlayWorldQueued;
/** Has a request to spectate the map been made?													 								*/
var const bool bStartInSpectatorMode;
/** Did the request include the optional location and rotation?										 								*/
var const bool bHasPlayWorldPlacement;
/** True to enable mobile preview mode when launching the game from the editor on PC platform */
var const bool bUseMobilePreviewForPlayWorld;
/** Where did the person want to play? Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer	*/
var const int	PlayWorldDestination;
/** The current play world destination (I.E console).  -1 means no current play world destination, 0 or more is an index into the GConsoleSupportContainer	*/
var const int	CurrentPlayWorldDestination;
/** Mobile/PC preview settings for what features/resolution to use */
var config int PlayInEditorWidth;
/** Mobile/PC preview settings for what features/resolution to use */
var config int PlayInEditorHeight;

/** Mobile preview settings for what orientation to default to */
var config bool bMobilePreviewPortrait;

/** Currently targeted device for mobile previewer. */
var config int BuildPlayDevice;

/** Folders in which the editor looks for template map files */
var(Templates) config array<string> TemplateMapFolders;

/** When set to anything other than -1, indicates a specific In-Editor viewport index that PIE should use */
var const int PlayInEditorViewportIndex;

/** Play world url string edited by a user. */
var string UserEditedPlayWorldURL;

/** The width resolution that we want to use for the matinee capture */
var config int MatineeCaptureResolutionX;

/** The height resolution that we want to use for the matinee capture */
var config int MatineeCaptureResolutionY;

/** Contains a list of breakpoints that were hit while PlayWorld was active */
var transient array<string> KismetDebuggerBreakpointQueue;
/** If true, will cause a Kismet debugger window to be opened after the editor world has been restored */
var transient bool bIsKismetDebuggerRequested;

// possible object propagators
var const pointer InEditorPropagator;
var const pointer RemotePropagator;

var bool bIsPushingView;
var const transient bool bDecalUpdateRequested;

/** Temporary render target that can be used by the editor. */
var const transient TextureRenderTarget2D ScratchRenderTarget2048;
var const transient TextureRenderTarget2D ScratchRenderTarget1024;
var const transient TextureRenderTarget2D ScratchRenderTarget512;
var const transient TextureRenderTarget2D ScratchRenderTarget256;

/**
 *	Display StreamingBounds for textures
 */
var const transient Texture2D StreamingBoundsTexture;

/** Global instance of the editor user settings */
var const EditorUserSettings UserSettings;

/** Stores the class hierarchy generated from the make commandlet*/
var native const pointer EditorClassHierarchy {FEditorClassHierarchy};

/** The full paths to meshes used to preview a static mesh in editor. */
var array<string> PreviewMeshNames;
/** A mesh component used to preview in editor without spawning a static mesh actor. */
var const transient StaticMeshComponent PreviewMeshComp; 
/** The index of the mesh to use from the list of preview meshes. */
var const int PreviewMeshIndex;
/** When TRUE, the preview mesh mode is activated. */
var const bool bPreviewPlayerHeight;

/** If "Camera Align" emitter handling uses a custom zoom or not */
var	config bool	bCustomCameraAlignEmitter;
/** The distance to place the camera from an emitter actor when custom zooming is enabled */
var config float CustomCameraAlignEmitterDistance;

/** If true, then draw sockets when socket snapping is enabled in 'g' mode */
var config bool bDrawSocketsInGMode;

/** If true, then draw particle debug helpers in editor viewports */
var transient bool bDrawParticleHelpers;

var array<GroupActor> ActiveGroupActors;

/** Actor list for the intermediary buffer level used for moving actors between levels */
var transient array<Actor> BufferLevelActors;

/** Force PIE to start in exact place suppressing kismet. It forces all levels to be streamed in, skips all level begin events and sets all matinees to skipable.*/
var bool bForcePlayFromHere;

/** Keeps track of the last actor that had the camera aligned to it in Exec_Camera() */
var transient Actor LastCameraAlignTarget;

/** If true, then do slow reference checks during map check */
var transient bool bDoReferenceChecks;

/** 
* A mapping of all startup packages to whether or not we have warned the user about editing them
*/
var native transient map{UPackage*, UBOOL} StartupPackageToWarnState;

defaultproperties
{
     Bad=Texture2D'EditorResources.Bad'
     Bkgnd=Texture2D'EditorResources.Bkgnd'
     BkgndHi=Texture2D'EditorResources.BkgndHi'
	 MaterialArrow=Texture2D'EditorResources.MaterialArrow'
	 MaterialBackdrop=Texture2D'EditorResources.MaterialBackdrop'
	 BadHighlight=Texture2D'EditorResources.BadHighlight'
	 TexPropCube=StaticMesh'EditorMeshes.TexPropCube'
	 TexPropSphere=StaticMesh'EditorMeshes.TexPropSphere'
	 TexPropPlane=StaticMesh'EditorMeshes.TexPropPlane'
	 TexPropCylinder=StaticMesh'EditorMeshes.TexPropCylinder'
	 EditorFont=Font'EditorResources.SmallFont'
	 DetailMode=3

	 PlayInEditorViewportIndex= -1;
	 CurrentPlayWorldDestination = -1;
}
