/*=============================================================================
	UIEditor.cpp
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "WarfareGame.h"


void UUIContainerFactory::StaticConstructor()
{
	SupportedClass = UUIContainer::StaticClass();
	bCreateNew = 1;
	Description = TEXT("UI Container");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

UObject* UUIContainerFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

#ifndef _XBOX

#include "UnLinkedObjDrawUtils.h"

void WxUIEditor::OpenNewObjectMenu()
{
	wxMenu* menu = new EUIEdNewObjectMenu(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxUIEditor::OpenObjectOptionsMenu()
{
	wxMenu* menu = new EUIEdObjectOptionsMenu(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxUIEditor::DrawLinkedObjects(FRenderInterface* RI)
{
	WxLinkedObjEd::DrawLinkedObjects(RI);
	if (Container != NULL)
	{
		// create a temporary canvas for the elements to render with
		UCanvas *tmpCanvas = ConstructObject<UCanvas>(UCanvas::StaticClass(),UObject::GetTransientPackage(),TEXT("tmpCanvas"));
		check(tmpCanvas != NULL && TEXT("Failed to create canvas"));
		tmpCanvas->RenderInterface = RI;
		tmpCanvas->SizeX = Width;
		tmpCanvas->SizeY = Height;
		FLOAT sizeX = LinkedObjVC->Viewport->GetSizeX()/2.f - Width/2.f, sizeY = LinkedObjVC->Viewport->GetSizeY()/2.f - Height/2.f;
		tmpCanvas->OrgX = sizeX;
		tmpCanvas->OrgY = sizeY;
		tmpCanvas->ClipX = sizeX + tmpCanvas->SizeX;
		tmpCanvas->ClipY = sizeY + tmpCanvas->SizeY;
		// draw background to represent the screen space
		RI->DrawTile(sizeX,sizeY,Width,Height,0,0,2,2,FColor(47,47,47,128));
		// draw the grid lines
		if (bShowGrid)
		{
			// vertical lines first
			INT gridCnt = (INT)(Width/GridSize);
			for (INT idx = 0; idx < gridCnt; idx++)
			{
				INT drawX = sizeX + idx * GridSize, drawY = sizeY;
				RI->DrawLine2D(drawX,drawY,drawX,drawY+Height,FColor(47,47,47,255));
			}
			// horizontal lines
			gridCnt = (INT)(Height/GridSize);
			for (INT idx = 0; idx < gridCnt; idx++)
			{
				INT drawX = sizeX, drawY = sizeY + idx * GridSize;
				RI->DrawLine2D(drawX,drawY,drawX+Width,drawY,FColor(47,47,47,255));
			}
		}
		// draw the container contents
		// NOTE: draw the elements individually so that we can specify hit proxies for each one
		UBOOL bHitTesting = 1;
		for (INT idx = 0; idx < Container->Elements.Num(); idx++)
		{
			UUIElement *element = Container->Elements(idx);
			if (element != NULL)
			{
				INT drawW = Width, drawH = Height, drawX, drawY;
				element->GetDimensions(drawX,drawY,drawW,drawH);
				drawX += sizeX - 2;
				drawY += sizeY - 2;
				if (element->bSelected)
				{
					// draw a border if selected
					drawW += 4;
					drawH += 4;
					RI->DrawTile(drawX,drawY,drawW,drawH,0,0,2,2,FColor(255,0,0));
				}
				if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(element) );
				element->DrawElement(tmpCanvas);
				if(bHitTesting) RI->SetHitProxy( NULL );
				// and draw a handle for scaling
				if (element->bSelected &&
					element->bSizeDrag)
				{
					const INT HandleSize = 10;
					FIntPoint A(drawX + drawW, drawY + drawH);
					FIntPoint B(drawX + drawW, drawY + drawH - HandleSize);
					FIntPoint C(drawX + drawW - HandleSize,	drawY + drawH);

					if(bHitTesting)  RI->SetHitProxy( new HLinkedObjProxySpecial(element, idx) );
					RI->DrawTriangle2D( A, 0.f, 0.f, B, 0.f, 0.f, C, 0.f, 0.f, FColor(0,0,0) );
					if(bHitTesting)  RI->SetHitProxy( NULL );
				}
				// draw name if specified
				if (bShowNames)
				{
					FString elementName = element->GetName();
					if (element->bAcceptsFocus)
					{
						elementName = FString::Printf(TEXT("%s - %d"),*elementName,element->FocusId);
					}
					INT textW, textH;
					UCanvas::ClippedStrLen(GEngine->SmallFont,1.f,1.f,textW,textH,*elementName);
					if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(element) );
					RI->DrawTile(drawX,drawY,
								 textW,textH,
								 0,0,
								 2,2,
								 FColor(32,32,32,255));
					if(bHitTesting) RI->SetHitProxy( NULL );
					RI->DrawString(drawX,drawY,*elementName,GEngine->SmallFont,FColor(255,255,255,255));
				}
			}
		}
		delete tmpCanvas;
	}
}

/**
 * Sets the selected objects for the property window, using the Container if there
 * are no objects selected.
 */
void WxUIEditor::UpdatePropertyWindow()
{
	if (SelectedElements.Num() == 0)
	{
		PropertyWindow->SetObject(Container,0,0,1);
	}
	else
	{
		TArray<UObject*> selectedObjs;
		for (INT idx = 0; idx < SelectedElements.Num(); idx++)
		{
			selectedObjs.AddItem(SelectedElements(idx).Element);
		}
		PropertyWindow->SetObjectArray(&selectedObjs,0,0,1);
	}
}

void WxUIEditor::EmptySelection()
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{
		SelectedElements(idx).Element->bSelected = 0;
	}
	SelectedElements.Empty();
}

void WxUIEditor::AddToSelection( UObject* Obj )
{
	UUIElement *element = Cast<UUIElement>(Obj);
	if (element != NULL)
	{
		element->bSelected = 1;
		// add to selected list
		INT idx = SelectedElements.AddZeroed();
		SelectedElements(idx).Element = element;
		// grab the initial position values
		SelectedElements(idx).ElementX = element->DrawX;
		SelectedElements(idx).ElementY = element->DrawY;
		SelectedElements(idx).ElementW = element->DrawW;
		SelectedElements(idx).ElementH = element->DrawH;
	}
	// apply initial snapping
	SnapSelected();
}

void WxUIEditor::SnapSelected()
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{
		UUIElement *element = Cast<UUIElement>(SelectedElements(idx).Element);
		if (element != NULL)
		{
			if (bSnapToGridXY &&
				GridSize != 0.f)
			{
				// grab snapped values
				element->DrawX = GridSize * appRound(SelectedElements(idx).ElementX/GridSize);
				element->DrawY = GridSize * appRound(SelectedElements(idx).ElementY/GridSize);
			}
			else
			{
				// otherwise use the absolute values
				element->DrawX = SelectedElements(idx).ElementX;
				element->DrawY = SelectedElements(idx).ElementY;
			}
			// check for width/height snapping
			if (element->bSizeDrag)
			{
				if (bSnapToGridWH &&
					GridSize != 0.f)
				{
					element->DrawW = Max(GridSize,(GridSize * appRound((element->DrawX + SelectedElements(idx).ElementW)/GridSize)) - element->DrawX);
					element->DrawH = Max(GridSize,(GridSize * appRound((element->DrawY + SelectedElements(idx).ElementH)/GridSize)) - element->DrawY);
				}
				else
				{
					element->DrawW = SelectedElements(idx).ElementW;
					element->DrawH = SelectedElements(idx).ElementH;
				}
			}
		}
	}
}

UBOOL WxUIEditor::IsInSelection( UObject* Obj )
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{
		if (SelectedElements(idx).Element == Obj)
		{
			return 1;
		}
	}
	return 0;
}

UBOOL WxUIEditor::HaveObjectsSelected()
{
	return (SelectedElements.Num() > 0);
}


void WxUIEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{
		UUIElement *element = SelectedElements(idx).Element;
		if (element != NULL)
		{
			SelectedElements(idx).ElementX += DeltaX;
			SelectedElements(idx).ElementY += DeltaY;
		}
	}
	SnapSelected();
}

void WxUIEditor::EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);
	if (Event == IE_Pressed)
	{
		if(Key == KEY_Delete)
		{
			// Delete selection etc...
			for (INT idx = 0; idx < SelectedElements.Num(); idx++)
			{
				UUIElement *element = SelectedElements(idx).Element;
				if (element != NULL)
				{
					INT elementIdx = Container->Elements.FindItemIndex(element);
					if (elementIdx != INDEX_NONE)
					{
						Container->Elements.Remove(elementIdx,1);
						SelectedElements.Remove(idx--,1);
						delete element;
					}
				}
			}
		}
		else
		if (bCtrlDown && Key == KEY_W)
		{
			for (INT idx = 0; idx < SelectedElements.Num(); idx++)
			{
				if (SelectedElements(idx).Element != NULL)
				{
					UUIElement *newElement = ConstructObject<UUIElement>(SelectedElements(idx).Element->GetClass(),Container,NAME_None,0,SelectedElements(idx).Element);
					if (newElement != NULL)
					{
						Container->Elements.AddItem(newElement);
						// offset slightly
						newElement->DrawX += 0.1f;
						newElement->DrawY += 0.1f;
						// replace the duplicated w/ the duplicatee
						SelectedElements(idx).Element->bSelected = 0;
						SelectedElements(idx).Element = newElement;
					}
				}
			}
		}
		else
		if (Key == KEY_Left ||
			Key == KEY_Right ||
			Key == KEY_Up ||
			Key == KEY_Down)
		{
			INT moveSize = GridSize == 0.f ? 2 : (INT)GridSize;
			INT deltaX = 0, deltaY = 0;
			if (Key == KEY_Left)
			{
				deltaX -= moveSize;
			}
			else
			if (Key == KEY_Right)
			{
				deltaX += moveSize;
			}
			else
			if (Key == KEY_Up)
			{
				deltaY -= moveSize;
			}
			else
			if (Key == KEY_Down)
			{
				deltaY += moveSize;
			}
			MoveSelectedObjects(deltaX,deltaY);
		}
	}
}

void WxUIEditor::OnMouseOver(UObject* Obj)
{

}


void WxUIEditor::SpecialDrag( INT DeltaX, INT DeltaY, INT SpecialIndex )
{
	// Handle dragging handles and stuff
	UUIElement *element = Container->Elements(SpecialIndex);
	if (element != NULL)
	{
		// find which selection this is
		for (INT idx = 0; idx < SelectedElements.Num(); idx++)
		{
			if (SelectedElements(idx).Element == element)
			{
				// updated the real values
				SelectedElements(idx).ElementW += DeltaX;
				SelectedElements(idx).ElementH += DeltaY;
			}
		}
		// and apply any snapping
		SnapSelected();
	}
}

void WxUIEditor::OnContextNewObject(wxCommandEvent& In)
{
	INT classIdx = In.GetId() - IDM_UIEDITOR_NEWOBJECT_START;
	INT elementClassCnt = 0;
	UClass *elementClass = NULL;
	for (TObjectIterator<UClass> It; It && elementClass == NULL; ++It)
	{
		if (It->IsChildOf(UUIElement::StaticClass()) &&
			!(It->ClassFlags & CLASS_Abstract))
		{
			if (classIdx == elementClassCnt)
			{
				elementClass = *It;
			}
			elementClassCnt++;
		}
	}
	if (elementClass != NULL)
	{
		UUIElement *element = ConstructObject<UUIElement>(elementClass,Container,NAME_None);
		if (element != NULL)
		{
			Container->Elements.AddItem(element);
			element->DrawX = LinkedObjVC->NewX;
			element->DrawY = LinkedObjVC->NewY;
		}
	}
}

void WxUIEditor::OnContextMoveObjectLevel(wxCommandEvent& In)
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{
		UUIElement *element = SelectedElements(idx).Element;
		if (element != NULL)
		{
			INT elementIdx = Container->Elements.FindItemIndex(element);
			if (elementIdx != INDEX_NONE)
			{
				switch (In.GetId())
				{
				case IDM_UIEDITOR_MOVEUP:
					if (elementIdx < Container->Elements.Num() - 1)
					{
						Container->Elements.SwapItems(elementIdx,elementIdx+1);
					}
					break;
				case IDM_UIEDITOR_MOVEDOWN:
					if (elementIdx > 0)
					{
						Container->Elements.SwapItems(elementIdx,elementIdx-1);
					}
					break;
				case IDM_UIEDITOR_MOVETOTOP:
					if (elementIdx != Container->Elements.Num() - 1)
					{
						Container->Elements.AddItem(element);
						Container->Elements.Remove(elementIdx,1);
					}
					break;
				case IDM_UIEDITOR_MOVETOBOTTOM:
					if (elementIdx != 0)
					{
						Container->Elements.Remove(elementIdx,1);
						Container->Elements.Insert(0,1);
						Container->Elements(0) = element;
					}
					break;
				}
			}
		}
	}
}

void WxUIEditor::OnContextGridSize(wxCommandEvent &In)
{
	INT selection = In.GetInt();
	if (selection == 0)
	{
		bShowGrid = 0;
	}
	else
	{
		bShowGrid = 1;
		GridSize = (INT)appPow(2,selection);
	}
}

void WxUIEditor::OnContextToggleSnap(wxCommandEvent &In)
{
	if (In.GetId() == IDM_UIEDITOR_TOGGLESNAPXY)
	{
		bSnapToGridXY = In.IsChecked();
	}
	else
	if (In.GetId() == IDM_UIEDITOR_TOGGLESNAPWH)
	{
		bSnapToGridWH = In.IsChecked();
	}
}

void WxUIEditor::OnContextRename(wxCommandEvent &In)
{
	for (INT idx = 0; idx < SelectedElements.Num(); idx++)
	{				  
		UUIElement *element = SelectedElements(idx).Element;
		if (element != NULL)
		{
			WxDlgGenericStringEntry dlg;
			if (dlg.ShowModal(TEXT("Rename Element"), TEXT("Name:"), element->GetName()) == wxID_OK)
			{
				element->Modify();
				FString newElementName = dlg.EnteredString;
				newElementName = newElementName.Replace(TEXT(" "),TEXT("_"));
				element->Rename(*newElementName,Container);
			}
		}
	}
}

void WxUIEditor::OnContextToggleNames(wxCommandEvent &In)
{
	bShowNames = In.IsChecked();
}

/*-----------------------------------------------------------------------------
	WxUIEditor
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxUIEditor, wxFrame )
	EVT_MENU_RANGE(IDM_UIEDITOR_NEWOBJECT_START, IDM_UIEDITOR_NEWOBJECT_END, WxUIEditor::OnContextNewObject)
	EVT_MENU(IDM_UIEDITOR_MOVEUP, WxUIEditor::OnContextMoveObjectLevel)
	EVT_MENU(IDM_UIEDITOR_MOVEDOWN, WxUIEditor::OnContextMoveObjectLevel)
	EVT_MENU(IDM_UIEDITOR_MOVETOTOP, WxUIEditor::OnContextMoveObjectLevel)
	EVT_MENU(IDM_UIEDITOR_MOVETOBOTTOM, WxUIEditor::OnContextMoveObjectLevel)
	EVT_COMBOBOX(IDM_UIEDITOR_GRIDCOMBO, WxUIEditor::OnContextGridSize)
	EVT_MENU(IDM_UIEDITOR_TOGGLESNAPXY, WxUIEditor::OnContextToggleSnap)
	EVT_MENU(IDM_UIEDITOR_TOGGLESNAPWH, WxUIEditor::OnContextToggleSnap)
	EVT_MENU(IDM_UIEDITOR_RENAME, WxUIEditor::OnContextRename)
	EVT_MENU(IDM_UIEDITOR_TOGGLENAMES, WxUIEditor::OnContextToggleNames)
END_EVENT_TABLE()


WxUIEditor::WxUIEditor( wxWindow* InParent, wxWindowID InID, UUIContainer *editContainer )
: WxLinkedObjEd( InParent, InID, TEXT("UIEditor") )
, Container(editContainer)
, Width(640)
, Height(480)
, bShowGrid(0)
, bSnapToGridXY(0)
, bSnapToGridWH(0)
, GridSize(16)
{
	LinkedObjVC->MaxZoom2D = 2.f;
	ToolBar = new EUIEdToolBar( this, -1 );
	SetToolBar( ToolBar );
	BackgroundTexture = NULL;
	//BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.SoundCueBackground"), NULL, LOAD_NoFail, NULL);
}

WxUIEditor::~WxUIEditor()
{

}

/*-----------------------------------------------------------------------------
	EUIEdToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( EUIEdToolBar, wxToolBar )
END_EVENT_TABLE()

EUIEdToolBar::EUIEdToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	// load some bitmaps, call AddTool a bit...

	SetToolBitmapSize(wxSize(18,18));
	WxMaskedBitmap snapXYB, snapWHB;
	snapXYB.Load(TEXT("MAT_ToggleSnap"));
	AddCheckTool(IDM_UIEDITOR_TOGGLESNAPXY, TEXT("Toggle Snap X/Y To Grid"), snapXYB);
	snapWHB.Load(TEXT("MAT_ToggleSnap"));
	AddCheckTool(IDM_UIEDITOR_TOGGLESNAPWH, TEXT("Toggle Snap W/H To Grid"), snapWHB);
	AddSeparator();
	wxComboBox *gridSize = new wxComboBox( this, IDM_UIEDITOR_GRIDCOMBO, TEXT("Grid"), wxDefaultPosition, wxSize(60, -1), 0, NULL, wxCB_READONLY );
	gridSize->Append(TEXT("Off"));
	for (INT idx = 1; idx < 6; idx++)
	{
		gridSize->Append(*FString::Printf(TEXT("%d"),(INT)appPow(2,idx)));
	}
	gridSize->SetSelection(0);
	AddControl(gridSize);
	AddSeparator();
	WxMaskedBitmap toggleNamesB;
	toggleNamesB.Load(TEXT("MAT_AddKey"));
	AddCheckTool(IDM_UIEDITOR_TOGGLENAMES, TEXT("Toggle Element Names"), toggleNamesB);

	Realize();
}

EUIEdToolBar::~EUIEdToolBar()
{
}

/*-----------------------------------------------------------------------------
	EUIEdNewObjectMenu.
-----------------------------------------------------------------------------*/

static wxMenu* BuildNewElementMenu()
{
	wxMenu *elementMenu = new wxMenu();
	INT elementClassCnt = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UUIElement::StaticClass()) &&
			!(It->ClassFlags & CLASS_Abstract))
		{
			elementMenu->Append(IDM_UIEDITOR_NEWOBJECT_START+elementClassCnt++,It->GetName(),TEXT(""));
		}
	}
	return elementMenu;
}

EUIEdNewObjectMenu::EUIEdNewObjectMenu(WxUIEditor* UIEditor)
{
	// Add all the things you want to be able to create
	Append(IDM_UIEDITOR_NEWOBJECT, TEXT("New Element"), BuildNewElementMenu());
}

EUIEdNewObjectMenu::~EUIEdNewObjectMenu()
{

}

/*-----------------------------------------------------------------------------
	EUIEdObjectOptionsMenu.
-----------------------------------------------------------------------------*/

EUIEdObjectOptionsMenu::EUIEdObjectOptionsMenu(WxUIEditor* UIEditor)
{
	// Add any options based on the current selection.
	Append(IDM_UIEDITOR_NEWOBJECT, TEXT("New Element"), BuildNewElementMenu());
	AppendSeparator();
	Append(IDM_UIEDITOR_MOVEUP,TEXT("Move Up"),TEXT(""));
	Append(IDM_UIEDITOR_MOVEDOWN,TEXT("Move Down"),TEXT(""));
	AppendSeparator();
	Append(IDM_UIEDITOR_MOVETOTOP,TEXT("Move To Top"),TEXT(""));
	Append(IDM_UIEDITOR_MOVETOBOTTOM,TEXT("Move To Bottom"),TEXT(""));
	AppendSeparator();
	Append(IDM_UIEDITOR_RENAME,TEXT("Rename Element(s)"),TEXT(""));
}

EUIEdObjectOptionsMenu::~EUIEdObjectOptionsMenu()
{

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif
