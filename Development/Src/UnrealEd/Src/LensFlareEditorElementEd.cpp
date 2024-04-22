/*=============================================================================
	LensFlareEditorElementEd.cpp: LensFlare editor element editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "CurveEd.h"
//#include "EngineLensFlareClasses.h"
#include "LensFlare.h"
#include "LensFlareEditor.h"

static const INT	ElementWidth = 172;
static const INT	ElementHeight = 172;
static const INT	ElementStringHeight = 20;
static const INT	ElementThumbBorder = 5;

static FLinearColor LFEmptyBackgroundColor(112,112,112);
static FLinearColor LFSourceElementBackgroundColor(120, 120, 120);
static FLinearColor LFSourceElementSelectedColor(255, 150, 50);
static FLinearColor LFSourceElementUnselectedColor(200, 200, 200);
static FLinearColor LFElementBackgroundColor(130, 130, 130);
static FLinearColor LFElementSelectedColor(255, 130, 30);
static FLinearColor LFElementUnselectedColor(180, 180, 180);

/*-----------------------------------------------------------------------------
FLensFlareEditorPreviewViewportClient
-----------------------------------------------------------------------------*/

FLensFlareElementEdViewportClient::FLensFlareElementEdViewportClient(WxLensFlareEditor* InLensFlareEditor) :
	  LensFlareEditor(InLensFlareEditor)
{
	// This window will be 2D/canvas only, so set the viewport type to None
	ViewportType = LVT_None;

	MouseHoldOffset = FIntPoint(0,0);
	MousePressPosition = FIntPoint(0,0);
	bMouseDragging = false;
	bMouseDown = false;
	bPanning = false;

	Origin2D = FIntPoint(0,0);
	OldMouseX = 0;
	OldMouseY = 0;

	LFEmptyBackgroundColor			= LensFlareEditor->EditorOptions->LFED_Empty_Background;
	LFSourceElementBackgroundColor	= LensFlareEditor->EditorOptions->LFED_Source_ElementEd_Background;
	LFSourceElementSelectedColor	= LensFlareEditor->EditorOptions->LFED_Source_Selected;
	LFSourceElementUnselectedColor	= LensFlareEditor->EditorOptions->LFED_Source_Unselected;
	LFElementBackgroundColor		= LensFlareEditor->EditorOptions->LFED_ElementEd_Background;
	LFElementSelectedColor			= LensFlareEditor->EditorOptions->LFED_Element_Selected;
	LFElementUnselectedColor		= LensFlareEditor->EditorOptions->LFED_Element_Unselected;

	CreateIconMaterials();
}

FLensFlareElementEdViewportClient::~FLensFlareElementEdViewportClient()
{

}

struct FLFElementOrder
{
	INT		ElementIndex;
	FLOAT	RayDistance;
	
	FLFElementOrder(INT InElementIndex,FLOAT InRayDistance):
		ElementIndex(InElementIndex),
		RayDistance(InRayDistance)
	{}
};

IMPLEMENT_COMPARE_CONSTREF(FLFElementOrder,LensFlareEditorElementEd,{ return ((A.RayDistance >= B.RayDistance) ? 1 : -1); });

void LensFlareEditorSort(ULensFlare* LensFlare, TArray<FLFElementOrder>& ElementOrder)
{
	INT ElementCount = LensFlare->Reflections.Num();
	ElementOrder.Empty(ElementCount + 1);

	// Add the 'source' element
	new(ElementOrder)FLFElementOrder(-1,0.0f);

	// Now, all the others
	for (INT ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		const FLensFlareElement* LFElement = LensFlare->GetElement(ElementIndex);
		if (LFElement)
		{
			new(ElementOrder)FLFElementOrder(ElementIndex, LFElement->RayDistance);
		}
	}
	Sort<USE_COMPARE_CONSTREF(FLFElementOrder,LensFlareEditorElementEd)>(&(ElementOrder(0)),ElementOrder.Num());
}

void FLensFlareElementEdViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	Canvas->PushAbsoluteTransform(FTranslationMatrix(FVector(Origin2D.X,Origin2D.Y,0)));

    // Clear the background to gray and set the 2D draw origin for the viewport
    if (Canvas->IsHitTesting() == FALSE)
	{
		Clear(Canvas,LFEmptyBackgroundColor);
	}
	else
	{
		Clear(Canvas,FLinearColor(1.0f,1.0f,1.0f,1.0f));
	}

	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();

	ULensFlare* LensFlare = LensFlareEditor->LensFlare;

#if 0 
	INT XPos = 0;
	for (INT ElementIndex = -1; ElementIndex < LensFlare->Reflections.Num(); ElementIndex++)
	{
		const FLensFlareElement* LFElement = LensFlare->GetElement(ElementIndex);
		if (LFElement)
		{
		    DrawLensFlareElement(ElementIndex, XPos, LensFlare, Viewport, Canvas);
			// Move X position on to next element.
			XPos += ElementWidth;
			// Draw vertical line after last column
			DrawTile(Canvas,XPos - 1, 0, 1, ViewY - Origin2D.Y, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
		}
	}
#else
	TArray<FLFElementOrder> SortedElements;
	LensFlareEditorSort(LensFlare, SortedElements);
	INT XPos = 0;
	for (INT ElementIndex = 0; ElementIndex < SortedElements.Num(); ElementIndex++)
	{
		const FLensFlareElement* LFElement = LensFlare->GetElement(SortedElements(ElementIndex).ElementIndex);
		if (LFElement)
		{
		    DrawLensFlareElement(SortedElements(ElementIndex).ElementIndex, XPos, LensFlare, Viewport, Canvas);
			// Move X position on to next element.
			XPos += ElementWidth;
			// Draw vertical line after last column
			DrawTile(Canvas,XPos - 1, 0, 1, ViewY - Origin2D.Y, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
		}
	}
#endif
	// Draw line under emitter headers
//	DrawTile(Canvas,0, EmitterHeadHeight-1, ViewX - Origin2D.X, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);

	Canvas->PopTransform();
}

void FLensFlareElementEdViewportClient::DrawLensFlareElement(
	INT Index, INT XPos, ULensFlare* LensFlare, FViewport* Viewport, FCanvas* Canvas)
{
	const FLensFlareElement* LFElement = LensFlare->GetElement(Index);
	if (LFElement == NULL)
	{
		return;
	}

	// Draw the element block
	INT ViewY = Viewport->GetSizeY();
	FLinearColor HeadColor = (Index == -1) ? 
		((Index == LensFlareEditor->SelectedElementIndex) ? LFSourceElementSelectedColor : LFSourceElementUnselectedColor) : 
		((Index == LensFlareEditor->SelectedElementIndex) ? LFElementSelectedColor : LFElementUnselectedColor);

	// Hit proxy rendering
	if (Canvas->IsHitTesting())
	{
        Canvas->SetHitProxy(new HLensFlareElementProxy(LensFlare, Index));
	}

	// Draw the background
	if (LFElement->bIsEnabled == TRUE)
	{
		DrawTile(Canvas,XPos, 0, ElementWidth, ElementHeight, 0.f, 0.f, 1.f, 1.f, HeadColor);
	}
	else
	{
		FTexture* Tex = GetTextureDisabledBackground();
		DrawTile(Canvas,XPos, 0, ElementWidth, ElementHeight, 0.f, 0.f, 1.f, 1.f, HeadColor, Tex);
	}

	if (!Canvas->IsHitTesting())
	{
		FString TempString;

		TempString = LFElement->ElementName.ToString();
		DrawShadowedString(Canvas,XPos + 5, ElementThumbBorder, *TempString, GEngine->SmallFont, FLinearColor::White);

		INT ThumbSize = ElementWidth - (2 * ElementThumbBorder) - (2 * (ElementStringHeight + ElementThumbBorder));
		INT ThumbOffset = (ElementWidth - ThumbSize) / 2;
		FIntPoint ThumbPos(XPos + ThumbOffset, 2 * ElementThumbBorder + ElementStringHeight);
		ThumbPos.X += Origin2D.X;
		ThumbPos.Y += Origin2D.Y;

		// Draw the material thumbnail.
		UMaterialInterface* MatInterface = NULL;
		if (LFElement->LFMaterials.Num() > 0)
		{
			MatInterface = LFElement->LFMaterials(0);
		}
		if (MatInterface)
		{
			// Get the rendering info for this object
			FThumbnailRenderingInfo* RenderInfo =
				GUnrealEd->GetThumbnailManager()->GetRenderingInfo(MatInterface);
			// If there is an object configured to handle it, draw the thumbnail
			if ((RenderInfo != NULL) && (RenderInfo->Renderer != NULL))
			{
				RenderInfo->Renderer->Draw(MatInterface, TPT_Plane, ThumbPos.X, ThumbPos.Y,
					ThumbSize, ThumbSize, Viewport, Canvas, TBT_None, FColor(0, 0, 0), FColor(0, 0, 0));
			}
		}
		else
		{
			DrawTile(Canvas, ThumbPos.X - Origin2D.X, ThumbPos.Y - Origin2D.Y, ThumbSize, ThumbSize, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);		
		}

		TempString = FString::Printf(TEXT("Dist. = %f"), LFElement->RayDistance);
		DrawShadowedString(Canvas,XPos + 5, ElementHeight - 20 - ElementThumbBorder, *TempString, GEngine->SmallFont, FLinearColor::White);
	}

	// Draw column background
	DrawTile(Canvas,XPos, ElementHeight, ElementWidth, ViewY - ElementHeight - Origin2D.Y, 0.f, 0.f, 1.f, 1.f, LFElementBackgroundColor);

	// Disable hit proxy
	if (Canvas->IsHitTesting())
	{
        Canvas->SetHitProxy(NULL);
	}

	//@todo.SAS. Need the DrawTile(..., MaterialInterface*) version of the function!
	// Draw enable/disable button
	FTexture* EnabledIconTxtr = NULL;
	if (LFElement->bIsEnabled == TRUE)
	{
		EnabledIconTxtr	= GetIconTexture(LFEDITOR_Icon_Module_Enabled);
	}
	else
	{
		EnabledIconTxtr	= GetIconTexture(LFEDITOR_Icon_Module_Disabled);
	}
	check(EnabledIconTxtr);

	if (Canvas->IsHitTesting())
	{
        Canvas->SetHitProxy(new HLensFlareElementEnableProxy(LensFlare, Index));
	}
	DrawTile(Canvas,XPos + (ElementWidth - 20), ElementHeight - 20, 16, 16, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, EnabledIconTxtr);
	if (Canvas->IsHitTesting())
	{
        Canvas->SetHitProxy(NULL);
	}

	DrawColorButton(Index, XPos, LensFlare, Canvas->IsHitTesting(), Viewport, Canvas);
}

void FLensFlareElementEdViewportClient::DrawColorButton(
	INT Index, INT XPos, ULensFlare* LensFlare, UBOOL bHitTesting, FViewport* Viewport, FCanvas* Canvas)
{
	if (bHitTesting)
	{
		Canvas->SetHitProxy(new HLensFlareElementColorButtonProxy(LensFlare, Index));
	}
	DrawTile(Canvas,XPos, 0, 5, ElementHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::White/*Module->ModuleEditorColor*/);
	if (bHitTesting)
	{
		Canvas->SetHitProxy(NULL);
	}
}

void FLensFlareElementEdViewportClient::DrawEnableButton(
	INT Index, INT XPos, ULensFlare* LensFlare, UBOOL bHitTesting, FViewport* Viewport, FCanvas* Canvas)
{
	if (bHitTesting)
	{
		Canvas->SetHitProxy(new HLensFlareElementEnableProxy(LensFlare, Index));
	}
/***
	if (Module->bEnabled)
	{
		DrawTile(Canvas,EmitterWidth - 20, 21, 16, 16, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, GetIconTexture(CASC_Icon_Module_Enabled));
	}
	else
	{
		DrawTile(Canvas,EmitterWidth - 20, 21, 16, 16, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, GetIconTexture(CASC_Icon_Module_Disabled));
	}
***/
	if (bHitTesting)
	{
		Canvas->SetHitProxy(NULL);
	}
}

void FLensFlareElementEdViewportClient::SetCanvas(INT X, INT Y)
{
	Origin2D.X = X;
	Origin2D.X = Min(0, Origin2D.X);
	
	Origin2D.Y = Y;
	Origin2D.Y = Min(0, Origin2D.Y);

	Viewport->Invalidate();
	// Force it to draw so the view change is seen
	Viewport->Draw();
}

void FLensFlareElementEdViewportClient::PanCanvas(INT DeltaX, INT DeltaY)
{
	Origin2D.X += DeltaX;
	Origin2D.X = Min(0, Origin2D.X);
	
	Origin2D.Y += DeltaY;
	Origin2D.Y = Min(0, Origin2D.Y);

	LensFlareEditor->LensFlareElementEdWindow->UpdateScrollBar(Origin2D.X, Origin2D.Y);
	Viewport->Invalidate();
}

FMaterialRenderProxy* FLensFlareElementEdViewportClient::GetIcon(Icons eIcon)
{
	check(!TEXT("LensFlareEditor: Invalid Icon Request!"));
	return NULL;
}

FTexture* FLensFlareElementEdViewportClient::GetIconTexture(Icons eIcon)
{
	if ((eIcon >= 0) && (eIcon < LFEDITOR_Icon_COUNT))
	{
		UTexture2D* IconTexture = IconTex[eIcon];
		if (IconTexture)
		{
            return IconTexture->Resource;
		}
	}

	check(!TEXT("LensFlareEditor: Invalid Icon Request!"));
	return NULL;
}

FTexture* FLensFlareElementEdViewportClient::GetTextureDisabledBackground()
{
	return TexElementDisabledBackground->Resource;
}

void FLensFlareElementEdViewportClient::CreateIconMaterials()
{
	IconTex[LFEDITOR_Icon_Module_Enabled]	= (UTexture2D*)UObject::StaticLoadObject(
		UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.Cascade.CASC_ModuleEnable"),NULL,LOAD_None,NULL);
	IconTex[LFEDITOR_Icon_Module_Disabled]	= (UTexture2D*)UObject::StaticLoadObject(
		UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.Cascade.CASC_ModuleDisable"),NULL,LOAD_None,NULL);

	check(IconTex[LFEDITOR_Icon_Module_Enabled]);
	check(IconTex[LFEDITOR_Icon_Module_Disabled]);

	TexElementDisabledBackground = (UTexture2D*)UObject::StaticLoadObject(
		UTexture2D::StaticClass(), NULL, TEXT("EditorMaterials.Cascade.CASC_DisabledModule"), NULL, LOAD_None, NULL);
	check(TexElementDisabledBackground);
}

UBOOL FLensFlareElementEdViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	Viewport->LockMouseToWindow(Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_MiddleMouseButton));

	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();
	FIntPoint MousePos = FIntPoint(HitX, HitY);

	if (Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton)
	{
		if (Event == IE_Pressed)
		{
			// Ignore pressing other mouse buttons while panning around.
			if (bPanning)
				return FALSE;

			HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
			wxMenu* Menu = NULL;

			if (Key == KEY_LeftMouseButton)
			{
				MousePressPosition = MousePos;
				bMouseDown = true;
			}
			else
			if (Key == KEY_RightMouseButton)
			{
				Menu = new WxMBLensFlareEditor(LensFlareEditor);
			}

			// Short-term, performing a quick-out
			UBOOL bHandledHitProxy = TRUE;
			if (HitResult)
			{
				if (HitResult->IsA(HLensFlareElementProxy::StaticGetType()))
				{
					HLensFlareElementProxy* ElementProxy = (HLensFlareElementProxy*)HitResult;
					LensFlareEditor->SetSelectedElement(ElementProxy->ElementIndex);
				}
				else
				if (HitResult->IsA(HLensFlareElementEnableProxy::StaticGetType()))
				{
					HLensFlareElementEnableProxy* EnableProxy = (HLensFlareElementEnableProxy*)HitResult;

					ULensFlare* LocalLensFlare = EnableProxy->LensFlare;
					check(LocalLensFlare);
					const FLensFlareElement* LFElement = LocalLensFlare->GetElement(EnableProxy->ElementIndex);
					LocalLensFlare->SetElementEnabled(EnableProxy->ElementIndex, !(LFElement->bIsEnabled));
				}
				else
				if (HitResult->IsA(HLensFlareElementColorButtonProxy::StaticGetType()))
				{
					HLensFlareElementColorButtonProxy* ColorButtonProxy = (HLensFlareElementColorButtonProxy*)HitResult;
					FColor ColorIn (255,255,255);

					// Let go of the mouse lock...
					Viewport->LockMouseToWindow(FALSE);
					Viewport->CaptureMouse(FALSE);

					// Get the color from the user
					FPickColorStruct PickColorStruct;
					PickColorStruct.RefreshWindows.AddItem(LensFlareEditor);
					PickColorStruct.DWORDColorArray.AddItem(&ColorIn);
					PickColorStruct.bModal = TRUE;
					if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
					{
						// Assign it
					}
					Viewport->Invalidate();
					LensFlareEditor->CurveEd->CurveEdVC->Viewport->Invalidate();
				}
				else
				{
					bHandledHitProxy = FALSE;
				}
			}
			else
			{
				bHandledHitProxy = FALSE;
			}

			if (bHandledHitProxy == FALSE)
			{
				LensFlareEditor->SetSelectedElement(-1);
			}

			if (Menu)
			{
				FTrackPopupMenu tpm(LensFlareEditor, Menu);
				tpm.Show();
				delete Menu;
			}
		}
		else 
		if (Event == IE_Released)
		{
			bMouseDown = false;
			bMouseDragging = false;
			Viewport->Invalidate();
		}
	}
	else
	if (Key == KEY_MiddleMouseButton)
	{
		if (Event == IE_Pressed)
		{
			bPanning = true;

			OldMouseX = HitX;
			OldMouseY = HitY;
		}
		else
		if (Event == IE_Released)
		{
			bPanning = false;
		}
	}

	if (Event == IE_Pressed)
	{
		if (Key == KEY_Delete)
		{
			LensFlareEditor->DeleteSelectedElement();
		}
		else
		if (Key == KEY_Left)
		{
			LensFlareEditor->MoveSelectedElement(-1);
		}
		else
		if (Key == KEY_Right)
		{
			LensFlareEditor->MoveSelectedElement(1);
		}
		else
		if ((Key == KEY_Z) && bCtrlDown)
		{
			LensFlareEditor->LensFlareEditorUndo();
		}
		else
		if ((Key == KEY_Y) && bCtrlDown)
		{
			LensFlareEditor->LensFlareEditorRedo();
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

void FLensFlareElementEdViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	if (bPanning)
	{
		INT DeltaX = X - OldMouseX;
		OldMouseX = X;

		INT DeltaY = Y - OldMouseY;
		OldMouseY = Y;

		PanCanvas(DeltaX, DeltaY);
		return;
	}

	// Update bMouseDragging.
	if (bMouseDown && !bMouseDragging)
	{
		// See how far mouse has moved since we pressed button.
		FIntPoint TotalMouseMove = FIntPoint(X,Y) - MousePressPosition;

		if (TotalMouseMove.Size() > 4)
		{
			bMouseDragging = TRUE;
		}
/***
		// If we are moving a module, here is where we remove it from its emitter.
		// Should not be able to change the CurrentMoveMode unless a module is selected.
		if (bMouseDragging && (CurrentMoveMode != CMMM_None))
		{
			if (LensFlareEditor->SelectedModule)
			{
				DraggedModule = LensFlareEditor->SelectedModule;

				// DraggedModules
				if (DraggedModules.Num() == 0)
				{
					if (LensFlareEditor->SelectedEmitter)
					{
						// We are pulling from an emitter...
						DraggedModules.Insert(0, LensFlareEditor->SelectedEmitter->LODLevels.Num());
						for (INT LODIndex = 0; LODIndex < LensFlareEditor->SelectedEmitter->LODLevels.Num(); LODIndex++)
						{
							UParticleLODLevel* LODLevel = LensFlareEditor->SelectedEmitter->LODLevels(LODIndex);
							if (LODLevel)
							{
								if (LensFlareEditor->SelectedModuleIndex >= 0)
								{
									DraggedModules(LODIndex)	= LODLevel->Modules(LensFlareEditor->SelectedModuleIndex);
								}
								else
								{
									//@todo. Implement code for type data modules!
									DraggedModules(LODIndex)	= LODLevel->TypeDataModule;
								}
							}
						}
					}
				}

				if (CurrentMoveMode == CMMM_Move)
				{
					// Remeber where to put this module back to if we abort the move.
					ResetDragModIndex = INDEX_NONE;
					if (LensFlareEditor->SelectedEmitter)
					{
						UParticleLODLevel* LODLevel = LensFlareEditor->SelectedEmitter->GetClosestLODLevel(LensFlareEditor->ToolBar->LODSlider->GetValue());
						for (INT i=0; i < LODLevel->Modules.Num(); i++)
						{
							if (LODLevel->Modules(i) == LensFlareEditor->SelectedModule)
								ResetDragModIndex = i;
						}
						if (ResetDragModIndex == INDEX_NONE)
						{
							if (LensFlareEditor->SelectedModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
							{
								ResetDragModIndex = INDEX_TYPEDATAMODULE;
							}							
						}

						check(ResetDragModIndex != INDEX_NONE);
						LensFlareEditor->DeleteSelectedModule();
					}
					else
					{
						// Remove the module from the dump
						LensFlareEditor->RemoveModuleFromDump(LensFlareEditor->SelectedModule);
					}
				}
			}
		}
***/
	}

	// If dragging a module around, update each frame.
	if (bMouseDragging)
	{
		Viewport->Invalidate();
	}
}

UBOOL FLensFlareElementEdViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	WxLensFlareElementEd
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxLensFlareElementEd, wxWindow)
	EVT_SIZE(WxLensFlareElementEd::OnSize)
	EVT_SCROLL(WxLensFlareElementEd::OnScroll)
	EVT_MOUSEWHEEL(WxLensFlareElementEd::OnMouseWheel)
END_EVENT_TABLE()

WxLensFlareElementEd::WxLensFlareElementEd(wxWindow* InParent, wxWindowID InID, class WxLensFlareEditor* InLensFlareEditor ) : 
	wxWindow(InParent, InID)
{
	ScrollBar_Horz = new wxScrollBar(this, ID_CASCADE_HORZ_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_HORIZONTAL);
	ScrollBar_Vert = new wxScrollBar(this, ID_CASCADE_VERT_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);

	wxRect rc = GetClientRect();
	wxRect rcSBH = ScrollBar_Horz->GetClientRect();
	wxRect rcSBV = ScrollBar_Vert->GetClientRect();
	ScrollBar_Horz->SetSize(rc.GetLeft(), rc.GetTop() + rc.GetHeight() - rcSBH.GetHeight(), rc.GetWidth() - rcSBV.GetWidth(), rcSBH.GetHeight());
	ScrollBar_Vert->SetSize(rc.GetLeft() + rc.GetWidth() - rcSBV.GetWidth(), rc.GetTop(), rcSBV.GetWidth(), rc.GetHeight());

	ScrollBar_Horz->SetThumbPosition(0);
    ScrollBar_Horz->SetScrollbar(0, ElementWidth, ElementWidth * 25, ElementWidth - 1);
    ScrollBar_Vert->SetThumbPosition(0);
    ScrollBar_Vert->SetScrollbar(0, ElementHeight, ElementHeight * 25, ElementHeight - 1);

	ThumbPos_Horz = 0;
	ThumbPos_Vert = 0;

	ElementEdVC = new FLensFlareElementEdViewportClient(InLensFlareEditor);
	ElementEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(ElementEdVC, (HWND)GetHandle());
	ElementEdVC->Viewport->CaptureJoystickInput(false);

	ElementEdVC->SetCanvas(-ThumbPos_Horz, -ThumbPos_Vert);
}

WxLensFlareElementEd::~WxLensFlareElementEd()
{
	GEngine->Client->CloseViewport(ElementEdVC->Viewport);
	ElementEdVC->Viewport = NULL;
	delete ElementEdVC;
	delete ScrollBar_Horz;
	delete ScrollBar_Vert;
}

void WxLensFlareElementEd::OnSize(wxSizeEvent& In)
{
	wxRect rc = GetClientRect();

	wxRect rcSBH = ScrollBar_Horz->GetClientRect();
	wxRect rcSBV = ScrollBar_Vert->GetClientRect();
	ScrollBar_Horz->SetSize(rc.GetLeft(), rc.GetTop() + rc.GetHeight() - rcSBH.GetHeight(), rc.GetWidth() - rcSBV.GetWidth(), rcSBH.GetHeight());
	ScrollBar_Vert->SetSize(rc.GetLeft() + rc.GetWidth() - rcSBV.GetWidth(), rc.GetTop(), rcSBV.GetWidth(), rc.GetHeight());

	INT	PosX	= 0;
	INT	PosY	= 0;
	INT	Width	= rc.GetWidth() - rcSBV.GetWidth();
	INT	Height	= rc.GetHeight() - rcSBH.GetHeight();

	::MoveWindow((HWND)ElementEdVC->Viewport->GetWindow(), PosX, PosY, Width, Height, 1);

	Refresh();
	if (ElementEdVC && ElementEdVC->Viewport && ElementEdVC->LensFlareEditor && ElementEdVC->LensFlareEditor->ToolBar)
	{
        ElementEdVC->Viewport->Invalidate();
		ElementEdVC->Viewport->Draw();
	}
}

// Updates the scrollbars values
void WxLensFlareElementEd::UpdateScrollBar(INT Horz, INT Vert)
{
	ThumbPos_Horz = -Horz;
	ThumbPos_Vert = -Vert;
	ScrollBar_Horz->SetThumbPosition(ThumbPos_Horz);
	ScrollBar_Vert->SetThumbPosition(ThumbPos_Vert);
}

void WxLensFlareElementEd::OnScroll(wxScrollEvent& In)
{
    wxScrollBar* InScrollBar = wxDynamicCast(In.GetEventObject(), wxScrollBar);
    if (InScrollBar) 
	{
        if (InScrollBar->IsVertical())
		{
			ThumbPos_Vert = In.GetPosition();
			ElementEdVC->SetCanvas(-ThumbPos_Horz, -ThumbPos_Vert);
		}
        else
		{
			ThumbPos_Horz = In.GetPosition();
			ElementEdVC->SetCanvas(-ThumbPos_Horz, -ThumbPos_Vert);
		}
    }
}

void WxLensFlareElementEd::OnMouseWheel(wxMouseEvent& In)
{
}
