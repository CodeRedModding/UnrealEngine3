/**
 *	TerrainEditor.cpp
 *	Terrain editing dialog
 *
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "Factories.h"
#include "UnTerrain.h"
#include "EngineMaterialClasses.h"
#include "FileHelpers.h"
#include "PropertyWindow.h"
#include "TerrainEditor.h"

#include "wx\spinctrl.h"

IMPLEMENT_CLASS(UTerrainEditOptions);

//@todo. Put these in a configuration file
const INT TerrainEditorBrowser_BlockHeight	= 48;
const INT TerrainEditorBrowser_IconSize		= 32;
const INT TerrainEditorBrowser_IconSpacing	= 8;

struct FTBVIconInfo
{
	INT		PositionOffset;
	INT		Size;
};

// 16x16 icons...
FTBVIconInfo TLBIconInfo[FTerrainLayerViewportClient::TLVCI_MAX_COUNT]	= 
{
	{   4,	16	},		//TLVCI_ViewHide
	{  28,	16	},		//TLVCI_LockUnlock
	{  52,	16	},		//TLVCI_Wireframe
	{  76,	16	},		//TLVCI_Solid
	{ 108,	16	},		//TLVCI_OpenClose
	{ 132,	16	},		//TLVCI_OpenClose2
	{ 172,	42	},		//TLVCI_Thumbnail,
	{ 224,	42	},		//TLVCI_Thumbnail2,
	{ 272,	32	},		//TLVCI_Name,
	{ -40,	32	},		//TLVCI_LayerType
};
INT TerrainEditorBrowser_DividerAOffset	= 100;//TerrainEditorBrowser_SolidOffset + TerrainEditorBrowser_IconSize + TerrainEditorBrowser_IconSpacing;
INT	 TerrainEditorBrowser_ChildOffset	= 48;

/*-----------------------------------------------------------------------------
	FTerrainBrowserBlock
-----------------------------------------------------------------------------*/
/**
 *	Constructor
 */
FTerrainBrowserBlock::FTerrainBrowserBlock() :
	  Type(TLT_Empty)
	, Layer(NULL)
	, bView(TRUE)
	, bLockable(TRUE)
	, bLock(FALSE)
	, bOpenable(TRUE)
	, bOpen(FALSE)
	, bUseSecondOpen(FALSE)
	, bOverlayable(FALSE)
	, bDrawWireOverlay(FALSE)
	, bDrawShadeOverlay(FALSE)
	, ParentBlock(NULL)
	, bDraw(FALSE)
	, HighlightColor(1.0f, 0.0f, 0.0f, 1.0f)
	, WireframeColor(1.0f, 0.0f, 0.0f, 1.0f)
	, HighlightMatInst(NULL)
	, WireframeMatInst(NULL)
{
}

/**
 *	Destructor
 */
FTerrainBrowserBlock::~FTerrainBrowserBlock()
{
	if (HighlightMatInst)
	{
		HighlightMatInst->ClearFlags(RF_Standalone);
		HighlightMatInst->SetFlags(RF_Transient);
	}

	if (WireframeMatInst)
	{
		WireframeMatInst->ClearFlags(RF_Standalone);
		WireframeMatInst->SetFlags(RF_Transient);
	}
}

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
void FTerrainBrowserBlock::SetupBlock(ETerrainLayerType InType, INT InArrayIndex, 
	FTerrainLayer* InLayer, FTerrainFilteredMaterial* InFilteredMaterial,
    FTerrainDecoLayer* InDecoLayer,
	FTerrainDecoration*	InDecoration, UTerrainMaterial* InTerrainMaterial, 
	UBOOL bInViewable, UBOOL bInView, UBOOL bInLockable, UBOOL bInLock, 
	UBOOL bInOpenable, UBOOL bInOpen, UBOOL bInUseSecondOpen, 
	UBOOL bInOverlayable, UBOOL bInDrawWireOverlay,
	UBOOL bInDrawSolidOverlay, UBOOL bInDrawThumbnail, UBOOL bInDrawThumbnail2,
	FTerrainBrowserBlock* InParentBlock)
{
	Type		= InType;
	ArrayIndex	= InArrayIndex;
	Layer		= NULL;

	switch (Type)
	{
	case TLT_Empty:																break;
	case TLT_HeightMap:															break;
	case TLT_Layer:					Layer				= InLayer;				break;
	case TLT_DecoLayer:				DecoLayer			= InDecoLayer;			break;
	case TLT_TerrainMaterial:		TerrainMaterial		= InTerrainMaterial;	break;
	case TLT_FilteredMaterial:		FilteredMaterial	= InFilteredMaterial;	break;
	case TLT_Decoration:			Decoration			= InDecoration;			break;
	}

	bViewable			= bInViewable;
	bView				= bInView;
	bLockable			= bInLockable;
	bLock				= bInLock;
	bOpenable			= bInOpenable;
	bOpen				= bInOpen;
	bUseSecondOpen		= bInUseSecondOpen;
	bOverlayable		= bInOverlayable;
	bDrawWireOverlay	= bInDrawWireOverlay;
	bDrawShadeOverlay	= bInDrawSolidOverlay;
	bDrawThumbnail		= bInDrawThumbnail;
	bDrawThumbnail2		= bInDrawThumbnail2;
	ParentBlock			= InParentBlock;
}

/**
 *	Serialize - this is called by the owning TerrainBrowser to avoid garbage 
 *	collecting the UMaterialInstanceConstant members.
 */
void FTerrainBrowserBlock::Serialize(FArchive& Ar)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		if (HighlightMatInst)
		{
			Ar << HighlightMatInst;
		}

		if (WireframeMatInst)
		{
			Ar << WireframeMatInst;
		}
	}
}

/**
 *	Getters
 */
UBOOL FTerrainBrowserBlock::IsRenamable()
{
	if ((Type != TLT_HeightMap) && (ParentBlock == NULL))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 *	Setters
 */
void FTerrainBrowserBlock::SetHighlightColor(FColor& Color)
{
	HighlightColor = Color;
	if (HighlightMatInst)
	{
		HighlightMatInst->SetVectorParameterValue(FName(TEXT("Color")), HighlightColor);
	}
}

void FTerrainBrowserBlock::SetWireframeColor(FColor& Color)
{
	WireframeColor = Color;
	if (WireframeMatInst)
	{
		WireframeMatInst->SetVectorParameterValue(FName(TEXT("Color")), WireframeColor);
	}
}

/**
 *	CreateOverlayMaterialInstances
 *	Called by the terrain browser to ensure that the info has the material instances 
 *	properly setup, if it requires them.
 */
UBOOL FTerrainBrowserBlock::CreateOverlayMaterialInstances(UMaterial* WireframeMat, UMaterial* SolidMat)
{
	if (!bOverlayable)
	{
		return TRUE;
	}

	if (HighlightMatInst == NULL)
	{
		HighlightMatInst	= ConstructObject<UMaterialInstanceConstant>(
			UMaterialInstanceConstant::StaticClass(),
			INVALID_OBJECT, 
			NAME_None, 
			RF_Standalone, 
			NULL );
		HighlightMatInst->SetParent(SolidMat);
	}
	else
	if (HighlightMatInst->IsValid() == FALSE)
	{
		debugf(TEXT("HighlightMatInst invalid!"));
	}

	HighlightMatInst->SetVectorParameterValue(FName(TEXT("Color")), HighlightColor);

	if (WireframeMatInst == NULL)
	{
		WireframeMatInst	= ConstructObject<UMaterialInstanceConstant>(
			UMaterialInstanceConstant::StaticClass(),
			INVALID_OBJECT, 
			NAME_None, 
			RF_Standalone, 
			NULL );
		WireframeMatInst->SetParent(WireframeMat);
	}
	else
	if (WireframeMatInst->IsValid() == FALSE)
	{
		debugf(TEXT("WireframeMatInstinvalid!"));
	}

	WireframeMatInst->SetVectorParameterValue(FName(TEXT("Color")), WireframeColor);

	return TRUE;
}

/*-----------------------------------------------------------------------------
	WxMBTerrainEditorRightClick
-----------------------------------------------------------------------------*/
/**
 *	Constructor
 *
 *	@param	Browser			Pointer to the browser window (its parent)
 *	@param	InOptions		What menu elements to display
 */
WxMBTerrainEditorRightClick::WxMBTerrainEditorRightClick(WxTerrainEditor* Browser, 
	INT InOptions, INT InNewOptions, INT InAddOptions)
{
	FillMenu(InOptions, InNewOptions, InAddOptions);
}

/**
 *	Destructor
 */
WxMBTerrainEditorRightClick::~WxMBTerrainEditorRightClick()
{
}

void WxMBTerrainEditorRightClick::FillMenu(INT Options, INT InNewOptions, INT InAddOptions)
{
	INT	Count;
	UBOOL bPrevEntries;
	
	Count = FillMenu_Common(Options);
	bPrevEntries = (Count > 0) ? TRUE : FALSE;
	Count = FillMenu_New(InNewOptions, bPrevEntries);
	bPrevEntries = (Count > 0) ? TRUE : FALSE;
	Count = FillMenu_Add(InAddOptions, bPrevEntries);
}

INT WxMBTerrainEditorRightClick::FillMenu_Common(INT Options)
{
	FString	MenuString;
	INT		EntryCount = 0;

	if (Options & OPT_RENAME)
	{
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainBrowser_Rename"));
		Append(IDM_TERRAINBROWSER_RENAME, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (EntryCount == 1)
	{
		AppendSeparator();
	}

	if (Options & OPT_COLOR)
	{
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainBrowser_ChangeColor"));
		Append(IDM_TERRAINBROWSER_COLOR, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (EntryCount == 2)
	{
		AppendSeparator();
	}

	if (Options & OPT_CREATE)
	{
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainBrowser_CreateNew"));
		Append(IDM_TERRAINBROWSER_CREATE, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (Options & OPT_DELETE)
	{
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainBrowser_Delete"));
		Append(IDM_TERRAINBROWSER_DELETE, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (Options & OPT_USESELECTED)
	{
		AppendSeparator();
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainProp_Tooltip_UseSelected"));
		Append(IDM_TERRAINBROWSER_USESELECTED, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (Options & OPT_SAVEPRESET)
	{
		if (EntryCount)
		{
			AppendSeparator();
		}
		MenuString	= FString::Printf(*LocalizeUnrealEd("TerrainProp_Tooltip_SavePreset"));
		Append(IDM_TERRAINBROWSER_SAVEPRESET, *MenuString, TEXT(""));
		EntryCount++;
	}

	return EntryCount;
}

INT WxMBTerrainEditorRightClick::FillMenu_New(INT InNewOptions, UBOOL bPreviousEntries)
{
	if (InNewOptions == 0)
	{
		return 0;
	}

	FString	MenuString;
	INT		EntryCount = 0;

	if (bPreviousEntries == TRUE)
	{
		AppendSeparator();
	}

	if (InNewOptions & OPT_NEWLAYER_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_NEWLAYER_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWLAYER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"), TEXT("")));
		Append(IDM_TERRAINBROWSER_NEWLAYER, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWLAYER_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_NEWLAYER_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWLAYER_AUTO)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAutomaticallyFromMat")));
		Append(IDM_TERRAINBROWSER_NEWLAYER_AUTO, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWLAYER_AUTO_SELECT)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAutomaticallyFromMatSelectPackage")));
		Append(IDM_TERRAINBROWSER_NEWLAYER_AUTO_SELECT, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (InNewOptions & OPT_NEWMATERIAL_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewMaterial"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_NEWMATERIAL_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWMATERIAL)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewMaterial"), TEXT("")));
		Append(IDM_TERRAINBROWSER_NEWMATERIAL, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWMATERIAL_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewMaterial"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_NEWMATERIAL_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (InNewOptions & OPT_NEWDECOLAYER_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewDecoLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_NEWDECOLAYER_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWDECOLAYER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewDecoLayer"), TEXT("")));
		Append(IDM_TERRAINBROWSER_NEWDECOLAYER, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InNewOptions & OPT_NEWDECOLAYER_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewDecoLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_NEWDECOLAYER_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}

	return EntryCount;
}

INT WxMBTerrainEditorRightClick::FillMenu_Add(INT InAddOptions, UBOOL bPreviousEntries)
{
	if (InAddOptions == 0)
	{
		return 0;
	}

	FString	MenuString;
	INT		EntryCount = 0;

	if (bPreviousEntries == TRUE)
	{
		AppendSeparator();
	}

	if (InAddOptions & OPT_ADDSELECTED_LAYER_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_LAYER_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_LAYER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedLayer"), TEXT("")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_LAYER, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_LAYER_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedLayer"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_LAYER_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (InAddOptions & OPT_ADDSELECTED_MATERIAL_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedMaterial"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_MATERIAL)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedMaterial"), TEXT("")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_MATERIAL_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedMaterial"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_MATERIAL_AUTO)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedMaterial"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAutomatically")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_AUTO, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (InAddOptions & OPT_ADDSELECTED_DISPLACEMENT)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedDisplacement"), TEXT("")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_DISPLACEMENT, *MenuString, TEXT(""));
		EntryCount++;
	}

	if (InAddOptions & OPT_ADDSELECTED_DECORATION_BEFORE)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedDecoration"),
			*LocalizeUnrealEd("TerrainBrowser_CreateBefore")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION_BEFORE, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_DECORATION)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedDecoration"), TEXT("")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION, *MenuString, TEXT(""));
		EntryCount++;
	}
	if (InAddOptions & OPT_ADDSELECTED_DECORATION_AFTER)
	{
		MenuString	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_AddSelectedDecoration"),
			*LocalizeUnrealEd("TerrainBrowser_CreateAfter")));
		Append(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION_AFTER, *MenuString, TEXT(""));
		EntryCount++;
	}

	return EntryCount;
}

/*-----------------------------------------------------------------------------
	FTerrainLayerViewportClient
-----------------------------------------------------------------------------*/
/** 
 *	Constructor
 *
 *	@param	InTerrainBrowser		The parent terrain browser window
 */
FTerrainLayerViewportClient::FTerrainLayerViewportClient(WxTerrainEditor* InTerrainEditor)
{
	TerrainEditor		= InTerrainEditor;
	MouseHoldOffset		= FIntPoint(0,0);
	MousePressPosition	= FIntPoint(0,0);
	bMouseDragging		= FALSE;
	bMouseDown			= FALSE;
	bPanning			= FALSE;
	Origin2D			= FIntPoint(0,0);
	OldMouseX			= 0;
	OldMouseY			= 0;

	CreateIcons();
}

/** 
 *	Destructor
 */
FTerrainLayerViewportClient::~FTerrainLayerViewportClient()
{
}

// FEditorLevelViewportClient interface
void FTerrainLayerViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (TerrainEditor == NULL)
	{
		return;
	}

    // Clear the background to gray and set the 2D draw origin for the viewport
	Clear(Canvas,TerrainEditor->GetBackgroundColor(0));
	Canvas->PushRelativeTransform(FTranslationMatrix(FVector(Origin2D.X,Origin2D.Y,0)));
	DrawLayerArray(Viewport, Canvas);
	if (!Canvas->IsHitTesting())
	{
		DrawConnectors(Viewport, Canvas);
	}
	Canvas->PopTransform();
	// Dividing line
	INT ViewY = Viewport->GetSizeY();
	DrawTile(Canvas,TerrainEditorBrowser_DividerAOffset, 0, 1, ViewY, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
}

UBOOL FTerrainLayerViewportClient::InputKey(FViewport* Viewport, INT ControllerId,
	FName Key, EInputEvent Event, FLOAT AmountDepressed,UBOOL Gamepad)
{
	Viewport->LockMouseToWindow(Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_MiddleMouseButton));

	UBOOL bCtrlDown		= Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown	= Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown		= Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	INT HitX			= Viewport->GetMouseX();
	INT HitY			= Viewport->GetMouseY();
	FIntPoint MousePos	= FIntPoint(HitX, HitY);

	if ((Key == KEY_LeftMouseButton) || (Key == KEY_RightMouseButton))
	{
		if (Event == IE_Pressed)
		{
			// Ignore pressing other mouse buttons while panning around.
			if (bPanning)
			{
				return TRUE;
			}

			if (Key == KEY_LeftMouseButton)
			{
				MousePressPosition = MousePos;
				bMouseDown = TRUE;
			}

			wxMenu*		Menu		= NULL;
			HHitProxy*	HitResult	= Viewport->GetHitProxy(HitX,HitY);
			if (HitResult)
			{
				if (HitResult->IsA(HTEBHitProxy::StaticGetType()))
				{
					HTEBHitProxy*	TLBProxy	= (HTEBHitProxy*)HitResult;

					TArray<FTerrainBrowserBlock>* InfoBlocks = TerrainEditor->GetInfoBlocks();
					FTerrainBrowserBlock* Info	= &((*InfoBlocks)(TLBProxy->LayerInfoIndex));

					if (Key == KEY_LeftMouseButton)
					{
						switch (TLBProxy->eIcon)
						{
						case TLVCI_Stub:
							{
								check(Info);
								if (Info->GetParentBlock())
								{
									TerrainEditor->SetSelectedChildBlock(TLBProxy->LayerInfoIndex);
								}
								else
								{
									TerrainEditor->SetSelectedBlock(TLBProxy->LayerInfoIndex);
									TerrainEditor->SetSelectedChildBlock(-1);
								}
							}
							break;
						case TLVCI_ViewHide:
							ToggleViewHideInfoBlock(TLBProxy->LayerInfoIndex);
							break;
						case TLVCI_LockUnlock:
							ToggleLockUnlockInfoBlock(TLBProxy->LayerInfoIndex);
							break;
						case TLVCI_Wireframe:
							ToggleWireframeInfoBlock(TLBProxy->LayerInfoIndex);
							break;
						case TLVCI_Solid:
							ToggleSolidInfoBlock(TLBProxy->LayerInfoIndex);
							break;
						case TLVCI_OpenClose:
							{
								INT	CanvasOffset	= 0;
								ToggleOpenCloseInfoBlock(TLBProxy->LayerInfoIndex, CanvasOffset);
								if (CanvasOffset > 0)
								{
									if ((Origin2D.Y + CanvasOffset) > 0)
									{
										Origin2D.Y = 0;
									}
									else
									{
										Origin2D.Y += CanvasOffset;
									}
									SetCanvas(Origin2D.X, Origin2D.Y);
								}
								TerrainEditor->UpdateScrollBar();
							}
							break;
						case TLVCI_OpenClose2:
							{
								INT	CanvasOffset	= 0;
								ToggleOpenCloseInfoBlock(TLBProxy->LayerInfoIndex, CanvasOffset);
								if (CanvasOffset > 0)
								{
									if ((Origin2D.Y + CanvasOffset) > 0)
									{
										Origin2D.Y = 0;
									}
									else
									{
										Origin2D.Y += CanvasOffset;
									}
									SetCanvas(Origin2D.X, Origin2D.Y);
								}
								TerrainEditor->UpdateScrollBar();
							}
							break;
						case TLVCI_Thumbnail:
							break;
						case TLVCI_Thumbnail2:
							break;
						case TLVCI_LayerType:
							ProcessLayerTypeHit(TLBProxy->LayerInfoIndex);
							break;
						}
					}
					else
					if (Key == KEY_RightMouseButton)
					{
						TerrainEditor->SetCurrentClick(TLBProxy->LayerInfoIndex, TLBProxy->eIcon);
						switch (TLBProxy->eIcon)
						{
						case TLVCI_Stub:
							{
								check(Info);
								if (Info->GetParentBlock())
								{
									TerrainEditor->SetSelectedChildBlock(TLBProxy->LayerInfoIndex);
								}
								else
								{
									TerrainEditor->SetSelectedBlock(TLBProxy->LayerInfoIndex);
									TerrainEditor->SetSelectedChildBlock(-1);
								}

								INT	Flags		= 0;
								INT	NewFlags	= 0;
								INT	AddFlags	= 0;

								if (Info->IsRenamable())
								{
									Flags	|= WxMBTerrainEditorRightClick::OPT_RENAME;
								}
								if ((Info->GetLayerType() == TLT_Layer) ||
									(Info->GetLayerType() == TLT_TerrainMaterial) ||
									(Info->GetLayerType() == TLT_FilteredMaterial))
								{
									Flags	|= WxMBTerrainEditorRightClick::OPT_CREATE;
								}

								Flags	|= WxMBTerrainEditorRightClick::OPT_DELETE;

								if (Info->GetLayerType() == TLT_Layer)
								{
									Flags		|= WxMBTerrainEditorRightClick::OPT_USESELECTED;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_BEFORE;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_AFTER;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWMATERIAL;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_LAYER_BEFORE;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_LAYER_AFTER;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_MATERIAL;
								}

								// Always give the option to create a new Layer
								NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER;
								NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_AUTO;
								NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_AUTO_SELECT;
								AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_LAYER;

								if (Info->GetLayerType() == TLT_DecoLayer)
								{
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWDECOLAYER_BEFORE;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWDECOLAYER_AFTER;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DECORATION;
								}

								// Always give the option to create a new DecoLayer
								NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWDECOLAYER;

								if ((Info->GetLayerType() == TLT_TerrainMaterial) ||
									(Info->GetLayerType() == TLT_FilteredMaterial))
								{
									Flags		|= WxMBTerrainEditorRightClick::OPT_USESELECTED;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWMATERIAL_BEFORE;
									NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWMATERIAL_AFTER;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_MATERIAL_BEFORE;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_MATERIAL_AFTER;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DISPLACEMENT_BEFORE;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DISPLACEMENT;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DISPLACEMENT_AFTER;
								}

								if (Info->GetLayerType() == TLT_Decoration)
								{
									Flags		|= WxMBTerrainEditorRightClick::OPT_USESELECTED;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DECORATION_BEFORE;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DECORATION;
									AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_DECORATION_AFTER;
								}

								Menu = new WxMBTerrainEditorRightClick(TerrainEditor, Flags, NewFlags, AddFlags);
							}
							break;
						case TLVCI_Wireframe:
						case TLVCI_Solid:
							{
								Menu = new WxMBTerrainEditorRightClick(TerrainEditor, WxMBTerrainEditorRightClick::OPT_COLOR);
							}
							break;
						}
					}
					
					if (Menu)
					{
						FTrackPopupMenu tpm(TerrainEditor, Menu);
						tpm.Show();
						delete Menu;
					}

					Invalidate( FALSE );
				}
			}
			else
			{
				if (TerrainEditor->GetCurrentTerrain() && (Key == KEY_RightMouseButton))
				{
					debugf(TEXT("TerrainEditor: No block!"));

					INT	Flags		= 0;
					INT	NewFlags	= 0;
					INT	AddFlags	= 0;

					// Always give the option to create a new Layer
					NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER;
					NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_AUTO;
					NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWLAYER_AUTO_SELECT;
					// Always give the option to create a new DecoLayer
					NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWDECOLAYER;
					// Alway give the option to create a new material
					NewFlags	|= WxMBTerrainEditorRightClick::OPT_NEWMATERIAL;
					AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_LAYER;
					AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_MATERIAL;
					AddFlags	|= WxMBTerrainEditorRightClick::OPT_ADDSELECTED_MATERIAL_AUTO;

					Menu = new WxMBTerrainEditorRightClick(TerrainEditor, Flags, NewFlags, AddFlags);

					if (Menu)
					{
						FTrackPopupMenu tpm(TerrainEditor, Menu);
						tpm.Show();
						delete Menu;
					}

					Invalidate( FALSE );
				}
			}
		}
		else 
		if (Event == IE_Released)
		{
			bMouseDown = FALSE;
			bMouseDragging = FALSE;

			Invalidate( FALSE );
		}
	}
	else
	if (Key == KEY_MiddleMouseButton)
	{
		if (Event == IE_Pressed)
		{
			bPanning = TRUE;

			OldMouseX = HitX;
			OldMouseY = HitY;
		}
		else
		if (Event == IE_Released)
		{
			bPanning = FALSE;
		}
	}

	if (Event == IE_Pressed)
	{
		if (Key == KEY_Delete)
		{
			TerrainEditor->DeleteSelected();
		}
		else
		if (Key == KEY_Up)
		{
		}
		else
		if (Key == KEY_Down)
		{
		}
		else
		if (Key == KEY_Left)
		{
		}
		else
		if (Key == KEY_Right)
		{
		}
		else
		if ((Key == KEY_Z) && bCtrlDown)
		{
			TerrainEditor->Undo();
		}
		else
		if ((Key == KEY_Y) && bCtrlDown)
		{
			TerrainEditor->Redo();
		}
		else
		if (Key == KEY_SpaceBar)
		{
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

/** 
 *	SetCanvas
 *	Set the position of the canvas being draw
 *
 *	@param	X		The desired X-position
 *	@param	Y		The desired Y-position
 */
void FTerrainLayerViewportClient::SetCanvas(INT X, INT Y)
{
	Origin2D.X = X;
	Origin2D.X = Min(0, Origin2D.X);
	
	Origin2D.Y = Y;
	Origin2D.Y = Min(0, Origin2D.Y);

	Invalidate( FALSE );
	// Force it to draw so the view change is seen
	Viewport->Draw();
}

/**
 *	
 */
UBOOL FTerrainLayerViewportClient::CreateIcons()
{
	DWORD LoadFlags = LOAD_NoWarn | LOAD_FindIfFail;

	ViewIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_View"), NULL, LoadFlags, NULL);
	check(ViewIcon);
	HideIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Empty"), NULL, LoadFlags, NULL);
	check(HideIcon);
	LockIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Lock"), NULL, LoadFlags, NULL);
	check(LockIcon);
	UnlockIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Empty"), NULL, LoadFlags, NULL);
	check(UnlockIcon);
	WireframeOffIcon		= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Wireframe"), NULL, LoadFlags, NULL);//TLB_WireframeOff"));
	check(WireframeOffIcon);
	WireframeIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Wireframe"), NULL, LoadFlags, NULL);
	check(WireframeIcon);
	SolidIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Solid"), NULL, LoadFlags, NULL);
	check(SolidIcon);
	SolidOffIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Solid"), NULL, LoadFlags, NULL);//TLB_SolidOff"));
	check(SolidOffIcon);
	OpenedIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Open_Alpha"), NULL, LoadFlags, NULL);
	check(OpenedIcon);
	ClosedIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Closed_Alpha"), NULL, LoadFlags, NULL);
	check(ClosedIcon);
	LayerIcon				= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Layer_Icon_Alpha"), NULL, LoadFlags, NULL);
	check(LayerIcon);
	MaterialIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Material_Icon_Alpha"), NULL, LoadFlags, NULL);
	check(MaterialIcon);
	DecoLayerIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_DecoLayer_Icon_Alpha"), NULL, LoadFlags, NULL);
	check(DecoLayerIcon);
	DecorationIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_Decoration_Icon_Alpha"), NULL, LoadFlags, NULL);
	check(DecorationIcon);
	HeightMapIcon			= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TLB_HeightMap_Icon_Alpha"), NULL, LoadFlags, NULL);
	check(HeightMapIcon);
	ProceduralOverlayIcon	= (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.TerrainLayerBrowser.TerrainProp_ProceduralOverlay"), NULL, LoadFlags, NULL);
	check(ProceduralOverlayIcon);

	return TRUE;
}

/**
 *	
 */
INT FTerrainLayerViewportClient::GetIconOffset(ViewIcons eIcon, INT ViewX)
{
	if ((eIcon < 0) || (eIcon >= TLVCI_MAX_COUNT))
	{
		warnf(TEXT("TLB: Invalid icon request (0x%08x)"), (INT)eIcon);
		return 0;
	}

	INT	Offset = TLBIconInfo[eIcon].PositionOffset;
	if (Offset < 0)
	{
		Offset	+= ViewX;
	}

	return Offset;
}

/**
 *	
 */
FTexture* FTerrainLayerViewportClient::GetIcon(ViewIcons eIcon, INT InfoIndex, FLinearColor& Color)
{
	// Look up the layer in the array of layer blocks...
	TArray<FTerrainBrowserBlock>*		InfoBlocks	= TerrainEditor->GetInfoBlocks();
	FTerrainBrowserBlock*				Info		= &((*InfoBlocks)(InfoIndex));
	
    switch (eIcon)
	{
	case TLVCI_ViewHide:
		if (Info->GetView())
		{
			return GetViewIcon()->Resource;
		}
		else
		{
			return GetHideIcon()->Resource;
		}
		break;
	case TLVCI_LockUnlock:
		if (Info->GetLock())
		{
			return GetLockIcon()->Resource;
		}
		else
		{
			return GetUnlockIcon()->Resource;
		}
		break;
	case TLVCI_Wireframe:
		if (Info->GetDrawWireOverlay())
		{
			Color = FLinearColor(Info->GetWireframeColor());
			Color.A = 1.0f;
			return GetWireframeIcon()->Resource;
		}
		else
		{
			return GetWireframeOffIcon()->Resource;
		}
		break;
	case TLVCI_Solid:
		if (Info->GetDrawShadeOverlay())
		{
			Color = FLinearColor(Info->GetHighlightColor());
			Color.A = 1.0f;
			return GetSolidIcon()->Resource;
		}
		else
		{
			return GetSolidOffIcon()->Resource;
		}
		break;
	case TLVCI_OpenClose:
	case TLVCI_OpenClose2:
		if (Info->GetOpen())
		{
			return GetOpenedIcon()->Resource;
		}
		else
		{
			return GetClosedIcon()->Resource;
		}
		break;
	case TLVCI_Thumbnail:
	case TLVCI_Thumbnail2:
		return NULL;
	case TLVCI_LayerType:
		switch (Info->GetLayerType())
		{
		case TLT_HeightMap:				return GetHeightMapIcon()->Resource;
		case TLT_Layer:					return GetLayerIcon()->Resource;
		case TLT_DecoLayer:				return GetDecoLayerIcon()->Resource;
		case TLT_TerrainMaterial:		return GetMaterialIcon()->Resource;
		case TLT_FilteredMaterial:		return GetMaterialIcon()->Resource;
		case TLT_Decoration:			return GetDecorationIcon()->Resource;
		}
	case TLVCI_ProceduralOverlay:
		return GetProceduralOverlayIcon()->Resource;
	}
	
	return NULL;
}

/**
 *	
 */
void FTerrainLayerViewportClient::DrawLayerArray(FViewport* Viewport, FCanvas* Canvas)
{
	INT		DividerCount	= 0;

	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	for (INT InfoIndex = 0; InfoIndex < InfoBlocks->Num(); InfoIndex++)
	{
		FTerrainBrowserBlock* Info	= &((*InfoBlocks)(InfoIndex));
		DrawLayer(Viewport, Canvas, Info, InfoIndex, DividerCount);
	}
}

/**
 *	
 */
void FTerrainLayerViewportClient::DrawLayer(FViewport* Viewport, FCanvas* Canvas,
	FTerrainBrowserBlock* Info, INT InfoIndex, INT& YIndex)
{
	if (Info->GetDraw() && (Info->GetLayerType() != TLT_Empty))
	{
		INT		ViewX			= Viewport->GetSizeX();
		INT		XOffset			= 0;
		
		if (Info->GetParentBlock())
		{
			XOffset	+= TerrainEditorBrowser_ChildOffset;
			if (Info->GetParentBlock()->GetParentBlock())
			{
				XOffset	+= TerrainEditorBrowser_ChildOffset;
			}
		}

		// Draw the selection box... ie the background for the item
		INT		ViewY			= Viewport->GetSizeY();
		INT		YPos			= (TerrainEditorBrowser_BlockHeight * YIndex) + 1;
		INT		XSize			= ViewX;

		if (XSize > 0)
		{
			FLinearColor	BlockColor;
			INT				ColorIndex	= 0;
			
			if (Info->GetParentBlock())
			{
				ColorIndex++;
				if (Info->GetParentBlock()->GetParentBlock())
				{
					ColorIndex++;
				}
			}

			if (Info->GetParentBlock())
			{
				if (InfoIndex == TerrainEditor->GetSelectedChildBlock())
				{
					BlockColor	= TerrainEditor->GetSelectedColor(ColorIndex);
				}
				else
				{
					BlockColor	= TerrainEditor->GetBackgroundColor(ColorIndex);
				}
			}
			else
			{
				if (InfoIndex == TerrainEditor->GetSelectedBlock())
				{
					BlockColor	= TerrainEditor->GetSelectedColor(ColorIndex);
				}
				else
				{
					BlockColor	= TerrainEditor->GetBackgroundColor(ColorIndex);
				}
			}
			BlockColor.A	= 1.0f;

			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy(new HTEBHitProxy(TLVCI_Stub, InfoIndex));
			}

			DrawTile(Canvas,0, YPos, XSize, TerrainEditorBrowser_BlockHeight - 2, 
				0.f, 0.f, 1.f, 1.f, BlockColor);

			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy(NULL);
			}
		}

		if (Info->GetViewable())
		{
			// The View/Hide button
			DrawTLBIcon(TLVCI_ViewHide, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting());
		}

		if (Info->GetLockable())
		{
			// The Lock/Unlock button
			DrawTLBIcon(TLVCI_LockUnlock, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting());
		}

		if (Info->GetOverlayable())
		{
			// The Wireframe button
			DrawTLBIcon(TLVCI_Wireframe, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting());

			// The Solid button
			DrawTLBIcon(TLVCI_Solid, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting());
		}

		if (Info->GetOpenable())
		{
			if (Info->GetUseSecondOpen())
			{
				// The Open/Close2 button - only draw if the layer block can be opened...
				DrawTLBIcon(TLVCI_OpenClose2, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting(), XOffset);
			}
			else
			{
				// The Open/Close button - only draw if the layer block can be opened...
				DrawTLBIcon(TLVCI_OpenClose, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting(), XOffset);
			}
		}

		// Draw the thumbnail...
		if (Info->GetDrawThumbnail())
		{
			DrawThumbnail(TLVCI_Thumbnail, Info, InfoIndex, YIndex, Viewport, Canvas, Canvas->IsHitTesting(), XOffset);
		}
		if (Info->GetDrawThumbnail2())
		{
			DrawThumbnail(TLVCI_Thumbnail2, Info, InfoIndex, YIndex, Viewport, Canvas, Canvas->IsHitTesting(), XOffset);
		}

		// Draw the layer name
		DrawName(Info, InfoIndex, YIndex, Viewport, Canvas, XOffset);

		// Draw the layer icon
		DrawTLBIcon(TLVCI_LayerType, InfoIndex, Info, YIndex, Viewport, Canvas, Canvas->IsHitTesting());

		// Draw the dividing line
		YIndex++;
		DrawTile(Canvas,0, TerrainEditorBrowser_BlockHeight * YIndex, ViewX - Origin2D.X, 
			1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
	}
}

/**
 *	DrawTLBIcon
 *	A helper function for drawing the given icon for the given info entry.
 */
void FTerrainLayerViewportClient::DrawTLBIcon(ViewIcons eIcon, INT InfoIndex, FTerrainBrowserBlock* Info, 
	INT YCount, FViewport* Viewport, FCanvas* Canvas, UBOOL bHitTesting, INT XOffset)
{
	FLinearColor WhiteColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	FTexture* Icon = GetIcon(eIcon, InfoIndex, HighlightColor);
	if (Icon == NULL)
	{
		return;
	}

	UBOOL	bDrawOutline	= TRUE;
	if ((eIcon == TLVCI_LayerType) || (eIcon == TLVCI_OpenClose) || (eIcon == TLVCI_OpenClose2))
	{
		bDrawOutline = FALSE;
	}

	INT	XPos, YPos;
	INT	Size	= TerrainEditorBrowser_IconSize;
	INT ViewX	= Viewport->GetSizeX();

	if (bHitTesting)
	{
        Canvas->SetHitProxy(new HTEBHitProxy(eIcon, InfoIndex));
	}
	Size	= TLBIconInfo[eIcon].Size;
	XPos	= GetIconOffset(eIcon, ViewX) + XOffset;
	YPos	= ((TerrainEditorBrowser_BlockHeight / 2) - (Size / 2)) + (YCount * TerrainEditorBrowser_BlockHeight);
	if (bDrawOutline)
	{
		DrawTile(Canvas,XPos - 1, YPos - 1, Size + 1, Size + 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);
	}
	if (!(HighlightColor == FLinearColor::White))
	{
		DrawTile(Canvas,XPos, YPos, Size, Size, 0.f, 0.f, 1.f, 1.f, HighlightColor);
		DrawTile(Canvas,XPos + 2, YPos + 2, Size - 4, Size - 4, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, Icon);
	}
	else
	{
		DrawTile(Canvas,XPos, YPos, Size, Size, 0.f, 0.f, 1.f, 1.f, HighlightColor, Icon);
	}
	if (bHitTesting)
	{
        Canvas->SetHitProxy(NULL);
	}
}

/**
 *	DrawThumbnail
 *	A helper function for rendering thumbnail images of items in the browser.
 */
void FTerrainLayerViewportClient::DrawThumbnail(ViewIcons eIcon, FTerrainBrowserBlock* Info, 
	INT InfoIndex, INT YIndex, FViewport* Viewport, FCanvas* Canvas, 
	UBOOL bHitTesting, INT XOffset)
{
	if ((eIcon != TLVCI_Thumbnail) && (eIcon != TLVCI_Thumbnail2))
	{
		warnf(TEXT("TLB: Invalid Icon passed to DrawThumbnail"));
		return;
	}

	FLinearColor HighlightColor;
	FTexture* Overlay = NULL;

	INT	YPos = (TerrainEditorBrowser_BlockHeight * YIndex) + (TerrainEditorBrowser_BlockHeight / 2) - (TLBIconInfo[eIcon].Size / 2);
	DrawTile(Canvas,TLBIconInfo[eIcon].PositionOffset + XOffset, YPos, TLBIconInfo[eIcon].Size + 2, TLBIconInfo[eIcon].Size + 2, 0.f, 0.f, 0.f, 0.f, TerrainEditor->GetBorderColor());

	UBOOL bDecrementOverlay = FALSE;

	if (Canvas->IsHitTesting() == FALSE)
	{
		UObject*	RenderObject	= NULL;
		switch (Info->GetLayerType())
		{
		case TLT_HeightMap:
			break;
		case TLT_Layer:
			{
				RenderObject	= Info->GetLayer()->Setup;
				FTerrainLayer* Layer = Info->GetLayer();
				if (Layer->Setup)
				{
					if (Layer->Setup->Materials.Num() > 1)
					{
						Overlay = GetIcon(TLVCI_ProceduralOverlay, InfoIndex, HighlightColor);
					}
				}
			}
			break;
		case TLT_DecoLayer:
			break;
		case TLT_TerrainMaterial:
		case TLT_FilteredMaterial:
			if (Info->GetFilteredMaterial() && Info->GetFilteredMaterial()->Material)
			{
				if (eIcon == TLVCI_Thumbnail)
				{
					RenderObject	= Info->GetFilteredMaterial()->Material->Material;
					FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();
					YPos += Canvas->GetTransform().GetOrigin().Y;
				}
				else
				{
					RenderObject	= Info->GetFilteredMaterial()->Material->DisplacementMap;
				}
//				YPos += Canvas->GetTransform().GetOrigin().Y;
			}
			break;
		case TLT_Decoration:
			if (Info->GetDecoration())
			{
				FTerrainDecoration*	Decoration	= Info->GetDecoration();

				if (Decoration->Instances.Num())
				{
					FTerrainDecorationInstance* Instance	= &(Decoration->Instances(0));
					if (Instance)
					{
						UPrimitiveComponent*	Component	= Instance->Component;
						if (Component->IsA(UStaticMeshComponent::StaticClass()))
						{
							UStaticMeshComponent* StaticMeshComp	= Cast<UStaticMeshComponent>(Component);
							RenderObject	= StaticMeshComp->StaticMesh;
							YPos += Canvas->GetTransform().GetOrigin().Y;
						}
					}
				}
			}
			break;
		}

		if (RenderObject)
		{
			// Get the rendering info for this object
			FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo(RenderObject);
			// If there is an object configured to handle it, draw the thumbnail
			if ((RenderInfo != NULL) && (RenderInfo->Renderer != NULL))
			{
				// Now draw it
				RenderInfo->Renderer->Draw(RenderObject, TPT_Plane, TLBIconInfo[eIcon].PositionOffset + XOffset, YPos, 
					TLBIconInfo[eIcon].Size, TLBIconInfo[eIcon].Size, Viewport, Canvas, TBT_None, FColor(0, 0, 0), FColor(0, 0, 0));
				if (Overlay)
				{
					if (bDecrementOverlay)
					{
						YPos -= Canvas->GetTransform().GetOrigin().Y;
					}
					DrawTile(Canvas,TLBIconInfo[eIcon].PositionOffset + XOffset, YPos, 
						TLBIconInfo[eIcon].Size, TLBIconInfo[eIcon].Size, 
						0.f, 0.f, 1.f, 1.f, FLinearColor::White, Overlay);
				}
			}
		}
	}
}

/**
 *	DrawName
 *	A helper function for rendering the name of the given info block.
 */
void FTerrainLayerViewportClient::DrawName(FTerrainBrowserBlock* Info, INT InfoIndex, INT YIndex, 
	FViewport* Viewport, FCanvas* Canvas, INT XOffset)
{
	FLOAT	CharWidth, CharHeight;
	TCHAR	TestChar[2] = TEXT("X");
	GEngine->SmallFont->GetCharSize(TestChar[0], CharWidth, CharHeight);

	INT	XPos	= TLBIconInfo[TLVCI_Name].PositionOffset + XOffset;
	INT	YPos	= (TerrainEditorBrowser_BlockHeight * YIndex) + (TerrainEditorBrowser_BlockHeight / 2) - appTrunc(CharHeight / 2);

	FString	Name(TEXT(""));

	switch (Info->GetLayerType())
	{
	case TLT_HeightMap:
		Name	= FString(TEXT("HeightMap"));
		break;
	case TLT_Layer:
		{
			FTerrainLayer* Layer = Info->GetLayer();
			Name	= FString::Printf(TEXT("%s"), *Layer->Name);
		}
		break;
	case TLT_DecoLayer:
		{
			FTerrainDecoLayer* DecoLayer = Info->GetDecoLayer();
			Name	= FString::Printf(TEXT("%s"), *DecoLayer->Name);
		}
		break;
	case TLT_TerrainMaterial:
	case TLT_FilteredMaterial:
		{
			FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();
			if (Material->Material)
			{
				Name	= FString::Printf(TEXT("%s"), *Material->Material->GetName());
			}
		}
		break;
	case TLT_Decoration:
		{
			FTerrainDecoration* Decoration = Info->GetDecoration();
			if (Decoration->Factory && Decoration->Factory->FactoryIsValid())
			{
				Name	= FString::Printf(TEXT("%s"), *Decoration->Factory->GetName());
			}
		}
		break;
	}

	DrawShadowedString(Canvas,XPos, YPos, *Name, GEngine->SmallFont, FLinearColor::White);
}

/**
 *	DrawConnectors
 *	A helper function for drawing the grid lines for the open blocks.
 */
void FTerrainLayerViewportClient::DrawConnectors(FViewport* Viewport, FCanvas* Canvas)
{
	// For each info block, draw the connecting lines between the parent block
	// and its children.
	INT	YIndex	= 0;
	INT	XPos, YPos, YEndPos;

	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();

	FTerrainBrowserBlock* Info				= NULL;
	FTerrainBrowserBlock* ChildInfo			= NULL;
	FTerrainBrowserBlock* GrandParentInfo	= NULL;
	FTerrainBrowserBlock* GrandChildInfo	= NULL;

	// First, draw open/close button 0 lines
	INT InfoIndex = 0;
	if (InfoBlocks->Num())
	{
		Info	= &((*InfoBlocks)(InfoIndex));
		while (Info)
		{
			// Count the number of drawn items prior to this...
			YIndex	= 0;
			for (INT CheckIndex = 0; CheckIndex < InfoIndex; CheckIndex++)
			{
				FTerrainBrowserBlock*	CheckInfo	= &((*InfoBlocks)(CheckIndex));
				if (CheckInfo->GetDraw())
				{
					YIndex++;
				}
			}

			TArray<INT>	ChildPos;
			TArray<INT>	ChildDrawPos;
			ChildPos.Empty();
			ChildDrawPos.Empty();

			if (Info->GetDraw())
			{
				if (Info->GetOpenable() && Info->GetOpen() && !Info->GetUseSecondOpen())
				{
					INT	StartIndex	= YIndex;
					YPos	= (YIndex * TerrainEditorBrowser_BlockHeight) + (TerrainEditorBrowser_BlockHeight / 2) + (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
					YEndPos	= YPos;

					for (INT ChildIndex = InfoIndex + 1; ChildIndex < InfoBlocks->Num(); ChildIndex++)
					{
						ChildInfo	= &((*InfoBlocks)(ChildIndex));
						if (ChildInfo->GetParentBlock() == NULL)
						{
							InfoIndex	= ChildIndex;
							break;
						}

						if (ChildInfo->GetDraw())
						{
							YIndex++;
							// Determine if the block has children...
							if (ChildInfo->GetParentBlock() == Info)
							{
								INT	AddIndex = ChildPos.Add(1);
								ChildPos(AddIndex)		= YIndex;
								AddIndex = ChildDrawPos.Add(1);
								ChildDrawPos(AddIndex)	= ChildIndex;
							}
						}

						if ((ChildIndex + 1) >= InfoBlocks->Num())
						{
							InfoIndex	= ChildIndex;
							break;
						}
					}

					if (ChildPos.Num())
					{
						YIndex	= ChildPos(ChildPos.Num() - 1);

						XPos	= TLBIconInfo[TLVCI_OpenClose].PositionOffset + (TLBIconInfo[TLVCI_OpenClose].Size / 2);
						YEndPos	= ((YIndex - StartIndex) * TerrainEditorBrowser_BlockHeight) - (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
						DrawTile(Canvas,XPos, YPos, 1, YEndPos, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);

						for (INT ChildIndex = 0; ChildIndex < ChildPos.Num(); ChildIndex++)
						{
							YIndex	= ChildPos(ChildIndex);
							YEndPos	= YPos + ((YIndex - StartIndex) * TerrainEditorBrowser_BlockHeight) - (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
							
							INT		Index	= ChildDrawPos(ChildIndex);
							INT		XOffset	= 0;

							ChildInfo	= &((*InfoBlocks)(Index));
							if (ChildInfo->GetParentBlock())
							{
								XOffset	+= TerrainEditorBrowser_ChildOffset;
								if (ChildInfo->GetParentBlock()->GetParentBlock())
								{
									XOffset	+= TerrainEditorBrowser_ChildOffset;
								}
							}

							DrawTile(Canvas,XPos, YEndPos, TLBIconInfo[TLVCI_OpenClose2].Size + XOffset, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
						}
					}
				}
				else
				{
					InfoIndex++;
				}
				YIndex++;
			}
			else
			{
				InfoIndex++;
			}

			if ((InfoIndex + 1) >= InfoBlocks->Num())
			{
				Info	= NULL;
			}
			else
			{
				Info	= &((*InfoBlocks)(InfoIndex));
			}
		}
	}

	// Now, draw open/close button 1 lines
	InfoIndex = 0;
	if (InfoBlocks->Num())
	{
		Info	= &((*InfoBlocks)(InfoIndex));
		while (Info)
		{
			// Count the number of drawn items prior to this...
			YIndex	= 0;
			for (INT CheckIndex = 0; CheckIndex < InfoIndex; CheckIndex++)
			{
				FTerrainBrowserBlock*	CheckInfo	= &((*InfoBlocks)(CheckIndex));
				if (CheckInfo->GetDraw())
				{
					YIndex++;
				}
			}

			TArray<INT>	ChildPos;
			TArray<INT>	ChildDrawPos;
			ChildPos.Empty();
			ChildDrawPos.Empty();

			if (Info->GetDraw())
			{
				if (Info->GetOpenable() && Info->GetOpen() && !Info->GetUseSecondOpen())
				{
					GrandParentInfo	= Info;
					InfoIndex++;
				}
				else
				if (Info->GetOpenable() && Info->GetOpen() && Info->GetUseSecondOpen())
				{
					INT	StartIndex	= YIndex;
					YPos	= (YIndex * TerrainEditorBrowser_BlockHeight) + (TerrainEditorBrowser_BlockHeight / 2) + (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
					YEndPos	= YPos;

					for (INT ChildIndex = InfoIndex + 1; ChildIndex < InfoBlocks->Num(); ChildIndex++)
					{
						ChildInfo	= &((*InfoBlocks)(ChildIndex));
						if ((ChildInfo->GetParentBlock() == NULL) ||
							(ChildInfo->GetParentBlock() == GrandParentInfo))
						{
							InfoIndex	= ChildIndex;
							break;
						}

						if (ChildInfo->GetDraw())
						{
							YIndex++;
							// Determine if the block has children...
							if (ChildInfo->GetParentBlock() == Info)
							{
								INT	AddIndex = ChildPos.Add(1);
								ChildPos(AddIndex) = YIndex;
								AddIndex = ChildDrawPos.Add(1);
								ChildDrawPos(AddIndex)	= ChildIndex;
							}
						}

						if ((ChildIndex + 1) >= InfoBlocks->Num())
						{
							InfoIndex	= ChildIndex;
							break;
						}
					}

					if (ChildPos.Num())
					{
						YIndex	= ChildPos(ChildPos.Num() - 1);
						INT		Index;
						INT		XOffset	= TerrainEditorBrowser_ChildOffset;

						XPos	= TLBIconInfo[TLVCI_OpenClose2].PositionOffset + (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
						YEndPos	= ((YIndex - StartIndex) * TerrainEditorBrowser_BlockHeight) - (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
						DrawTile(Canvas,XPos + XOffset, YPos, 1, YEndPos, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);

						for (INT ChildIndex = 0; ChildIndex < ChildPos.Num(); ChildIndex++)
						{
							YIndex	= ChildPos(ChildIndex);
							YEndPos	= YPos + ((YIndex - StartIndex) * TerrainEditorBrowser_BlockHeight) - (TLBIconInfo[TLVCI_OpenClose2].Size / 2);
							XOffset	= 0;

							Index	= ChildDrawPos(ChildIndex);

							ChildInfo	= &((*InfoBlocks)(Index));
							if (ChildInfo->GetParentBlock())
							{
								if (ChildInfo->GetParentBlock()->GetParentBlock())
								{
									XOffset	+= TerrainEditorBrowser_ChildOffset;
								}
							}

							DrawTile(Canvas,XPos + TerrainEditorBrowser_ChildOffset, YEndPos, TLBIconInfo[TLVCI_OpenClose2].Size + XOffset, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
						}
					}
				}
				else
				{
					InfoIndex++;
				}
				YIndex++;
			}
			else
			{
				InfoIndex++;
			}

			if ((InfoIndex + 1) >= InfoBlocks->Num())
			{
				Info	= NULL;
			}
			else
			{
				Info	= &((*InfoBlocks)(InfoIndex));
			}
		}
	}
}

/**
 *	ToggleViewHideInfoBlock
 *	Toggles the view/hide state for the given info block.
 */
void FTerrainLayerViewportClient::ToggleViewHideInfoBlock(INT InfoIndex)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ToggleViewHideInfoBlock> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* Info = &((*InfoBlocks)(InfoIndex));

	Info->SetView(!Info->GetView());
	if (Info->GetLayerType() == TLT_Layer)
	{
		FTerrainLayer* Layer	= Info->GetLayer();
		Layer->Hidden	= !Info->GetView();
		if (TerrainEditor->GetCurrentTerrain())
		{
			TerrainEditor->GetCurrentTerrain()->PostEditChange();
		}
	}
}

/**
 *	ToggleLockUnlockInfoBlock
 *	Toggles the lock/unlock state of the given info block.
 */
void FTerrainLayerViewportClient::ToggleLockUnlockInfoBlock(INT InfoIndex)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ToggleLockUnlockInfoBlock> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* Info = &((*InfoBlocks)(InfoIndex));

	Info->SetLock(!Info->GetLock());
	if (Info->GetLayerType() == TLT_Layer)
	{
		FTerrainLayer* Layer	= Info->GetLayer();
		Layer->Locked = Info->GetLock();
		if (TerrainEditor->GetCurrentTerrain())
		{
			TerrainEditor->GetCurrentTerrain()->PostEditChange();
		}
	}
	else if(Info->GetLayerType() == TLT_HeightMap)
	{
		ATerrain* Terrain = TerrainEditor->GetCurrentTerrain();

		if(Terrain)
		{
			Terrain->bHeightmapLocked = Info->GetLock();
			TerrainEditor->GetCurrentTerrain()->PostEditChange();
		}
	}
}

/**
 *	ToggleOpenCloseInfoBlock
 *	Toggles the open/close state of the given info block.
 */
void FTerrainLayerViewportClient::ToggleOpenCloseInfoBlock(INT InfoIndex, INT& CanvasOffset, INT ForceOpenClose)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ToggleOpenCloseInfoBlock> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* ParentInfo = &((*InfoBlocks)(InfoIndex));
	if (ParentInfo->GetOpenable())
	{
		UBOOL bOpening = !(ParentInfo->GetOpen());
		if (ForceOpenClose == 1)
		{
			bOpening = TRUE;
		}
		else
		if (ForceOpenClose == -1)
		{
			bOpening = FALSE;
		}

		FTerrainBrowserBlock* Info;
		for (INT CheckIndex = InfoIndex + 1; CheckIndex < InfoBlocks->Num(); CheckIndex++)
		{
			Info	= &((*InfoBlocks)(CheckIndex));
			if (bOpening)
			{
				if (Info->GetParentBlock() == ParentInfo)
				{
					ToggleOpenCloseInfoBlock(CheckIndex, CanvasOffset, 1);
					Info->SetOpen(TRUE);
					Info->SetDraw(TRUE);
				}
			}
			else
			{
				if (Info->GetParentBlock() == ParentInfo)
				{
					ToggleOpenCloseInfoBlock(CheckIndex, CanvasOffset, -1);
					Info->SetOpen(FALSE);
					Info->SetDraw(FALSE);
					CanvasOffset += TerrainEditorBrowser_BlockHeight;
				}
			}
		}

		ParentInfo->SetOpen(bOpening);
	}
}

/**
 *	ToggleWireframeInfoBlock
 *	Toggles the wireframe overlay state of the given info block.
 */
void FTerrainLayerViewportClient::ToggleWireframeInfoBlock(INT InfoIndex)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ToggleViewHideInfoBlock> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* Info = &((*InfoBlocks)(InfoIndex));

	Info->SetDrawWireOverlay(!Info->GetDrawWireOverlay());
	if (Info->GetLayerType() == TLT_Layer)
	{
		FTerrainLayer* Layer	= Info->GetLayer();
		Layer->WireframeHighlighted = Info->GetDrawWireOverlay();
		Layer->WireframeColor = Info->GetWireframeColor();

		if (TerrainEditor->GetCurrentTerrain())
		{
			TerrainEditor->GetCurrentTerrain()->PostEditChange();
		}
	}
}

/**
 *	ToggleSolidInfoBlock
 *	Toggles the solid overlay state of the given info block.
 */
void FTerrainLayerViewportClient::ToggleSolidInfoBlock(INT InfoIndex)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ToggleViewHideInfoBlock> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* Info = &((*InfoBlocks)(InfoIndex));

	Info->SetDrawShadeOverlay(!Info->GetDrawShadeOverlay());
	if (Info->GetLayerType() == TLT_Layer)
	{
		FTerrainLayer* Layer		= Info->GetLayer();
		Layer->Highlighted			= Info->GetDrawShadeOverlay();
		Layer->HighlightColor		= Info->GetHighlightColor();

		ATerrain* CurrTerrain = TerrainEditor->GetCurrentTerrain();
		if (CurrTerrain)
		{
			CurrTerrain->RecacheMaterials();
		}
	}
}

/**
 *	ProcessLayerTypeHit
 *	Handles the 'clicking' of the type icon (on the right hand side).
 */
void FTerrainLayerViewportClient::ProcessLayerTypeHit(INT InfoIndex)
{
	TArray<FTerrainBrowserBlock>* InfoBlocks	= TerrainEditor->GetInfoBlocks();
	if ((InfoIndex < 0) || (InfoIndex >= InfoBlocks->Num()))
	{
		warnf(TEXT("TLB::ProcessLayerTypeHit> Invalid Index %d"), InfoIndex);
		return;
	}

	FTerrainBrowserBlock* Info = &((*InfoBlocks)(InfoIndex));

	UObject*	DisplayObject	= NULL;

	switch (Info->GetLayerType())
	{
	case TLT_HeightMap:
		break;
	case TLT_Layer:
		{
			FTerrainLayer* Layer = Info->GetLayer();
			DisplayObject	= Layer->Setup;
		}
		break;
	case TLT_DecoLayer:
		{
			FTerrainDecoLayer* DecoLayer = Info->GetDecoLayer();
/***
			UTerrainDecoLayerProperty*	TempProp	= ConstructObject<UTerrainDecoLayerProperty>(UTerrainDecoLayerProperty::StaticClass());
			check(TempProp);
			TempProp->DecoLayerPtr	= DecoLayer;

			DisplayObject	= TempProp;
***/
		}
		break;
	case TLT_TerrainMaterial:
	case TLT_FilteredMaterial:
		{
			FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();
			DisplayObject	= Material->Material;
			if (DisplayObject == NULL)
			{
				// Use the layer
				FTerrainBrowserBlock*	ParentInfo	= Info->GetParentBlock();
				if (ParentInfo)
				{
					FTerrainLayer*	Layer	= ParentInfo->GetLayer();
					if (Layer)
					{
						DisplayObject	= Layer->Setup;
					}
				}
			}
		}
		break;
	case TLT_Decoration:
		{
			FTerrainDecoration* Decoration = Info->GetDecoration();
			if (Decoration && Decoration->Factory && Decoration->Factory->FactoryIsValid())
			{
				UStaticMeshComponentFactory* MeshFactory = Cast<UStaticMeshComponentFactory>(Decoration->Factory);
				if (MeshFactory)
				{
					DisplayObject	= MeshFactory->StaticMesh;
				}
			}
		}
		break;
	}

	if (DisplayObject)
	{
		WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
		Properties->Create(TerrainEditor, -1);
		Properties->AllowClose();
		Properties->SetObject(DisplayObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories);
		Properties->SetTitle(*FString::Printf(TEXT("Properties: %s"), *DisplayObject->GetPathName()));
		Properties->Show();
	}
}

/*-----------------------------------------------------------------------------
	WxTerrainEditor
-----------------------------------------------------------------------------*/
/** Event map													*/
//BEGIN_EVENT_TABLE(WxTerrainEditor, wxWindow)
BEGIN_EVENT_TABLE(WxTerrainEditor, WxTrackableDialog)
	EVT_ACTIVATE(													WxTerrainEditor::OnActivate)
	// Tools
    EVT_BUTTON(ID_TERRAIN_TOOL_0,									WxTerrainEditor::OnToolButton)
    EVT_BUTTON(ID_TERRAIN_TOOL_1,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_2,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_3,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_4,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_5,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_6,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_7,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_8,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_9,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_10,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_11,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_12,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_13,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_14,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_15,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_16,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_17,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_18,									WxTerrainEditor::OnToolButton)
	EVT_BUTTON(ID_TERRAIN_TOOL_19,									WxTerrainEditor::OnToolButton)
    EVT_SPINCTRL(ID_TERRAIN_TOOL_SPECIFIC_VALUE,					WxTerrainEditor::OnSpecificValue)
    EVT_TEXT(ID_TERRAIN_TOOL_SPECIFIC_VALUE,						WxTerrainEditor::OnSpecificValueText)
	// Import/Export
	EVT_BUTTON(ID_TERRAIN_IMPEXP_IMPORT,							WxTerrainEditor::OnImpExpButton)
    EVT_CHECKBOX(ID_TERRAIN_IMPEXP_HEIGHTMAP_ONLY,					WxTerrainEditor::OnImpExpHeightMapOnly)
	EVT_CHECKBOX(ID_TERRAIN_IMPEXP_RETAIN_CURRENT_TERRAIN,			WxTerrainEditor::OnImpExpRetainCurrentTerrain)
    EVT_BUTTON(ID_TERRAIN_IMPEXP_EXPORT,							WxTerrainEditor::OnImpExpButton)
    EVT_CHECKBOX(ID_TERRAIN_IMPEXP_BAKE_DISPLACEMENT,				WxTerrainEditor::OnImpExpBakeDisplacementMap)
    EVT_COMBOBOX(ID_TERRAIN_IMPEXP_CLASS,							WxTerrainEditor::OnImpExpHeightMapClassCombo)
	// ViewSettings
    EVT_COMBOBOX(ID_TERRAIN_VIEWSETTINGS_COMBOBOX_TERRAIN,			WxTerrainEditor::OnTerrainCombo)
    EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_PROPERTIES,					WxTerrainEditor::OnTerrainProperties)
    EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_HIDE,						WxTerrainEditor::OnTerrainHide)
	EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_LOCK,						WxTerrainEditor::OnTerrainLock)
	EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIRE,				WxTerrainEditor::OnTerrainWire)
	EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIREFRAME_COLOR,		WxTerrainEditor::OnTerrainWireframeColor)
	EVT_BUTTON(ID_TERRAIN_VIEWSETTINGS_RECACHEMATERIALS,			WxTerrainEditor::OnTerrainRecacheMaterials)

	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIRE,	WxTerrainEditor::OnTerrainRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIREFRAME_COLOR,	WxTerrainEditor::OnTerrainRightClick)
    EVT_TOOL_RCLICKED(wxID_ANY,										WxTerrainEditor::OnTerrainRightClick)
	EVT_COMMAND_RIGHT_CLICK(wxID_ANY,								WxTerrainEditor::OnTerrainRightClick)
	EVT_RIGHT_DOWN(													WxTerrainEditor::OnTerrainRightDown)
	EVT_RIGHT_UP(													WxTerrainEditor::OnTerrainRightUp)

	// Brushes
    EVT_BUTTON(ID_TERRAIN_BRUSH_0,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_1,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_2,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_3,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_4,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_5,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_6,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_7,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_8,									WxTerrainEditor::OnBrushButton)
	EVT_BUTTON(ID_TERRAIN_BRUSH_9,									WxTerrainEditor::OnBrushButton)

    EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_0,						WxTerrainEditor::OnBrushButtonRightClick)
    EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_0,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_1,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_1,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_2,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_2,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_3,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_3,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_4,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_4,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_5,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_5,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_6,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_6,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_7,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_7,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_8,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_8,							WxTerrainEditor::OnBrushButtonRightClick)
	EVT_COMMAND_RIGHT_CLICK(ID_TERRAIN_BRUSH_9,						WxTerrainEditor::OnBrushButtonRightClick)
	EVT_TOOL_RCLICKED(ID_TERRAIN_BRUSH_9,							WxTerrainEditor::OnBrushButtonRightClick)

	// BrushSettings
    EVT_CHECKBOX(ID_TERRAIN_SETTINGS_PER_TOOL,						WxTerrainEditor::OnSettingsPerTool)
    EVT_CHECKBOX(ID_TERRAIN_SETTINGS_FLATTEN_ANGLE,					WxTerrainEditor::OnFlattenAngle)
    EVT_TEXT(ID_TERRAIN_SETTINGS_FLATTEN_HEIGHT,					WxTerrainEditor::OnFlattenHeight)
    EVT_COMBOBOX(ID_TERRAIN_SETTINGS_COMBOBOX_SCALE,				WxTerrainEditor::OnSettingsScaleCombo)
    EVT_TEXT(ID_TERRAIN_SETTINGS_COMBOBOX_SCALE,					WxTerrainEditor::OnSettingsScaleComboText)
	EVT_CHECKBOX(ID_TERRAIN_SETTINGS_SOFTSELECT,					WxTerrainEditor::OnSettingsSoftSelect)
	EVT_CHECKBOX(ID_TERRAIN_SETTINGS_CONSTRAINED,					WxTerrainEditor::OnSettingsConstrained)
    EVT_COMMAND_SCROLL(ID_TERRAIN_SETTINGS_SLIDER_STRENGTH,			WxTerrainEditor::OnSettingsSlider)
    EVT_TEXT(ID_TERRAIN_SETTINGS_TEXT_STRENGTH,						WxTerrainEditor::OnSettingsText)
    EVT_COMMAND_SCROLL(ID_TERRAIN_SETTINGS_SLIDER_RADIUS,			WxTerrainEditor::OnSettingsSlider)
	EVT_TEXT(ID_TERRAIN_SETTINGS_TEXT_RADIUS,						WxTerrainEditor::OnSettingsText)
    EVT_COMMAND_SCROLL(ID_TERRAIN_SETTINGS_SLIDER_FALLOFF,			WxTerrainEditor::OnSettingsSlider)
    EVT_TEXT(ID_TERRAIN_SETTINGS_TEXT_FALLOFF,						WxTerrainEditor::OnSettingsText)
    EVT_COMBOBOX(ID_TERRAIN_SETTINGS_COMBOBOX_MIRROR,				WxTerrainEditor::OnSettingsMirrorCombo)
	
	// Tessellation Group
    EVT_BUTTON(ID_TERRAIN_TESSELLATE_INCREASE,						WxTerrainEditor::OnTessellationButton)
	EVT_BUTTON(ID_TERRAIN_TESSELLATE_DECREASE,						WxTerrainEditor::OnTessellationButton)
	// Middle Box
	//wxPanel* ID_TERRAIN_BROWSER_PANEL
	EVT_SCROLL(														WxTerrainEditor::OnBrowserScroll)
	// Bottom Box
    EVT_BUTTON(ID_TERRAIN_BROWSER_USE_SELECTED,						WxTerrainEditor::OnUseSelectedButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_ADD_LAYER,						WxTerrainEditor::OnNewLayerButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_ADD_MATERIAL,						WxTerrainEditor::OnNewMaterialButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_ADD_DECOLAYER,					WxTerrainEditor::OnNewDecoLayerButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_DELETE_SELECTED,					WxTerrainEditor::OnDeleteSelectedButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_MOVE_LAYER_UP,					WxTerrainEditor::OnShiftLayerUpButton)
	EVT_BUTTON(ID_TERRAIN_BROWSER_MOVE_LAYER_DOWN,					WxTerrainEditor::OnShiftLayerDownButton)
    EVT_CHECKBOX(ID_TERRAIN_BROWSER_MOVE_LAYER_RETAIN_ALPHA,		WxTerrainEditor::OnRetainAlpha)
	// Right-click menu
	EVT_MENU(IDM_TERRAINBROWSER_RENAME,								WxTerrainEditor::OnRenameItem)
	EVT_MENU(IDM_TERRAINBROWSER_COLOR,								WxTerrainEditor::OnChangeColor)
	EVT_MENU(IDM_TERRAINBROWSER_CREATE,								WxTerrainEditor::OnCreate)
	EVT_MENU(IDM_TERRAINBROWSER_DELETE,								WxTerrainEditor::OnDelete)
	EVT_MENU(IDM_TERRAINBROWSER_USESELECTED,						WxTerrainEditor::OnUseSelected)
	EVT_MENU(IDM_TERRAINBROWSER_SAVEPRESET,							WxTerrainEditor::OnSavePreset)
	EVT_MENU(IDM_TERRAINBROWSER_NEWLAYER,							WxTerrainEditor::OnNewLayer)
	EVT_MENU(IDM_TERRAINBROWSER_NEWLAYER_BEFORE,					WxTerrainEditor::OnNewLayer)
	EVT_MENU(IDM_TERRAINBROWSER_NEWLAYER_AFTER,						WxTerrainEditor::OnNewLayer)
	EVT_MENU(IDM_TERRAINBROWSER_NEWLAYER_AUTO,						WxTerrainEditor::OnNewLayerAuto)
	EVT_MENU(IDM_TERRAINBROWSER_NEWLAYER_AUTO_SELECT,				WxTerrainEditor::OnNewLayerAuto)
	EVT_MENU(IDM_TERRAINBROWSER_NEWMATERIAL,						WxTerrainEditor::OnNewMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_NEWMATERIAL_BEFORE,					WxTerrainEditor::OnNewMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_NEWMATERIAL_AFTER,					WxTerrainEditor::OnNewMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_NEWDECOLAYER,						WxTerrainEditor::OnNewDecoLayer)
	EVT_MENU(IDM_TERRAINBROWSER_NEWDECOLAYER_BEFORE,				WxTerrainEditor::OnNewDecoLayer)
	EVT_MENU(IDM_TERRAINBROWSER_NEWDECOLAYER_AFTER,					WxTerrainEditor::OnNewDecoLayer)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_LAYER_BEFORE,			WxTerrainEditor::OnAddSelectedLayer)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_LAYER,					WxTerrainEditor::OnAddSelectedLayer)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_LAYER_AFTER,			WxTerrainEditor::OnAddSelectedLayer)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_BEFORE,		WxTerrainEditor::OnAddSelectedMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL,				WxTerrainEditor::OnAddSelectedMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_AFTER,			WxTerrainEditor::OnAddSelectedMaterial)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_MATERIAL_AUTO,			WxTerrainEditor::OnAddSelectedMaterialAuto)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_DISPLACEMENT,			WxTerrainEditor::OnAddSelectedDisplacement)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION_BEFORE,		WxTerrainEditor::OnAddSelectedDecoration)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION,				WxTerrainEditor::OnAddSelectedDecoration)
	EVT_MENU(IDM_TERRAINBROWSER_ADDSELECTED_DECORATION_AFTER,		WxTerrainEditor::OnAddSelectedDecoration)

	EVT_CLOSE( WxTerrainEditor::OnClose )
END_EVENT_TABLE()

/**
 *	WxTerrainEditor
 */
/**
 *	Constructor
 */
WxTerrainEditor::WxTerrainEditor(wxWindow* InParent) :
//	  wxWindow(InParent, -1, wxDefaultPosition, wxDefaultSize)
	  WxTrackableDialog(InParent, -1, wxString(*LocalizeUnrealEd("TerrainEdit_Title")))
	, EdMode(NULL)
	, LayerViewport(NULL)
	, CurrentTerrain(NULL)
	, CurrentTool(ETETB_MAX)
	, SpecificValue(NULL)
	, ImportButton(NULL)
	, ExportButton(NULL)
	, HeightMapOnly(NULL)
	, BakeDisplacementMap(NULL)
	, HeightmapClassLabel(NULL)
	, HeightmapClass(NULL)
	, ViewSettingsTerrainComboLabel(NULL)
	, ViewSettingsTerrainCombo(NULL)
	, ViewSettingsTerrainPropertiesLabel(NULL)
	, ViewSettingsTerrainProperties(NULL)
	, ViewSettingsHideLabel(NULL)
	, ViewSettingsLockLabel(NULL)
	, ViewSettingsWireLabel(NULL)
	, ViewSettingsWireframeColorLabel(NULL)
	, ViewSettingsRecacheMaterials(NULL)
	, CurrentBrush(ETEBB_Custom)
	, SettingsPerTool(NULL)
	, SoftSelectionCheck(NULL)
	, ConstrainedTool(NULL)
	, FlattenAngle(NULL)
	, ToolScaleLabel(NULL)
	, ToolScaleCombo(NULL)
	, ToolMirrorLabel(NULL)
	, ToolMirrorCombo(NULL)
	, TessellationIncreaseLabel(NULL)
	, TessellationIncrease(NULL)
	, TessellationDecreaseLabel(NULL)
	, TessellationDecrease(NULL)
	, TerrainBrowserPanel(NULL)
	, UseSelectedButton(NULL)
	, NewLayerButton(NULL)
	, NewMaterialButton(NULL)
	, NewDecoLayerButton(NULL)
	, DeleteSelectedButton(NULL)
	, ShiftLayerUpButton(NULL)
	, ShiftLayerDownButton(NULL)
	, RetainAlpha(NULL)
	, SelectedBlock(-1)
	, SelectedChildBlock(-1)
	, CurrentRightClickIndex(-1)
{
	// Register our callbacks
	GCallbackEvent->Register(CALLBACK_RefreshEditor_TerrainBrowser, this);
	GCallbackEvent->Register(CALLBACK_RefreshEditor_AllBrowsers, this);
	GCallbackEvent->Register(CALLBACK_SelectObject, this);

	EdMode	= (FEdModeTerrainEditing*)GEditorModeTools().FindMode(EM_TerrainEdit);

	StrExportDirectory	= FString(TEXT(""));

	INT	Index;

	for (Index = 0; Index < ETETB_MAX; Index++)
	{
		ToolButtons[Index].Tool		= GetTool((EToolButton)Index);
		ToolButtons[Index].Button	= NULL;
		ToolButtons[Index].Active	= NULL;
		ToolButtons[Index].Inactive	= NULL;
	}

	ViewSettingsView.Button		= NULL;
	ViewSettingsView.Active		= NULL;
	ViewSettingsView.Inactive	= NULL;
	ViewSettingsLock.Button		= NULL;
	ViewSettingsLock.Active		= NULL;
	ViewSettingsLock.Inactive	= NULL;
	ViewSettingsWire.Button		= NULL;
	ViewSettingsWire.Active		= NULL;
	ViewSettingsWire.Inactive	= NULL;
	ViewSettingsWireframeColor.Button	= NULL;
	ViewSettingsWireframeColor.Active	= NULL;
	ViewSettingsWireframeColor.Inactive	= NULL;

	for (Index = 0; Index < ETETS_MAX; Index++)
	{
		Settings[Index].Label	= NULL;
		Settings[Index].Slider	= NULL;
		Settings[Index].Text	= NULL;
	}

	EditorOptions = ConstructObject<UTerrainEditOptions>(UTerrainEditOptions::StaticClass());
	check(EditorOptions);

	bShowDecoarationMeshes	= EditorOptions->bShowDecoarationMeshes;
	BackgroundColor[0]		= EditorOptions->TerrainLayerBrowser_BackgroundColor;
	BackgroundColor[1]		= EditorOptions->TerrainLayerBrowser_BackgroundColor2;
	BackgroundColor[2]		= EditorOptions->TerrainLayerBrowser_BackgroundColor3;
	SelectedColor[0]		= EditorOptions->TerrainLayerBrowser_SelectedColor;
	SelectedColor[1]		= EditorOptions->TerrainLayerBrowser_SelectedColor2;
	SelectedColor[2]		= EditorOptions->TerrainLayerBrowser_SelectedColor3;
	BorderColor				= EditorOptions->TerrainLayerBrowser_BorderColor;

	for (Index = 0; Index < ETEBB_MAX; Index++)
	{
		//@todo. Move this into a configuration file!
		switch (Index)
		{
		case ETEBB_Brush0:
			BrushButtons[Index].Strength	= EditorOptions->Solid1_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Solid1_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Solid1_Falloff;
			break;
		case ETEBB_Brush1:
			BrushButtons[Index].Strength	= EditorOptions->Solid2_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Solid2_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Solid2_Falloff;
			break;
		case ETEBB_Brush2:
			BrushButtons[Index].Strength	= EditorOptions->Solid3_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Solid3_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Solid3_Falloff;
			break;
		case ETEBB_Brush3:
			BrushButtons[Index].Strength	= EditorOptions->Solid4_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Solid4_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Solid4_Falloff;
			break;
		case ETEBB_Brush4:
			BrushButtons[Index].Strength	= EditorOptions->Solid5_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Solid5_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Solid5_Falloff;
			break;
		case ETEBB_Brush5:
			BrushButtons[Index].Strength	= EditorOptions->Noisy1_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Noisy1_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Noisy1_Falloff;
			break;
		case ETEBB_Brush6:
			BrushButtons[Index].Strength	= EditorOptions->Noisy2_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Noisy2_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Noisy2_Falloff;
			break;
		case ETEBB_Brush7:
			BrushButtons[Index].Strength	= EditorOptions->Noisy3_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Noisy3_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Noisy3_Falloff;
			break;
		case ETEBB_Brush8:
			BrushButtons[Index].Strength	= EditorOptions->Noisy4_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Noisy4_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Noisy4_Falloff;
			break;
		case ETEBB_Brush9:
			BrushButtons[Index].Strength	= EditorOptions->Noisy5_Strength;
			BrushButtons[Index].Radius		= EditorOptions->Noisy5_Radius;
			BrushButtons[Index].Falloff		= EditorOptions->Noisy5_Falloff;
			break;
		}
		BrushButtons[Index].Button		= NULL;
		BrushButtons[Index].Active		= NULL;
		BrushButtons[Index].Inactive	= NULL;
	}

	bTerrainIsLocked			= FALSE;
	bTerrainIsInView			= TRUE;

	if (CreateControls() == FALSE)
	{
		//@todo. Localize this string...
		appMsgf(AMT_OK, TEXT("Failed to create TerrainEditor controls"));
	}
}

/**
 *	Destructor
 */
WxTerrainEditor::~WxTerrainEditor()
{
	//@todo. Kill all the bitmaps?

	// Unregister all of our events
	GCallbackEvent->UnregisterAll(this);
#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif
}

/**
 * Called when there is a Callback issued.
 * @param InType	The type of callback that was issued.
 * @param InObject	Object that was modified.
 */
void WxTerrainEditor::Send( ECallbackEventType InType, UObject* InObject )
{
	switch(InType)
	{
	case CALLBACK_SelectObject:
		{
			ATerrain* Terrain = Cast<ATerrain>(InObject);
			if (Terrain)
			{
				SetSelected(Terrain, TLT_HeightMap, -1);
			}
		}
		break;
	}
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxTerrainEditor::OnSelected()
{
	if(!IsShown())
	{
		Show();
	}

	WxTrackableDialog::OnSelected();
}

/**
 *	Show
 *
 *	@param	show	TRUE to show window, FALSE to hide
 *
 *	@return	TRUE	if operation was successful
 */
bool WxTerrainEditor::Show(bool show)
{
	bool bResult = wxWindow::Show(show);
	if (bResult)
	{
		if (show)
		{
			CurrentTerrain = NULL;
			FillTerrainCombo();
			FillImportExportData();

			//@todo.SAS. Have the editor remember the last settings...

			// Set the current tool to Paint
			wxCommandEvent ToolEvent;
			if ((EditorOptions->Current_Tool >= 0) && (EditorOptions->Current_Tool < ETETB_MAX))
			{
				ToolEvent.SetId(ID_TERRAIN_TOOL_0 + EditorOptions->Current_Tool);
			}
			else
			{
				ToolEvent.SetId(ID_TERRAIN_TOOL_2);
				EditorOptions->Current_Tool = ETETB_Paint;
			}
			OnToolButton(ToolEvent);

			if (EditorOptions->Current_Brush != ETEBB_Custom)
			{
				ToolEvent.SetId(ID_TERRAIN_BRUSH_0 + EditorOptions->Current_Brush);
				OnBrushButton(ToolEvent);
			}
			else
			{
				wxScrollEvent In;

				In.SetId(ID_TERRAIN_SETTINGS_SLIDER_STRENGTH);
				In.SetEventObject(Settings[ETETS_Strength].Slider);
				In.SetPosition(EditorOptions->Current_Strength);
				OnSettingsSlider(In);

				In.SetId(ID_TERRAIN_SETTINGS_SLIDER_RADIUS);
				In.SetEventObject(Settings[ETETS_Radius].Slider);
				In.SetPosition(EditorOptions->Current_Radius);
				OnSettingsSlider(In);

				In.SetId(ID_TERRAIN_SETTINGS_SLIDER_FALLOFF);
				In.SetEventObject(Settings[ETETS_Falloff].Slider);
				In.SetPosition(EditorOptions->Current_Falloff);
				OnSettingsSlider(In);
			}

			if (EdMode)
			{
				EdMode->bConstrained = EditorOptions->bConstrainedEditing;

				FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(EdMode->GetSettings());
				if (ToolSettings)
				{
					ToolSettings->MirrorSetting = (FTerrainToolSettings::MirrorFlags)(EditorOptions->Current_MirrorFlag);
				}
			}

			TerrainBrowserPanel->Show(show);
		}
		else
		{
			EditorOptions->SaveConfig();
			CurrentTerrain = NULL;
			InfoBlocks.Empty();
			FModeTool_Terrain::ClearPartialData();
		}
	}

	return bResult;
}

/** Hides the terrain editor and changes the editor mode to camera mode. */
void WxTerrainEditor::HideAndChangeMode()
{
	// Hide the terrain editor.
	Show( false );

	// Change over to camera mode.
	wxCommandEvent StubEvent;
	check(GApp);
	if (GApp->EditorFrame)
	{
		if (GApp->EditorFrame->ButtonBar)
		{
			if (GApp->EditorFrame->ButtonBar->ButtonGroups(0))
			{
				GApp->EditorFrame->ButtonBar->ButtonGroups(0)->OnModeCamera(StubEvent);
			}
		}
	}
}

/** Hides the terrain editor and changes the editor mode to camera mode. */
void WxTerrainEditor::OnClose( wxCloseEvent& In )
{
	HideAndChangeMode();
}

/** Updates the terrain wireframe color dialog box */
void WxTerrainEditor::OnActivate(wxActivateEvent& In)
{
	if(CurrentTerrain)
	{
		ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColour(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B));
	}
}

/**
 *	Fills in the combo boxes with the terrain and layer/decolayer information
 */
void WxTerrainEditor::RefillCombos()
{
	LoadData();
}

/**
 *	UpdateScrollBar
 *	Update the size of the thumb and number of pages in the scroll bar
 *	according to the number of info entries being displayed
 */
void WxTerrainEditor::UpdateScrollBar()
{
	// Set the scroll bar size...
	INT	Height	= TerrainBrowserPanel->Viewport->GetSizeY();
	INT	Count	= 0;
	for (INT Index = 0; Index < InfoBlocks.Num(); Index++)
	{
		FTerrainBrowserBlock* Info = &InfoBlocks(Index);
		if (Info->GetDraw() && (Info->GetLayerType() != TLT_Empty))
		{
			Count++;
		}
	}

	INT	RequiredSize	= TerrainEditorBrowser_BlockHeight * Count;

	if (RequiredSize <= Height)
	{
	    TerrainBrowserScrollBar->SetScrollbar(0, 1, 1, 1);
		TerrainBrowserPos_Vert = 0;
		LayerViewport->SetCanvas(0, 0);
	}
	else
	{
		// Determine the fit count...
		INT	FitCount	= Height / TerrainEditorBrowser_BlockHeight;

		TerrainBrowserScrollBar->SetScrollbar(0, FitCount * TerrainEditorBrowser_BlockHeight, 
			RequiredSize, TerrainEditorBrowser_BlockHeight);
	}
}

/**
 *	Set the selected block.
 *
 *	@param	InSelectedBlock		The block index to set as the selected one
 */
void WxTerrainEditor::SetSelectedBlock(INT InSelectedBlock)
{
	if (SelectedBlock != InSelectedBlock)
	{
		SelectedBlock		= InSelectedBlock;
		SelectedChildBlock	= -1;

		if (SelectedBlock != -1)
		{
			UBOOL bSendIt	= TRUE;

			FTerrainBrowserBlock*	Info	= &InfoBlocks(SelectedBlock);
			INT						Index	= -1;
			ETerrainLayerType		eType	= Info->GetLayerType();
			switch (eType)
			{
			case TLT_HeightMap:
				// Leave them as is
				break;
			case TLT_Layer:
				Index	= Info->GetArrayIndex();
				break;
			case TLT_DecoLayer:
				Index	= Info->GetArrayIndex();
				break;
				//@todo. Grab the proper parent block and send it...
			case TLT_TerrainMaterial:
			case TLT_FilteredMaterial:
				bSendIt	= FALSE;
				break;
			case TLT_Decoration:
				SetSelectedChildBlock(InSelectedBlock);
				bSendIt	= FALSE;
				break;
			}

			SaveData();
			Refresh(TRUE);
		}
	}
}

/**
 *	Set the selected child block.
 *
 *	@param	InSelectedBlock		The block index to set as the selected one
 */
void WxTerrainEditor::SetSelectedChildBlock(INT InSelectedBlock)
{
	INT	NewSelectedBlock	= SelectedBlock;

	if (SelectedChildBlock != InSelectedBlock)
	{
		if (InSelectedBlock == -1)
		{
			SelectedChildBlock = InSelectedBlock;
			return;
		}

		FTerrainBrowserBlock* ChildInfo		= &InfoBlocks(InSelectedBlock);
		FTerrainBrowserBlock* ParentInfo	= NULL;
		// Is there a selected block
		if (SelectedBlock != -1)
		{
			ParentInfo	= &InfoBlocks(SelectedBlock);
			if (ChildInfo->GetParentBlock())
			{
				UBOOL	bIsParent		= (ParentInfo == ChildInfo->GetParentBlock());
				UBOOL	bIsGrandParent	= ChildInfo->GetParentBlock()->GetParentBlock() ? (ParentInfo == ChildInfo->GetParentBlock()->GetParentBlock()) : FALSE;

				if (!bIsParent && !bIsGrandParent)
				{
					NewSelectedBlock	= -1;
				}
			}
		}

		if (NewSelectedBlock == -1)
		{
			ParentInfo	= ChildInfo->GetParentBlock();
			if (ParentInfo->GetParentBlock())
			{
				ParentInfo	= ParentInfo->GetParentBlock();
			}

			// Find it in the list...
			for (INT InfoIndex = 0; InfoIndex < InfoBlocks.Num(); InfoIndex++)
			{
				if (ParentInfo == &InfoBlocks(InfoIndex))
				{
					NewSelectedBlock	= InfoIndex;
					break;
				}
			}
		}

		SetSelectedBlock(NewSelectedBlock);
		SelectedChildBlock = InSelectedBlock;

		SaveData();
		Refresh(TRUE);
	}
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
UBOOL WxTerrainEditor::GetSelected(ATerrain*& Terrain, ETerrainLayerType& eType, INT& Index)
{
	check(EdMode->CurrentTerrain == CurrentTerrain);

	Terrain	= CurrentTerrain;
	
	if (SelectedBlock != -1)
	{
		FTerrainBrowserBlock* Info	= &InfoBlocks(SelectedBlock);
		
		eType	= Info->GetLayerType();
		switch (eType)
		{
		case TLT_HeightMap:
			break;
		case TLT_Layer:
		case TLT_DecoLayer:
			Index	= Info->GetArrayIndex();
			break;
		default:
			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}

/**
 *	Sets the currently selected information
 *
 *	@param	Terrain		Pointer to the selected terrain
 *	@param	eType		The selected type (heightmap, layer, etc.)
 *	@param	Index		The index of the selected entry in the InfoBlocks array
 *
 *	@return	TRUE		if successful
 */
UBOOL WxTerrainEditor::SetSelected(ATerrain* Terrain, ETerrainLayerType eType, INT Index)
{
	ATerrain* LastTerrain = NULL;
	if (ViewSettingsTerrainCombo->GetSelection() != -1)
	{
		LastTerrain = (ATerrain*)ViewSettingsTerrainCombo->GetClientData(ViewSettingsTerrainCombo->GetSelection());
	}

	// Find the terrain in the combo
	for (UINT TerrainIndex = 0; TerrainIndex < ViewSettingsTerrainCombo->GetCount(); TerrainIndex++)
	{
		ATerrain* ComboTerrain	= (ATerrain*)(ViewSettingsTerrainCombo->GetClientData(TerrainIndex));
		if (ComboTerrain == Terrain)
		{
			ViewSettingsTerrainCombo->SetSelection(TerrainIndex);
			break;
		}
	}

	if (Terrain != CurrentTerrain)
	{
		CurrentTerrain = Terrain;
		FillInfoArray();

		// Update the view settings dialog
		if (CurrentTerrain)
		{
			if (CurrentTerrain->bLocked)
			{
				ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Active);
			}
			else
			{
				ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Inactive);
			}
			if (CurrentTerrain->IsHiddenEd())
			{
				ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Inactive);
			}
			else
			{
				ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Active);
			}
		}
	}

	return FALSE;
}

/**
 *	Deletes the currently selected block.
 */
UBOOL WxTerrainEditor::DeleteSelected()
{
	debugf(TEXT("OnDeleteSelected"));

	UBOOL					bRefreshTerrain	= FALSE;
	FTerrainBrowserBlock*	ParentInfo		= NULL;
	FTerrainBrowserBlock*	Info			= NULL;
	
	if (SelectedBlock != -1)
	{
		Info	= &InfoBlocks(SelectedBlock);
		if (SelectedChildBlock != -1)
		{
			ParentInfo	= Info;
			Info		= &InfoBlocks(SelectedChildBlock);
			if (Info->GetParentBlock() != ParentInfo)
			{
				ParentInfo	= Info->GetParentBlock();
			}
		}

		if (Info)
		{
			if (Info->GetLayerType() == TLT_HeightMap)
			{
				appMsgf(AMT_OK, TEXT("Deleting the HeightMap is not allowed"));
				return FALSE;
			}

			FString	DeletePrompt	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_ConfirmDeletion"), GetTerrainLayerTypeString(Info->GetLayerType())));
			UBOOL	bDoDelete		= appMsgf(AMT_YesNo, *DeletePrompt);
			if (bDoDelete == FALSE)
			{
				return FALSE;
			}

			BeginTransaction(TEXT("TLB_DeleteSelected"));

			if (CurrentTerrain)
			{
				CurrentTerrain->PreEditChange(NULL);
			}

			switch (Info->GetLayerType())
			{
			case TLT_HeightMap:
				{
					appMsgf(AMT_OK, TEXT("Deleting the HeightMap is not allowed"));
				}
				break;
			case TLT_Layer:
				{
					if (CurrentTerrain)
					{
						INT	Index	= Info->GetArrayIndex();
						if (Index >= 0)
						{
							CurrentTerrain->Layers.Remove(Index);
							bRefreshTerrain	= TRUE;

							SelectedBlock--;

							if (SelectedBlock >= 0)
							{
								if (CurrentTerrain->Layers.Num() <= SelectedBlock)
								{
									SelectedBlock = CurrentTerrain->Layers.Num() - 1;
								}
								else
								if (CurrentTerrain->Layers.Num() == 0)
								{
									SelectedBlock = -1;
								}
							}
							SelectedChildBlock = -1;
						}
					}
				}
				break;
			case TLT_DecoLayer:
				{
					if (CurrentTerrain)
					{
						INT	Index	= Info->GetArrayIndex();
						if (Index >= 0)
						{
							FTerrainDecoLayer*	DecoLayer	= &CurrentTerrain->DecoLayers(Index);
							if (DecoLayer)
							{
								for (INT DecorationIndex = 0; DecorationIndex < DecoLayer->Decorations.Num(); DecorationIndex++)
								{
									FTerrainDecoration* Decoration	= &DecoLayer->Decorations(DecorationIndex);
									for (INT InstanceIndex = 0; InstanceIndex < Decoration->Instances.Num(); InstanceIndex++)
									{
										FTerrainDecorationInstance&	DecorationInstance = Decoration->Instances(InstanceIndex);
										check(DecorationInstance.Component);
										debugf(TEXT("Destroying decoration %s"), *DecorationInstance.Component->GetName());
										DecorationInstance.Component->ConditionalDetach();
									}
								}
							}
							CurrentTerrain->DecoLayers.Remove(Index);
							bRefreshTerrain	= TRUE;
							
							SelectedBlock--;

							if (SelectedBlock >= 0)
							{
								if (CurrentTerrain->DecoLayers.Num() <= SelectedBlock)
								{
									SelectedBlock = CurrentTerrain->DecoLayers.Num() - 1;
								}
								else
								if (CurrentTerrain->DecoLayers.Num() == 0)
								{
									SelectedBlock = -1;
								}
							}
							SelectedChildBlock = -1;
						}
					}
				}
				break;
			case TLT_TerrainMaterial:
			case TLT_FilteredMaterial:
				{
					check(ParentInfo);
					FTerrainLayer*	Layer	= ParentInfo->GetLayer();
					if (Layer->Setup)
					{
						INT	Index	= Info->GetArrayIndex();
						if (Index >= 0)
						{
							Layer->Setup->Materials.Remove(Index);
							bRefreshTerrain	= TRUE;

							if (Layer->Setup->Materials.Num() == 0)
							{
								ParentInfo->SetOpen(FALSE);
								ParentInfo->SetOpenable(FALSE);
							}

							SelectedChildBlock = -1;
						}
					}
				}
				break;
			case TLT_Decoration:
				{
					FTerrainDecoLayer*	DecoLayer	= ParentInfo->GetDecoLayer();
					if (DecoLayer)
					{
						INT	Index	= Info->GetArrayIndex();
						if (Index >= 0)
						{
							FTerrainDecoration* Decoration	= &DecoLayer->Decorations(Index);
							for (INT InstanceIndex = 0; InstanceIndex < Decoration->Instances.Num(); InstanceIndex++)
							{
								FTerrainDecorationInstance&	DecorationInstance = Decoration->Instances(InstanceIndex);
								check(DecorationInstance.Component);
								debugf(TEXT("Destroying decoration %s"), *DecorationInstance.Component->GetName());
								DecorationInstance.Component->ConditionalDetach();
							}

							DecoLayer->Decorations.Remove(Index);
							bRefreshTerrain	= TRUE;
							if (DecoLayer->Decorations.Num() == 0)
							{
								ParentInfo->SetOpen(FALSE);
								ParentInfo->SetOpenable(FALSE);
							}

							SelectedChildBlock = -1;
						}
					}
				}
				break;
			}

			if (bRefreshTerrain)
			{
				RefreshTerrain();
				FillInfoArray();
				if (CurrentTerrain)
				{
					CurrentTerrain->MarkPackageDirty();
				}
			}

			EndTransaction(TEXT("TLB_DeleteSelected"));

			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		}
	}

	return TRUE;
}

/**
 *	Undo the last action
 */
void WxTerrainEditor::Undo()
{
	GEditor->UndoTransaction();
	RefreshTerrain();
	if (CurrentTerrain)
	{
		CurrentTerrain->MarkPackageDirty();
	}
}

/**
 *	Redo the last undo
 */
void WxTerrainEditor::Redo()
{
	GEditor->RedoTransaction();
	RefreshTerrain();
	if (CurrentTerrain)
	{
		CurrentTerrain->MarkPackageDirty();
	}
}

// FSerializableObject interface
void WxTerrainEditor::Serialize(FArchive& Ar)
{
	if ((Ar.IsLoading() == FALSE) && (Ar.IsSaving() == FALSE))
	{
		// Don't let the EditorOptions get GC'd...
		Ar << EditorOptions;
	}
}

/** Event handlers											*/
/** Tool handlers											*/
/**
 *	Called when a tool button is pressed
 *
 *	@param	In	wxCommandEvent identifying the pressed button
 */
void WxTerrainEditor::OnToolButton(wxCommandEvent& In)
{
	wxImage		TempImage;
	UBOOL		bSpecificValueRequired	= FALSE;

	EToolButton	NewButton	= ETETB_Paint;
	switch (In.GetId())
	{
	case ID_TERRAIN_TOOL_0:	//ID_TERRAINTOOLS_ADDREMOVE_SECTORS:
		NewButton = ETETB_AddRemoveSectors;
		break;
	case ID_TERRAIN_TOOL_1:	//ID_TERRAINTOOLS_ADDREMOVE_POLYS:
		NewButton = ETETB_AddRemovePolys;
		break;
	case ID_TERRAIN_TOOL_2:	//ID_TERRAINTOOLS_PAINT:
		NewButton = ETETB_Paint;
		break;
	case ID_TERRAIN_TOOL_3:	//ID_TERRAINTOOLS_PAINT_VERTEX:
		NewButton = ETETB_PaintVertex;
		break;
	case ID_TERRAIN_TOOL_4:	//ID_TERRAINTOOLS_MANUAL_EDIT:
		NewButton = ETETB_ManualEdit;
		break;
	case ID_TERRAIN_TOOL_5:	//ID_TERRAINTOOLS_FLATTEN:
		NewButton = ETETB_Flatten;
		break;
	case ID_TERRAIN_TOOL_6:	//ID_TERRAINTOOLS_FLATTEN_SPECIFIC:
		NewButton = ETETB_FlattenSpecific;
		bSpecificValueRequired = TRUE;
		break;
	case ID_TERRAIN_TOOL_7:	//ID_TERRAINTOOLS_SMOOTH:
		NewButton = ETETB_Smooth;
		break;
	case ID_TERRAIN_TOOL_8:	//ID_TERRAINTOOLS_AVERAGE:
		NewButton = ETETB_Average;
		break;
	case ID_TERRAIN_TOOL_9:	//ID_TERRAINTOOLS_NOISE:
		NewButton = ETETB_Noise;
		break;
	case ID_TERRAIN_TOOL_10:
		NewButton = ETETB_Visibility;
		break;
	case ID_TERRAIN_TOOL_11:
		NewButton = ETETB_TexturePan;
		break;
	case ID_TERRAIN_TOOL_12:
		NewButton = ETETB_TextureRotate;
		break;
	case ID_TERRAIN_TOOL_13:
		NewButton = ETETB_TextureScale;
		break;
	case ID_TERRAIN_TOOL_14:
		NewButton = ETETB_VertexLock;
		break;
	case ID_TERRAIN_TOOL_15:
		NewButton = ETETB_MarkUnreachable;
		break;
	case ID_TERRAIN_TOOL_16:
		NewButton = ETETB_SplitX;
		break;
	case ID_TERRAIN_TOOL_17:
		NewButton = ETETB_SplitY;
		break;
	case ID_TERRAIN_TOOL_18:
		NewButton = ETETB_Merge;
		break;
	case ID_TERRAIN_TOOL_19:
		NewButton = ETETB_OrientationFlip;
		break;
	default:
		debugf(TEXT("Tool: UNKNOWN"));
		break;
	}

	SpecificValue->Enable(bSpecificValueRequired == TRUE);

	if (NewButton != CurrentTool)
	{
		if (CurrentTool != ETETB_MAX)
		{
			// Turn off the old one
			ToolButtons[CurrentTool].Button->SetBitmapLabel(*ToolButtons[CurrentTool].Inactive);
		}
		CurrentTool	= NewButton;
		EditorOptions->Current_Tool = CurrentTool;

		// Turn on the new one
		ToolButtons[CurrentTool].Button->SetBitmapLabel(*ToolButtons[CurrentTool].Active);

		// Safety catch so we don't try to set tools that haven't been implemented.
		if (ToolButtons[CurrentTool].Tool)
		{
			EdMode->SetCurrentTool(ToolButtons[CurrentTool].Tool);
		}

		FModeTool_Terrain::ZeroAllPartialData();

		LoadData();
		Refresh(TRUE);
	}
}

/**
 *	Called when the SpecificValue spinner is activated
 *
 *	@param	In	wxSpinEvent identifying the activated button
 */
void WxTerrainEditor::OnSpecificValue(wxSpinEvent& In)
{
	INT	Value	= SpecificValue->GetValue();
	if (EdMode && (EdMode->GetCurrentTool()->GetID() == MT_TerrainFlattenSpecific))
	{
//		FModeTool_TerrainFlattenSpecific*	Tool = (FModeTool_TerrainFlattenSpecific*)EdMode->GetCurrentTool();
//		Tool->SetSpecificValue(Value);
	}
}

/**
 *	Called when the SpecificValue text is edited
 *
 *	@param	In	wxCommandEvent identifying the event
 */
void WxTerrainEditor::OnSpecificValueText(wxCommandEvent& In)
{
	INT	Value	= SpecificValue->GetValue();
	if (EdMode && (EdMode->GetCurrentTool()->GetID() == MT_TerrainFlattenSpecific))
	{
//		FModeTool_TerrainFlattenSpecific*	Tool = (FModeTool_TerrainFlattenSpecific*)EdMode->GetCurrentTool();
//		Tool->SetSpecificValue(Value);
	}
}

/** Import/Export handlers									*/
/**
 *	Called when either the import or export button is pressed
 *
 *	@param	In	wxCommandEvent identifying the pressed button
 */
void WxTerrainEditor::OnImpExpButton(wxCommandEvent& In)
{
	if (In.GetId()	== ID_TERRAIN_IMPEXP_IMPORT)
	{
		debugf(TEXT("ImpExp: Import"));
		ImportTerrain();
		return;
	}
	else
	if (In.GetId()	== ID_TERRAIN_IMPEXP_EXPORT)
	{
		debugf(TEXT("ImpExp: Export"));
		ExportTerrain();
		return;
	}

	debugf(TEXT("ImpExp: UNKNOWN"));
}

/**
 *	Called when the HeightMapOnly check box is pressed
 *
 *	@param	In	wxCommandEvent identifying the pressed button
 */
void WxTerrainEditor::OnImpExpHeightMapOnly(wxCommandEvent& In)
{
	//@todo. Do we need this function?
}

/**
 *	Called when the RetainCurrentTerrain check box is pressed
 *
 *	@param	In	wxCommandEvent identifying the pressed button
 */
void WxTerrainEditor::OnImpExpRetainCurrentTerrain(wxCommandEvent& In)
{
	//@todo. Do we need this function?
}

/**
 *	Called when the BakeDisplacementMap check box is pressed
 *
 *	@param	In	wxCommandEvent identifying the pressed button
 */
void WxTerrainEditor::OnImpExpBakeDisplacementMap(wxCommandEvent& In)
{
	//@todo. Do we need this function?
}

/**
 *	Called when the HeightMap Class combo is changed
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnImpExpHeightMapClassCombo(wxCommandEvent& In)
{
	wxString SelString = HeightmapClass->GetString(HeightmapClass->GetSelection());
	if (SelString.IsEmpty())
	{
		// This should NOT happen...
		return;
	}

	// Add on the "TerrainHeightMapExporter"
	FString strSelected = FString(TEXT("TerrainHeightMapExporter"));
	strSelected += FString(SelString.c_str());
	GConfig->SetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), *strSelected, GEditorIni);
}

/** ViewSettings handlers									*/
/**
 *	Called when the Terrain combo is changed
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnTerrainCombo(wxCommandEvent& In)
{
	check(EdMode);
	if (EdMode)
	{
		if (ViewSettingsTerrainCombo->GetSelection() != -1)
		{
			EdMode->CurrentTerrain = (ATerrain*)ViewSettingsTerrainCombo->GetClientData(ViewSettingsTerrainCombo->GetSelection());
			USelection *Selection = GUnrealEd->GetSelectedActors();
			
			//NOTE: If the IsSelected() check is removed and the terrain is currently selected with its property window open an infinite update cycle that results in a crash will be created if the property window is modified
			if(!Selection->IsSelected(EdMode->CurrentTerrain))
			{
				GUnrealEd->SelectNone(FALSE, TRUE);
				GUnrealEd->SelectActor(EdMode->CurrentTerrain, TRUE, NULL, TRUE);
			}
		}
		else
		{
			EdMode->CurrentTerrain = NULL;
		}
	}

	if (EdMode->CurrentTerrain != CurrentTerrain)
	{
		CurrentTerrain = EdMode->CurrentTerrain;

		FillInfoArray();

		// Clear the selected block
		SetSelectedBlock(0);
		SetSelectedChildBlock(-1);
		CurrentRightClickIndex = -1;

		if (CurrentTerrain)
		{
			if (CurrentTerrain->bLocked)
			{
				ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Active);
			}
			else
			{
				ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Inactive);
			}
			if (CurrentTerrain->IsHiddenEd())
			{
				ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Inactive);
			}
			else
			{
				ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Active);
			}

			ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColor(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B));
		}
	}
}

/**
 *	Called when the TerrainProperties button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainProperties(wxCommandEvent& In)
{
	INT Index	= ViewSettingsTerrainCombo->GetSelection();
	if (Index == -1)
	{
		//@todo. Show a message box about this?
		return;
	}

	ATerrain* Terrain	= (ATerrain*)(ViewSettingsTerrainCombo->GetClientData(Index));
	if (Terrain)
	{
		WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
		Properties->Create(this, -1);
		Properties->AllowClose();
		Properties->SetObject(Terrain, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories);
		Properties->SetTitle(*FString::Printf(TEXT("Properties: %s"), *Terrain->GetPathName()));
		Properties->Show();
	}
}

/**
 *	Called when the RecacheMaterials button is pressed
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnTerrainRecacheMaterials(wxCommandEvent& In)
{
	if (CurrentTerrain)
	{
		FString RecachePrompt = LocalizeUnrealEd("TerrainBrowser_ConfirmRecache");
		UBOOL bDoRecache = appMsgf(AMT_YesNo, *RecachePrompt);
		if (bDoRecache)
		{
			CurrentTerrain->RecacheMaterials();
		}
	}
}

/**
 *	Called when the TerrainHide button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainHide(wxCommandEvent& In)
{
	if (CurrentTerrain == NULL)
	{
		return;
	}

	// Toggle the button
	CurrentTerrain->bHiddenEdTemporary = !CurrentTerrain->bHiddenEdTemporary;
	CurrentTerrain->PostEditChange();
	if (CurrentTerrain->IsHiddenEd())
	{
		GEditor->SelectActor( CurrentTerrain, FALSE, NULL, TRUE, FALSE );
		ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Inactive);
	}
	else
	{
		ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Active);
	}
}

/**
 *	Called when the TerrainLock button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainLock(wxCommandEvent& In)
{
	if (CurrentTerrain == NULL)
	{
		return;
	}

	// Toggle the button
	CurrentTerrain->bLocked = !CurrentTerrain->bLocked;
	if (CurrentTerrain->bLocked)
	{
		ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Active);
	}
	else
	{
		ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Inactive);
	}
}

/**
 *	Called when the TerrainWire button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainWire(wxCommandEvent& In)
{
	// Toggle the button
	ViewSettingsWire.Button->SetBitmapLabel(*ViewSettingsWire.Active);

	if(CurrentTerrain)
	{
		CurrentTerrain->bShowWireframe = !CurrentTerrain->bShowWireframe;
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

/**
 *	Called when the TerrainSolid button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainWireframeColor(wxCommandEvent& In)
{
	// Toggle the button

	if(CurrentTerrain)
	{
		FPickColorStruct PickColorStruct;
		PickColorStruct.RefreshWindows.AddItem(this);
		PickColorStruct.DWORDColorArray.AddItem(&CurrentTerrain->WireframeColor);
		PickColorStruct.bModal = TRUE;

		if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
		{
			wxColour BackgroundColor(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B);
			ViewSettingsWireframeColor.Button->SetBackgroundColour(BackgroundColor);

			GUnrealEd->RedrawLevelEditingViewports();
		}
	}
}

/**
 *	Called when the TerrainWire or TerrainSolid button is right-clicked
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainRightClick(wxCommandEvent& In)
{
	if (In.GetId() == ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIRE)
	{
		debugf(TEXT("Right-clicked WIRE!"));
	}
	else
	if (In.GetId() == ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIREFRAME_COLOR)
	{
		debugf(TEXT("Right-clicked Wireframe Color!"));
	}
	else
	{
		debugf(TEXT("Right-clicked ???? (%d)"), In.GetId());
	}
}

/**
 *	Called when the TerrainWire or TerrainSolid button is right-clicked
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTerrainRightDown(wxMouseEvent& In)
{
	wxMenu*	Menu = NULL;

	INT	Id = In.GetId();
	if ((Id >= ID_TERRAIN_BRUSH_0) && (Id <= ID_TERRAIN_BRUSH_9))
	{
		CurrentPresetBrush = (EBrushButton)(Id - ID_TERRAIN_BRUSH_0);
		Menu = new WxMBTerrainEditorRightClick(this, WxMBTerrainEditorRightClick::OPT_SAVEPRESET);
	}
	else
	{
		debugf(TEXT("Right-clicked ???? (%d)"), In.GetId());
	}

	if (Menu)
	{
		FTrackPopupMenu tpm(this, Menu);
		tpm.Show();
		delete Menu;
	}
}

void WxTerrainEditor::OnTerrainRightUp(wxMouseEvent& In)
{
}

/** Brush handlers											*/
/**
 *	Called when a TerrainBrush button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnBrushButton(wxCommandEvent& In)
{
	EBrushButton		NewBrush	= ETEBB_Custom;

	switch (In.GetId())
	{
	case ID_TERRAIN_BRUSH_0:		NewBrush	= ETEBB_Brush0;		break;	// Solid1
	case ID_TERRAIN_BRUSH_1:		NewBrush	= ETEBB_Brush1;		break;	// Solid2
	case ID_TERRAIN_BRUSH_2:		NewBrush	= ETEBB_Brush2;		break;	// Solid3
	case ID_TERRAIN_BRUSH_3:		NewBrush	= ETEBB_Brush3;		break;	// Solid4
	case ID_TERRAIN_BRUSH_4:		NewBrush	= ETEBB_Brush4;		break;	// Solid5
	case ID_TERRAIN_BRUSH_5:		NewBrush	= ETEBB_Brush5;		break;	// Noisy1
	case ID_TERRAIN_BRUSH_6:		NewBrush	= ETEBB_Brush6;		break;	// Noisy2
	case ID_TERRAIN_BRUSH_7:		NewBrush	= ETEBB_Brush7;		break;	// Noisy3
	case ID_TERRAIN_BRUSH_8:		NewBrush	= ETEBB_Brush8;		break;	// Noisy4
	case ID_TERRAIN_BRUSH_9:		NewBrush	= ETEBB_Brush9;		break;	// Noisy5
	default:														break;	// UNKNOWN
	}

	if (NewBrush != CurrentBrush)
	{
		if (CurrentBrush != ETEBB_Custom)
		{
			BrushButtons[CurrentBrush].Button->SetBitmapLabel(*BrushButtons[CurrentBrush].Inactive);
		}
		CurrentBrush	= NewBrush;
		if (CurrentBrush != ETEBB_Custom)
		{
			BrushButtons[CurrentBrush].Button->SetBitmapLabel(*BrushButtons[CurrentBrush].Active);
			SetBrush(CurrentBrush);
		}
		Refresh(TRUE);
	}
}

/**
 *	Called when a TerrainBrush button is right-clicked
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnBrushButtonRightClick(wxCommandEvent& In)
{
}

/** Settings handlers										*/
/**
 *	Called when the Settings PerTool check box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsPerTool(wxCommandEvent& In)
{
	if (EdMode == NULL)
	{
		return;
	}

	if (SettingsPerTool->IsChecked())
	{
		EdMode->bPerToolSettings = TRUE;
		debugf(TEXT("SettingsPerTool ENABLED"));
	}
	else
	{
		EdMode->bPerToolSettings = FALSE;
		debugf(TEXT("SettingsPerTool DISABLED"));
	}
	SaveData();
}

/**
 *	Called when the Flatten Angle check box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnFlattenAngle(wxCommandEvent& In)
{
	SaveData();
}


/** Settings handlers										*/
/**
 *	Called when the Flatten Height edit box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnFlattenHeight(wxCommandEvent& In)
{
	SaveData();
}

/**
 *	Called when the Settings Scale combo is changed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsScaleCombo(wxCommandEvent& In)
{
	//@todo. Implement this...
}

/**
 *	Called when the Settings Scale combo text is changed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsScaleComboText(wxCommandEvent& In)
{
	//@todo. Implement this...
    wxComboBox* InCombo = wxDynamicCast(In.GetEventObject(), wxComboBox);
    if (InCombo)
	{
		FLOAT Value = appAtof(InCombo->GetValue());
		debugf(TEXT("ScaleCombo: Value = %f"), Value);
		Refresh(TRUE);
	}

	SaveData();
}

/**
 *	Called when the Settings SoftSelect check box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsSoftSelect(wxCommandEvent& In)
{
	SaveData();
}

/**
 *	Called when the Settings Constrained check box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsConstrained(wxCommandEvent& In)
{
	if (EdMode == NULL)
	{
		return;
	}

	if (ConstrainedTool->IsChecked())
	{
		EdMode->bConstrained = TRUE;
		debugf(TEXT("Constraining ENABLED"));
	}
	else
	{
		EdMode->bConstrained = FALSE;
		debugf(TEXT("Constraining DISABLED"));
	}
	EditorOptions->bConstrainedEditing = EdMode->bConstrained;
	SaveData();
}

/**
 *	Called when a Settings Slider is changed
 *
 *	@param	In	wxScrollEvent 
 */
void WxTerrainEditor::OnSettingsSlider(wxScrollEvent& In)
{
    wxSlider* InSlider = wxDynamicCast(In.GetEventObject(), wxSlider);
    if (InSlider)
	{
		EToolSettings Tool = ETETS_MAX;
		switch (InSlider->GetId())
		{
		case ID_TERRAIN_SETTINGS_SLIDER_STRENGTH:	Tool = ETETS_Strength;	break;
		case ID_TERRAIN_SETTINGS_SLIDER_RADIUS:		Tool = ETETS_Radius;	break;
		case ID_TERRAIN_SETTINGS_SLIDER_FALLOFF:	Tool = ETETS_Falloff;	break;
		}

		if (Tool != ETETS_MAX)
		{
			Settings[Tool].Text->SetValue(wxString(*FString::Printf(TEXT("%d"), In.GetPosition())));

			switch (Tool)
			{
			case ETETS_Strength:	EditorOptions->Current_Strength = In.GetPosition();		break;
			case ETETS_Radius:		EditorOptions->Current_Radius = In.GetPosition();		break;
			case ETETS_Falloff:		EditorOptions->Current_Falloff = In.GetPosition();		break;
			}

			SetBrush(ETEBB_Custom);
			Refresh(TRUE);
		}
	}
}

/**
 *	Called when a Settings combo text is edited
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsText(wxCommandEvent& In)
{
    WxTextCtrl* InText = wxDynamicCast(In.GetEventObject(), WxTextCtrl);
    if (InText)
	{
		EToolSettings Tool = ETETS_MAX;
		switch(InText->GetId())
		{
		case ID_TERRAIN_SETTINGS_TEXT_STRENGTH:	Tool = ETETS_Strength;	break;
		case ID_TERRAIN_SETTINGS_TEXT_RADIUS:	Tool = ETETS_Radius;	break;
		case ID_TERRAIN_SETTINGS_TEXT_FALLOFF:	Tool = ETETS_Falloff;	break;
		}

		if (Tool != ETETS_MAX)
		{
			INT Value = appAtoi(InText->GetValue());
			Settings[Tool].Slider->SetValue(Value);
			SetBrush(ETEBB_Custom);
			if (Tool == ETETS_Strength)
			{
				EditorOptions->Current_Strength = Value;
			}
			else
			if (Tool == ETETS_Radius)
			{
				EditorOptions->Current_Radius = Value;
			}
			else 
			if (Tool == ETETS_Falloff)
			{
				EditorOptions->Current_Falloff = Value;
			}

			Refresh(TRUE);
		}
	}
}

/**
 *	Called when the Settings Mirror combo is changed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSettingsMirrorCombo(wxCommandEvent& In)
{
    wxComboBox* InCombo = wxDynamicCast(In.GetEventObject(), wxComboBox);
    if (InCombo)
	{
		FTerrainToolSettings::MirrorFlags Flags = FTerrainToolSettings::TTMirror_NONE;
		switch (InCombo->GetSelection())
		{
		case 0:		Flags = FTerrainToolSettings::TTMirror_NONE;	break;
		case 1:		Flags = FTerrainToolSettings::TTMirror_X;		break;
		case 2:		Flags = FTerrainToolSettings::TTMirror_Y;		break;
		case 3:		Flags = FTerrainToolSettings::TTMirror_XY;		break;
		}

		FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(EdMode->GetSettings());
		ToolSettings->MirrorSetting = Flags;
		EditorOptions->Current_MirrorFlag = Flags;
		SaveData();
	}
}

/** Tessellation handlers									*/
/**
 *	Called when a Tessellation button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnTessellationButton(wxCommandEvent& In)
{
	WxBitmapButton* InButton = wxDynamicCast(In.GetEventObject(), WxBitmapButton);
	if (InButton)
	{
		switch (InButton->GetId())
		{
		case ID_TERRAIN_TESSELLATE_INCREASE:
			debugf(TEXT("Tessellation Increase"));
			PatchTessellationIncrease();
			break;
		case ID_TERRAIN_TESSELLATE_DECREASE:
			debugf(TEXT("Tessellation Decrease"));
			PatchTessellationDecrease();
			break;
		}

		GUnrealEd->RedrawLevelEditingViewports();
	}
}

/** Browser handlers										*/
/**
 *	Called when the scrollbar has scrolling stop
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnBrowserScroll(wxScrollEvent& In)
{
    wxScrollBar* InScrollBar = wxDynamicCast(In.GetEventObject(), wxScrollBar);
    if (InScrollBar && (InScrollBar->GetId() == ID_TERRAIN_BROWSER_SCROLLBAR)) 
	{
		INT NewPosition = In.GetPosition();
		if (TerrainBrowserPos_Vert != NewPosition)
		{
			TerrainBrowserPos_Vert = NewPosition;
			LayerViewport->SetCanvas(0, -TerrainBrowserPos_Vert);
		}
    }
}

/** Bottom button handlers									*/
/**
 *	Called when the UseSelected button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnUseSelectedButton(wxCommandEvent& In)
{
	UseSelected();
}

/**
 *	Called when the SavePreset option is selected
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnSavePreset(wxCommandEvent& In)
{
	if ((CurrentPresetBrush >= ETEBB_Brush0) && (CurrentPresetBrush <= ETEBB_Brush9))
	{
		debugf(TEXT("Save preset in brush slot #%d"), (INT)CurrentPresetBrush);

		INT ButtonIndex = (INT)CurrentPresetBrush;

		INT	Strength	= Settings[ETETS_Strength].Slider->GetValue();
		INT	Radius		= Settings[ETETS_Radius].Slider->GetValue();
		INT	Falloff		= Settings[ETETS_Falloff].Slider->GetValue();

		BrushButtons[ButtonIndex].Strength = Strength;
		BrushButtons[ButtonIndex].Radius = Radius;
		BrushButtons[ButtonIndex].Falloff = Falloff;

		FString ToolTip = FString::Printf(TEXT("S=%d R=%d F=%d"), BrushButtons[ButtonIndex].Strength, BrushButtons[ButtonIndex].Radius, BrushButtons[ButtonIndex].Falloff);
		BrushButtons[ButtonIndex].Button->SetToolTip(wxString(*ToolTip));

		switch (CurrentPresetBrush)
		{
		case ETEBB_Brush0:
			EditorOptions->Solid1_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Solid1_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Solid1_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush1:
			EditorOptions->Solid2_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Solid2_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Solid2_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush2:
			EditorOptions->Solid3_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Solid3_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Solid3_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush3:
			EditorOptions->Solid4_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Solid4_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Solid4_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush4:
			EditorOptions->Solid5_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Solid5_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Solid5_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush5:
			EditorOptions->Noisy1_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Noisy1_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Noisy1_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush6:
			EditorOptions->Noisy2_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Noisy2_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Noisy2_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush7:
			EditorOptions->Noisy3_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Noisy3_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Noisy3_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush8:
			EditorOptions->Noisy4_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Noisy4_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Noisy4_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		case ETEBB_Brush9:
			EditorOptions->Noisy5_Strength = BrushButtons[ButtonIndex].Strength;
			EditorOptions->Noisy5_Radius = BrushButtons[ButtonIndex].Radius;
			EditorOptions->Noisy5_Falloff = BrushButtons[ButtonIndex].Falloff;
			break;
		}

		EditorOptions->SaveConfig();
	}
}

/**
 *	Called when the AddLayer button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnNewLayerButton(wxCommandEvent& In)
{
	CreateNewLayer();
}

/**
 *	Called when the AddMaterial button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnNewMaterialButton(wxCommandEvent& In)
{
	debugf(TEXT("Add Material"));

	if (CheckAddValidity(TLT_TerrainMaterial))
	{
		// Ensure that a layer is selected
		if (SelectedBlock != -1)
		{
			FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);
			if (Info->GetLayerType() == TLT_Layer)
			{
				FTerrainLayer* Layer = Info->GetLayer();
				if (Layer)
				{
					if (Layer->Setup)
					{
						BeginTransaction(TEXT("TLB_AddMaterial"));

						INT	MaterialIndex	= Layer->Setup->Materials.AddZeroed(1);
						FTerrainFilteredMaterial*	FilteredMat	= &(Layer->Setup->Materials(MaterialIndex));
						FilteredMat->Alpha	= 1.0f;

						RefreshTerrain();
						FillInfoArray();
						CurrentTerrain->MarkPackageDirty();

						EndTransaction(TEXT("TLB_AddMaterial"));
					}
					else
					{
						FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSetup"));
						appMsgf(AMT_OK, *Message);
					}
				}
			}
			else
			{
				FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSelected"));
				appMsgf(AMT_OK, *Message);
			}
		}
		else
		{
			FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoSelection"));
			appMsgf(AMT_OK, *Message);
		}
	}
}

/**
 *	Called when the AddDecoLayer button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnNewDecoLayerButton(wxCommandEvent& In)
{
	debugf(TEXT("Add DecoLayer"));

	if (CheckAddValidity(TLT_DecoLayer))
	{
		BeginTransaction(TEXT("TLB_AddDecoLayer"));

		INT	DecoLayerIndex = CurrentTerrain->DecoLayers.AddZeroed(1);
		FTerrainDecoLayer* DecoLayer = &(CurrentTerrain->DecoLayers(DecoLayerIndex));
		DecoLayer->AlphaMapIndex	= INDEX_NONE;

		RefreshTerrain();
		FillInfoArray();
		CurrentTerrain->MarkPackageDirty();

		EndTransaction(TEXT("TLB_AddDecoLayer"));
	}
}

/**
 *	Called when the DeleteSelected button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnDeleteSelectedButton(wxCommandEvent& In)
{
	debugf(TEXT("Delete Selected"));

	DeleteSelected();
}

/**
 *	Called when the ShiftLayerUp button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnShiftLayerUpButton(wxCommandEvent& In)
{
	debugf(TEXT("Shift layer up"));

	ShiftSelected(TRUE);
}

/**
 *	Called when the ShiftLayerDown button is pressed
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnShiftLayerDownButton(wxCommandEvent& In)
{
	debugf(TEXT("Shift layer down"));

	ShiftSelected(FALSE);
}

/**
 *	Called when the RetainAlpha check box is hit
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnRetainAlpha(wxCommandEvent& In)
{
	//@todo. Do we need this?
}

/**
 *	Called when the right-click menu 'Rename' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnRenameItem(wxCommandEvent& In)
{
	debugf(TEXT("Rename item"));

	FTerrainBrowserBlock*	Info	= &InfoBlocks(CurrentRightClickIndex);
	if (!Info || !Info->IsRenamable())
	{
		return;
	}

	// Grab the current name
	FString	CurrentName(TEXT(""));

	switch (Info->GetLayerType())
	{
	case TLT_Layer:
		{
			FTerrainLayer*	Layer	= Info->GetLayer();
			CurrentName				= Layer->Name;
		}
		break;
	case TLT_DecoLayer:
		{
			FTerrainDecoLayer*	DecoLayer	= Info->GetDecoLayer();
			CurrentName						= DecoLayer->Name;
		}
		break;
	default:
		return;
	}

	WxDlgGenericStringEntry dlg;
	if (dlg.ShowModal(TEXT("TerrainBrowser_Rename"), TEXT("Name"), *CurrentName) == wxID_OK)
	{
		FString	newName = FString(*(dlg.GetEnteredString()));

		debugf(TEXT("Rename to %s"), *newName);

		switch (Info->GetLayerType())
		{
		case TLT_Layer:
			{
				FTerrainLayer*	Layer	= Info->GetLayer();
				Layer->Name	= newName;
			}
			break;
		case TLT_DecoLayer:
			{
				FTerrainDecoLayer*	DecoLayer	= Info->GetDecoLayer();
				DecoLayer->Name		= newName;
			}
			break;
		}

		RefreshTerrain();
		if (CurrentTerrain)
		{
			CurrentTerrain->MarkPackageDirty();
		}
	}
}

/**
 *	Called when the right-click menu 'Change Color' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnChangeColor(wxCommandEvent& In)
{
	if (CurrentRightClickIndex == -1)
	{
		return;
	}

	debugf(TEXT("Change color!"));

	FTerrainBrowserBlock* Info = &InfoBlocks(CurrentRightClickIndex);
	if (!Info || !Info->GetOverlayable())
	{
		return;
	}

	FColor OldColor;
	switch (eCurrentRightClick)
	{
	case FTerrainLayerViewportClient::TLVCI_Wireframe:
		{
			OldColor = Info->GetWireframeColor();
		}
		break;
	case FTerrainLayerViewportClient::TLVCI_Solid:
		{
			OldColor = Info->GetHighlightColor();
		}
		break;
	}
	
	// Get the current color and show a color picker dialog.
	FPickColorStruct PickColorStruct;
	PickColorStruct.RefreshWindows.AddItem(this);
	PickColorStruct.DWORDColorArray.AddItem(&OldColor);
	PickColorStruct.bModal = TRUE;

	if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
	{
		OldColor.A = 64;
	}
	FColor NewColor = OldColor;

	UBOOL bRefreshTerrain = FALSE;
	UBOOL bRecacheMaterials = FALSE;

	switch (eCurrentRightClick)
	{
	case FTerrainLayerViewportClient::TLVCI_Wireframe:
		{
			Info->SetWireframeColor(NewColor);

			FTerrainLayer* Layer	= Info->GetLayer();
			Layer->WireframeColor	= NewColor;

			bRefreshTerrain	= TRUE;
		}
		break;
	case FTerrainLayerViewportClient::TLVCI_Solid:
		{
			Info->SetHighlightColor(NewColor);

			FTerrainLayer* Layer	= Info->GetLayer();
			Layer->HighlightColor	= NewColor;

			bRefreshTerrain	= TRUE;
			if (Info->GetDrawShadeOverlay() == TRUE)
			{
				bRecacheMaterials = TRUE;
			}
		}
		break;
	}

	if (bRefreshTerrain)
	{
		check(EdMode);
		if (CurrentTerrain)
		{
			CurrentTerrain->PostEditChange();
			if (bRecacheMaterials)
			{
				CurrentTerrain->RecacheMaterials();
			}
		}
	}
}

/**
 *	Called when the right-click menu 'Create New' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnCreate(wxCommandEvent& In)
{
	debugf(TEXT("Create New"));

	if ((CurrentTerrain == NULL) || (CurrentRightClickIndex == -1))
	{
		return;
	}

	FTerrainBrowserBlock*	Info	= &InfoBlocks(CurrentRightClickIndex);
	if (!Info)
	{
		return;
	}

	UClass* FactoryClass = NULL;
	
	switch (Info->GetLayerType())
	{
	case TLT_Layer:
		{
			debugf(TEXT("Create Called for LAYER!"));
			FactoryClass = UTerrainLayerSetupFactoryNew::StaticClass();
		}
		break;
	case TLT_FilteredMaterial:
	case TLT_TerrainMaterial:
		{
			debugf(TEXT("Create Called for TERRAIN MATERIAL!"));
			FactoryClass = UTerrainMaterialFactoryNew::StaticClass();
		}
		break;
	default:
		return;
	}

	if (!FactoryClass)
	{
		appMsgf(AMT_OK, TEXT("Invalid factory class! TODO. Localize me!!!!"));
		return;
	}

	check(FactoryClass->IsChildOf(UFactory::StaticClass()));

	WxDlgNewGeneric dlg;

	UPackage* pkg = NULL;
	UPackage* grp = NULL;

	if (dlg.ShowModal(TEXT("MyPackage"), TEXT(""), FactoryClass) == wxID_OK)
	{
		if (Info->GetParentBlock())
		{
			SetSelectedChildBlock(CurrentRightClickIndex);
		}
		else
		{
			SetSelectedBlock(CurrentRightClickIndex);
		}

		wxCommandEvent DummyEvent;
		OnUseSelected(DummyEvent);
		RefreshTerrain();
		CurrentTerrain->MarkPackageDirty();
		Update();
	}
}

/**
 *	Called when the right-click menu 'Delete' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnDelete(wxCommandEvent& In)
{
	debugf(TEXT("Delete"));

	if (CurrentTerrain == NULL)
	{
		return;
	}

	UBOOL					bRefreshTerrain	= FALSE;
	FTerrainBrowserBlock*	ParentInfo		= NULL;
	FTerrainBrowserBlock*	ClickedInfo		= NULL;

	if (CurrentRightClickIndex != -1)
	{
		// Make sure the RightClick block is the selected one...
		SetSelectedBlock(CurrentRightClickIndex);
		// Delete it
		DeleteSelected();
		CurrentRightClickIndex = -1;
	}
}

/**
 *	Called when the right-click menu 'Use Selected' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnUseSelected(wxCommandEvent& In)
{
	debugf(TEXT("Use Selected"));

	UseSelected();
}

/**
 *	Called when the right-click menu 'New Layer' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnNewLayer(wxCommandEvent& In)
{
	// Are we adding it before, after, or at the end?
	INT	IndexOffset	= -1;
	if (In.GetId() == IDM_TERRAINBROWSER_NEWLAYER_BEFORE)
	{
		IndexOffset = 0;
	}
	else if (In.GetId() == IDM_TERRAINBROWSER_NEWLAYER_AFTER)
	{
		IndexOffset = 1;
	}

	CreateNewLayer(IndexOffset);
}

/** Creates a new terrain layer. */
void WxTerrainEditor::CreateNewLayer(INT IndexOffset)
{
	debugf(TEXT("New Layer"));

	if (CheckAddValidity(TLT_Layer))
	{
		FString	newName;
		UTerrainLayerSetup* NewLayerSetup = CreateNewLayer(newName);
		if (NewLayerSetup == NULL)
		{
			return;
		}

		BeginTransaction(TEXT("TLB_AddLayer"));

		INT	LayerIndex = -1;
		FTerrainLayer* ClickedLayer = NULL;

		if (CurrentRightClickIndex != -1)
		{
			// Determine the index of the clicked layer
			FTerrainBrowserBlock* ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
			if (ClickedInfo)
			{
				ClickedLayer = ClickedInfo->GetLayer();
				if (ClickedLayer)
				{
					LayerIndex = ClickedInfo->GetArrayIndex();
				}
				else
				{
					IndexOffset = -1;
				}
			}
		}
		else
		{
			IndexOffset = -1;
		}

		if (IndexOffset == -1)
		{
			LayerIndex = CurrentTerrain->Layers.AddZeroed(1);
		}
		else
		{
			// Insert the layer at the given index
			CurrentTerrain->Layers.InsertZeroed(LayerIndex + IndexOffset, 1);
			LayerIndex = LayerIndex + IndexOffset;
		}

		FTerrainLayer* Layer	= &(CurrentTerrain->Layers(LayerIndex));
		Layer->Name				= newName;
		Layer->AlphaMapIndex	= INDEX_NONE;
		Layer->Setup			= NewLayerSetup;
		Layer->HighlightColor	= FColor(255, 0, 0);
		RefreshTerrain();

		EndTransaction(TEXT("TLB_AddLayer"));

		FillInfoArray();
		CurrentTerrain->MarkPackageDirty();
		// If it's the base layer, then recache materials!
		if (LayerIndex == 0)
		{
			CurrentTerrain->RecacheMaterials();
		}
	}
}

/** Creates a new terrain layer material */
void WxTerrainEditor::CreateNewMaterial(INT IndexOffset)
{
	debugf(TEXT("New Material"));

	if (CheckAddValidity(TLT_TerrainMaterial))
	{
		// Ensure that a layer is selected
		if (SelectedBlock != -1)
		{
			FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);
			if (Info->GetLayerType() == TLT_Layer)
			{
				FTerrainLayer* Layer = Info->GetLayer();
				if (Layer)
				{
					if (Layer->Setup)
					{
						FString TMATName(TEXT(""));
						UTerrainMaterial* NewMaterial = CreateNewTerrainMaterial(TMATName);
						if (NewMaterial)
						{
							INT	MaterialIndex = -1;
							FTerrainFilteredMaterial* ClickedMaterial = NULL;

							// Determine the index of the clicked layer
							if (CurrentRightClickIndex != -1)
							{
								FTerrainBrowserBlock* ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
								if (ClickedInfo)
								{
									ClickedMaterial = ClickedInfo->GetFilteredMaterial();
									if (ClickedMaterial)
									{
										MaterialIndex = ClickedInfo->GetArrayIndex();
									}
									else
									{
										IndexOffset = -1;
									}
								}
							}
							else
							{
								IndexOffset = -1;
							}

							BeginTransaction(TEXT("TLB_AddMaterial"));

							if (IndexOffset == -1)
							{
								MaterialIndex = Layer->Setup->Materials.AddZeroed(1);
							}
							else
							{
								Layer->Setup->Materials.InsertZeroed(MaterialIndex + IndexOffset);
								MaterialIndex += IndexOffset;
							}

							FTerrainFilteredMaterial*	FilteredMat	= &(Layer->Setup->Materials(MaterialIndex));
							FilteredMat->Alpha		= 1.0f;
							FilteredMat->Material	= NewMaterial;

							RefreshTerrain();

							EndTransaction(TEXT("TLB_AddMaterial"));

							FillInfoArray();
							CurrentTerrain->MarkPackageDirty();
						}
					}
					else
					{
						FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSetup"));
						appMsgf(AMT_OK, *Message);
					}
				}
			}
			else
			{
				FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSelected"));
				appMsgf(AMT_OK, *Message);
			}
		}
		else
		{
			FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoSelection"));
			appMsgf(AMT_OK, *Message);
		}
	}
}

/**
 *	Called when the auto-create layer option is selected
 *
 *	@param	In	wxCommandEvent 
 */
void WxTerrainEditor::OnNewLayerAuto(wxCommandEvent& In)
{
	debugf(TEXT("New Layer Auto"));

	if (CheckAddValidity(TLT_Layer))
	{
		UBOOL bPromptForPackage = FALSE;
		if (In.GetId() == IDM_TERRAINBROWSER_NEWLAYER_AUTO_SELECT)
		{
			bPromptForPackage = TRUE;
		}

		void* SelectedItem = NULL;
		INT GetResult = GetSelectedItem(TLT_TerrainMaterial, SelectedItem);
		if (GetResult > 0)
		{
			UTerrainMaterial*			ObjTerrainMaterial	= NULL;
			UMaterial*					ObjMaterial			= NULL;
			UMaterialInstanceConstant*	ObjMaterialInst		= NULL;

			UObject* Object = (UObject*)SelectedItem;
			if (Object == NULL)
			{
				return;
			}

			FString	TLSName = FString(TEXT("TLS_"));
			FString	TMATName = FString(TEXT("TMAT_"));
			if (Object->GetClass() == UTerrainMaterial::StaticClass())
			{
				ObjTerrainMaterial = Cast<UTerrainMaterial>(Object);
				TLSName += ObjTerrainMaterial->GetName();
				TMATName += ObjTerrainMaterial->GetName();
			}
			else
			if (Object->GetClass() == UMaterial::StaticClass())
			{
				ObjMaterial = Cast<UMaterial>(Object);
				TLSName += ObjMaterial->GetName();
				TMATName += ObjMaterial->GetName();
			}
			else
			if (Object->GetClass() == UMaterialInstanceConstant::StaticClass())
			{
				ObjMaterialInst = Cast<UMaterialInstanceConstant>(Object);
				TLSName += ObjMaterialInst->GetName();
				TMATName += ObjMaterialInst->GetName();
			}
			else
			{
				return;
			}

			BeginTransaction(TEXT("TLB_AddLayer_Auto"));

			FString	PackageName(TEXT(""));
			FString	GroupName(TEXT(""));

			UPackage* InsertIntoPackage = NULL;
			if (bPromptForPackage == TRUE)
			{
				UBOOL bResult = FALSE;

				FString	ObjectName(TEXT(""));

				WxDlgPackageGroupName PGNDlg;
				if (PGNDlg.ShowModal(PackageName, GroupName, ObjectName, WxDlgPackageGroupName::PGN_NoName ) == wxID_OK)
				{
					PackageName	= PGNDlg.GetPackage();
					GroupName	= PGNDlg.GetGroup();
					ObjectName	= PGNDlg.GetObjectName();

					debugf(TEXT("NewLayer: %s.%s.%s"), *PackageName, *GroupName, *ObjectName);

					UPackage* BasePackage = UObject::FindPackage(NULL, *PackageName);
					if (BasePackage == NULL)
					{
						FString	CreatePackagePrompt = FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainEdit_CreatePackage"), *PackageName));
						if (appMsgf(AMT_YesNo, *CreatePackagePrompt) == FALSE)
						{
							return;
						}

						BasePackage = UObject::CreatePackage(NULL,*PackageName);
						if (BasePackage == NULL)
						{
							appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_FailedCreatePackage"));
							return;
						}
					}

					if (GroupName.Len() > 0)
					{
						InsertIntoPackage = UObject::FindPackage(BasePackage, *GroupName);
						if (InsertIntoPackage == NULL)
						{
							// Create the package
							InsertIntoPackage = UObject::CreatePackage(BasePackage,*GroupName);
						}
					}
					else
					{
						InsertIntoPackage = BasePackage;
					}
				}
			}

			UTerrainLayerSetup* NewLayerSetup = CreateNewLayer(TLSName, InsertIntoPackage, FALSE);
			if (NewLayerSetup == NULL)
			{
				warnf(TEXT("Failed to create layer..."));
				EndTransaction(TEXT("TLB_AddLayer_Auto"));
				return;
			}

			// Are we adding it before, after, or at the end?
			INT	LayerIndex = -1;
			FTerrainLayer* ClickedLayer = NULL;

			LayerIndex = CurrentTerrain->Layers.AddZeroed(1);

			FTerrainLayer* Layer	= &(CurrentTerrain->Layers(LayerIndex));
			Layer->Name				= TLSName;
			Layer->AlphaMapIndex	= INDEX_NONE;
			Layer->Setup			= NewLayerSetup;

			INT MaterialIndex = Layer->Setup->Materials.AddZeroed(1);

			FTerrainFilteredMaterial*	FilteredMat	= &(Layer->Setup->Materials(MaterialIndex));
			FilteredMat->Alpha		= 1.0f;

			if (ObjMaterial || ObjMaterialInst)
			{
				// Generate a new terrain material.
				UTerrainMaterial* NewMaterial = CreateNewTerrainMaterial(TMATName, InsertIntoPackage, FALSE);
				if (NewMaterial)
				{
					FilteredMat->Material	= NewMaterial;

					if (ObjMaterial)
					{
						NewMaterial->Material = ObjMaterial;
					}
					else
					if (ObjMaterialInst)
					{
						NewMaterial->Material = ObjMaterialInst;
					}
				}
			}
			else
			{
				FilteredMat->Material	= ObjTerrainMaterial;
			}

			RefreshTerrain();
			EndTransaction(TEXT("TLB_AddLayer_Auto"));
			FillInfoArray();

			NewLayerSetup->MarkPackageDirty();

			const DWORD UpdateMask = CBR_ObjectCreated|CBR_SyncAssetView;
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, NewLayerSetup));

			// If it's the base layer, then recache materials!
			if (LayerIndex == 0)
			{
				CurrentTerrain->RecacheMaterials();
			}
		}
		else
		{
			FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoSelection"));
			appMsgf(AMT_OK, *Message);
		}
	}
}

/**
 *	Called when the right-click menu 'New Material' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnNewMaterial(wxCommandEvent& In)
{
	// Are we adding it before, after, or at the end?
	INT	IndexOffset	= -1;
	if (In.GetId() == IDM_TERRAINBROWSER_NEWMATERIAL_BEFORE)
	{
		IndexOffset = 0;
	}
	else if (In.GetId() == IDM_TERRAINBROWSER_NEWMATERIAL_AFTER)
	{
		IndexOffset = 1;
	}

	CreateNewMaterial(IndexOffset);
}

/**
 *	Called when the right-click menu 'New DecoLayer' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnNewDecoLayer(wxCommandEvent& In)
{
	debugf(TEXT("New DecoLayer"));

	if (CheckAddValidity(TLT_DecoLayer))
	{
		FString	CurrentName(TEXT("NewDecoLayer"));
		WxDlgGenericStringEntry dlg;
		if (dlg.ShowModal(TEXT("TerrainBrowser_Rename"), TEXT("Name"), *CurrentName) != wxID_OK)
		{
			return;
		}

		FString	newName = FString(*(dlg.GetEnteredString()));

		// Are we adding it before, after, or at the end?
		INT	IndexOffset	= -1;
		if (In.GetId() == IDM_TERRAINBROWSER_NEWDECOLAYER_BEFORE)
		{
			IndexOffset = 0;
		}
		else
		if (In.GetId() == IDM_TERRAINBROWSER_NEWDECOLAYER_AFTER)
		{
			IndexOffset = 1;
		}

		BeginTransaction(TEXT("TLB_AddDecoLayer"));

		INT	DecoLayerIndex = -1;
		FTerrainDecoLayer* ClickedDecoLayer = NULL;

		if (CurrentRightClickIndex != -1)
		{
			// Determine the index of the clicked layer
			FTerrainBrowserBlock* ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
			if (ClickedInfo)
			{
				ClickedDecoLayer = ClickedInfo->GetDecoLayer();
				if (ClickedDecoLayer)
				{
					DecoLayerIndex = ClickedInfo->GetArrayIndex();
				}
				else
				{
					IndexOffset = -1;
				}
			}
		}
		else
		{
			IndexOffset = -1;
		}

		if (IndexOffset == -1)
		{
			DecoLayerIndex = CurrentTerrain->DecoLayers.AddZeroed(1);
		}
		else
		{
			// Insert the layer at the given index
			CurrentTerrain->DecoLayers.InsertZeroed(DecoLayerIndex + IndexOffset, 1);
			DecoLayerIndex += IndexOffset;
		}

		FTerrainDecoLayer* DecoLayer	= &(CurrentTerrain->DecoLayers(DecoLayerIndex));
		DecoLayer->Name					= newName;
		DecoLayer->AlphaMapIndex		= INDEX_NONE;
		RefreshTerrain();

		EndTransaction(TEXT("TLB_AddDecoLayer"));

		FillInfoArray();
		CurrentTerrain->MarkPackageDirty();
	}
}

/**
 *	Called when the right-click menu 'New Decoration' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnNewDecoration(wxCommandEvent& In)
{
	debugf(TEXT("New Decoration"));
}

/**
 *	Called when the right-click menu 'Add Current Layer' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnAddSelectedLayer(wxCommandEvent& In)
{
	debugf(TEXT("AddSelected Layer"));

	if (CurrentTerrain == NULL)
	{
		return;
	}

	if (CheckAddValidity(TLT_Layer))
	{
		void* SelectedItem = NULL;

		INT GetResult = GetSelectedItem(TLT_Layer, SelectedItem);
		if (GetResult > 0)
		{
			UTerrainLayerSetup* LayerSetup = (UTerrainLayerSetup*)SelectedItem;
			if (LayerSetup == NULL)
			{
				return;
			}

			// Are we adding it before, after, or at the end?
			INT	IndexOffset	= -1;
			if (In.GetId() == IDM_TERRAINBROWSER_ADDSELECTED_LAYER_BEFORE)
			{
				IndexOffset = 0;
			}
			else
			if (In.GetId() == IDM_TERRAINBROWSER_ADDSELECTED_LAYER_AFTER)
			{
				IndexOffset = 1;
			}

			BeginTransaction(TEXT("TLB_AddLayer"));

			INT	LayerIndex = -1;
			FTerrainLayer* ClickedLayer = NULL;

			if (CurrentRightClickIndex != -1)
			{
				// Determine the index of the clicked layer
				FTerrainBrowserBlock* ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
				if (ClickedInfo)
				{
					ClickedLayer = ClickedInfo->GetLayer();
					if (ClickedLayer)
					{
						LayerIndex = ClickedInfo->GetArrayIndex();
					}
					else
					{
						IndexOffset = -1;
					}
				}
			}
			else
			{
				IndexOffset = -1;
			}

			if (IndexOffset == -1)
			{
				LayerIndex = CurrentTerrain->Layers.AddZeroed(1);
			}
			else
			{
				// Insert the layer at the given index
				CurrentTerrain->Layers.InsertZeroed(LayerIndex + IndexOffset, 1);
				LayerIndex = LayerIndex + IndexOffset;
			}

			FTerrainLayer* Layer	= &(CurrentTerrain->Layers(LayerIndex));
			Layer->Name				= LayerSetup->GetName();
			Layer->AlphaMapIndex	= INDEX_NONE;
			Layer->Setup			= LayerSetup;

			RefreshTerrain();

			EndTransaction(TEXT("TLB_AddLayer"));

			FillInfoArray();
			CurrentTerrain->MarkPackageDirty();
		}
	}
}

/**
 *	Called when the right-click menu 'Add Current Material' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnAddSelectedMaterial(wxCommandEvent& In)
{
	debugf(TEXT("AddSelected Material"));

	if (CheckAddValidity(TLT_TerrainMaterial))
	{
		void* SelectedItem = NULL;

		INT GetResult = GetSelectedItem(TLT_TerrainMaterial, SelectedItem);
		if (GetResult > 0)
		{
			UTerrainMaterial*			ObjTerrainMaterial	= NULL;
			UMaterial*					ObjMaterial			= NULL;
			UMaterialInstanceConstant*	ObjMaterialInst		= NULL;

			UObject* Object = (UObject*)SelectedItem;
			if (Object == NULL)
			{
				return;
			}

			if (Object->GetClass() == UTerrainMaterial::StaticClass())
			{
				ObjTerrainMaterial = Cast<UTerrainMaterial>(Object);
			}
			else
			if (Object->GetClass() == UMaterial::StaticClass())
			{
				ObjMaterial = Cast<UMaterial>(Object);
			}
			else
			if (Object->GetClass() == UMaterialInstanceConstant::StaticClass())
			{
				ObjMaterialInst = Cast<UMaterialInstanceConstant>(Object);
			}
			else
			{
				return;
			}

			// Ensure that a layer is selected
			if (SelectedBlock != -1)
			{
				FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);
				if (Info->GetLayerType() == TLT_Layer)
				{
					FTerrainLayer* Layer = Info->GetLayer();
					if (Layer)
					{
						if (Layer->Setup)
						{
							// Are we adding it before, after, or at the end?
							INT	IndexOffset	= -1;
							if (In.GetId() == IDM_TERRAINBROWSER_NEWMATERIAL_BEFORE)
							{
								IndexOffset = 0;
							}
							else
							if (In.GetId() == IDM_TERRAINBROWSER_NEWMATERIAL_AFTER)
							{
								IndexOffset = 1;
							}
							
							INT	MaterialIndex = -1;
							FTerrainFilteredMaterial* ClickedMaterial = NULL;
							// Determine the index of the clicked layer
							FTerrainBrowserBlock* ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
							if (ClickedInfo)
							{
								ClickedMaterial = ClickedInfo->GetFilteredMaterial();
								if (ClickedMaterial)
								{
									MaterialIndex = ClickedInfo->GetArrayIndex();
								}
								else
								{
									IndexOffset = -1;
								}
							}

							BeginTransaction(TEXT("TLB_AddMaterial"));
							
							if (IndexOffset == -1)
							{
								MaterialIndex = Layer->Setup->Materials.AddZeroed(1);
							}
							else
							{
								Layer->Setup->Materials.InsertZeroed(MaterialIndex + IndexOffset);
								MaterialIndex += IndexOffset;
							}

							FTerrainFilteredMaterial*	FilteredMat	= &(Layer->Setup->Materials(MaterialIndex));
							FilteredMat->Alpha		= 1.0f;

							if (ObjMaterial || ObjMaterialInst)
							{
								FString TMATName(TEXT(""));
								// Generate a new terrain material.
								UTerrainMaterial* NewMaterial = CreateNewTerrainMaterial(TMATName);
								if (NewMaterial)
								{
									FilteredMat->Material	= NewMaterial;

									if (ObjMaterial)
									{
										NewMaterial->Material = ObjMaterial;
									}
									else
									if (ObjMaterialInst)
									{
										NewMaterial->Material = ObjMaterialInst;
									}
								}
							}
							else
							{
								FilteredMat->Material	= ObjTerrainMaterial;
							}

							RefreshTerrain();

							EndTransaction(TEXT("TLB_AddMaterial"));

							FillInfoArray();
							CurrentTerrain->MarkPackageDirty();
						}
					}
					else
					{
						FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSetup"));
						appMsgf(AMT_OK, *Message);
					}
				}
			}
			else
			{
				FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoLayerSelected"));
				appMsgf(AMT_OK, *Message);
			}
		}
		else
		{
			FString	Message	= FString::Printf(TEXT("%s: %s"), *LocalizeUnrealEd("TerrainBrowser_AddMaterial"), *LocalizeUnrealEd("TerrainBrowser_NoSelection"));
			appMsgf(AMT_OK, *Message);
		}
	}
}

/**
 *	Called when the right-click menu 'Add Current Material (auto-create)' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnAddSelectedMaterialAuto(wxCommandEvent& In)
{
	debugf(TEXT("AddSelected Material Auto"));

}

/**
 *	Called when the right-click menu 'Add Current Displacement' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnAddSelectedDisplacement(wxCommandEvent& In)
{
	debugf(TEXT("AddSelected Displacement"));

	// We need to have a terrain material right-clicked...
	INT	LayerIndex = -1;
	FTerrainBrowserBlock* ClickedInfo = NULL;
	FTerrainFilteredMaterial* ClickedMaterial = NULL;
	UTerrainMaterial* TerrainMaterial = NULL;

	if (CurrentRightClickIndex != -1)
	{
		// Determine the index of the clicked layer
		ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
		if (ClickedInfo && (ClickedInfo->GetLayerType() == TLT_FilteredMaterial))
		{
			ClickedMaterial = ClickedInfo->GetFilteredMaterial();
			if (ClickedMaterial)
			{
				TerrainMaterial = ClickedMaterial->Material;
			}
		}
	}

	if (TerrainMaterial == NULL)
	{
		//@todo. Warn user...
		return;
	}

	void* SelectedItem = NULL;

	INT GetResult = GetSelectedItem(TLT_DisplacementMap, SelectedItem);
	if (GetResult > 0)
	{
		UObject* Object = (UObject*)SelectedItem;
		if (Object && Object->IsA(UTexture2D::StaticClass()))
		{
			UTexture2D* DisplacementTexture = Cast<UTexture2D>(Object);
			if (DisplacementTexture)
			{
				TerrainMaterial->DisplacementMap = DisplacementTexture;
				ClickedInfo->SetDrawThumbnail2(TRUE);
				CurrentTerrain->MarkPackageDirty();
			}
		}
	}
}

/**
 *	Called when the right-click menu 'Add Current Decoration' option is selected
 *
 *	@param	In	wxCommandEvent
 */
void WxTerrainEditor::OnAddSelectedDecoration(wxCommandEvent& In)
{
	debugf(TEXT("AddSelected Decoration"));

	// We need to have a terrain material right-clicked...
	INT	LayerIndex = -1;
	FTerrainBrowserBlock* ClickedInfo = NULL;
	FTerrainDecoLayer* ClickedDecoLayer = NULL;

	if (CurrentRightClickIndex != -1)
	{
		// Determine the index of the clicked layer
		ClickedInfo = &InfoBlocks(CurrentRightClickIndex);
		if (ClickedInfo && (ClickedInfo->GetLayerType() == TLT_DecoLayer))
		{
			ClickedDecoLayer = ClickedInfo->GetDecoLayer();
		}
	}

	if (ClickedDecoLayer == NULL)
	{
		//@todo. Warn user...
		return;
	}

	void* SelectedItem = NULL;

	INT GetResult = GetSelectedItem(TLT_Decoration, SelectedItem);
	if (GetResult > 0)
	{
		UObject* Object = (UObject*)SelectedItem;
		if (Object && Object->IsA(UStaticMesh::StaticClass()))
		{
			UStaticMesh* Mesh = Cast<UStaticMesh>(Object);
			if (Mesh)
			{
				INT NewIndex = ClickedDecoLayer->Decorations.AddZeroed();

				FTerrainDecoration* Decoration = &(ClickedDecoLayer->Decorations(NewIndex));
				if (Decoration)
				{
					UStaticMeshComponentFactory* Factory = ConstructObject<UStaticMeshComponentFactory>(
						UStaticMeshComponentFactory::StaticClass(), CurrentTerrain);
					Factory->StaticMesh	= Mesh;
					Decoration->Factory	= Factory;
					RefreshTerrain();
					FillInfoArray();
					CurrentTerrain->MarkPackageDirty();
				}
			}
		}
	}
}

/** Helper functions										*/
/**
 *	Creates all the required controls
 *
 *	@return	TRUE	if successful
 *			FALSE	if not
 */
bool WxTerrainEditor::CreateControls()
{
	if (GenerateControls())
	{
		if (PlaceControls())
		{
			if (CreateLayerViewport())
			{
				Update();
//				FLocalizeWindow(this);
				return TRUE;
			}
		}
	}

	return FALSE;
}

bool WxTerrainEditor::GenerateControls()
{
	INT			Index;
    wxString*	ComboBoxStrings = NULL;
	WxBitmap*	TempBitmap;

	/** Members													*/
//	CurrentTool	= ETETB_Paint;
//	FModeTool*	NewTool	= GetTool(CurrentTool);
//	EdMode->SetCurrentTool(NewTool);

	/** Tool Controls											*/
	wxImage	TempImage;
	wxImage	Image_Active;
	wxImage	Image_Inactive;
	FString	ToolTip;
	UBOOL	bActive = FALSE;

	for (Index = 0; Index < ETETB_MAX; Index++)
	{
		// If there is a tool, set the button active
		FModeTool* Tool = WxTerrainEditor::GetTool((EToolButton)Index);
		bActive = Tool ? TRUE : FALSE;

		switch (Index)
		{
		case ETETB_AddRemoveSectors:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_AddRemoveSectors.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_AddRemoveSectors_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_AddRemoveSectors"));
			break;
		case ETETB_AddRemovePolys:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_AddRemovePolys.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_AddRemovePolys_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_AddRemovePolys"));
			break;
		case ETETB_Paint:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Paint.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Paint_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Paint"));
			break;
		case ETETB_PaintVertex:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_PaintVertex.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_PaintVertex_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_PaintVertex"));
			break;
		case ETETB_ManualEdit:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_ManualEdit.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_ManualEdit_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_ManualEdit"));
			break;
		case ETETB_Flatten:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Flatten.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Flatten_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Flatten"));
			break;
		case ETETB_FlattenSpecific:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_FlattenSpecific.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_FlattenSpecific_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_FlattenSpecific"));
			break;
		case ETETB_Smooth:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Smooth.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Smooth_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Smooth"));
			break;
		case ETETB_Average:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Average.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Average_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Average"));
			break;
		case ETETB_Noise:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noise.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noise_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip			= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noise"));
			break;
		case ETETB_Visibility:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Visibility.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Visibility_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Visibility"));
			break;
		case ETETB_TexturePan:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TexturePan.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TexturePan_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_TexturePan"));
			break;
		case ETETB_TextureRotate:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TextureRotate.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TextureRotate_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_TextureRotate"));
			break;
		case ETETB_TextureScale:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TextureScale.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_TextureScale_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_TextureScale"));
			break;
		case ETETB_VertexLock:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_LockVertex.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_LockVertex_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_VertexLock"));
			break;
		case ETETB_MarkUnreachable:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_MarkUnreachable.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_MarkUnreachable_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_MarkUnreachable"));
			break;
		case ETETB_SplitX:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_SplitX.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_SplitX_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_SplitX"));
			break;
		case ETETB_SplitY:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_SplitY.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_SplitY_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_SplitY"));
			break;
		case ETETB_Merge:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Merge.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Merge_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Merge"));
			break;
		case ETETB_OrientationFlip:
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Orientation.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Orientation_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip	= *FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Orientation"));
			break;
		}

		ToolButtons[Index].Active	= new WxBitmap(Image_Active);
		ToolButtons[Index].Inactive	= new WxBitmap(Image_Inactive);
		ToolButtons[Index].Button	= new WxBitmapButton(this, ID_TERRAIN_TOOL_0 + Index, *ToolButtons[Index].Inactive, wxDefaultPosition, wxSize(32, 32));
		ToolButtons[Index].Button->SetBitmapLabel(*ToolButtons[Index].Inactive);
		ToolButtons[Index].Button->SetToolTip(wxString(*ToolTip));
		ToolButtons[Index].Button->Enable(bActive == TRUE);
	}

	SpecificValue = new wxSpinCtrl(this, ID_TERRAIN_TOOL_SPECIFIC_VALUE, _("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0);

	/** Import/Export											*/
	ImportButton = new wxButton(this, ID_TERRAIN_IMPEXP_IMPORT, *LocalizeUnrealEd("Import"), wxDefaultPosition, wxDefaultSize, 0 );

	HeightMapOnly = new wxCheckBox(this, ID_TERRAIN_IMPEXP_HEIGHTMAP_ONLY, *LocalizeUnrealEd("TerrainEdit_HeightMapOnly"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    HeightMapOnly->SetValue(FALSE);

	RetainCurrentTerrain = new wxCheckBox(this, ID_TERRAIN_IMPEXP_RETAIN_CURRENT_TERRAIN, *LocalizeUnrealEd("TerrainEdit_RetainCurrentTerrain"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	RetainCurrentTerrain->SetValue(FALSE);

	ExportButton = new wxButton(this, ID_TERRAIN_IMPEXP_EXPORT, *LocalizeUnrealEd("Export"), wxDefaultPosition, wxDefaultSize, 0 );

	BakeDisplacementMap	= new wxCheckBox(this, ID_TERRAIN_IMPEXP_BAKE_DISPLACEMENT, *LocalizeUnrealEd("TerrainEdit_BakeDisplacementMap"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    BakeDisplacementMap->SetValue(FALSE);

	HeightmapClassLabel	= new wxStaticText(this, wxID_STATIC, *LocalizeUnrealEd("TerrainEdit_HeightmapClassLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	HeightmapClass = new wxComboBox(this, ID_TERRAIN_IMPEXP_CLASS, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, ComboBoxStrings, wxCB_DROPDOWN);

	/** ViewSettings Controls									*/
	ViewSettingsTerrainComboLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_TERRAIN, *LocalizeUnrealEd("TerrainEdit_CurrentTerrainLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	ViewSettingsTerrainCombo = new wxComboBox(this, ID_TERRAIN_VIEWSETTINGS_COMBOBOX_TERRAIN, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, ComboBoxStrings, wxCB_READONLY);
	
	ViewSettingsTerrainPropertiesLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_PROPERTIES, *LocalizeUnrealEd("TerrainViewSettingsPropertiesLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Properties.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	ViewSettingsTerrainProperties = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_PROPERTIES, *TempBitmap, wxDefaultPosition, wxSize(24, 24));
	delete TempBitmap;
	
	ViewSettingsHideLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_HIDE, *LocalizeUnrealEd("TerrainViewSettingsViewLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Empty.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsView.Inactive = new WxBitmap(TempImage);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_View.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsView.Active = new WxBitmap(TempImage);
	ViewSettingsView.Button = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_HIDE, *ViewSettingsView.Active, wxDefaultPosition, wxSize(24, 24));
	ViewSettingsView.Button->SetBitmapLabel(*ViewSettingsView.Active);
	ViewSettingsView.Button->SetBitmapSelected(*ViewSettingsView.Active);
	ViewSettingsView.Button->SetToolTip(wxString(*LocalizeUnrealEd("TerrainViewSettingsHide_ToolTip")));

	ViewSettingsLockLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_LOCK, *LocalizeUnrealEd("TerrainViewSettingsLockLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Empty.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsLock.Inactive = new WxBitmap(TempImage);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Lock.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsLock.Active = new WxBitmap(TempImage);
	ViewSettingsLock.Button = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_LOCK, *ViewSettingsLock.Active, wxDefaultPosition, wxSize(24, 24));
	ViewSettingsLock.Button->SetBitmapLabel(*ViewSettingsLock.Inactive);
	ViewSettingsLock.Button->SetBitmapSelected(*ViewSettingsLock.Active);
	ViewSettingsLock.Button->SetToolTip(wxString(*LocalizeUnrealEd("TerrainViewSettingsLock_ToolTip")));

	ViewSettingsWireLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_OVERLAY_WIRE, *LocalizeUnrealEd("TerrainViewSettingsWireLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Wireframe.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsWire.Inactive = new WxBitmap(TempImage);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Wireframe.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsWire.Active = new WxBitmap(TempImage);
	ViewSettingsWire.Button = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIRE, *ViewSettingsWire.Active, wxDefaultPosition, wxSize(24, 24));
	ViewSettingsWire.Button->SetBitmapLabel(*ViewSettingsWire.Inactive);
	ViewSettingsWire.Button->SetBitmapSelected(*ViewSettingsWire.Active);
	ViewSettingsWire.Button->SetToolTip(wxString(*LocalizeUnrealEd("TerrainViewSettingsWire_ToolTip")));

	ViewSettingsWireframeColorLabel = new wxStaticText(this, ID_TERRAIN_VIEWSETTINGS_LABEL_OVERLAY_SOLID, *LocalizeUnrealEd("TerrainBrowser_WireframeColor"), wxDefaultPosition, wxDefaultSize, 0 );
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Solid.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsWireframeColor.Inactive = new WxBitmap(TempImage);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Solid.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	ViewSettingsWireframeColor.Active = new WxBitmap(TempImage);
	ViewSettingsWireframeColor.Button = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_OVERLAY_WIREFRAME_COLOR, *ViewSettingsWireframeColor.Active, wxDefaultPosition, wxSize(24, 24));
	ViewSettingsWireframeColor.Button->SetBitmapLabel(wxBitmap(0,0));
	ViewSettingsWireframeColor.Button->SetToolTip(wxString(*LocalizeUnrealEd("TerrainBrowser_WireframeColor")));

	if(CurrentTerrain)
	{
		ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColour(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B));
	}
	else
	{
		ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColour(0, 255, 255));
	}

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_RecacheMaterials.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	ViewSettingsRecacheMaterials = new WxBitmapButton(this, ID_TERRAIN_VIEWSETTINGS_RECACHEMATERIALS, *TempBitmap, wxDefaultPosition, wxSize(24, 24));
	ViewSettingsRecacheMaterials->SetToolTip(*LocalizeUnrealEd("TerrainBrowser_RecacheMaterials_ToolTip"));
	delete TempBitmap;

	/** Brushes													*/
	CurrentBrush	= ETEBB_Custom;
	CurrentPresetBrush = ETEBB_Custom;

	for (Index = 0; Index < ETEBB_MAX; Index++)
	{
		switch (ID_TERRAIN_BRUSH_0 + Index)
		{
		case ID_TERRAIN_BRUSH_0:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid1_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid1.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Solid1"));
			break;
		case ID_TERRAIN_BRUSH_1:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid2_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid2.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Solid2"));
			break;
		case ID_TERRAIN_BRUSH_2:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid3_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid3.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Solid3"));
			break;
		case ID_TERRAIN_BRUSH_3:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid4_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid4.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Solid4"));
			break;
		case ID_TERRAIN_BRUSH_4:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid5_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Solid5.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Solid5"));
			break;
		case ID_TERRAIN_BRUSH_5:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy1_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy1.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noisy1"));
			break;
		case ID_TERRAIN_BRUSH_6:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy2_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy2.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noisy2"));
			break;
		case ID_TERRAIN_BRUSH_7:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy3_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy3.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noisy3"));
			break;
		case ID_TERRAIN_BRUSH_8:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy4_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy4.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noisy4"));
			break;
		case ID_TERRAIN_BRUSH_9:
			Image_Active.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy5_Active.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			Image_Inactive.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEdit_Noisy5.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_Tooltip_Noisy5"));
			break;
		}

		ToolTip = FString::Printf(TEXT("S=%d R=%d F=%d"), BrushButtons[Index].Strength, BrushButtons[Index].Radius, BrushButtons[Index].Falloff);
		BrushButtons[Index].Active		= new WxBitmap(Image_Active);
		BrushButtons[Index].Inactive	= new WxBitmap(Image_Inactive);
		BrushButtons[Index].Button		= new WxBitmapButton(this, ID_TERRAIN_BRUSH_0 + Index, *BrushButtons[Index].Inactive, wxDefaultPosition, wxSize(32, 32));
		BrushButtons[Index].Button->SetBitmapLabel(*BrushButtons[Index].Inactive);
		BrushButtons[Index].Button->SetBitmapSelected(*BrushButtons[Index].Active);
		BrushButtons[Index].Button->SetToolTip(wxString(*ToolTip));
	}

	/** Settings Controls										*/
	SettingsPerTool	= new wxCheckBox(this, ID_TERRAIN_SETTINGS_PER_TOOL, *LocalizeUnrealEd("TerrainEdit_SettingsPerTool"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    SettingsPerTool->SetValue(FALSE);
	SoftSelectionCheck = new wxCheckBox(this, ID_TERRAIN_SETTINGS_SOFTSELECT, *LocalizeUnrealEd("TerrainEdit_SettingsSoftSelect"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	SoftSelectionCheck->SetValue(EditorOptions->bSoftSelectEnabled);
	SoftSelectionCheck->Enable(FALSE);
	ConstrainedTool = new wxCheckBox(this, ID_TERRAIN_SETTINGS_CONSTRAINED, *LocalizeUnrealEd("TerrainEdit_SettingsConstrained"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    ConstrainedTool->SetValue(EditorOptions->bConstrainedEditing);

	FlattenAngle = new wxCheckBox(this, ID_TERRAIN_SETTINGS_FLATTEN_ANGLE, *LocalizeUnrealEd("TerrainEdit_FlattenAngle"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    FlattenAngle->SetValue(FALSE);

	FlattenHeightLabel = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("TerrainEdit_FlattenHeight"), wxDefaultPosition, wxDefaultSize, 0 );
	FlattenHeightEdit = new WxTextCtrl( this, ID_TERRAIN_SETTINGS_FLATTEN_HEIGHT, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );

	ToolScaleLabel = new wxStaticText(this, wxID_STATIC, *LocalizeUnrealEd("TerrainEdit_ToolScaleLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	ToolScaleCombo = new wxComboBox(this, ID_TERRAIN_SETTINGS_COMBOBOX_SCALE, TEXT(""), wxDefaultPosition, wxSize(88, -1), 0, ComboBoxStrings, wxCB_DROPDOWN);

	INT RangeLow = 0;
	INT RangeHigh = 2048;
	for (Index = 0; Index < ETETS_MAX; Index++)
	{
		switch (Index)
		{
		case 0:
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_StrengthLabel"));
			RangeLow = EditorOptions->SliderRange_Low_Strength;
			RangeHigh = EditorOptions->SliderRange_High_Strength;
			break;
		case 1:
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_RadiusLabel"));
			RangeLow = EditorOptions->SliderRange_Low_Radius;
			RangeHigh = EditorOptions->SliderRange_High_Radius;
			break;
		case 2:
			ToolTip = FString::Printf(*LocalizeUnrealEd("TerrainEdit_FalloffLabel"));
			RangeLow = EditorOptions->SliderRange_Low_Falloff;
			RangeHigh = EditorOptions->SliderRange_High_Falloff;
			break;
		}

		Settings[Index].Label = new wxStaticText(this, ID_TERRAIN_SETTINGS_LABEL_STRENGTH + (3 * Index), *ToolTip, wxDefaultPosition, wxDefaultSize, 0);
		Settings[Index].Slider = new wxSlider(this, ID_TERRAIN_SETTINGS_SLIDER_STRENGTH + (3 * Index), RangeLow, RangeLow, RangeHigh);
		Settings[Index].Text = new WxTextCtrl(this, ID_TERRAIN_SETTINGS_TEXT_STRENGTH + (3 * Index), TEXT(""), wxDefaultPosition, wxSize(64, -1));
	}

	ToolMirrorLabel = new wxStaticText(this, wxID_STATIC, *LocalizeUnrealEd("TerrainEdit_ToolMirrorLabel"), wxDefaultPosition, wxDefaultSize, 0 );
	ToolMirrorCombo = new wxComboBox(this, ID_TERRAIN_SETTINGS_COMBOBOX_MIRROR, TEXT(""), wxDefaultPosition, wxSize(64, -1), wxArrayString());

	/** Tessellation Controls									*/
	TessellationIncreaseLabel = new wxStaticText(this, ID_TERRAIN_TESSELLATE_LABEL_INCREASE, *LocalizeUnrealEd("TerrainTessellationIncrease"), wxDefaultPosition, wxDefaultSize, 0);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainTessellation_Increase.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	TessellationIncrease = new WxBitmapButton(this, ID_TERRAIN_TESSELLATE_INCREASE, *TempBitmap, wxDefaultPosition, wxSize(32, 32));
	delete TempBitmap;
	TessellationDecreaseLabel = new wxStaticText(this, ID_TERRAIN_TESSELLATE_LABEL_DECREASE, *LocalizeUnrealEd("TerrainTessellationDecrease"), wxDefaultPosition, wxDefaultSize, 0);
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainTessellation_Decrease.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	TessellationDecrease = new WxBitmapButton(this, ID_TERRAIN_TESSELLATE_DECREASE, *TempBitmap, wxDefaultPosition, wxSize(32, 32));
	delete TempBitmap;

	/** Browser Panel											*/
    TerrainBrowserPanel = new WxViewportHolder(this, ID_TERRAIN_BROWSER_PANEL, FALSE, 
		wxDefaultPosition, wxSize(640, 250), wxSUNKEN_BORDER|wxTAB_TRAVERSAL);
	TerrainBrowserScrollBar = new wxScrollBar(this, ID_TERRAIN_BROWSER_SCROLLBAR, wxDefaultPosition, wxSize(-1, 250), wxSB_VERTICAL);
    TerrainBrowserScrollBar->SetScrollbar(0, 1, 100, 1);

	/** Bottom row Controls										*/
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_BrowserSelect.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	UseSelectedButton = new WxBitmapButton(this, ID_TERRAIN_BROWSER_USE_SELECTED, *TempBitmap, wxDefaultPosition, wxSize(32, 32));
	UseSelectedButton->SetBitmapLabel(*TempBitmap);
	UseSelectedButton->SetToolTip(*LocalizeUnrealEd("TerrainBrowser_UseSelected_ToolTip"));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_BrowserSelect_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	UseSelectedButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_AddLayer.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	NewLayerButton = new WxBitmapButton(this, ID_TERRAIN_BROWSER_ADD_LAYER, *TempBitmap, wxDefaultPosition, wxSize(32, 32));
	NewLayerButton->SetBitmapLabel(*TempBitmap);
	NewLayerButton->SetToolTip(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewLayer"), TEXT(""))));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_AddLayer_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	NewLayerButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_AddMaterial.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	NewMaterialButton = new wxBitmapButton(this, ID_TERRAIN_BROWSER_ADD_MATERIAL, *TempBitmap, wxDefaultPosition, wxSize(32, 32), wxBU_AUTODRAW|wxBU_EXACTFIT);
	NewMaterialButton->SetBitmapLabel(*TempBitmap);
	NewMaterialButton->SetToolTip(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewMaterial"), TEXT(""))));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_AddMaterial_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	NewMaterialButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_DecoLayer.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	NewDecoLayerButton = new wxBitmapButton(this, ID_TERRAIN_BROWSER_ADD_DECOLAYER, *TempBitmap, wxDefaultPosition, wxSize(32, 32), wxBU_AUTODRAW|wxBU_EXACTFIT);
	NewDecoLayerButton->SetBitmapLabel(*TempBitmap);
	NewDecoLayerButton->SetToolTip(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("TerrainBrowser_CreateNewDecoLayer"), TEXT(""))));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_DecoLayer_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	NewDecoLayerButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Delete.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	DeleteSelectedButton = new wxBitmapButton(this, ID_TERRAIN_BROWSER_DELETE_SELECTED, *TempBitmap, wxDefaultPosition, wxSize(32, 32), wxBU_AUTODRAW|wxBU_EXACTFIT);
	DeleteSelectedButton->SetBitmapLabel(*TempBitmap);
	DeleteSelectedButton->SetToolTip(*LocalizeUnrealEd("TerrainBrowser_Delete"));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainProp_Delete_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	DeleteSelectedButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEditor_ShiftLayerUp.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	ShiftLayerUpButton = new wxBitmapButton(this, ID_TERRAIN_BROWSER_MOVE_LAYER_UP, *TempBitmap, wxDefaultPosition, wxSize(32, 32), wxBU_AUTODRAW|wxBU_EXACTFIT);
	ShiftLayerUpButton->SetBitmapLabel(*TempBitmap);
	ShiftLayerUpButton->SetToolTip(*LocalizeUnrealEd("TerrainBrowser_MoveLayerUp_ToolTip"));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEditor_ShiftLayerUp_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	ShiftLayerUpButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEditor_ShiftLayerDown.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap = new WxBitmap(TempImage);
	ShiftLayerDownButton = new wxBitmapButton(this, ID_TERRAIN_BROWSER_MOVE_LAYER_DOWN, *TempBitmap, wxDefaultPosition, wxSize(32, 32), wxBU_AUTODRAW|wxBU_EXACTFIT);
	ShiftLayerDownButton->SetBitmapLabel(*TempBitmap);
	ShiftLayerDownButton->SetToolTip(*LocalizeUnrealEd("TerrainBrowser_MoveLayerDown_ToolTip"));
	delete TempBitmap;
	TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\TerrainEditor_ShiftLayerDown_Pressed.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
	TempBitmap	= new WxBitmap(TempImage);
	ShiftLayerDownButton->SetBitmapSelected(*TempBitmap);
	delete TempBitmap;

	RetainAlpha	= new wxCheckBox(this, ID_TERRAIN_BROWSER_MOVE_LAYER_RETAIN_ALPHA, *LocalizeUnrealEd("TerrainLayerRetainAlpha"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    RetainAlpha->SetValue(FALSE);

	return TRUE;
}

bool WxTerrainEditor::CreateLayerViewport()
{
	if (TerrainBrowserPanel)
	{
		LayerViewport = new FTerrainLayerViewportClient(this);
		wxRect rc = TerrainBrowserPanel->GetRect();
		LayerViewport->Viewport = GEngine->Client->CreateWindowChildViewport(LayerViewport, (HWND)TerrainBrowserPanel->GetHandle(), rc.width, rc.height, 0, 0);
		LayerViewport->Viewport->CaptureJoystickInput(FALSE);
		TerrainBrowserPanel->SetViewport(LayerViewport->Viewport);
		LayerViewport->ViewportType = LVT_OrthoXY;
		LayerViewport->ShowFlags = (SHOW_DefaultEditor & ~SHOW_ViewMode_Mask) | SHOW_ViewMode_Wireframe;
		TerrainBrowserPanel->Show();

		LayerViewport->SetCanvas(0, 0);
	}
	else
	{
		debugf(TEXT("Invalid layer browser panel!"));
		return FALSE;
	}

	return TRUE;
}

bool WxTerrainEditor::PlaceControls()
{
////@begin MyDialog content construction
    wxBoxSizer* TotalWindowBoxSizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(TotalWindowBoxSizer);

	// Grouping Sizer
    wxFlexGridSizer* GroupingsFlexGridSizer = new wxFlexGridSizer(2, 3, 0, 0);
    TotalWindowBoxSizer->Add(GroupingsFlexGridSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	// Tool Sizer
    wxStaticBox* ToolOverallStaticBoxSizer = new wxStaticBox(this, wxID_ANY, _("Tool"));
    wxStaticBoxSizer* ToolStaticBoxSizer = new wxStaticBoxSizer(ToolOverallStaticBoxSizer, wxVERTICAL);
    GroupingsFlexGridSizer->Add(ToolStaticBoxSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* ToolVerticalBoxSizer = new wxBoxSizer(wxVERTICAL);
    ToolStaticBoxSizer->Add(ToolVerticalBoxSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* ToolRow1Sizer = new wxBoxSizer(wxHORIZONTAL);
    ToolVerticalBoxSizer->Add(ToolRow1Sizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 2);

    ToolRow1Sizer->Add(ToolButtons[ETETB_AddRemoveSectors].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow1Sizer->Add(ToolButtons[ETETB_AddRemovePolys].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow1Sizer->Add(ToolButtons[ETETB_Paint].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow1Sizer->Add(ToolButtons[ETETB_PaintVertex].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow1Sizer->Add(ToolButtons[ETETB_ManualEdit].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

	wxBoxSizer* ToolRow2Sizer = new wxBoxSizer(wxHORIZONTAL);
    ToolVerticalBoxSizer->Add(ToolRow2Sizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 2);

	ToolRow2Sizer->Add(ToolButtons[ETETB_Flatten].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow2Sizer->Add(ToolButtons[ETETB_FlattenSpecific].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow2Sizer->Add(ToolButtons[ETETB_Smooth].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow2Sizer->Add(ToolButtons[ETETB_Average].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow2Sizer->Add(ToolButtons[ETETB_Noise].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

	wxBoxSizer* ToolRow3Sizer = new wxBoxSizer(wxHORIZONTAL);
    ToolVerticalBoxSizer->Add(ToolRow3Sizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 2);

	ToolRow3Sizer->Add(ToolButtons[ETETB_Visibility].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow3Sizer->Add(ToolButtons[ETETB_TexturePan].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow3Sizer->Add(ToolButtons[ETETB_TextureRotate].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow3Sizer->Add(ToolButtons[ETETB_TextureScale].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow3Sizer->Add(ToolButtons[ETETB_VertexLock].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

	wxBoxSizer* ToolRow4Sizer = new wxBoxSizer(wxHORIZONTAL);
    ToolVerticalBoxSizer->Add(ToolRow4Sizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 2);

	ToolRow4Sizer->Add(ToolButtons[ETETB_MarkUnreachable].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
	ToolRow4Sizer->Add(ToolButtons[ETETB_SplitX].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow4Sizer->Add(ToolButtons[ETETB_SplitY].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow4Sizer->Add(ToolButtons[ETETB_Merge].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    ToolRow4Sizer->Add(ToolButtons[ETETB_OrientationFlip].Button, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

    wxBoxSizer* ToolSpecificBoxSizer = new wxBoxSizer(wxVERTICAL);
    ToolStaticBoxSizer->Add(ToolSpecificBoxSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* ToolSpecificHorzSizer = new wxBoxSizer(wxHORIZONTAL);
    ToolSpecificBoxSizer->Add(ToolSpecificHorzSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    ToolSpecificHorzSizer->Add(SpecificValue, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

	// Setting Grouping
    wxStaticBox* SettingGroupBox = new wxStaticBox(this, wxID_ANY, _("Settings"));
    wxStaticBoxSizer* SettingGroupVerticalSizer = new wxStaticBoxSizer(SettingGroupBox, wxVERTICAL);
    GroupingsFlexGridSizer->Add(SettingGroupVerticalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* SettingGroupBoxSizer = new wxBoxSizer(wxVERTICAL);
    SettingGroupVerticalSizer->Add(SettingGroupBoxSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxFlexGridSizer* SettingGroupFlexSizerTop = new wxFlexGridSizer(4, 3, 0, 0);
    SettingGroupBoxSizer->Add(SettingGroupFlexSizerTop, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    SettingGroupFlexSizerTop->Add(SettingsPerTool, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->Add(ToolScaleLabel, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->Add(ToolScaleCombo, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->Add(SoftSelectionCheck, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->AddSpacer(5);
    SettingGroupFlexSizerTop->Add(ConstrainedTool, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->Add(FlattenAngle, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerTop->Add(FlattenHeightLabel, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 0);
	SettingGroupFlexSizerTop->Add(FlattenHeightEdit, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxFlexGridSizer* SettingGroupFlexSizerMiddle = new wxFlexGridSizer(3, 3, 0, 0);
    SettingGroupBoxSizer->Add(SettingGroupFlexSizerMiddle, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Strength].Label, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Strength].Slider, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Strength].Text, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Radius].Label, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Radius].Slider, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Radius].Text, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Falloff].Label, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Falloff].Slider, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerMiddle->Add(Settings[ETETS_Falloff].Text, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

	wxFlexGridSizer* SettingGroupFlexSizerBottom = new wxFlexGridSizer(1, 2, 0, 0);
    SettingGroupBoxSizer->Add(SettingGroupFlexSizerBottom, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	SettingGroupFlexSizerBottom->Add(ToolMirrorLabel, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
	SettingGroupFlexSizerBottom->Add(ToolMirrorCombo, 5, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

	// ViewSettings Grouping
    wxStaticBox* ViewSettingsGroupBoxSizer = new wxStaticBox(this, wxID_ANY, _("View Settings"));
    wxStaticBoxSizer* ViewSettingsGroupVerticalSizer = new wxStaticBoxSizer(ViewSettingsGroupBoxSizer, wxVERTICAL);
    GroupingsFlexGridSizer->Add(ViewSettingsGroupVerticalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* ViewSettingsGroupFlexSizer = new wxFlexGridSizer(7, 2, 0, 0);
    ViewSettingsGroupVerticalSizer->Add(ViewSettingsGroupFlexSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    ViewSettingsGroupFlexSizer->Add(ViewSettingsTerrainComboLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 0);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsTerrainCombo, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsTerrainProperties, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsTerrainPropertiesLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsView.Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsHideLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsLock.Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsLockLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsWire.Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsWireLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsWireframeColor.Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsWireframeColorLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->Add(ViewSettingsRecacheMaterials, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 2);
    ViewSettingsGroupFlexSizer->AddSpacer(5);

	// Import/Export Grouping
    wxStaticBox* ImpExpGroupBoxSizer = new wxStaticBox(this, wxID_ANY, _("Import/Export"));
    wxStaticBoxSizer* ImpExpGroupVerticalSizer = new wxStaticBoxSizer(ImpExpGroupBoxSizer, wxVERTICAL);
    GroupingsFlexGridSizer->Add(ImpExpGroupVerticalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* ImpExpGroupFlexSizer = new wxFlexGridSizer(4, 2, 0, 0);
    ImpExpGroupVerticalSizer->Add(ImpExpGroupFlexSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    ImpExpGroupFlexSizer->Add(ImportButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    ImpExpGroupFlexSizer->Add(HeightMapOnly, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
	ImpExpGroupFlexSizer->AddSpacer(5);
	ImpExpGroupFlexSizer->Add(RetainCurrentTerrain, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    ImpExpGroupFlexSizer->Add(ExportButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    ImpExpGroupFlexSizer->Add(BakeDisplacementMap, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    ImpExpGroupFlexSizer->Add(HeightmapClassLabel, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);
    ImpExpGroupFlexSizer->Add(HeightmapClass, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Brush Grouping
    wxStaticBox* BrushGroupBoxSizer = new wxStaticBox(this, wxID_ANY, _("Brush"));
    wxStaticBoxSizer* BrushGroupVerticalSizer = new wxStaticBoxSizer(BrushGroupBoxSizer, wxVERTICAL);
    GroupingsFlexGridSizer->Add(BrushGroupVerticalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* BrushGroupFlexGridSizer = new wxFlexGridSizer(2, 5, 0, 0);
    BrushGroupVerticalSizer->Add(BrushGroupFlexGridSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush0].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush1].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush2].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush3].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush4].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush5].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush6].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush7].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush8].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    BrushGroupFlexGridSizer->Add(BrushButtons[ETEBB_Brush9].Button, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

	// Tessellation Grouping
    wxStaticBox* TessellationGroupBox = new wxStaticBox(this, wxID_ANY, _("Tessellation"));
    wxStaticBoxSizer* TessellationGroupVerticalSizer = new wxStaticBoxSizer(TessellationGroupBox, wxVERTICAL);
    GroupingsFlexGridSizer->Add(TessellationGroupVerticalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* TessellationGroupFlexSizer = new wxFlexGridSizer(2, 2, 0, 0);
    TessellationGroupVerticalSizer->Add(TessellationGroupFlexSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    TessellationGroupFlexSizer->Add(TessellationIncrease, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    TessellationGroupFlexSizer->Add(TessellationIncreaseLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 1);
    TessellationGroupFlexSizer->Add(TessellationDecrease, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    TessellationGroupFlexSizer->Add(TessellationDecreaseLabel, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 1);

	// Browser Panel Grouping
    wxBoxSizer* BrowserGroupSizer = new wxBoxSizer(wxVERTICAL);
    TotalWindowBoxSizer->Add(BrowserGroupSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    wxBoxSizer* BrowserHorzSizer = new wxBoxSizer(wxHORIZONTAL);
    BrowserGroupSizer->Add(BrowserHorzSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    BrowserHorzSizer->Add(TerrainBrowserPanel, 500, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    BrowserHorzSizer->Add(TerrainBrowserScrollBar, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// BottomRow Button Grouping
    wxBoxSizer* BottomRowHorizontalSizer = new wxBoxSizer(wxHORIZONTAL);
    TotalWindowBoxSizer->Add(BottomRowHorizontalSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(UseSelectedButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(NewLayerButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(NewMaterialButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(NewDecoLayerButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(DeleteSelectedButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(ShiftLayerUpButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(ShiftLayerDownButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    BottomRowHorizontalSizer->Add(RetainAlpha, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
////@end MyDialog content construction

	Fit();
    GetSizer()->SetSizeHints(this);
	Centre();

	return TRUE;
}

void WxTerrainEditor::FillTerrainCombo()
{
	INT Save = ViewSettingsTerrainCombo->GetSelection();
	if (Save == -1)
	{
		Save = 0;
	}

	TerrainArray.Empty();
	ViewSettingsTerrainCombo->Clear();
	for (FActorIterator It; It; ++It)
	{
		ATerrain* Terrain = Cast<ATerrain>(*It);
		if (Terrain)
		{
			ViewSettingsTerrainCombo->Append(*Terrain->GetName(), Terrain);
			TerrainArray.AddItem(Terrain);
			FModeTool_Terrain::AddTerrainToPartialData(Terrain);
		}
	}
	ViewSettingsTerrainCombo->SetSelection(Save);

	// Fake the call to fill in the layers!
	wxCommandEvent DummyEvent;
	OnTerrainCombo(DummyEvent);
}

void WxTerrainEditor::SetInfoArraySize(INT Count)
{
	if (InfoBlocks.Num() < Count)
	{
		InfoBlocks.AddZeroed(Count - InfoBlocks.Num());
	}
}

void WxTerrainEditor::FillInfoArray()
{
	TMap<PTRINT, UBOOL>	OpenMap;

	// Clear the info array
	for (INT InfoIndex = 0; InfoIndex < InfoBlocks.Num(); InfoIndex++)
	{
		FTerrainBrowserBlock* Info	= &InfoBlocks(InfoIndex);
		switch (Info->GetLayerType())
		{
		case TLT_Layer:
			{
				FTerrainLayer*	Layer	= Info->GetLayer();
				OpenMap.Set((PTRINT)Layer, Info->GetOpen());
			}
			break;
		case TLT_DecoLayer:
			{
				FTerrainDecoLayer*	DecoLayer	= Info->GetDecoLayer();
				OpenMap.Set((PTRINT)DecoLayer, Info->GetOpen());
			}
			break;
		case TLT_TerrainMaterial:
			{
				FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();
				OpenMap.Set((PTRINT)Material, Info->GetOpen());
			}
			break;
		case TLT_FilteredMaterial:
			{
				FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();
				OpenMap.Set((PTRINT)Material, Info->GetOpen());
			}
			break;
		case TLT_HeightMap:
		case TLT_Decoration:
			break;
		}
		InfoBlocks(InfoIndex).ClearBlock();
	}

	if (CurrentTerrain)
	{
		UBOOL				bOpenable;
		UBOOL				bOpen;
		UBOOL*				bOpenPtr;
		FTerrainBrowserBlock*	Info;

		ATerrain* Terrain = CurrentTerrain;
		INT	CurrentCount	= 1;

		InfoBlocks.Empty();

		// Add the height map
		SetInfoArraySize(CurrentCount);
		InfoBlocks(CurrentCount - 1).SetupBlock(TLT_HeightMap, -1, 
				NULL, NULL, NULL, NULL, NULL, 
				FALSE, TRUE, TRUE, Terrain->bHeightmapLocked, FALSE, FALSE, FALSE, 
				FALSE, FALSE, FALSE, TRUE, FALSE, NULL);
		InfoBlocks(CurrentCount - 1).SetDraw(TRUE);
		CurrentCount++;

		// Layers...
		if (Terrain->Layers.Num() > 0)
		{
			for (INT LayerIndex = 0; LayerIndex < Terrain->Layers.Num(); LayerIndex++)
			{
				SetInfoArraySize(CurrentCount);
				Info	= &InfoBlocks(CurrentCount - 1);
				CurrentCount++;
				FTerrainBrowserBlock*	LayerInfoBlock	= Info;

				FTerrainLayer*		Layer	= &(Terrain->Layers(LayerIndex));

				bOpenable	= TRUE;
				bOpen		= FALSE;
				bOpenPtr	= OpenMap.Find((PTRINT)Layer);
				if (bOpenPtr)
				{
					bOpen	= *bOpenPtr;
				}

				if (!Layer->Setup || (Layer->Setup->Materials.Num() == 0))
				{
					bOpen		= FALSE;
					bOpenable	= FALSE;
				}

				Info->SetupBlock(TLT_Layer, LayerIndex, Layer, NULL, NULL, NULL, NULL, 
					TRUE, !Layer->Hidden, TRUE, Layer->Locked, bOpenable, bOpen, FALSE, TRUE, 
					Layer->WireframeHighlighted, Layer->Highlighted, TRUE, FALSE, NULL);
				Info->SetDraw(TRUE);

				UTerrainLayerSetup* LayerSetup	= Layer->Setup;
				if (LayerSetup)
				{
					Info->SetHighlightColor(Layer->HighlightColor);
					Info->SetWireframeColor(Layer->WireframeColor);

					for (INT MatIndex = 0; MatIndex < LayerSetup->Materials.Num(); MatIndex++)
					{
						FTerrainFilteredMaterial* Material = &(LayerSetup->Materials(MatIndex));
						SetInfoArraySize(CurrentCount);
						Info	= &InfoBlocks(CurrentCount - 1);
						CurrentCount++;

						FTerrainBrowserBlock*	TFMInfo	= Info;

						UBOOL	bCanOpen	= FALSE;
						UBOOL	bDispMap	= FALSE;
						if (Material->Material)
						{
							if (Material->Material->DisplacementMap)
							{
								bDispMap	= TRUE;
							}
						}

						bOpen		= FALSE;
						bOpenPtr	= OpenMap.Find((PTRINT)Material);
						if (bCanOpen && bOpenPtr)
						{
							bOpen	= *bOpenPtr;
						}
						Info->SetupBlock(TLT_FilteredMaterial, MatIndex, NULL, Material, NULL, NULL, NULL,
							FALSE, TRUE, FALSE, FALSE, bCanOpen, bOpen, TRUE, FALSE, FALSE, FALSE, TRUE, bDispMap, 
							LayerInfoBlock);
						Info->SetDraw(LayerInfoBlock->GetOpen());
					}
				}
			}
		}

		// DecoLayers...
		if (Terrain->DecoLayers.Num() > 0)
		{
			for (INT DecoLayerIndex = 0; DecoLayerIndex < Terrain->DecoLayers.Num(); DecoLayerIndex++)
			{
				SetInfoArraySize(CurrentCount);
				Info	= &InfoBlocks(CurrentCount - 1);
				CurrentCount++;
				FTerrainBrowserBlock*	DecoLayerInfoBlock	= Info;

				FTerrainDecoLayer*	DecoLayer	= &(Terrain->DecoLayers(DecoLayerIndex));

				bOpenable	= TRUE;
				bOpen		= FALSE;
				bOpenPtr	= OpenMap.Find((PTRINT)DecoLayer);
				if (bOpenPtr)
				{
					bOpen	= *bOpenPtr;
				}

				if (!DecoLayer || (DecoLayer->Decorations.Num() == 0))
				{
					bOpen		= FALSE;
					bOpenable	= FALSE;
				}

				Info->SetupBlock(TLT_DecoLayer, DecoLayerIndex, NULL, NULL, DecoLayer, NULL, NULL, 
					TRUE, TRUE, TRUE, FALSE, bOpenable, bOpen, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, NULL);
				Info->SetDraw(TRUE);

				if (bShowDecoarationMeshes)
				{
					for (INT DecoIndex = 0; DecoIndex < DecoLayer->Decorations.Num(); DecoIndex++)
					{
						FTerrainDecoration*	Decoration	= &(DecoLayer->Decorations(DecoIndex));
						SetInfoArraySize(CurrentCount);
						Info	= &InfoBlocks(CurrentCount - 1);
						CurrentCount++;

						Info->SetupBlock(TLT_Decoration, DecoIndex, NULL, NULL, NULL, Decoration, NULL, 
							FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, 
							DecoLayerInfoBlock);
						Info->SetDraw(DecoLayerInfoBlock->GetOpen());
					}
				}
			}
		}
	}
	else
	{
		for (INT InfoIndex = 0; InfoIndex < InfoBlocks.Num(); InfoIndex++)
		{
			FTerrainBrowserBlock* Info = &InfoBlocks(InfoIndex);
			if (Info)
			{
				Info->ClearBlock();
			}
		}
	}

	// Debugging...
	for (INT InfoIndex = 0; InfoIndex < InfoBlocks.Num(); InfoIndex++)
	{
		FTerrainBrowserBlock*	Info	= &InfoBlocks(InfoIndex);
	}

	UpdateScrollBar();

	LayerViewport->Viewport->Invalidate();
}

void WxTerrainEditor::FillImportExportData()
{
	if (HeightmapClass)
	{
		HeightmapClass->Clear();
	}

	FString SelectedString;
	GConfig->GetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), 
		SelectedString, GEditorIni);

	check( GEditorModeTools().IsModeActive( EM_TerrainEdit ) );

	FTerrainToolSettings* Settings = 
		(FTerrainToolSettings*)GEditorModeTools().GetActiveMode( EM_TerrainEdit )->GetSettings();
	if (Settings)
	{
		if (appStrlen(*(Settings->HeightMapExporterClass)) == 0)
		{
			Settings->HeightMapExporterClass = SelectedString;
		}
	}

	INT DefLen = appStrlen(TEXT("TerrainHeightMapExporter"));

	INT Selected = -1;
	INT Count = -1;
	if (HeightmapClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UTerrainHeightMapExporter::StaticClass()))
			{
				UBOOL bSelected = FALSE;
				// Get the name
				FString strName = It->GetFName().ToString();
				if (appStrlen(*strName) - DefLen)
				{
					if (strName == SelectedString)
					{
						bSelected = TRUE;
					}

					// 'Parse' off the "TerrainHeightMapExporter" portion
					FString strParsed = strName.Right(appStrlen(*strName) - DefLen);
					// If it isn't the 'base' exporter, put it in the list.
					if (strParsed != TEXT(""))
					{
						Count++;
						if (bSelected)
						{
							Selected = Count;
						}
						HeightmapClass->Append(*strParsed);
					}
				}
			}
		}
	}

	if (HeightmapClass && (Selected != -1))
	{
		HeightmapClass->SetSelection(Selected);
	}
}

void WxTerrainEditor::FillMirrorCombo()
{
	check(ToolMirrorCombo);
	ToolMirrorCombo->Clear();
	ToolMirrorCombo->Append(TEXT("NONE"));
	ToolMirrorCombo->Append(TEXT("X"));
	ToolMirrorCombo->Append(TEXT("Y"));
	ToolMirrorCombo->Append(TEXT("XY"));

	ToolMirrorCombo->SetSelection(EditorOptions->Current_MirrorFlag);
}

void WxTerrainEditor::SetBrush(EBrushButton Brush)
{
	if (Brush == ETEBB_Custom) 
	{
		// Clear the selected brush
		if (CurrentBrush != ETEBB_Custom)
		{
			BrushButtons[CurrentBrush].Button->SetBitmapLabel(*BrushButtons[CurrentBrush].Inactive);
		}
		CurrentBrush	= ETEBB_Custom;
	}
	else
	if (Brush < ETEBB_MAX)
	{
		Settings[ETETS_Strength].Slider->SetValue(BrushButtons[Brush].Strength);
		Settings[ETETS_Strength].Text->SetValue(wxString(*FString::Printf(TEXT("%d"), BrushButtons[Brush].Strength)));
		Settings[ETETS_Radius].Slider->SetValue(BrushButtons[Brush].Radius);
		Settings[ETETS_Radius].Text->SetValue(wxString(*FString::Printf(TEXT("%d"), BrushButtons[Brush].Radius)));
		Settings[ETETS_Falloff].Slider->SetValue(BrushButtons[Brush].Falloff);
		Settings[ETETS_Falloff].Text->SetValue(wxString(*FString::Printf(TEXT("%d"), BrushButtons[Brush].Falloff)));

		INT	Strength	= Settings[ETETS_Strength].Slider->GetValue();
		INT	Radius		= Settings[ETETS_Radius].Slider->GetValue();
		INT	Falloff		= Settings[ETETS_Falloff].Slider->GetValue();

		SetStrength(Strength);
		SetRadius(Radius);
		SetFalloff(Falloff);
	}

	EditorOptions->Current_Brush = CurrentBrush;

	SaveData();
}

FModeTool* WxTerrainEditor::GetTool(EToolButton Tool)
{
	FString	ToolName(TEXT(""));

	// If a ToolName exists for the tools, then the button will be enabled.
	// Otherwise, it will be disabled (and the tool can't be selected).
	switch (Tool)
	{
	case ETETB_AddRemoveSectors:	ToolName	= FString(TEXT("AddRemoveSectors"));	break;
	case ETETB_AddRemovePolys:															break;
	case ETETB_Paint:				ToolName	= FString(TEXT("Paint"));				break;
	case ETETB_PaintVertex:			ToolName	= FString(TEXT("VertexEdit"));			break;
	case ETETB_ManualEdit:																break;
	case ETETB_Flatten:				ToolName	= FString(TEXT("Flatten"));				break;
	case ETETB_FlattenSpecific:		ToolName	= FString(TEXT("FlattenSpecific"));		break;
	case ETETB_Smooth:				ToolName	= FString(TEXT("Smooth"));				break;
	case ETETB_Average:				ToolName	= FString(TEXT("Average"));				break;
	case ETETB_Noise:				ToolName	= FString(TEXT("Noise"));				break;
	case ETETB_Visibility:			ToolName	= FString(TEXT("Visibility"));			break;
	case ETETB_TexturePan:			ToolName	= FString(TEXT("Tex Pan"));				break;
	case ETETB_TextureRotate:		ToolName	= FString(TEXT("Tex Rotate"));			break;
	case ETETB_TextureScale:		ToolName	= FString(TEXT("Tex Scale"));			break;
	case ETETB_VertexLock:																break;
	case ETETB_MarkUnreachable:															break;
	case ETETB_SplitX:				ToolName	= FString(TEXT("SplitX"));				break;
	case ETETB_SplitY:				ToolName	= FString(TEXT("SplitY"));				break;
	case ETETB_Merge:				ToolName	= FString(TEXT("Merge"));				break;
	case ETETB_OrientationFlip:		ToolName	= FString(TEXT("OrientationFlip"));		break;
	}

	if (ToolName != FString(TEXT("")))
	{
		TArray<FModeTool*>		Tools	= EdMode->GetTools();
		for (INT ToolIndex = 0; ToolIndex < Tools.Num(); ToolIndex++)
		{
			if (Tools(ToolIndex)->GetName() == ToolName)
			{
				return Tools(ToolIndex);
			}
		}
	}

	return NULL;
}

void WxTerrainEditor::SetStrength(INT Strength)
{
	FTerrainToolSettings* Settings = (FTerrainToolSettings*)(EdMode->GetSettings());
	Settings->Strength = Strength;

	EditorOptions->Current_Strength = Strength;
}

void WxTerrainEditor::SetRadius(INT Radius)
{
	FTerrainToolSettings* Settings = (FTerrainToolSettings*)(EdMode->GetSettings());
	INT Falloff = Settings->RadiusMax - Settings->RadiusMin;
	Settings->RadiusMin = Radius;
	Settings->RadiusMax = Settings->RadiusMin + Falloff;

	EditorOptions->Current_Radius = Radius;
}

void WxTerrainEditor::SetFalloff(INT Falloff)
{
	FTerrainToolSettings* Settings = (FTerrainToolSettings*)(EdMode->GetSettings());
	Settings->RadiusMax = Settings->RadiusMin + Falloff;

	EditorOptions->Current_Falloff = Falloff;
}

void WxTerrainEditor::LoadData()
{
	FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(EdMode->GetSettings());

	// Misc
	FString	Value;
	if (ToolSettings)
	{
		// Remember radius and falloff here because Settings[ETETS_Strength].Text->SetValue() will cause
		// ToolSettings->RadiusMax etc to be overwritten with the values in the box.
		INT Radius  = ToolSettings->RadiusMin;
		INT Falloff = ToolSettings->RadiusMax - ToolSettings->RadiusMin;

		Settings[ETETS_Strength].Slider->SetValue(ToolSettings->Strength);
		Value	= FString::Printf(TEXT("%d"), (INT)(ToolSettings->Strength));
		Settings[ETETS_Strength].Text->SetValue(wxString(*Value));

		Settings[ETETS_Radius].Slider->SetValue(Radius);
		Value	= FString::Printf(TEXT("%d"), Radius);
		Settings[ETETS_Radius].Text->SetValue(wxString(*Value));

		Settings[ETETS_Falloff].Slider->SetValue(Falloff);
		Value	= FString::Printf(TEXT("%d"), Falloff);
		Settings[ETETS_Falloff].Text->SetValue(wxString(*Value));

		FlattenAngle->SetValue(ToolSettings->FlattenAngle ? TRUE : FALSE);

		if( !ToolSettings->FlattenAngle && ToolSettings->UseFlattenHeight )
		{
			FlattenHeightEdit->SetValue(*FString::Printf(TEXT("%f"), ToolSettings->FlattenHeight));
		}
		else
		{
			FlattenHeightEdit->SetValue(TEXT(""));
		}

		SoftSelectionCheck->SetValue(ToolSettings->bSoftSelectionEnabled ? TRUE : FALSE);

		ToolScaleCombo->SetValue(*FString::Printf(TEXT("%f"), ToolSettings->ScaleFactor));
	}

	if (EdMode->bPerToolSettings)
	{
		SettingsPerTool->SetValue(TRUE);
	}
	else
	{
		SettingsPerTool->SetValue(FALSE);
	}

	// Terrains
	INT Save = ViewSettingsTerrainCombo->GetSelection();
	if (Save == -1)
	{
		Save = 0;
	}

	INT TerrainCount = 0;

	if (GWorld != NULL)
	{
		for (FActorIterator It; It; ++It)
		{
			ATerrain* Terrain = Cast<ATerrain>(*It);
			if (Terrain)
			{
				TerrainCount++;
			}
		}
		FillTerrainCombo();

		if (ViewSettingsTerrainCombo->GetSelection() >= 0)
		{
			CurrentTerrain = (ATerrain*)ViewSettingsTerrainCombo->GetClientData(ViewSettingsTerrainCombo->GetSelection());
		}
		else
		{
			CurrentTerrain = NULL;
		}

		if (TerrainCount > 0)
		{
			FillInfoArray();
		}
		else
		{
			CurrentTerrain = NULL;
			InfoBlocks.Empty();
		}

		wxCommandEvent DummyEvent;
		OnTerrainCombo(DummyEvent);

		// HeightMap exporter
		FillImportExportData();

		//
		FillMirrorCombo();
	}
}

void WxTerrainEditor::SaveData()
{
	static UBOOL InSaveData = FALSE;
	if( InSaveData )
		return;
	InSaveData = TRUE;

	FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(EdMode->GetSettings());

	// Misc
	ToolSettings->CurrentTerrain = CurrentTerrain;
	ToolSettings->RadiusMin	= appAtoi(Settings[ETETS_Radius].Text->GetValue().c_str());
	ToolSettings->RadiusMax	= ToolSettings->RadiusMin + appAtoi(Settings[ETETS_Falloff].Text->GetValue().c_str());
	ToolSettings->Strength	= appAtof(Settings[ETETS_Strength].Text->GetValue().c_str());

	ToolSettings->ScaleFactor = appAtof(ToolScaleCombo->GetValue());
	if (ToolSettings->ScaleFactor == 0.0f)
	{
		// NEVER ALLOW A SCALE OF ZERO
		// Assume that it is empty or an error
		ToolSettings->ScaleFactor = 1.0f;
	}

	if( EdMode->GetCurrentTool()->GetID() == MT_TerrainFlatten )
	{
		FlattenAngle->Enable(TRUE);
		ToolSettings->FlattenAngle = (UBOOL)(FlattenAngle->IsChecked());

		FlattenHeightLabel->Enable(!ToolSettings->FlattenAngle);
		FlattenHeightEdit->Enable(!ToolSettings->FlattenAngle);

		FString HeightText = FlattenHeightEdit->GetValue().c_str();
		if( !ToolSettings->FlattenAngle && HeightText.IsNumeric() )
		{
			ToolSettings->UseFlattenHeight = TRUE;
			ToolSettings->FlattenHeight = appAtof(*HeightText);
		}
		else
		{
			ToolSettings->UseFlattenHeight = FALSE;
			FlattenHeightEdit->SetValue(TEXT(""));
		}

		SoftSelectionCheck->Enable(FALSE);
		ToolScaleCombo->Enable(FALSE);
	}
	else
	if( EdMode->GetCurrentTool()->GetID() == MT_TerrainVertexEdit )
	{
		SoftSelectionCheck->Enable(TRUE);
		ToolSettings->bSoftSelectionEnabled = (UBOOL)(SoftSelectionCheck->IsChecked());
		FlattenAngle->Enable(FALSE);
		FlattenHeightLabel->Enable(FALSE);
		FlattenHeightEdit->Enable(FALSE);
		ToolScaleCombo->Enable(TRUE);
	}
	else
	{
		FlattenAngle->Enable(FALSE);
		FlattenHeightLabel->Enable(FALSE);
		FlattenHeightEdit->Enable(FALSE);
		SoftSelectionCheck->Enable(FALSE);
		ToolScaleCombo->Enable(FALSE);
	}  

	// Layer
	if ((SelectedBlock != -1) && (SelectedBlock < InfoBlocks.Num()))
	{
		// Grab the selected block
		FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);

		if (Info)
		{
			FTerrainLayerId	LayerClientData;

			LayerClientData.Type	= Info->GetLayerType();
			LayerClientData.Index	= Info->GetArrayIndex();

			ToolSettings->MaterialIndex = -1;

			if (LayerClientData.Type == TLT_HeightMap)
			{
				ToolSettings->LayerIndex = INDEX_NONE;
			}
			else
			if (LayerClientData.Type == TLT_Layer)
			{
				ToolSettings->DecoLayer = 0;
				ToolSettings->LayerIndex = LayerClientData.Index;

				// See if there is a specific material selected...
				if (SelectedChildBlock != -1)
				{
					FTerrainBrowserBlock* ChildInfo = &InfoBlocks(SelectedChildBlock);
					if (ChildInfo)
					{
						ToolSettings->MaterialIndex = ChildInfo->GetArrayIndex();
					}
				}
			}
			else
			{
				ToolSettings->DecoLayer = 1;
				ToolSettings->LayerIndex = LayerClientData.Index;
			}
		}
	}
	else
	{
		ToolSettings->LayerIndex = INDEX_NONE;
	}

	// HeightMap exporter
	wxString SelString = HeightmapClass->GetString(HeightmapClass->GetSelection());
	if (SelString.IsEmpty())
	{
		InSaveData = FALSE;
		// This should NOT happen...
		return;
	}
	ToolSettings->HeightMapExporterClass = FString(TEXT("TerrainHeightMapExporter")) + FString(SelString.c_str());

	InSaveData = FALSE;
}

/**
 *	Import terrain or a height-map
 */
void WxTerrainEditor::ImportTerrain()
{
	UBOOL bHeightMapOnly = (UBOOL)(HeightMapOnly->IsChecked());
	UBOOL bRetainCurrentTerrain = (UBOOL)(RetainCurrentTerrain->IsChecked());

	if (bHeightMapOnly == FALSE)
	{
		FEditorFileUtils::Import(TRUE);
		return;
	}

	if ((bHeightMapOnly == TRUE) && (bRetainCurrentTerrain == TRUE))
	{
		// Make sure there is a currently selected terrain
		if (CurrentTerrain == NULL)
		{
			//@todo. Prompt the user
			appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_NoTerrainSelected"));
			return;
		}
	}

	wxString	Message(TEXT("Import from..."));
	wxString	DefaultDir(*StrExportDirectory);
	wxString	DefaultFile(TEXT(""));
	wxString	Wildcard;

	FString		ImportFile;

	if (bHeightMapOnly)
	{
		debugf(TEXT("TerrainImport: HeightMap only!"));
		Wildcard = wxString(TEXT("BMP files (*.bmp)|*.bmp|All files (*.*)|*.*"));
	}
	else
	{
		debugf(TEXT("TerrainImport: T3D Import!"));
		Wildcard = wxString(TEXT("T3D files (*.T3D)|*.T3D"));
	}


	WxFileDialog ImportFileDlg(GApp->EditorFrame, Message, DefaultDir, DefaultFile, Wildcard, wxOPEN);
	if (ImportFileDlg.ShowModal() != wxID_OK)
	{
		return;
	}

	ImportFile = FString(ImportFileDlg.GetPath());
	debugf(TEXT("TerrainEdit Importing %s"), *ImportFile);

	if (bHeightMapOnly)
	{
		// The extension of the file, will indicate the importer...
		FFilename FileName(*ImportFile);

		if (FileName.GetExtension() == TEXT("BMP"))
		{
			ATerrain* Terrain = NULL;

			FTerrainLayer* SelectedLayer = NULL;
			FTerrainFilteredMaterial* SelectedMaterial = NULL;

			if (InfoBlocks.Num() == 0)
			{
				SelectedBlock = -1;
				SelectedChildBlock = -1;
			}

			if (SelectedBlock != -1)
			{
				FTerrainBrowserBlock* ParentInfo = NULL;
				FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);
				if (Info && (Info->GetLayerType() == TLT_Layer))
				{
					SelectedLayer = Info->GetLayer();
					if (SelectedLayer->Setup)
					{
						if (SelectedLayer->Setup->Materials.Num() == 1)
						{
							SelectedMaterial = &(SelectedLayer->Setup->Materials(0));
						}
					}
				}
			}

			if (SelectedLayer != NULL)
			{
				if (SelectedLayer->AlphaMapIndex != -1)
				{
					Terrain = CurrentTerrain;
					check(Terrain);

					UTerrainHeightMapFactoryG16BMP* pkFactory = ConstructObject<UTerrainHeightMapFactoryG16BMP>(UTerrainHeightMapFactoryG16BMP::StaticClass());
					check(pkFactory);

					if (pkFactory->ImportLayerDataFromFile(Terrain, SelectedLayer, *FileName, GWarn) == FALSE)
					{
						appMsgf(AMT_OK, TEXT("Failed to import alpha map for layer (%s) of terrain %s"), 
							*(SelectedLayer->Name), *Terrain->GetName());
					}
				}
				else
				{
					appMsgf(AMT_OK, TEXT("Cannot import the base layer (%s) of terrain %s"), 
						*(SelectedLayer->Name), *Terrain->GetName());
				}
			}
			else
			{
				if (bRetainCurrentTerrain)
				{
					Terrain = CurrentTerrain;
				}
				else
				{
					AActor* Actor = GWorld->SpawnActor(ATerrain::StaticClass(), NAME_None, FVector(0,0,0), FRotator(0,0,0), NULL, 1, 0);
					check(Actor);
					Terrain = Cast<ATerrain>(Actor);
				}
				// Set the terrain Min/Max tess levels to 1 to allow importing of 
				// any height map sizes. (Otherwise, the heightmap must be (N*MaxTess + 1)
				Terrain->MaxTesselationLevel = 1;
				Terrain->MinTessellationLevel = 1;

				UTerrainHeightMapFactoryG16BMP* pkFactory = ConstructObject<UTerrainHeightMapFactoryG16BMP>(UTerrainHeightMapFactoryG16BMP::StaticClass());
				check(pkFactory);

				if (pkFactory->ImportHeightDataFromFile(Terrain, *FileName, GWarn, TRUE) == FALSE)
				{
				}
			}
		}
	}
	else
	{
		//@todo. Fill this in...
	}
}

/**
 *	Export the selected terrain (or just its height map)
 */
void WxTerrainEditor::ExportTerrain()
{
	TArray<ATerrain*> SelectedTerrain;

	// If there is no terrain selected, don't do anything.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ATerrain* TerrainActor = Cast<ATerrain>( Actor );
		if ( TerrainActor )
		{
			SelectedTerrain.AddItem(TerrainActor);
		}
	}

	if (SelectedTerrain.Num() == 0)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_NoTerrainSelected"));
		return;
	}

	UBOOL bHeightMapOnly = (UBOOL)(HeightMapOnly->IsChecked());
	if (bHeightMapOnly)
	{
		debugf(TEXT("TerrainExport: HeightMap only!"));

		// Get the HeightMapExporter class.
		FString HMExporter;

		if (!GConfig->GetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), HMExporter, GEditorIni))
		{
			HMExporter = TEXT("TerrainHeightMapExporterTextT3D");
			GConfig->SetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), *HMExporter, GEditorIni);
		}

		UTerrainHeightMapExporter* pkExporter = NULL;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UTerrainHeightMapExporter::StaticClass()))
			{
				if (appStrcmp(&(**It->GetName()), *HMExporter) == 0)
				{
					pkExporter = ConstructObject<UTerrainHeightMapExporter>(*It);
				}
			}
		}

		if (pkExporter == NULL)
		{
			appMsgf(AMT_OK, TEXT("Could not find HeightMap exporter"));
			return;
		}

		//@todo. Store the MRU directory???
		// Get the directory to export to
		FName	DialogTitle(TEXT("Export to..."));
		FName	DefaultDir(*StrExportDirectory);
		if (PromptUserForDirectory(StrExportDirectory, DialogTitle, DefaultDir))
		{
			for (INT ii = 0; ii < SelectedTerrain.Num(); ii++)
			{
				ATerrain* Terrain = SelectedTerrain(ii);
				FTerrainLayer* SelectedLayer = NULL;
				FTerrainFilteredMaterial* SelectedMaterial = NULL;
				if (SelectedBlock != -1)
				{
					FTerrainBrowserBlock* ParentInfo = NULL;
					FTerrainBrowserBlock* Info = &InfoBlocks(SelectedBlock);
					if (Info->GetLayerType() == TLT_Layer)
					{
/***
						if (SelectedChildBlock != -1)
						{
							ParentInfo = Info;
							Info = &InfoBlocks(SelectedChildBlock);
						}
						else
***/
						{
							SelectedLayer = Info->GetLayer();
							if (SelectedLayer->Setup)
							{
								if (SelectedLayer->Setup->Materials.Num() == 1)
								{
									SelectedMaterial = &(SelectedLayer->Setup->Materials(0));
								}
							}
						}
					}
				}

				if (SelectedLayer != NULL)
				{
					if (SelectedLayer->AlphaMapIndex != -1)
					{
						if (!pkExporter->ExportLayerDataToFile(Terrain, SelectedLayer, *StrExportDirectory, GWarn))
						{
							appMsgf(AMT_OK, TEXT("Failed to export Layer %s for terrain %s"), 
								*(SelectedLayer->Name), *Terrain->GetName());
						}
					}
					else
					{
						appMsgf(AMT_OK, TEXT("Cannot export the base layer (%s) of terrain %s"), 
							*(SelectedLayer->Name), *Terrain->GetName());
					}
				}
				else
				{
					if (!pkExporter->ExportHeightDataToFile(Terrain, *StrExportDirectory, GWarn))
					{
						appMsgf(AMT_OK, TEXT("Failed to export HeightMap for terrain %s"), *Terrain->GetName());
					}
				}
			}
		}
	}
	else
	{
		FName	DialogTitle(TEXT("Export to..."));
		FName	DefaultDir(*StrExportDirectory);
		if (PromptUserForDirectory(StrExportDirectory, DialogTitle, DefaultDir))
		{
			UTerrainExporterT3D*	Exporter		= ConstructObject<UTerrainExporterT3D>(UTerrainExporterT3D::StaticClass());
			check(Exporter);
			Exporter->SetIsExportingTerrainOnly(TRUE);
			for (INT ii = 0; ii < SelectedTerrain.Num(); ii++)
			{
				ATerrain*	Terrain		= SelectedTerrain(ii);
				FString		Filename	= FString::Printf(TEXT("%s\\%s.T3D"), *StrExportDirectory, *Terrain->GetName());
				Exporter->ExportToFile(Terrain, Exporter, *Filename, FALSE);
			}
			debugf(TEXT("TerrainExport: Complete!"));
		}
	}
}

/**
 *	Increase the tessellation of the selected patch
 */
void WxTerrainEditor::PatchTessellationIncrease()
{
	if (CurrentTerrain == NULL)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_NoTerrainSelected"));
		return;
	}

	FString	IncreasePrompt = FString::Printf(*LocalizeUnrealEd("TerrainEdit_ConfirmIncrease"));
	UBOOL	bDoIncrease = appMsgf(AMT_YesNo, *IncreasePrompt);
	if (bDoIncrease == FALSE)
	{
		return;
	}

	BeginTransaction(TEXT("Terrain_TessellateUp"));
	CurrentTerrain->TessellateTerrainUp();
	//Update the terrain's partial data to account for the tessellation increase
	FModeTool_Terrain::CheckPartialData(CurrentTerrain);
	EndTransaction(TEXT("Terrain_TessellateUp"));
}

/**
 *	Decrease the tessellation of the selected patch
 */
void WxTerrainEditor::PatchTessellationDecrease()
{
	if (CurrentTerrain == NULL)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_NoTerrainSelected"));
		return;
	}

	FString	DecreasePrompt;
	if (CurrentTerrain->MaxTesselationLevel == 1)
	{
		// Can not decrease any further!
		DecreasePrompt = FString::Printf(*LocalizeUnrealEd("TerrainEdit_CannotDecrease_AtMTLOne"));
		appMsgf(AMT_OK, *DecreasePrompt);
		return;
	}
	if ((CurrentTerrain->NumPatchesX <= CurrentTerrain->MaxTesselationLevel) && 
		(CurrentTerrain->NumPatchesY <= CurrentTerrain->MaxTesselationLevel)
		)
	{
		// Can not decrease any further!
		DecreasePrompt = FString::Printf(*LocalizeUnrealEd("TerrainEdit_CannotDecrease_MTLPrevent"));
		appMsgf(AMT_OK, *DecreasePrompt);
		return;
	}

	DecreasePrompt = FString::Printf(*LocalizeUnrealEd("TerrainEdit_ConfirmDecrease"));
	UBOOL	bDoDecrease = appMsgf(AMT_YesNo, *DecreasePrompt);
	if (bDoDecrease == FALSE)
	{
		return;
	}

	BeginTransaction(TEXT("Terrain_TessellateDown"));
	CurrentTerrain->TessellateTerrainDown();
	//Update the terrain's partial data to account for the tessellation decrease
	FModeTool_Terrain::CheckPartialData(CurrentTerrain);
	EndTransaction(TEXT("Terrain_TessellateDown"));
}

/**
 *	Use the item currently selected in the ContentBrowser.
 *
 *	@param	TypeRestriction		The type of object to restrict usage to
 *
 *	@return	TRUE				If successful
 *			FALSE				If not
 */
UBOOL WxTerrainEditor::UseSelected(ETerrainLayerType TypeRestriction)
{
    if (TypeRestriction == TLT_Empty)
	{
	}

	UClass*					RequiredClass		= NULL;
	UBOOL					bRefreshTerrain		= FALSE;
	FTerrainBrowserBlock*	ParentInfo			= NULL;
	FTerrainBrowserBlock*	Info				= NULL;

	if (SelectedBlock != -1)
	{
		BeginTransaction(TEXT("TLB_UseSelected"));

		Info	= &InfoBlocks(SelectedBlock);
		if (SelectedChildBlock != -1)
		{
			ParentInfo	= Info;
			Info		= &InfoBlocks(SelectedChildBlock);
			if (Info->GetParentBlock() != ParentInfo)
			{
				ParentInfo	= Info->GetParentBlock();
			}
		}

		if (Info)
		{
			switch (Info->GetLayerType())
			{
			case TLT_HeightMap:
				{
					// Nothing to use...
				}
				break;
			case TLT_Layer:
				{
					FTerrainLayer* Layer	= Info->GetLayer();
					if (Layer)
					{
						RequiredClass	= UTerrainLayerSetup::StaticClass();
					}
				}
				break;
			case TLT_DecoLayer:
				{
					// Nothing to use...
				}
				break;
			case TLT_TerrainMaterial:
			case TLT_FilteredMaterial:
				{
				}
				break;
			case TLT_Decoration:
				{
					RequiredClass	= UStaticMesh::StaticClass();
				}
				break;
			}
	
			if (RequiredClass)
			{
				GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
				UObject*	Obj	= GEditor->GetSelectedObjects()->GetTop(RequiredClass);

				if (Obj)
				{
					switch (Info->GetLayerType())
					{
					case TLT_HeightMap:
						{
							// Nothing to use...
						}
						break;
					case TLT_Layer:
						{
							FTerrainLayer* Layer	= Info->GetLayer();
							if (Layer)
							{
								UTerrainLayerSetup* NewSetup = Cast<UTerrainLayerSetup>(Obj);
								if (NewSetup != Layer->Setup)
								{
									// Remove the old one...
									if (CurrentTerrain)
									{
										CurrentTerrain->RemoveLayerSetup(Layer->Setup);
									}

									// Update with the new one
									Layer->Setup		= NewSetup;
									bRefreshTerrain		= TRUE;
									if (CurrentTerrain)
									{
										CurrentTerrain->UpdateLayerSetup(Layer->Setup);
										CurrentTerrain->MarkPackageDirty();
									}
									if (Layer->Setup)
									{
										Layer->Setup->MarkPackageDirty();
									}
								}
							}
						}
						break;
					case TLT_DecoLayer:
						{
							// Nothing to use...
						}
						break;
					case TLT_TerrainMaterial:
					case TLT_FilteredMaterial:
						{
							// Grab the parent info layer setup...
							if (ParentInfo)
							{
								FTerrainLayer* Layer	= ParentInfo->GetLayer();
								if (Layer && Layer->Setup)
								{
									INT	Index	= Info->GetArrayIndex();
									if (Index != -1)
									{
										UTerrainMaterial* NewTMat = Cast<UTerrainMaterial>(Obj);

										FTerrainFilteredMaterial* FilteredMaterial = &(Layer->Setup->Materials(Index));

										if (FilteredMaterial->Material != NewTMat)
										{
											if (CurrentTerrain)
											{
												CurrentTerrain->RemoveTerrainMaterial(FilteredMaterial->Material);
											}
											FilteredMaterial->Material	= NewTMat;
											if (CurrentTerrain)
											{
												CurrentTerrain->UpdateTerrainMaterial(FilteredMaterial->Material);
											}
											bRefreshTerrain	= TRUE;
										}

										Layer->Setup->MarkPackageDirty();
										CurrentTerrain->MarkPackageDirty();
									}
									else
									{
										appMsgf(AMT_OK, TEXT("ERROR: Invalid array index"));
									}
								}
							}
						}
						break;
					case TLT_Decoration:
						{
							// Grab the parent deco layer setup...
							if (ParentInfo)
							{
								FTerrainDecoLayer* DecoLayer	= ParentInfo->GetDecoLayer();
								if (DecoLayer)
								{
									INT	Index	= Info->GetArrayIndex();
									if (Index != -1)
									{
										FTerrainDecoration*				Decoration	= &(DecoLayer->Decorations(Index));
										UStaticMeshComponentFactory*	Factory		= ConstructObject<UStaticMeshComponentFactory>(
											UStaticMeshComponentFactory::StaticClass(), CurrentTerrain);
										Factory->StaticMesh	= Cast<UStaticMesh>(Obj);
										Decoration->Factory	= Factory;
										bRefreshTerrain	= TRUE;
									}
									else
									{
										appMsgf(AMT_OK, TEXT("ERROR: Invalid array index"));
									}
								}
							}
						}
						break;
					}
				}
			}
			else
			if ((Info->GetLayerType() == TLT_TerrainMaterial) ||
				(Info->GetLayerType() == TLT_FilteredMaterial))
			{
				GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
				USelection* SelectionSet = GEditor->GetSelectedObjects();
				UObject* ObjTerrainMaterial	= SelectionSet->GetTop(UTerrainMaterial::StaticClass());
				UObject* ObjMaterial		= SelectionSet->GetTop(UMaterial::StaticClass());
				UObject* ObjMaterialInst	= SelectionSet->GetTop(UMaterialInstanceConstant::StaticClass());
				
				if (ObjTerrainMaterial || ObjMaterial || ObjMaterialInst)
				{
					// Grab the parent info layer setup...
					if (ParentInfo)
					{
						FTerrainLayer* Layer	= ParentInfo->GetLayer();
						if (Layer && Layer->Setup)
						{
							INT	Index	= Info->GetArrayIndex();
							if (Index != -1)
							{
								FTerrainFilteredMaterial*	FilteredMaterial	= &(Layer->Setup->Materials(Index));
								if (ObjTerrainMaterial)
								{
									FilteredMaterial->Material	= Cast<UTerrainMaterial>(ObjTerrainMaterial);
								}
								else
								if (ObjMaterial || ObjMaterialInst)
								{
									if (FilteredMaterial->Material)
									{
										UMaterial* NewMat = NULL;
										UMaterialInterface* NewMatInst = NULL;
										if (ObjMaterialInst)
										{
											NewMatInst = Cast<UMaterialInterface>(ObjMaterialInst);
											if (FilteredMaterial->Material->Material != NewMatInst)
											{
												if (CurrentTerrain && FilteredMaterial->Material->Material)
												{
													CurrentTerrain->RemoveCachedMaterial(FilteredMaterial->Material->Material->GetMaterial());
												}
												FilteredMaterial->Material->Material = NewMatInst;
												if (CurrentTerrain)
												{
													CurrentTerrain->UpdateCachedMaterial(FilteredMaterial->Material->Material->GetMaterial());
												}
											}
										}
										else
										{
											NewMat	= Cast<UMaterial>(ObjMaterial);
											if (FilteredMaterial->Material->Material != NewMat)
											{
												if (CurrentTerrain && FilteredMaterial->Material->Material)
												{
													CurrentTerrain->RemoveCachedMaterial(FilteredMaterial->Material->Material->GetMaterial());
												}
												FilteredMaterial->Material->Material = NewMat;
												if (CurrentTerrain)
												{
													CurrentTerrain->UpdateCachedMaterial(FilteredMaterial->Material->Material->GetMaterial());
												}
											}
										}

										FilteredMaterial->Material->MarkPackageDirty();
									}
									else
									{
										appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainBrowser_InvalidTerrainMaterial"));
									}
								}
								bRefreshTerrain	= TRUE;

								Layer->Setup->MarkPackageDirty();
							}
							else
							{
								appMsgf(AMT_OK, TEXT("ERROR: Invalid array index"));
							}
						}
					}
				}
			}

		}
	
		if (bRefreshTerrain)
		{
			RefreshTerrain();
		}

		EndTransaction(TEXT("TLB_UseSelected"));
	}

	return TRUE;
}

/**
 *	Checks the validity of adding the given type
 *
 *	@param	Type	Type of item to add
 *
 *	@return	TRUE	if valid for add
 *			FALSE	if not
 */
UBOOL WxTerrainEditor::CheckAddValidity(ETerrainLayerType Type)
{
	if (CurrentTerrain == NULL)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("TerrainEdit_NoTerrainSelected"), GetTerrainLayerTypeString(Type));
		return FALSE;
	}

	return TRUE;
}

/**
 *	Refreshes the terrain
 *
 *	@param	bCollectGarbage		Indicates whether garbage collection should be performed.
 */
void WxTerrainEditor::RefreshTerrain(UBOOL bCollectGarbage)
{
	if (CurrentTerrain)
	{
		ViewSettingsWireframeColor.Button->SetBackgroundColour(wxColour(CurrentTerrain->WireframeColor.R, CurrentTerrain->WireframeColor.G, CurrentTerrain->WireframeColor.B));

		CurrentTerrain->ClearComponents();
		if (bCollectGarbage)
		{
			UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		CurrentTerrain->PostEditChange();
		Update();
	}
}

/**
 *	BeginTransaction
 *
 *	@param	Transaction		String naming the transaction
 *
 *	@return	TRUE			If succeeds
 *			FALSE			If fails - transaction was in progress
 */
UBOOL WxTerrainEditor::BeginTransaction(const TCHAR* Transaction)
{
	if (IsTransactionInProgress() == TRUE)
	{
		FString ErrorString(*LocalizeUnrealEd("Error_FailedToBeginTransaction"));
		ErrorString += TransactionName;
		check(!*ErrorString);
		return FALSE;
	}

	GEditor->BeginTransaction(Transaction);
	TransactionName = FString(Transaction);
	bTransactionInProgress = TRUE;

	TransactionModify();

	return TRUE;
}

/**
 *	EndTransaction
 *
 *	@param	Transaction		String naming the transaction
 *
 *	@return	TRUE			If succeeds
 *			FALSE			If fails - transaction was not in progress, or transaction name mismatched
 */
UBOOL WxTerrainEditor::EndTransaction(const TCHAR* Transaction)
{
	if (IsTransactionInProgress() == FALSE)
	{
		FString ErrorString(*LocalizeUnrealEd("Error_FailedToEndTransaction"));
		ErrorString += TransactionName;
		check(!*ErrorString);
		return FALSE;
	}

	if (appStrcmp(*TransactionName, Transaction) != 0)
	{
		debugf(TEXT("TerrainEditor -   EndTransaction = %s --- Curr = %s"), 
			Transaction, *TransactionName);
		return FALSE;
	}

	GEditor->EndTransaction();

	TransactionName = TEXT("");
	bTransactionInProgress = FALSE;

	return TRUE;
}

/**
 *	TransactionModify
 *	Used to house the code that will 'touch' the terrain at the start of a transaction
 */
void WxTerrainEditor::TransactionModify()
{
	if (CurrentTerrain)
	{
		CurrentTerrain->Modify();
	}
}

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
UBOOL WxTerrainEditor::CreateNewItem(ETerrainLayerType eType, FString& ObjectName, void*& CreatedItem, UPackage* InPackage, UBOOL bPrompt)
{
	if (CurrentTerrain == NULL)
	{
		return FALSE;
	}

	FString DefaultPackageName(TEXT(""));
	UPackage* Package = InPackage;
		
	if (Package == NULL)
	{
		Package = Cast<UPackage>(CurrentTerrain->GetOutermost());
	}
	else
	{
		// Passing in a package trumps prompting
		bPrompt = FALSE;
	}

	if (Package)
	{
		DefaultPackageName = Package->GetName();
	}
	else
	{
		DefaultPackageName = TEXT("MyPackage");
	}

	FTerrainBrowserBlock* Info = NULL;
	if (CurrentRightClickIndex != -1)
	{
		Info = &InfoBlocks(CurrentRightClickIndex);
	}

	UClass* FactoryClass = NULL;
	UClass* RequiredClass = NULL;

	switch (eType)
	{
	case TLT_Layer:
		{
			debugf(TEXT("Create Called for LAYER!"));
			FactoryClass = UTerrainLayerSetupFactoryNew::StaticClass();
			RequiredClass = UTerrainLayerSetup::StaticClass();
		}
		break;
	case TLT_FilteredMaterial:
	case TLT_TerrainMaterial:
		{
			debugf(TEXT("Create Called for TERRAIN MATERIAL!"));
			FactoryClass = UTerrainMaterialFactoryNew::StaticClass();
			RequiredClass = UTerrainMaterial::StaticClass();
		}
		break;
	default:
		return FALSE;
	}

	if (!FactoryClass)
	{
		appMsgf(AMT_OK, TEXT("Invalid factory class! TODO. Localize me!!!!"));
		return FALSE;
	}

	check(FactoryClass->IsChildOf(UFactory::StaticClass()));

	WxDlgNewGeneric dlg;

	UPackage* pkg = NULL;
	UPackage* grp = NULL;

	UBOOL bResult = FALSE;

	GEditor->GetSelectedObjects()->DeselectAll();
	GEditor->GetSelectedActors()->DeselectAll();

	FString	PackageName	= DefaultPackageName;
	FString	GroupName(TEXT(""));
	FString	GroupAndName(TEXT(""));
	if (bPrompt)
	{
		if (dlg.ShowModal(*DefaultPackageName, TEXT(""), FactoryClass) == wxID_OK)
		{
			PackageName	= dlg.GetPackageName();
			GroupName	= dlg.GetGroupName();
			ObjectName	= dlg.GetObjectName();

			FString	ItemName		= PackageName;

			if (GroupName.Len() > 0)
			{
				ItemName += TEXT(".");
				ItemName += GroupName;
				GroupAndName += GroupName;
				GroupAndName += TEXT(".");
			}
			ItemName += TEXT(".");
			ItemName += ObjectName;
			GroupAndName += ObjectName;
		}
	}
	else
	{
		GroupAndName = ObjectName;

		UFactory* Factory = ConstructObject<UFactory>(FactoryClass);
		if (Factory)
		{
			UObject* NewObj = Factory->FactoryCreateNew(Factory->GetSupportedClass(), Package, FName(*GroupAndName), RF_Public|RF_Standalone, NULL, GWarn);
			if (NewObj)
			{
				// Set the new objects as the sole selection.
				USelection* SelectionSet = GEditor->GetSelectedObjects();
				SelectionSet->DeselectAll();
				SelectionSet->Select(NewObj);
			}
		}
	}

	UObject* Obj;
	UPackage* LocalPackage	= FindObject<UPackage>(NULL, *PackageName);
	if (LocalPackage)
	{
		Obj = UObject::StaticFindObject(RequiredClass, LocalPackage, *GroupAndName);
	}
	else
	{
		Obj	= GEditor->GetSelectedObjects()->GetTop(RequiredClass);
		if (Obj == NULL)
		{
			// This seems very very wrong...
			Obj	= GEditor->GetSelectedActors()->GetTop(RequiredClass);
		}
	}

	if (Obj)
	{
		if (FactoryClass == UTerrainLayerSetupFactoryNew::StaticClass())
		{
			UTerrainLayerSetup* Setup = Cast<UTerrainLayerSetup>(Obj);
			CreatedItem	= (void*)Setup;
			if (Setup)
			{
				bResult = TRUE;
			}
		}
		else
		if (FactoryClass == UTerrainMaterialFactoryNew::StaticClass())
		{
			UTerrainMaterial* Material =  Cast<UTerrainMaterial>(Obj);
			CreatedItem	= (void*)Material;
			if (Material)
			{
				bResult = TRUE;
			}
		}
	}

	return bResult;
}

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
INT WxTerrainEditor::GetSelectedItem(ETerrainLayerType eType, void*& SelectedItem)
{
	INT		ItemCount		= 0;
	UClass*	RequiredClass	= NULL;
	UBOOL	bInvalid		= FALSE;

	SelectedItem = NULL;

	switch (eType)
	{
	case TLT_Layer:
		{
			RequiredClass	= UTerrainLayerSetup::StaticClass();
		}
		break;
	case TLT_TerrainMaterial:
	case TLT_FilteredMaterial:
		{
		}
		break;
	case TLT_Decoration:
		{
			RequiredClass	= UStaticMesh::StaticClass();
		}
		break;
	case TLT_DisplacementMap:
		{
			RequiredClass	= UTexture2D::StaticClass();
		}
		break;
	default:
		{
			debugf(TEXT("TerrainEditor::GetSelectedItem> Invalid type (0x%08x)"), (INT)eType);
			bInvalid = TRUE;
		}
		break;
	}

	if (bInvalid == FALSE)
	{
		// Make sure the object is loaded...
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

		if (RequiredClass)
		{
			UObject* Obj = GEditor->GetSelectedObjects()->GetTop(RequiredClass);
			if (Obj)
			{
				switch (eType)
				{
				case TLT_Layer:
					{
						UTerrainLayerSetup* Setup = Cast<UTerrainLayerSetup>(Obj);
						SelectedItem = (void*)Setup;
						if (Setup)
						{
							ItemCount++;
						}
					}
					break;
				case TLT_TerrainMaterial:
				case TLT_FilteredMaterial:
					{
						UTerrainMaterial* Material = Cast<UTerrainMaterial>(Obj);
						SelectedItem = (void*)Material;
						if (SelectedItem)
						{
							ItemCount++;
						}
					}
					break;
				case TLT_Decoration:
					{
						UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
						SelectedItem = (void*)StaticMesh;
						if (SelectedItem)
						{
							ItemCount++;
						}
					}
					break;
				case TLT_DisplacementMap:
					{
						UTexture2D* Texture = Cast<UTexture2D>(Obj);
						SelectedItem = (void*)Texture;
						if (SelectedItem)
						{
							ItemCount++;
						}
					}
					break;
				}
			}
		}
		else
		if ((eType == TLT_TerrainMaterial) || (eType == TLT_FilteredMaterial))
		{
			USelection* SelectionSet = GEditor->GetSelectedObjects();
			UObject* ObjTerrainMaterial	= SelectionSet->GetTop(UTerrainMaterial::StaticClass());
			UObject* ObjMaterial		= SelectionSet->GetTop(UMaterial::StaticClass());
			UObject* ObjMaterialInst	= SelectionSet->GetTop(UMaterialInstanceConstant::StaticClass());
			
			if (ObjTerrainMaterial)
			{
				SelectedItem = (void*)ObjTerrainMaterial;
			}
			if (ObjMaterial)
			{
				SelectedItem = (void*)ObjMaterial;
			}
			if (ObjMaterialInst)
			{
				SelectedItem = (void*)ObjMaterialInst;
			}

			if (SelectedItem)
			{
				ItemCount++;
			}
		}
	}

	return ItemCount;
}

/**
 *	Create a new layer
 *
 *	@param	ObjectName		The name of the layer to create
 *	@param	bPrompt			If TRUE, prompt the user for the package/group/name
 *
 *	@return	Pointer to the created layer
 */
UTerrainLayerSetup* WxTerrainEditor::CreateNewLayer(FString& ObjectName, UPackage* InPackage, UBOOL bPrompt)
{
	void* CreatedItem;
	if (CreateNewItem(TLT_Layer, ObjectName, CreatedItem, InPackage, bPrompt) == TRUE)
	{
		UTerrainLayerSetup* Setup = static_cast<UTerrainLayerSetup*>(CreatedItem);
		return Setup;
	}

	return NULL;
}

/**
 *	Create a new terrain material
 *
 *	@return	Pointer to the created terrain material
 */
UTerrainMaterial* WxTerrainEditor::CreateNewTerrainMaterial(FString& ObjectName, UPackage* InPackage, UBOOL bPrompt)
{
	void* CreatedItem;
	if (CreateNewItem(TLT_TerrainMaterial, ObjectName, CreatedItem, InPackage, bPrompt) == TRUE)
	{
		UTerrainMaterial* Material = static_cast<UTerrainMaterial*>(CreatedItem);
		return Material;
	}

	return NULL;
}

/**
 *	Shift the selected item up or down in the list
 *
 *	@param	bUp		If TRUE, shift the item up in the list, else shift down
 */
void WxTerrainEditor::ShiftSelected(UBOOL bUp)
{
	if ((CurrentTerrain == NULL) || (SelectedBlock == -1))
	{
		return;
	}

	FTerrainBrowserBlock* Info			= NULL;
	FTerrainBrowserBlock* ParentInfo	= NULL;

	UBOOL bRetainAlpha	= RetainAlpha->IsChecked();

	if (SelectedBlock != -1)
	{
		Info = &InfoBlocks(SelectedBlock);
	}

	if (SelectedChildBlock != -1)
	{
		ParentInfo	= Info;
		Info		= &InfoBlocks(SelectedChildBlock);
	}

	UBOOL bRefillInfo = FALSE;

	if (Info)
	{
		switch (Info->GetLayerType())
		{
		case TLT_HeightMap:
			{
				debugf(*LocalizeUnrealEd("TerrainEditor_ERROR_ShiftHeighMap"));
			}
			break;
		case TLT_Layer:
			{
				if (CurrentTerrain->Layers.Num() <= 1)
				{
					// Nowhere to shift to...
					return;
				}

				// Find the layer in the terrain array
				FTerrainLayer* Layer = Info->GetLayer();
				check(Layer);

				FTerrainLayer LayerCopy;

				INT	LayerIndex	= -1;
				INT LayerCount	= CurrentTerrain->Layers.Num();

				for (INT Index = 0; Index < LayerCount; Index++)
				{
					if (Layer == &(CurrentTerrain->Layers(Index)))
					{
						LayerIndex = Index;
						break;
					}
				}

				if (bUp)
				{
					if (LayerIndex > 0)
					{
						// Swap the layers
						INT	TargetIndex	= LayerIndex - 1;

						FTerrainLayer TargetLayer = CurrentTerrain->Layers(TargetIndex);

						LayerCopy = *Layer;

						CurrentTerrain->Layers(LayerIndex) = TargetLayer;
						CurrentTerrain->Layers(TargetIndex) = LayerCopy;

						if (CurrentTerrain->NormalMapLayer == LayerIndex)
						{
							// Adjust it
							CurrentTerrain->NormalMapLayer--;
						}

						//@todo. If retaining alpha, we need to swap the alpha map...

						bRefillInfo = TRUE;
					}
				}
				else
				{
					if (LayerIndex + 1 < LayerCount)
					{
						// Swap the layers
						INT	TargetIndex	= LayerIndex + 1;

						FTerrainLayer TargetLayer = CurrentTerrain->Layers(TargetIndex);

						LayerCopy = *Layer;

						CurrentTerrain->Layers(LayerIndex) = TargetLayer;
						CurrentTerrain->Layers(TargetIndex) = LayerCopy;

						if (CurrentTerrain->NormalMapLayer == LayerIndex)
						{
							// Adjust it
							CurrentTerrain->NormalMapLayer++;
						}

						//@todo. If retaining alpha, we need to swap the alpha map...

						bRefillInfo = TRUE;
					}
				}
			}
			break;
		case TLT_DecoLayer:
			{
				if (CurrentTerrain->DecoLayers.Num() <= 1)
				{
					// Nowhere to shift to...
					return;
				}

				// Find the layer in the terrain array
				FTerrainDecoLayer* DecoLayer = Info->GetDecoLayer();
				check(DecoLayer);

				FTerrainDecoLayer DecoLayerCopy;

				INT	DecoLayerIndex	= -1;
				INT DecoLayerCount	= CurrentTerrain->DecoLayers.Num();

				for (INT Index = 0; Index < DecoLayerCount; Index++)
				{
					if (DecoLayer == &(CurrentTerrain->DecoLayers(Index)))
					{
						DecoLayerIndex = Index;
						DecoLayerCopy = CurrentTerrain->DecoLayers(Index);
						break;
					}
				}

				if (bUp)
				{
					if (DecoLayerIndex > 0)
					{
						// Swap the layers
						INT	TargetIndex	= DecoLayerIndex - 1;

						FTerrainDecoLayer TargetDecoLayer = CurrentTerrain->DecoLayers(TargetIndex);

						CurrentTerrain->DecoLayers(DecoLayerIndex) = TargetDecoLayer;
						CurrentTerrain->DecoLayers(TargetIndex) = DecoLayerCopy;

						//@todo. If retaining alpha, we need to swap the alpha map...

						bRefillInfo = TRUE;
					}
				}
				else
				{
					if (DecoLayerIndex + 1 < DecoLayerCount)
					{
						// Swap the layers
						INT	TargetIndex	= DecoLayerIndex + 1;

						FTerrainDecoLayer TargetDecoLayer = CurrentTerrain->DecoLayers(TargetIndex);

						DecoLayerCopy = *DecoLayer;

						CurrentTerrain->DecoLayers(DecoLayerIndex) = TargetDecoLayer;
						CurrentTerrain->DecoLayers(TargetIndex) = DecoLayerCopy;

						//@todo. If retaining alpha, we need to swap the alpha map...

						bRefillInfo = TRUE;
					}
				}
			}
			break;
		case TLT_TerrainMaterial:
		case TLT_FilteredMaterial:
			{
				check(ParentInfo);
				if (ParentInfo->GetLayerType() == TLT_Layer)
				{
					FTerrainLayer* Layer = ParentInfo->GetLayer();
					FTerrainFilteredMaterial* Material = Info->GetFilteredMaterial();

					if (Layer && Layer->Setup && Material)
					{
						// Find the material in the array
						INT	MaterialIndex = -1;
						INT MaterialCount = Layer->Setup->Materials.Num();

						for (INT Index = 0; Index < MaterialCount; Index++)
						{
							if (Layer->Setup->Materials(Index) == *Material)
							{
								MaterialIndex = Index;
								break;
							}
						}

						if (MaterialIndex != -1)
						{
							if (bUp)
							{
								if (MaterialIndex > 0)
								{
									FTerrainFilteredMaterial Temp = Layer->Setup->Materials(MaterialIndex - 1);
									Layer->Setup->Materials(MaterialIndex - 1) = *Material;
									Layer->Setup->Materials(MaterialIndex) = Temp;
								}
							}
							else
							{
								if (MaterialIndex + 1 < MaterialCount)
								{
									FTerrainFilteredMaterial Temp = Layer->Setup->Materials(MaterialIndex + 1);
									Layer->Setup->Materials(MaterialIndex + 1) = *Material;
									Layer->Setup->Materials(MaterialIndex) = Temp;
								}
							}
						}
					}
				}
			}
			break;
		case TLT_Decoration:
			{
				if (ParentInfo->GetLayerType() == TLT_DecoLayer)
				{
					FTerrainDecoLayer* DecoLayer = ParentInfo->GetDecoLayer();
					FTerrainDecoration* Decoration = Info->GetDecoration();

					if (DecoLayer && Decoration)
					{
						// Find the decoration in the array
						INT	DecorationIndex = -1;
						INT DecorationCount = DecoLayer->Decorations.Num();

						for (INT Index = 0; Index < DecorationCount; Index++)
						{
							if (DecoLayer->Decorations(Index) == *Decoration)
							{
								DecorationIndex = Index;
								break;
							}
						}

						if (DecorationIndex != -1)
						{
							if (bUp)
							{
								if (DecorationIndex > 0)
								{
									FTerrainDecoration Temp = DecoLayer->Decorations(DecorationIndex - 1);
									DecoLayer->Decorations(DecorationIndex - 1) = *Decoration;
									DecoLayer->Decorations(DecorationIndex) = Temp;
								}
							}
							else
							{
								if (DecorationIndex + 1 < DecorationCount)
								{
									FTerrainDecoration Temp = DecoLayer->Decorations(DecorationIndex + 1);
									DecoLayer->Decorations(DecorationIndex + 1) = *Decoration;
									DecoLayer->Decorations(DecorationIndex) = Temp;
								}
							}
						}
					}
				}
			}
			break;
		}
	}

	if (bRefillInfo)
	{
		FillInfoArray();
		RefreshTerrain();
		CurrentTerrain->MarkPackageDirty();
	}

	LayerViewport->Viewport->Invalidate();
}
