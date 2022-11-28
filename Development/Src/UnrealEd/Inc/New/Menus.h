/*=============================================================================
	Menus.h: The menus used by the editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxMainMenu.

	Sits at the top of the main editor frame.
-----------------------------------------------------------------------------*/

class WxMainMenu : public wxMenuBar
{
public:
	wxMenu *FileMenu;
		wxMenu *MRUMenu, *ImportMenu, *ExportMenu;
	wxMenu *EditMenu;
	wxMenu *ViewMenu;
		wxMenu *BrowserMenu, *ViewportConfigMenu;
	wxMenu *BrushMenu;
	wxMenu *VolumeMenu;
	wxMenu *BuildMenu;
	wxMenu *ToolsMenu;
	wxMenu *HelpMenu;

	FMRUList* MRUFiles;

	WxMainMenu();
	~WxMainMenu();
};

/*-----------------------------------------------------------------------------
	WxMainContextMenuBase.

	Menus created when right clicking in the main editor viewports.
-----------------------------------------------------------------------------*/

class WxMainContextMenuBase : public wxMenu
{	
public:
	wxMenu* ActorFactoryMenu;
	wxMenu* ActorFactoryMenuAdv;
	wxMenu* RecentClassesMenu;

	WxMainContextMenuBase();
	~WxMainContextMenuBase();

	void AppendAddActorMenu();
};


class WxMainContextMenu : public WxMainContextMenuBase
{
public:
	wxMenu* OrderMenu;
	wxMenu* PolygonsMenu;
	wxMenu* CSGMenu;
	wxMenu* SolidityMenu;
	wxMenu* SelectMenu;
	wxMenu* AlignMenu;
	wxMenu* PivotMenu;
	wxMenu* TransformMenu;
	wxMenu* VolumeMenu;
	wxMenu* PathMenu;

	WxMainContextMenu();
	~WxMainContextMenu();
};


class WxMainContextSurfaceMenu : public WxMainContextMenuBase
{
public:
	wxMenu* SelectSurfMenu;
	wxMenu* ExtrudeMenu;
	wxMenu* AlignmentMenu;

	WxMainContextSurfaceMenu();
	~WxMainContextSurfaceMenu();
};

/*-----------------------------------------------------------------------------
	WxDragGridMenu.

	Menu for controlling options related to the drag grid.
-----------------------------------------------------------------------------*/

class WxDragGridMenu : public wxMenu
{
public:
	WxDragGridMenu();
	~WxDragGridMenu();
};

/*-----------------------------------------------------------------------------
	WxRotationGridMenu.

	Menu for controlling options related to the rotation grid.
-----------------------------------------------------------------------------*/

class WxRotationGridMenu : public wxMenu
{
public:
	WxRotationGridMenu();
	~WxRotationGridMenu();
};

/*-----------------------------------------------------------------------------
	WxMBGroupBrowser
-----------------------------------------------------------------------------*/

class WxMBGroupBrowser : public wxMenuBar
{
public:
	WxMBGroupBrowser();
	~WxMBGroupBrowser();
};

/*-----------------------------------------------------------------------------
	WxMBLayerBrowser
-----------------------------------------------------------------------------*/

class WxMBLayerBrowser : public wxMenuBar
{
public:
	WxMBLayerBrowser();
	~WxMBLayerBrowser();
};

/*-----------------------------------------------------------------------------
	WxMBActorBrowser
-----------------------------------------------------------------------------*/

class WxMBActorBrowser : public wxMenuBar
{
public:
	WxMBActorBrowser();
	~WxMBActorBrowser();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowser
-----------------------------------------------------------------------------*/

class WxMBGenericBrowser : public wxMenuBar
{
public:
	WxMBGenericBrowser();
	~WxMBGenericBrowser();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext : public wxMenu
{
public:
	void AppendObjectMenu();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Object
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_Object : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_Object();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Material.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_Material : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_Material();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Texture.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_Texture : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_Texture();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_StaticMesh.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_StaticMesh : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_StaticMesh();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Sound.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_Sound : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_Sound();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_SoundCue.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_SoundCue : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_SoundCue();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_PhysicsAsset.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_PhysicsAsset : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_PhysicsAsset();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Skeletal.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_Skeletal : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_Skeletal();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_ParticleSystem.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_ParticleSystem : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_ParticleSystem();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_AnimSet.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_AnimSet : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_AnimSet();
};

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_AnimTree.
-----------------------------------------------------------------------------*/

class WxMBGenericBrowserContext_AnimTree : public WxMBGenericBrowserContext
{
public:
	WxMBGenericBrowserContext_AnimTree();
};

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_Base.
-----------------------------------------------------------------------------*/

class WxMBMaterialEditorContext_Base : public wxMenu
{
public:
	WxMBMaterialEditorContext_Base();
	~WxMBMaterialEditorContext_Base();
};

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_TextureSample.
-----------------------------------------------------------------------------*/

class WxMBMaterialEditorContext_TextureSample : public wxMenu
{
public:
	WxMBMaterialEditorContext_TextureSample();
	~WxMBMaterialEditorContext_TextureSample();
};

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_OutputHandle.
-----------------------------------------------------------------------------*/

class WxMBMaterialEditorContext_OutputHandle : public wxMenu
{
public:
	WxMBMaterialEditorContext_OutputHandle();
	~WxMBMaterialEditorContext_OutputHandle();
};

/*-----------------------------------------------------------------------------
	WxStaticMeshEditMenu
-----------------------------------------------------------------------------*/

class WxStaticMeshEditMenu : public wxMenuBar
{
public:
	WxStaticMeshEditMenu();
	~WxStaticMeshEditMenu();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
