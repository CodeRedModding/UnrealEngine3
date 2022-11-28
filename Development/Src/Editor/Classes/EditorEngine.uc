//=============================================================================
// EditorEngine: The UnrealEd subsystem.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class EditorEngine extends Engine
	native
	config(Engine)
	noexport
	transient;

// Objects.
var const level       Level;
var const level       PlayLevel;
var const model       TempModel;
var const transbuffer Trans;
var const textbuffer  Results;
var const array<pointer>  ActorProperties;
var const pointer     LevelProperties;
var const pointer     UseDest;
var const int         AutosaveCounter;

// Textures.
var const texture2D Bad, Bkgnd, BkgndHi, BadHighlight, MaterialArrow, MaterialBackdrop;

// Used in UnrealEd for showing materials
var staticmesh	TexPropCube;
var staticmesh	TexPropSphere;
var staticmesh	TexPropPlane;
var staticmesh	TexPropCylinder;

// Toggles.
var const bool bFastRebuild, bBootstrapping;

// Other variables.
var const config int AutoSaveIndex;
var const int AutoSaveCount, TerrainEditBrush, ClickFlags;
var const float MovementSpeed;
var const package ParentContext;
var const vector ClickLocation;
var const plane ClickPlane;
var const vector MouseMovement;
var const native array<int> ViewportClients;

// Misc.
var const array<Object> Tools;
var const class BrowseClass;

// BEGIN FEditorConstraints
var					noexport const	pointer	ConstraintsVtbl;

// Grid.
var(Grid)			noexport config bool	GridEnabled;
var(Grid)			noexport config bool	SnapVertices;
var(Grid)			noexport config float	SnapDistance;
var(Grid)			noexport config float	GridSizes[11];		// FEditorConstraints::MAX_GRID_SIZES = 11 in native code
var(Grid)			noexport config int		CurrentGridSz;		// Index into GridSizes
// Rotation grid.
var(RotationGrid)	noexport config bool	RotGridEnabled;
var(RotationGrid)	noexport config rotator RotGridSize;
// END FEditorConstraints


// Advanced.
var(Advanced) config bool UseSizingBox;
var(Advanced) config bool UseAxisIndicator;
var(Advanced) config float FOVAngle;
var(Advanced) config bool GodMode;
var(Advanced) config bool AutoSave;
var(Advanced) config byte AutosaveTimeMinutes;
var(Advanced) config string AutoSaveDir;
var(Advanced) config bool InvertwidgetZAxis;
var(Advanced) config string GameCommandLine;
var(Advanced) config array<string> EditPackages;
var(Advanced) config string EditPackagesInPath;
var(Advanced) config string EditPackagesOutPath;
var(Advanced) config bool AlwaysShowTerrain;
var(Advanced) config bool UseActorRotationGizmo;
var(Advanced) config bool bShowBrushMarkerPolys;

var const array<ActorFactory> ActorFactories;

defaultproperties
{
     Bad=Texture2D'EditorResources.Bad'
     Bkgnd=Texture2D'EditorResources.Bkgnd'
     BkgndHi=Texture2D'EditorResources.BkgndHi'
	 MaterialArrow=Texture2D'EditorResources.MaterialArrow'
	 MaterialBackdrop=Texture2D'EditorResources.MaterialBackdrop'
	 BadHighlight=Texture2D'EditorResources.BadHighlight'
	 CurrentGridSz=4
	 TexPropCube=StaticMesh'EditorMeshes.TexPropCube'
	 TexPropSphere=StaticMesh'EditorMeshes.TexPropSphere'
	 TexPropPlane=StaticMesh'EditorMeshes.TexPropPlane'
	 TexPropCylinder=StaticMesh'EditorMeshes.TexPropCylinder'
}