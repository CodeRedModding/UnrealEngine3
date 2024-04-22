/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This class contains a list of thumbnail rendering entries, which can be
 * configured from Editor.ini. The idea is for thumbnail rendering to be
 * extensible without having to modify Epic code.
 */
class ThumbnailManager extends Object
	native
	config(Editor);

/**
 * Types of primitives for drawing thumbnails of resources.
 */
enum EThumbnailPrimType
{
	TPT_None,
	TPT_Sphere,
	TPT_Cube,
	TPT_Plane,
	TPT_Cylinder
};

/**
* Types of backgrounds to use for rendering thumbnails
*/
enum EThumbnailBackgroundType
{
	TBT_None,
	TBT_DefaultBackground,
	TBT_SolidBackground
};

/**
 * Holds the settings for a class that needs a thumbnail renderer. Each entry
 * maps to a corresponding class and holds the information needed
 * to render the thumbnail, including which object to render via and its
 * border color.
 */
struct native ThumbnailRenderingInfo
{
	/**
	 * The name of the class that this thumbnail is for (so we can lazy bind)
	 */
	var String ClassNeedingThumbnailName;
	/**
	 * This is the class that this entry is for, i.e. the class that
	 * will be rendered in the thumbnail views
	 */
	var Class ClassNeedingThumbnail;
	/**
	 * The name of the class to load when rendering this thumbnail
	 * NOTE: This is a string to avoid any dependencies of compilation
	 */
	var String RendererClassName;
	/**
	 * The instance of the renderer class
	 */
	var ThumbnailRenderer Renderer;
	/**
	 * The name of the class to load when rendering labels for this thumbnail
	 * NOTE: This is a string to avoid any dependencies of compilation
	 */
	var String LabelRendererClassName;
	/**
	 * The instance of the label renderer class
	 */
	var ThumbnailLabelRenderer LabelRenderer;
	/**
	 * This is the border color to use for this type
	 */
	var Color BorderColor;
	/**
	 * Icon for objects that don't have specialized drawing needs but still
	 * want to be able to see a thumbnail
	 */
	var String IconName;
};

/**
 * The array of thumbnail rendering information entries. Each type that supports
 * thumbnail rendering has an entry in here.
 */
var const config array<ThumbnailRenderingInfo> RenderableThumbnailTypes;

/**
 * The array of thumbnail rendering information entries which support archetypes. Each type that supports
 * archetype thumbnail rendering must have an entry in the .ini file.
 */
var const config array<ThumbnailRenderingInfo> ArchetypeRenderableThumbnailTypes;

/**
 * Determines whether the initialization function is needed or not
 */
var const bool bIsInitialized;

// The following members are present for performance optimizations

/**
 * Whether to update the map or not (GC usually causes this)
 */
var const bool bMapNeedsUpdate;

/**
 * This holds a map of object type to render info entries
 */
var private{private} native transient const pointer RenderInfoMap;

/**
 * This holds a map of object type to render info entries for archetypes
 */
var private{private} native transient const pointer ArchetypeRenderInfoMap{TMap<UClass *,FThumbnailRenderingInfo *>};

/**
 * The render info to share across all object types when the object doesn't
 * support rendering of thumbnails
 */
var const ThumbnailRenderingInfo NotSupported;

/**
 * Cached background component instead of creating and destroying them for each
 * thumbnail that is rendered
 */
var const StaticMeshComponent BackgroundComponent;

/**
 * Cached static mesh component instead of creating and destroying them for
 * each thumbnail that is rendered
 */
var const StaticMeshComponent SMPreviewComponent;

/**
 * Cached skeletal mesh component instead of creating and destroying them for
 * each thumbnail that is rendered
 */
var const SkeletalMeshComponent SKPreviewComponent;

/**
 *	When TRUE, ParticleSystem thumbnails will render a real-time preview
 */
var bool bPSysRealTime;

// All these meshes/materials/textures are preloaded via default properties

var const StaticMesh TexPropCube;
var const StaticMesh TexPropSphere;
var const StaticMesh TexPropCylinder;
var const StaticMesh TexPropPlane;
var const Material ThumbnailBackground;
var const Material ThumbnailBackgroundSolid;
var const MaterialInstanceConstant ThumbnailBackgroundSolidMatInst;
var const array<MaterialInterface> MeshMaterialArray;

cpptext
{
	typedef TMap<UClass*,FThumbnailRenderingInfo*> FClassToRenderInfoMap;

protected:
	/**
	 * Returns the pointer to the map as a reference. Creates it if one isn't
	 * already instanced.
	 */
	inline FClassToRenderInfoMap& GetRenderInfoMap(void)
	{
		if (RenderInfoMap == NULL)
		{
			RenderInfoMap = (void*)new FClassToRenderInfoMap();
		}
		return *(FClassToRenderInfoMap*)RenderInfoMap;
	}

	inline FClassToRenderInfoMap& GetArchetypeRenderInfoMap(void)
	{
		if ( ArchetypeRenderInfoMap == NULL )
		{
			ArchetypeRenderInfoMap = new FClassToRenderInfoMap();
		}
		return *ArchetypeRenderInfoMap;
	}

public:
	/**
	 * Returns the component to use for rendering a background. Creates one
	 * if needed.
	 */
	inline UStaticMeshComponent* GetBackgroundComponent(void)
	{
		if (BackgroundComponent == NULL)
		{
			BackgroundComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());;
		}
		return BackgroundComponent;
	}

	/**
	 * Returns the component to use for rendering a static mesh. Creates one
	 * if needed.
	 */
	inline UStaticMeshComponent* GetStaticMeshPreviewComponent(void)
	{
		if (SMPreviewComponent == NULL)
		{
			SMPreviewComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());;
		}
		// Reset the static-mesh's CastShadow flag to its default value.
		SMPreviewComponent->CastShadow = TRUE;
		return SMPreviewComponent;
	}

	/**
	 * Returns the component to use for rendering a skeletal mesh. Creates one
	 * if needed.
	 */
	inline USkeletalMeshComponent* GetSkeletalMeshPreviewComponent(void)
	{
		if (SKPreviewComponent == NULL)
		{
			SKPreviewComponent = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());;
		}
		return SKPreviewComponent;
	}

	/**
	 * Sets Material Array for meshes
	 */
	void SetMeshPreviewMaterial (TArray<UMaterialInterface*>& InMaterialArray)
	{
		MeshMaterialArray = InMaterialArray;
	}

	/**
	 * Gets Material Array for meshes
	 */
	TArrayNoInit <UMaterialInterface*>& GetStaticMeshMaterialArray(void)
	{
		return MeshMaterialArray;
	}

	/**
	 * Clears Material Array for meshes.  Should be called directly after 
	 */
	void ClearStaticMeshmaterialArray (void)
	{
		MeshMaterialArray.Empty();
	}

	/**
	 * Clears cached components.
	 */
	void ClearComponents(void);

	/**
	 * Cleans up any allocations that won't be GCed (UObject interface)
	 */
	void FinishDestroy(void);

	/**
	 * Serializes any object renferences and sets the map needs update flag
	 *
	 * @param Ar the archive to serialize to/from
	 */
	void Serialize(FArchive& Ar);

	/**
	 * Fixes up any classes that need to be loaded in the thumbnail types
	 */
	void Initialize(void);

	/**
	 * Returns the entry for the specified object
	 *
	 * @param Object the object to find thumbnail rendering info for
	 *
	 * @return A pointer to the rendering info if valid, otherwise NULL
	 */
	FThumbnailRenderingInfo* GetRenderingInfo(UObject* Object);

protected:
	/**
	 * Fixes up any classes that need to be loaded in the thumbnail types per-map type
	 */
	void InitializeRenderTypeArray( TArray<struct FThumbnailRenderingInfo>& ThumbnailRendererTypes, FClassToRenderInfoMap& ThumbnailMap );
}

defaultproperties
{
	TexPropCube=StaticMesh'EditorMeshes.TexPropCube'
	TexPropSphere=StaticMesh'EditorMeshes.TexPropSphere'
	TexPropCylinder=StaticMesh'EditorMeshes.TexPropCylinder'
	TexPropPlane=StaticMesh'EditorMeshes.TexPropPlane'
	ThumbnailBackground=Material'EditorMaterials.ThumbnailBack'
	ThumbnailBackgroundSolid=Material'EditorMaterials.ThumbnailSolid'
	ThumbnailBackgroundSolidMatInst=MaterialInstanceConstant'EditorMaterials.ThumbnailSolid_MATInst'

	bPSysRealTime=true
}
