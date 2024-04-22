/**
 *	TerrainEditor.h
 *	Terrain editing dialog
 *
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __TERRAINEDITOR_H__
#define __TERRAINEDITOR_H__

#include "UnrealEd.h"
#include "UnTerrain.h"
#include "TrackableWindow.h"

class wxButton;
class wxBitmapButton;
class wxStaticBitmap;
class wxStaticText;
class wxComboBox;
class wxCheckBox;
class wxSpinCtrl;
class wxSlider;

class WxTerrainEditor;
class WxViewportHolder;

/*-----------------------------------------------------------------------------
	FTerrainBrowserBlock
-----------------------------------------------------------------------------*/
/**
 *	FTerrainBrowserBlock
 *
 *	+-----------------+-------------------------------+
 *	+ [V] [L] [W] [S] | [X] [TN0] [TN1] Name      [I] |
 *	+-----------------+-------------------------------+
 *
 *	V	= View icon
 *	L	= Lock icon
 *	W	= Wireframe overlay icon
 *	S	= Solid overlay icon
 *	X	= Open/Close icon
 *	TN0	= Thumbnail0 - Material snapshot or identifying icon
 *	TN1	= Thumbnail1 - Displacement map (if present)
 *	I	= Information icon
 */
class FTerrainBrowserBlock
{
public:
	/**
	 *	Constructor
	 */
	FTerrainBrowserBlock();

	/**
	 *	Destructor
	 */
	~FTerrainBrowserBlock();

	/**
	 *	SetupBlock
	 *	Sets up an info block with the proper information
	 *
	 *	@param	InType				The info type identifier
	 *	@param	InArrayIndex		The index of the item in the corresponding terrain Layer/DecoLayer arrays
	 *	@param	InLayer				Pointer to the FTerrainLayer
	 *	@param	InFilteredMaterial	Pointer to the FTerrainFilteredMaterial
	 *	@param	InDecoLayer			Pointer to the FTerrainDecoLayer
	 *	@param	InDecoration		Pointer to the FTerrainDecoration
	 *	@param	InTerrainMaterial	Pointer to the UTerrainMaterial
	 *	@param	bInViewable			Whether the item can be 'turned off'
	 *	@param	bInView				Whether the item is being rendered
	 *	@param	bInLockable			Whether the item can be locked
	 *	@param	bInLock				Whether the item is locked
	 *	@param	bInOpenable			Whether the item has children
	 *	@param	bInOpen				Whether the item is opened
	 *	@param	bInUseSecondOpen	Whether the item should use the second open icon
	 *	@param	bInOverlayable		Whether the item can render an overlay
	 *	@param	bInDrawWireOverlay	Whether the item is rendering the wireframe overlay
	 *	@param	bInDrawSolidOverlay	Whether the item is rendering the solid overlay
	 *	@param	bInDrawThumbnail	Whether to draw the first thumbnail
	 *	@param	bInDrawThumbnail2	Whether to draw the second thumbnail
	 *	@param	InParentBlock		Pointer to the parent info block
	 */
	void	SetupBlock(ETerrainLayerType InType, INT InArrayIndex, 
				FTerrainLayer* InLayer, 
				FTerrainFilteredMaterial* InFilteredMaterial,
				FTerrainDecoLayer* InDecoLayer,
				FTerrainDecoration*	InDecoration,
				UTerrainMaterial* InTerrainMaterial, 
				UBOOL bInViewable, UBOOL bInView, 
				UBOOL bInLockable, UBOOL bInLock, 
				UBOOL bInOpenable, UBOOL bInOpen, 
				UBOOL bInUseSecondOpen, 
				UBOOL bInOverlayable,
				UBOOL bInDrawWireOverlay,
				UBOOL bInDrawSolidOverlay,
				UBOOL bInDrawThumbnail,
				UBOOL bInDrawThumbnail2,
                FTerrainBrowserBlock* InParentBlock);
	/**
	 *	ClearBlock
	 *	Used to set a block to a (conceptually) empty state.
	 */
	void	ClearBlock()
	{
		Type	= TLT_Empty;
		bDraw	= FALSE;
	}

	/**
	 *	Serialize - this is called by the owning TerrainBrowser to avoid garbage 
	 *	collecting the UMaterialInstanceConstant members.
	 */
	void	Serialize(FArchive& Ar);

	/**
	 *	Getters
	 */
	UBOOL						IsRenamable();
	ETerrainLayerType			GetLayerType()			{	return Type;				}
	INT							GetArrayIndex()			{	return ArrayIndex;			}
	FTerrainLayer*				GetLayer()				{	return Layer;				}
	FTerrainFilteredMaterial*	GetFilteredMaterial()	{	return FilteredMaterial;	}
	FTerrainDecoLayer*			GetDecoLayer()			{	return DecoLayer;			}
	FTerrainDecoration*			GetDecoration()			{	return Decoration;			}
	UBOOL						GetViewable()			{	return bViewable;			}
	UBOOL						GetView()				{	return bView;				}
	UBOOL						GetLockable()			{	return bLockable;			}
	UBOOL						GetLock()				{	return bLock;				}
	UBOOL						GetOpenable()			{	return bOpenable;			}
	UBOOL						GetOpen()				{	return bOpen;				}
	UBOOL						GetUseSecondOpen()		{	return bUseSecondOpen;		}
	UBOOL						GetOverlayable()		{	return bOverlayable;		}
	UBOOL						GetDrawWireOverlay()	{	return bDrawWireOverlay;	}
	UBOOL						GetDrawShadeOverlay()	{	return bDrawShadeOverlay;	}
	UBOOL						GetDrawThumbnail()		{	return bDrawThumbnail;		}
	UBOOL						GetDrawThumbnail2()		{	return bDrawThumbnail2;		}
	FTerrainBrowserBlock*		GetParentBlock()		{	return ParentBlock;			}
	FColor&						GetHighlightColor()		{	return HighlightColor;		}
	FColor&						GetWireframeColor()		{	return WireframeColor;		}
	UMaterialInstanceConstant*	GetHighlightMatInst()	{	return HighlightMatInst;	}
	UMaterialInstanceConstant*	GetWireframeMatInst()	{	return WireframeMatInst;	}
	UBOOL						GetDraw()				{	return bDraw;				}

	/**
	 *	Setters
	 */
	void	SetDraw(UBOOL bInDraw)							{	bDraw = bInDraw;							}
	void	SetView(UBOOL bInView)							{	bView = bInView;							}
	void	SetLock(UBOOL bInLock)							{	bLock = bInLock;							}
	void	SetOpenable(UBOOL bInOpenable)					{	bOpenable = bInOpenable;					}
	void	SetOpen(UBOOL bInOpen)							{	bOpen = bInOpen;							}
	void	SetDrawWireOverlay(UBOOL bInDrawWireOverlay)	{	bDrawWireOverlay = bInDrawWireOverlay;		}
	void	SetDrawShadeOverlay(UBOOL bInDrawShadeOverlay)	{	bDrawShadeOverlay = bInDrawShadeOverlay;	}
	void	SetHighlightColor(FColor& Color);
	void	SetWireframeColor(FColor& Color);
	void	SetDrawThumbnail(UBOOL bInDrawThumbnail)		{	bDrawThumbnail = bInDrawThumbnail;			}
	void	SetDrawThumbnail2(UBOOL bInDrawThumbnail2)		{	bDrawThumbnail2 = bInDrawThumbnail2;		}

	/**
	 *	CreateOverlayMaterialInstances
	 *	Called by the terrain browser to ensure that the info has the material instances 
	 *	properly setup, if it requires them.
	 */
	UBOOL	CreateOverlayMaterialInstances(UMaterial* WireframeMat, UMaterial* SolidMat);

protected:
	ETerrainLayerType	Type;
	INT					ArrayIndex;
	union
	{
		FTerrainLayer*				Layer;
		FTerrainFilteredMaterial*	FilteredMaterial;
		FTerrainDecoLayer*			DecoLayer;
		FTerrainDecoration*			Decoration;
		UTerrainMaterial*			TerrainMaterial;
	};

	UBOOL						bViewable;
	UBOOL						bView;
	UBOOL						bLockable;
	UBOOL						bLock;
	UBOOL						bOpenable;
	UBOOL						bOpen;
	UBOOL						bUseSecondOpen;
	UBOOL						bOverlayable;
	UBOOL						bDrawWireOverlay;
	UBOOL						bDrawShadeOverlay;
	UBOOL						bDrawThumbnail;
	UBOOL						bDrawThumbnail2;
	FTerrainBrowserBlock*		ParentBlock;

	UBOOL						bDraw;

	FColor						HighlightColor;
	FColor						WireframeColor;
	UMaterialInstanceConstant*	HighlightMatInst;
	UMaterialInstanceConstant*	WireframeMatInst;
};

/*-----------------------------------------------------------------------------
	WxMBTerrainEditorRightClick
-----------------------------------------------------------------------------*/
class WxMBTerrainEditorRightClick : public wxMenu
{
public:
	enum Options
	{
		/**	Rename - put the 'Rename' option in the menu					*/
		OPT_RENAME					= 0x00000001,
		/**	Color  - put the 'Change Color' option in the menu				*/
		OPT_COLOR					= 0x00000002,
		/**	Create - put the 'Create' option in the menu					*/
		OPT_CREATE					= 0x00000004,
		/**	Delete - put the 'Delete' option in the menu					*/
		OPT_DELETE					= 0x00000008,
		/**	UseSelected - put the 'Use Selected' option in the menu			*/
		OPT_USESELECTED				= 0x00000010,
		/**	SavePreset - put the 'Save Preset' option in the menu			*/
		OPT_SAVEPRESET				= 0x00000020,
	};

	enum NewOptions
	{
		/** NewLayer		- put the 'New Layer' option in the menu		*/
		OPT_NEWLAYER				= 0x00000001,
		/** NewMaterial		- put the 'New Material' option in the menu		*/
		OPT_NEWMATERIAL				= 0x00000002,
		/** NewDecoLayer	- put the 'New DecoLayer' option in the menu	*/
		OPT_NEWDECOLAYER			= 0x00000008,
		/** NewDecoration	- put the 'New Decoration' option in the menu	*/
		OPT_NEWDECORATION			= 0x00000010,
		/** NewLayer		- put the 'New Layer' option in the menu		*/
		OPT_NEWLAYER_BEFORE			= 0x00000020,
		/** NewMaterial		- put the 'New Material' option in the menu		*/
		OPT_NEWMATERIAL_BEFORE		= 0x00000040,
		/** NewDecoLayer	- put the 'New DecoLayer' option in the menu	*/
		OPT_NEWDECOLAYER_BEFORE		= 0x00000100,
		/** NewDecoration	- put the 'New Decoration' option in the menu	*/
		OPT_NEWDECORATION_BEFORE	= 0x00000200,
		/** NewLayer		- put the 'New Layer' option in the menu		*/
		OPT_NEWLAYER_AFTER			= 0x00000400,
		/** NewLayer		- put the 'New Layer from material' option in the menu		*/
		OPT_NEWLAYER_AUTO			= 0x00000800,
		/** NewLayer		- put the 'New Layer from material (select package)' option in the menu		*/
		OPT_NEWLAYER_AUTO_SELECT	= 0x00001000,
		/** NewMaterial		- put the 'New Material' option in the menu		*/
		OPT_NEWMATERIAL_AFTER		= 0x00010000,
		/** NewDecoLayer	- put the 'New DecoLayer' option in the menu	*/
		OPT_NEWDECOLAYER_AFTER		= 0x00040000,
		/** NewDecoration	- put the 'New Decoration' option in the menu	*/
		OPT_NEWDECORATION_AFTER		= 0x00080000,
	};

	enum AddOptions
	{
		/** AddSelectedLayer												*/
		OPT_ADDSELECTED_LAYER_BEFORE		= 0x00000001,
		OPT_ADDSELECTED_LAYER				= 0x00000002,
		OPT_ADDSELECTED_LAYER_AFTER			= 0x00000004,
		/** AddSelectedMaterial												*/
		OPT_ADDSELECTED_MATERIAL_BEFORE		= 0x00000008,
		OPT_ADDSELECTED_MATERIAL			= 0x00000010,
		OPT_ADDSELECTED_MATERIAL_AFTER		= 0x00000020,
		OPT_ADDSELECTED_MATERIAL_AUTO		= 0x00000040,
		/** AddSelectedDisplacement											*/
		OPT_ADDSELECTED_DISPLACEMENT_BEFORE	= 0x00000100,
		OPT_ADDSELECTED_DISPLACEMENT		= 0x00000200,
		OPT_ADDSELECTED_DISPLACEMENT_AFTER	= 0x00000400,
		/** AddSelectedDecoration											*/
		OPT_ADDSELECTED_DECORATION_BEFORE	= 0x00010000,
		OPT_ADDSELECTED_DECORATION			= 0x00020000,
		OPT_ADDSELECTED_DECORATION_AFTER	= 0x00040000,
	};

	/**
	 *	Constructor
	 *
	 *	@param	Browser			Pointer to the browser window (its parent)
	 *	@param	InOptions		What menu elements to display
	 */
	WxMBTerrainEditorRightClick(WxTerrainEditor* Browser, INT InOptions = OPT_RENAME,
		INT InNewOptions = 0, INT InAddOptions = 0);

	/**
	 *	Destructor
	 */
	~WxMBTerrainEditorRightClick();

protected:
	void	FillMenu(INT Options, INT InNewOptions, INT InAddOptions);
	INT		FillMenu_Common(INT Options);
	INT		FillMenu_New(INT InNewOptions, UBOOL bPreviousEntries);
	INT		FillMenu_Add(INT InAddOptions, UBOOL bPreviousEntries);
};

/*-----------------------------------------------------------------------------
	FTerrainLayerViewportClient
-----------------------------------------------------------------------------*/
class FTerrainLayerViewportClient : public FEditorLevelViewportClient
{
public:
	enum ViewIcons
	{
		TLVCI_Stub		= -1,
		TLVCI_ViewHide	= 0,
		TLVCI_LockUnlock,
		TLVCI_Wireframe,
		TLVCI_Solid,
		TLVCI_OpenClose,
		TLVCI_OpenClose2,
		TLVCI_Thumbnail,
		TLVCI_Thumbnail2,
		TLVCI_Name,
		TLVCI_LayerType,
		TLVCI_ProceduralOverlay,
		TLVCI_MAX_COUNT
	};

	/** 
	 *	Constructor
	 *
	 *	@param	InTerrainBrowser		The parent terrain browser window
	 */
	FTerrainLayerViewportClient(WxTerrainEditor* InTerrainEditor);

	/** 
	 *	Destructor
	 */
	~FTerrainLayerViewportClient();

	// FEditorLevelViewportClient interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas);

	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);

	//
	/** 
	 *	SetCanvas
	 *	Set the position of the canvas being draw
	 *
	 *	@param	X		The desired X-position
	 *	@param	Y		The desired Y-position
	 */
	virtual void SetCanvas(INT X, INT Y);

	/** 
	 *	Getters
	 */
	UTexture2D*	GetViewIcon() 				{	return ViewIcon;				}
	UTexture2D*	GetHideIcon()				{	return HideIcon;				}
	UTexture2D*	GetLockIcon()				{	return LockIcon;				}
	UTexture2D*	GetUnlockIcon()				{	return UnlockIcon;				}
	UTexture2D*	GetWireframeIcon()			{	return WireframeIcon;			}
	UTexture2D*	GetWireframeOffIcon()		{	return WireframeOffIcon;		}
	UTexture2D*	GetSolidIcon()				{	return SolidIcon;				}
	UTexture2D*	GetSolidOffIcon()			{	return SolidOffIcon;			}
	UTexture2D*	GetOpenedIcon()				{	return OpenedIcon;				}
	UTexture2D*	GetClosedIcon()				{	return ClosedIcon;				}
	UTexture2D*	GetLayerIcon()				{	return LayerIcon;				}
	UTexture2D*	GetMaterialIcon()			{	return MaterialIcon;			}
	UTexture2D*	GetDecoLayerIcon()			{	return DecoLayerIcon;			}
	UTexture2D*	GetDecorationIcon()			{	return DecorationIcon;			}
	UTexture2D*	GetHeightMapIcon()			{	return HeightMapIcon;			}
	UTexture2D*	GetProceduralOverlayIcon()	{	return ProceduralOverlayIcon;	}

protected:
	UBOOL				CreateIcons();
	INT					GetIconOffset(ViewIcons eIcon, INT ViewX);
	FTexture*			GetIcon(ViewIcons eIcon, INT InfoIndex, FLinearColor& Color);
	void				DrawLayerArray(FViewport* Viewport, FCanvas* Canvas);
	void				DrawLayer(FViewport* Viewport, FCanvas* Canvas, 
							FTerrainBrowserBlock* Info, INT InfoIndex, INT& YIndex);
	void				DrawTLBIcon(ViewIcons eIcon, INT InfoIndex, FTerrainBrowserBlock* Info, 
							INT YCount, FViewport* Viewport, FCanvas* Canvas, 
							UBOOL bHitTesting = FALSE, INT XOffset = 0);
	void				DrawThumbnail(ViewIcons eIcon, FTerrainBrowserBlock* Info, INT InfoIndex, INT YIndex, 
							FViewport* Viewport, FCanvas* Canvas, 
							UBOOL bHitTesting = FALSE, INT XOffset = 0);
	void				DrawName(FTerrainBrowserBlock* Info, INT InfoIndex, INT YIndex, 
							FViewport* Viewport, FCanvas* Canvas, INT XOffset = 0);
	void				DrawConnectors(FViewport* Viewport, FCanvas* Canvas);

	void				ToggleViewHideInfoBlock(INT InfoIndex);
	void				ToggleLockUnlockInfoBlock(INT InfoIndex);
	void				ToggleOpenCloseInfoBlock(INT InfoIndex, INT& CanvasOffset, INT ForceOpenClose = 0);
	void				ToggleWireframeInfoBlock(INT InfoIndex);
	void				ToggleSolidInfoBlock(INT InfoIndex);
	void				ProcessLayerTypeHit(INT InfoIndex);

	/** Members */
	WxTerrainEditor*		TerrainEditor;

	FIntPoint				MouseHoldOffset;	// Top-left corner of dragged module relative to mouse cursor.
	FIntPoint				MousePressPosition; // Location of cursor when mouse was pressed.
	UBOOL					bMouseDragging;
	UBOOL					bMouseDown;
	UBOOL					bPanning;
	FIntPoint				Origin2D;
	INT						OldMouseX, OldMouseY;
	UTexture2D*				ViewIcon;
	UTexture2D*				HideIcon;
	UTexture2D*				LockIcon;
	UTexture2D*				UnlockIcon;
	UTexture2D*				WireframeIcon;
	UTexture2D*				WireframeOffIcon;
	UTexture2D*				SolidIcon;
	UTexture2D*				SolidOffIcon;
	UTexture2D*				OpenedIcon;
	UTexture2D*				ClosedIcon;
	UTexture2D*				LayerIcon;
	UTexture2D*				MaterialIcon;
	UTexture2D*				DecoLayerIcon;
	UTexture2D*				DecorationIcon;
	UTexture2D*				HeightMapIcon;
	UTexture2D*				ProceduralOverlayIcon;
};

/*-----------------------------------------------------------------------------
	HTEBHitProxy
-----------------------------------------------------------------------------*/
struct HTEBHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HTEBHitProxy, HHitProxy);

	FTerrainLayerViewportClient::ViewIcons	eIcon;
	INT										LayerInfoIndex;

	HTEBHitProxy(FTerrainLayerViewportClient::ViewIcons eInIcon, INT InLayerInfoIndex) :
		HHitProxy(HPP_UI), 
		eIcon(eInIcon), 
		LayerInfoIndex(InLayerInfoIndex)
	{}
};

/*-----------------------------------------------------------------------------
	WxTerrainLayerPanel
-----------------------------------------------------------------------------*/
class WxTerrainLayerPanel : public wxPanel, FSerializableObject
{
};


/*-----------------------------------------------------------------------------
	WxTerrainEditor
-----------------------------------------------------------------------------*/
/**
 *	WxTerrainEditor
 */
class WxTerrainEditor : 
//	public wxWindow,
	public WxTrackableDialog,
	public FCallbackEventDevice, 	// Interface for event handling
	public FSerializableObject		// Interface for serializing objects
{
public:
	enum EToolButton
	{
		ETETB_AddRemoveSectors	= 0,
		ETETB_AddRemovePolys,
		ETETB_Paint,
		ETETB_PaintVertex,
		ETETB_ManualEdit,
		ETETB_Flatten,
		ETETB_FlattenSpecific,
		ETETB_Smooth,
		ETETB_Average,
		ETETB_Noise,
		ETETB_Visibility,
		ETETB_TexturePan,
		ETETB_TextureRotate,
		ETETB_TextureScale,
		ETETB_VertexLock,
		ETETB_MarkUnreachable,
		ETETB_SplitX,
		ETETB_SplitY,
		ETETB_Merge,
		ETETB_OrientationFlip,
		ETETB_MAX
	};
	
	enum EBrushButton
	{
		ETEBB_Custom	= -1,
		ETEBB_Brush0	= 0,
		ETEBB_Brush1,
		ETEBB_Brush2,
		ETEBB_Brush3,
		ETEBB_Brush4,
		ETEBB_Brush5,
		ETEBB_Brush6,
		ETEBB_Brush7,
		ETEBB_Brush8,
		ETEBB_Brush9,
		ETEBB_MAX
	};

	enum EToolSettings
	{
		ETETS_Strength	= 0,
		ETETS_Radius,
		ETETS_Falloff,
		ETETS_MAX
	};

protected:
	/** Helper structures										*/
	struct FToolInfo : public WxButtonCollection
	{
		FModeTool*			Tool;
	};

	struct FBrushInfo : public WxButtonCollection
	{
		INT		Strength;
		INT		Radius;
		INT		Falloff;
	};

	struct FSliderTextCollection
	{
		wxStaticText*		Label;
		wxSlider*			Slider;
		WxTextCtrl*			Text;
	};

	/** Members													*/
	FEdModeTerrainEditing*					EdMode;
	FTerrainLayerViewportClient*			LayerViewport;
	ATerrain*								CurrentTerrain;
	TArray<ATerrain*>						TerrainArray;
	TArray<FTerrainBrowserBlock>			InfoBlocks;
	INT										SelectedBlock;
	INT										SelectedChildBlock;
	UTerrainEditOptions*					EditorOptions;
	INT										CurrentRightClickIndex;
	FTerrainLayerViewportClient::ViewIcons	eCurrentRightClick;

	FString							StrExportDirectory;
	INT								TerrainBrowserPos_Vert;
	UBOOL							bShowDecoarationMeshes;
	// There are three possible 'depths' for layers...
	FColor							BackgroundColor[3];
	FColor							SelectedColor[3];
	FColor							BorderColor;

	//@todo. Move this into the Terrain class.
	UBOOL							bTerrainIsLocked;
	UBOOL							bTerrainIsInView;

	/** Undo/Redo Support										*/
	UBOOL							bTransactionInProgress;
	FString							TransactionName;

	/** Tool Controls											*/
	EToolButton				CurrentTool;
	FToolInfo				ToolButtons[ETETB_MAX];
	wxSpinCtrl*				SpecificValue;

	/** Import/Export											*/
	wxButton*				ImportButton;
	wxButton*				ExportButton;
	wxCheckBox*				HeightMapOnly;
	wxCheckBox*				RetainCurrentTerrain;
	wxCheckBox*				BakeDisplacementMap;
	wxStaticText*			HeightmapClassLabel;
	wxComboBox*				HeightmapClass;

	/** ViewSettings Controls									*/
	wxStaticText*			ViewSettingsTerrainComboLabel;
	wxComboBox*				ViewSettingsTerrainCombo;
	wxStaticText*			ViewSettingsTerrainPropertiesLabel;
	wxBitmapButton*			ViewSettingsTerrainProperties;
	wxStaticText*			ViewSettingsHideLabel;
	WxButtonCollection		ViewSettingsView;
	wxStaticText*			ViewSettingsLockLabel;
	WxButtonCollection		ViewSettingsLock;
	wxStaticText*			ViewSettingsWireLabel;
	WxButtonCollection		ViewSettingsWire;
	wxStaticText*			ViewSettingsWireframeColorLabel;
	WxButtonCollection		ViewSettingsWireframeColor;
	wxBitmapButton*			ViewSettingsRecacheMaterials;

	/** Brushes													*/
	EBrushButton			CurrentBrush;
	EBrushButton			CurrentPresetBrush;
	FBrushInfo				BrushButtons[ETEBB_MAX];

	/** Settings Controls										*/
	wxCheckBox*				SettingsPerTool;
	wxCheckBox*				SoftSelectionCheck;
	wxCheckBox*				ConstrainedTool;
	wxStaticText*			ToolScaleLabel;
	wxComboBox*				ToolScaleCombo;
	FSliderTextCollection	Settings[ETETS_MAX];
	wxStaticText*			ToolMirrorLabel;
	wxComboBox*				ToolMirrorCombo;
	wxCheckBox*				FlattenAngle;
	wxStaticText*			FlattenHeightLabel;
	WxTextCtrl*				FlattenHeightEdit;

	/** Tessellation Controls									*/
	wxStaticText*			TessellationIncreaseLabel;
	wxBitmapButton*			TessellationIncrease;
	wxStaticText*			TessellationDecreaseLabel;
	wxBitmapButton*			TessellationDecrease;

	/** Browser Panel											*/
	//@todo. Make this a derived browser, not a panel.
	WxViewportHolder*		TerrainBrowserPanel;
	wxScrollBar*			TerrainBrowserScrollBar;

	/** Bottom row Controls										*/
	wxBitmapButton*			UseSelectedButton;
	wxBitmapButton*			NewLayerButton;
	wxBitmapButton*			NewMaterialButton;
	wxBitmapButton*			NewDecoLayerButton;
	wxBitmapButton*			DeleteSelectedButton;
	wxBitmapButton*			ShiftLayerUpButton;
	wxBitmapButton*			ShiftLayerDownButton;
	wxCheckBox*				RetainAlpha;

public:
	/**
	 *	Constructor
	 */
	WxTerrainEditor(wxWindow* InParent);

	/**
	 *	Destructor
	 */
	~WxTerrainEditor();

	/**
	 * Called when there is a Callback issued.
	 * @param InType	The type of callback that was issued.
	 * @param InObject	Object that was modified.
	 */
	virtual void Send( ECallbackEventType InType, UObject* InObject );

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	 *	Show
	 *
	 *	@param	show	TRUE to show window, FALSE to hide
	 *
	 *	@return	TRUE	if operation was successful
	 */
	virtual bool	Show(bool show = true);

	/** Hides the terrain editor and changes the editor mode to camera mode. */
	void HideAndChangeMode();

	/**
	 *	Fills in the combo boxes with the terrain and layer/decolayer information
	 */
	void			RefillCombos();

	/**
	 *	UpdateScrollBar
	 *	Update the size of the thumb and number of pages in the scroll bar
	 *	according to the number of info entries being displayed
	 */
	void	UpdateScrollBar();

	/**
	 *	Return the selected 'major' block (height map, layer, or decolayer)
	 */
	INT		GetSelectedBlock()							{	return SelectedBlock;		}

	/**
	 *	Return the selected 'child' block (terrain material, decoration)
	 */
	INT		GetSelectedChildBlock()						{	return SelectedChildBlock;	}

	/**
	 *	Set the selected block.
	 *
	 *	@param	InSelectedBlock		The block index to set as the selected one
	 */
	void	SetSelectedBlock(INT InSelectedBlock);
	/**
	 *	Set the selected child block.
	 *
	 *	@param	InSelectedBlock		The block index to set as the selected one
	 */
	void	SetSelectedChildBlock(INT InSelectedBlock);
	
	/**
	 *	Set the block index and icon type that was right-clicked.
	 *
	 *	@param	InfoIndex	The index of the block that was right-clicked
	 *	@param	eIcon		The type of icon that was right-clicked
	 */
	void	SetCurrentClick(INT InfoIndex, FTerrainLayerViewportClient::ViewIcons eIcon)
	{
		CurrentRightClickIndex	= InfoIndex;
		eCurrentRightClick		= eIcon;
	}

	/**
	 *	Return the currently selected information
	 *
	 *	@param	Terrain		Pointer reference to be filled in with the selected terrain
	 *	@param	eType		Reference to be filled in with the selected type (heightmap, layer, etc.)
	 *	@param	Index		Reference to be filled in with the index of the selected entry in the InfoBlocks array
	 *
	 *	@return	TRUE		if successful
	 */
	UBOOL	GetSelected(ATerrain*& Terrain, ETerrainLayerType& eType, INT& Index);

	/**
	 *	Sets the currently selected information
	 *
	 *	@param	Terrain		Pointer to the selected terrain
	 *	@param	eType		The selected type (heightmap, layer, etc.)
	 *	@param	Index		The index of the selected entry in the InfoBlocks array
	 *
	 *	@return	TRUE		if successful
	 */
	UBOOL	SetSelected(ATerrain* Terrain, ETerrainLayerType eType, INT Index);

	/**
	 *	Deletes the currently selected block.
	 */
	UBOOL	DeleteSelected();

	/**
	 *	Undo the last action
	 */
	void	Undo();

	/**
	 *	Redo the last undo
	 */
	void	Redo();

	/**
	 *	FCallbackEventDevice function
	 */
	virtual void Send(ECallbackEventType InType)
	{
		// Do not bother doing anything if the window is not shown.
		if (IsShownOnScreen() == TRUE)
		{
			Update();
			RefillCombos();

			if(CurrentTerrain)
			{
				ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColour(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B));
			}
		}
		else
		{
			CurrentTerrain = NULL;
			InfoBlocks.Empty();
		}
	}

	// FSerializableObject interface
	void Serialize(FArchive& Ar);

	DECLARE_EVENT_TABLE()

protected:
	/** Event handlers											*/

	/** Tool handlers											*/
	/**
	 *	Called when a tool button is pressed
	 *
	 *	@param	In	wxCommandEvent identifying the pressed button
	 */
	void		OnToolButton(wxCommandEvent& In);
	/**
	 *	Called when the SpecificValue spinner is activated
	 *
	 *	@param	In	wxSpinEvent identifying the activated button
	 */
	void		OnSpecificValue(wxSpinEvent& In);
	/**
	 *	Called when the SpecificValue text is edited
	 *
	 *	@param	In	wxCommandEvent identifying the event
	 */
	void		OnSpecificValueText(wxCommandEvent& In);

	/** Import/Export handlers									*/
	/**
	 *	Called when either the import or export button is pressed
	 *
	 *	@param	In	wxCommandEvent identifying the pressed button
	 */
	void		OnImpExpButton(wxCommandEvent& In);
	/**
	 *	Called when the HeightMapOnly check box is pressed
	 *
	 *	@param	In	wxCommandEvent identifying the pressed button
	 */
	void		OnImpExpHeightMapOnly(wxCommandEvent& In);
	/**
	 *	Called when the RetainCurrentTerrain check box is pressed
	 *
	 *	@param	In	wxCommandEvent identifying the pressed button
	 */
	void		OnImpExpRetainCurrentTerrain(wxCommandEvent& In);
	/**
	 *	Called when the BakeDisplacementMap check box is pressed
	 *
	 *	@param	In	wxCommandEvent identifying the pressed button
	 */
	void		OnImpExpBakeDisplacementMap(wxCommandEvent& In);
	/**
	 *	Called when the HeightMap Class combo is changed
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnImpExpHeightMapClassCombo(wxCommandEvent& In);

	/** ViewSettings handlers									*/
	/**
	 *	Called when the Terrain combo is changed
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnTerrainCombo(wxCommandEvent& In);
	/**
	 *	Called when the TerrainProperties button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainProperties(wxCommandEvent& In);
	/**
	 *	Called when the RecacheMaterials button is pressed
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnTerrainRecacheMaterials(wxCommandEvent& In);
	/**
	 *	Called when the TerrainHide button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainHide(wxCommandEvent& In);
	/**
	 *	Called when the TerrainLock button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainLock(wxCommandEvent& In);
	/**
	 *	Called when the TerrainWire button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainWire(wxCommandEvent& In);
	/**
	 *	Called when the TerrainSolid button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainWireframeColor(wxCommandEvent& In);
	/**
	 *	Called when the TerrainWire or TerrainSolid button is right-clicked
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTerrainRightClick(wxCommandEvent& In);
	void		OnTerrainRightDown(wxMouseEvent& In);
	void		OnTerrainRightUp(wxMouseEvent& In);

	/** Brush handlers											*/
	/**
	 *	Called when a TerrainBrush button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnBrushButton(wxCommandEvent& In);

	/**
	 *	Called when a TerrainBrush button is right-clicked
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnBrushButtonRightClick(wxCommandEvent& In);

	/** Settings handlers										*/
	/**
	 *	Called when the Settings PerTool check box is hit
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnSettingsPerTool(wxCommandEvent& In);
	/**
	 *	Called when the Flatten Angle check box is hit
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnFlattenAngle(wxCommandEvent& In);
	/**
	 *	Called when the Flatten Height edit box is changed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnFlattenHeight(wxCommandEvent& In);
	/**
	 *	Called when the Settings Scale combo is changed
	 *
	 *	@param	In	wxCommandEvent 
	 */
    void		OnSettingsScaleCombo(wxCommandEvent& In);
	/**
	 *	Called when the Settings Scale combo text is changed
	 *
	 *	@param	In	wxCommandEvent 
	 */
    void		OnSettingsScaleComboText(wxCommandEvent& In);
	/**
	 *	Called when the Settings SoftSelect check box is hit
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnSettingsSoftSelect(wxCommandEvent& In);
	/**
	 *	Called when the Settings Constrained check box is hit
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnSettingsConstrained(wxCommandEvent& In);
	/**
	 *	Called when a Settings Slider is changed
	 *
	 *	@param	In	wxScrollEvent 
	 */
	void		OnSettingsSlider(wxScrollEvent& In);
	/**
	 *	Called when a Settings combo text is edited
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnSettingsText(wxCommandEvent& In);
	/**
	 *	Called when the Settings Mirror combo is changed
	 *
	 *	@param	In	wxCommandEvent 
	 */
    void		OnSettingsMirrorCombo(wxCommandEvent& In);
	
	/** Tessellation handlers									*/
	/**
	 *	Called when a Tessellation button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnTessellationButton(wxCommandEvent& In);

	/** Browser handlers										*/
	/**
	 *	Called when the scrollbar has scrolling stop
	 *
	 *	@param	In	wxScrollEvent
	 */
    void		OnBrowserScroll(wxScrollEvent& In);

	/** Bottom button handlers									*/
	/**
	 *	Called when the UseSelected button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnUseSelectedButton(wxCommandEvent& In);

	/**
	 *	Called when the SavePreset option is selected
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnSavePreset(wxCommandEvent& In);

	/**
	 *	Called when the AddLayer button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnNewLayerButton(wxCommandEvent& In);
	/**
	 *	Called when the AddMaterial button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnNewMaterialButton(wxCommandEvent& In);
	/**
	 *	Called when the AddDecoLayer button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnNewDecoLayerButton(wxCommandEvent& In);
	/**
	 *	Called when the DeleteSelected button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnDeleteSelectedButton(wxCommandEvent& In);
	/**
	 *	Called when the ShiftLayerUp button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnShiftLayerUpButton(wxCommandEvent& In);
	/**
	 *	Called when the ShiftLayerDown button is pressed
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnShiftLayerDownButton(wxCommandEvent& In);
	/**
	 *	Called when the RetainAlpha check box is hit
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnRetainAlpha(wxCommandEvent& In);

	/**
	 *	Called when the right-click menu 'Rename' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnRenameItem(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Change Color' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnChangeColor(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Create New' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnCreate(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Delete' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnDelete(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Use Selected' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnUseSelected(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'New Layer' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnNewLayer(wxCommandEvent& In);

	/** Creates a new terrain layer. */
	void		CreateNewLayer(INT IndexOffset = -1);

	/** Creates a new terrain layer material */
	void		CreateNewMaterial(INT IndexOffset = -1);

	/**
	 *	Called when the auto-create layer option is selected
	 *
	 *	@param	In	wxCommandEvent 
	 */
	void		OnNewLayerAuto(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'New Material' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnNewMaterial(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'New DecoLayer' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnNewDecoLayer(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'New Decoration' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnNewDecoration(wxCommandEvent& In);

	/**
	 *	Called when the right-click menu 'Add Current Layer' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnAddSelectedLayer(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Add Current Material' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnAddSelectedMaterial(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Add Current Material (auto-create)' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnAddSelectedMaterialAuto(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Add Current Displacement' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnAddSelectedDisplacement(wxCommandEvent& In);
	/**
	 *	Called when the right-click menu 'Add Current Decoration' option is selected
	 *
	 *	@param	In	wxCommandEvent
	 */
	void		OnAddSelectedDecoration(wxCommandEvent& In);

	/** Hides the terrain editor and changes the editor mode to camera mode. */
	void OnClose( wxCloseEvent& In );

	/** Updates the terrain wireframe color dialog box */
	void OnActivate(wxActivateEvent& In);

	/** Helper functions										*/
	/**
	 *	Creates all the required controls
	 *
	 *	@return	TRUE	if successful
	 *			FALSE	if not
	 */
	bool		CreateControls();
	bool		GenerateControls();
	bool		CreateLayerViewport();
	bool		PlaceControls();
	void		FillTerrainCombo();
	void		SetInfoArraySize(INT Count);
	void		FillInfoArray();
	void		FillImportExportData();
	void		FillMirrorCombo();
	void		SetBrush(EBrushButton Brush);
	FModeTool*	GetTool(EToolButton Tool);
	void		SetStrength(INT Strength);
	void		SetRadius(INT Radius);
	void		SetFalloff(INT Falloff);
	void		LoadData();
	void		SaveData();
	void		ImportTerrain();
	void		ExportTerrain();
	void		PatchTessellationIncrease();
	void		PatchTessellationDecrease();

	/**
	 *	Use the item currently selected in the ContentBrowser.
	 *
	 *	@param	TypeRestriction		The type of object to restrict usage to
	 *
	 *	@return	TRUE				If successful
	 *			FALSE				If not
	 */
	UBOOL		UseSelected(ETerrainLayerType TypeRestriction = TLT_Empty);

	/**
	 *	Checks the validity of adding the given type
	 *
	 *	@param	Type	Type of item to add
	 *
	 *	@return	TRUE	if valid for add
	 *			FALSE	if not
	 */
	UBOOL	CheckAddValidity(ETerrainLayerType Type);
	/**
	 *	Refreshes the terrain
	 *
	 *	@param	bCollectGarbage		Indicates whether garbage collection should be performed.
	 */
	void	RefreshTerrain(UBOOL bCollectGarbage = FALSE);

	/**
	 *	BeginTransaction
	 *
	 *	@param	Transaction		String naming the transaction
	 *
	 *	@return	TRUE			If succeeds
	 *			FALSE			If fails - transaction was in progress
	 */
	UBOOL		BeginTransaction(const TCHAR* Transaction);
	/**
	 *	EndTransaction
	 *
	 *	@param	Transaction		String naming the transaction
	 *
	 *	@return	TRUE			If succeeds
	 *			FALSE			If fails - transaction was not in progress, or transaction name mismatched
	 */
	UBOOL		EndTransaction(const TCHAR* Transaction);
	UBOOL		IsTransactionInProgress()		{	return bTransactionInProgress;	}
	/**
	 *	TransactionModify
	 *	Used to house the code that will 'touch' the terrain at the start of a transaction
	 */
	void		TransactionModify();

	/**
	 *	Generic interface for creating most terrain items
	 *
	 *	@param	eType			Enumeration of item type
	 *	@param	CreatedItem		Out - the item that was created
	 *	@param	InPackage		If non-NULL, create the object in this package.
	 *	@param	bPrompt			If TRUE, prompt the user for the package/group/name
	 *
	 *	@return	TRUE			if successful
	 *			FALSE			if failed
	 */
	UBOOL CreateNewItem(ETerrainLayerType eType, FString& ObjectName, void*& CreatedItem, UPackage* InPackage = NULL, UBOOL bPrompt = TRUE);

	/**
	 *	Generic interface for getting the given type of item from the selected item set.
	 *	NOTE: This will return the first found instance...
	 *
	 *	@param	eType			Enumeration of item type
	 *	@param	CreatedItem		Out - the item that was selected
	 *
	 *	@return	0				No item found
	 *			1				Found item
	 *			> 1				Number of items found (first one in CreateItem)
	 */
	INT GetSelectedItem(ETerrainLayerType eType, void*& SelectedItem);

	/**
	 *	Create a new layer
	 *
	 *	@param	ObjectName		The name of the layer to create
	 *	@param	InPackage		If non-NULL, create the object in this package.
	 *	@param	bPrompt			If TRUE, prompt the user for the package/group/name
	 *
	 *	@return	Pointer to the created layer
	 */
	UTerrainLayerSetup* CreateNewLayer(FString& ObjectName, UPackage* InPackage = NULL, UBOOL bPrompt = TRUE);

	/**
	 *	Create a new terrain material
	 *
	 *	@param	ObjectName		The name of the terrain material
	 *	@param	InPackage		If non-NULL, create the object in this package.
	 *	@param	bPrompt			If TRUE, prompt the user for the package/group/name
	 *
	 *	@return	Pointer to the created terrain material
	 */
	UTerrainMaterial* CreateNewTerrainMaterial(FString& ObjectName, UPackage* InPackage = NULL, UBOOL bPrompt = TRUE);

	/**
	 *	Shift the selected item up or down in the list
	 *
	 *	@param	bUp		If TRUE, shift the item up in the list, else shift down
	 */
	void ShiftSelected(UBOOL bUp);

public:
	// Accessors
	ATerrain*						GetCurrentTerrain()				{	return CurrentTerrain;			}
	FEdModeTerrainEditing*			GetEdMode()						{	return EdMode;					}
	WxViewportHolder*				GetViewportHolder()				{	return TerrainBrowserPanel;		}
	TArray<FTerrainBrowserBlock>*	GetInfoBlocks()					{	return &InfoBlocks;				}
	UTerrainEditOptions*			GetEditorOptions()				{	return EditorOptions;			}
	UBOOL							GetShowDecoarationMeshes()		{	return bShowDecoarationMeshes;	}
	FColor&							GetBorderColor()				{	return BorderColor;				}
	FLinearColor					GetBackgroundColor(INT Index)
	{
		if (Index >= 3)
			Index = 0;
		return BackgroundColor[Index];
	}
	FColor&							GetSelectedColor(INT Index)
	{
		if (Index >= 3)
			Index = 0;
		return SelectedColor[Index];
	}
	
	void							OffsetBrowserPosition(INT Offset)
	{
		TerrainBrowserPos_Vert += Offset;
	}

};

#endif // __TERRAINEDITOR_H__
