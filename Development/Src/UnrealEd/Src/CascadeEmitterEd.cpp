/*=============================================================================
	CascadeEmitterEd.cpp: 'Cascade' particle editor emitter editor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineParticleClasses.h"

static const INT	EmitterWidth = 180;
static const INT	EmitterHeadHeight = 60;
static const INT	EmitterThumbBorder = 5;

static const INT	ModuleHeight = 40;

static const FColor EmitterSelectedColor(255, 130, 30);
static const FColor EmitterUnselectedColor(180, 180, 180);

static const FColor TypeDataModuleSelectedColor(255, 200, 0);
static const FColor TypeDataModuleUnselectedColor(200, 200, 200);

static const FColor ModuleSelectedColor(255, 100, 0);
static const FColor ModuleUnselectedColor(160, 160, 160);

static const FColor RenderModeSelected(255,200,0);
static const FColor RenderModeUnselected(112,112,112);

static const FColor Module3DDrawModeEnabledColor(255,200,0);
static const FColor Module3DDrawModeDisabledColor(112,112,112);

#define INDEX_TYPEDATAMODULE	(INDEX_NONE - 1)

/*-----------------------------------------------------------------------------
FCascadePreviewViewportClient
-----------------------------------------------------------------------------*/

FCascadeEmitterEdViewportClient::FCascadeEmitterEdViewportClient(WxCascade* InCascade)
{
	Cascade = InCascade;

	CurrentMoveMode = CMMM_None;
	MouseHoldOffset = FIntPoint(0,0);
	MousePressPosition = FIntPoint(0,0);
	bMouseDragging = false;
	bMouseDown = false;
	bPanning = false;
	bDrawModuleDump = InCascade->EditorOptions->bShowModuleDump;

	DraggedModule = NULL;

	Origin2D = FIntPoint(0,0);
	OldMouseX = 0;
	OldMouseY = 0;

	ResetDragModIndex = INDEX_NONE;
}

FCascadeEmitterEdViewportClient::~FCascadeEmitterEdViewportClient()
{

}


void FCascadeEmitterEdViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
    // Clear the background to gray and set the 2D draw origin for the viewport
	RI->Clear(FColor(112,112,112));
	RI->SetOrigin2D(Origin2D);

	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	UBOOL bHitTesting = RI->IsHitTesting();

	UParticleSystem* PartSys = Cascade->PartSys;

	INT XPos = 0;
	for(INT i=0; i<PartSys->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(i);
        DrawEmitter(i, XPos, Emitter, Viewport, RI);
		// Move X position on to next emitter.
		XPos += EmitterWidth;
		// Draw vertical line after last column
		RI->DrawTile(XPos - 1, 0, 1, ViewY - Origin2D.Y, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
	}

	// Draw line under emitter headers
	RI->DrawTile(0, EmitterHeadHeight-1, ViewX - Origin2D.X, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);

    // Draw the module dump, if it is enabled
    if (bDrawModuleDump)
        DrawModuleDump(Viewport, RI);

	// When dragging a module.
	if ((CurrentMoveMode != CMMM_None) && bMouseDragging)
	{
		if (DraggedModule)
			DrawDraggedModule(DraggedModule, Viewport, RI);
	}

    // Restore the 2D draw origin to 0,0
	RI->SetOrigin2D(0,0);
}

void FCascadeEmitterEdViewportClient::DrawEmitter(INT Index, INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport, FRenderInterface* RI)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	INT ViewY = Viewport->GetSizeY();

    // Draw background block
	if (bHitTesting)
        RI->SetHitProxy(new HCascadeEmitterProxy(Emitter));

	// Draw header block
	DrawHeaderBlock(Index, XPos, Emitter, Viewport, RI);

	// Draw the type data module
	DrawTypeDataBlock(XPos, Emitter, Viewport, RI);

    // Draw each module
	INT YPos = EmitterHeadHeight + ModuleHeight;
    INT j;

    // Now, draw the remaining modules
	for(j = 0; j < Emitter->Modules.Num(); j++)
	{
		UParticleModule* Module = Emitter->Modules(j);
		check(Module);
        if (!Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
        {
            DrawModule(XPos, YPos, Emitter, Module, Viewport, RI);
            // Update Y position for next module.
		    YPos += ModuleHeight;
        }
	}
}

void FCascadeEmitterEdViewportClient::DrawHeaderBlock(INT Index, INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport,FRenderInterface* RI)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	INT ViewY = Viewport->GetSizeY();

	FColor HeadColor = (Emitter == Cascade->SelectedEmitter) ? EmitterSelectedColor : EmitterUnselectedColor;

	RI->DrawTile(XPos, 0, EmitterWidth, EmitterHeadHeight, 0.f, 0.f, 1.f, 1.f, HeadColor);
	RI->DrawTile(XPos, 0, 5, EmitterHeadHeight, 0.f, 0.f, 1.f, 1.f, Emitter->EmitterEditorColor);

	UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(Emitter);
	if (SpriteEmitter)
	{
		FString TempString;

		TempString = *(SpriteEmitter->GetEmitterName());
		RI->DrawShadowedString(XPos + 10, 5, *TempString, GEngine->SmallFont, FLinearColor::White);

		INT ThumbSize = EmitterHeadHeight - 2*EmitterThumbBorder;
		FIntPoint ThumbPos(XPos + EmitterWidth - ThumbSize - EmitterThumbBorder, EmitterThumbBorder);

		TempString = FString::Printf(TEXT("%4d"), Emitter->PeakActiveParticles);
		RI->DrawShadowedString(XPos + 75, 25, *TempString, GEngine->SmallFont, FLinearColor::White);

		// Draw sprite material thumbnail.
		if (SpriteEmitter->Material && !bHitTesting)
		{
			SpriteEmitter->Material->DrawThumbnail(TPT_Plane, ThumbPos.X, ThumbPos.Y, Viewport, RI, 1.f, false, 0.f, ThumbSize);
		}
		else
		{
			RI->DrawTile(ThumbPos.X, ThumbPos.Y, ThumbSize, ThumbSize, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);		
		}
	}

	// Draw column background
	RI->DrawTile(XPos, EmitterHeadHeight, EmitterWidth, ViewY - EmitterHeadHeight - Origin2D.Y, 0.f, 0.f, 1.f, 1.f, FColor(130, 130, 130));
	if (bHitTesting)
        RI->SetHitProxy(NULL);

	// Draw little rendering mode buttons.
	for(INT j=0; j<4; j++)
	{
		UBOOL bModeSelected = (Emitter->EmitterRenderMode == j);

		if (bHitTesting)
            RI->SetHitProxy(new HCascadeDrawModeButtonProxy(Emitter, j));

		RI->DrawTile(XPos + 11 + j*12, 25, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);
		if (bModeSelected)
			RI->DrawTile(XPos + 12 + j*12, 26, 6, 6, 0.f, 0.f, 1.f, 1.f, RenderModeSelected);
		else
			RI->DrawTile(XPos + 12 + j*12, 26, 6, 6, 0.f, 0.f, 1.f, 1.f, RenderModeUnselected);

		if (bHitTesting)
            RI->SetHitProxy(NULL);
	}

	// Draw button for sending emitter properties to graph.
	if (bHitTesting)
        RI->SetHitProxy(new HCascadeGraphButton(Emitter, NULL));

	RI->DrawTile(XPos + 11, 38, 8, 8, 0.f, 0.f, 1.f, 1.f, FColor(0,0,0));
	RI->DrawTile(XPos + 12, 39, 6, 6, 0.f, 0.f, 1.f, 1.f, FColor(100,200,100));

	if (bHitTesting)
        RI->SetHitProxy(NULL);
}

void FCascadeEmitterEdViewportClient::DrawTypeDataBlock(INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport, FRenderInterface* RI)
{
	UParticleModule* Module = Emitter->TypeDataModule;
	if (Module)
	{
        check(Module->IsA(UParticleModuleTypeDataBase::StaticClass()));
        DrawModule(XPos, EmitterHeadHeight, Emitter, Module, Viewport, RI);
	}
}

void FCascadeEmitterEdViewportClient::DrawModule(INT XPos, INT YPos, UParticleEmitter* Emitter, UParticleModule* Module, FChildViewport* Viewport, FRenderInterface* RI)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	
    // Hack to ensure no black modules...
	if (Module->ModuleEditorColor == FColor(0,0,0,0))
		Module->ModuleEditorColor = MakeRandomColor();

    // Grab the correct color to use
	FColor ModuleBkgColor;
	if (Module == Cascade->SelectedModule)
	{
        if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
        {
		    if (Emitter == Cascade->SelectedEmitter)
			    ModuleBkgColor = EmitterSelectedColor;
		    else
			    ModuleBkgColor = TypeDataModuleSelectedColor;
        }
        else
        {
		    if (Emitter == Cascade->SelectedEmitter)
			    ModuleBkgColor = EmitterSelectedColor;
		    else
			    ModuleBkgColor = ModuleSelectedColor;
        }
	}
	else
    {
        if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
        {
            ModuleBkgColor = TypeDataModuleUnselectedColor;
        }
        else
        {
    		ModuleBkgColor = ModuleUnselectedColor;
        }
    }

    // Offset the 2D draw origin
	RI->SetOrigin2D(XPos + Origin2D.X, YPos + Origin2D.Y);

    // Draw the module box and it's proxy
	if (bHitTesting)
        RI->SetHitProxy(new HCascadeModuleProxy(Emitter, Module));
	DrawModule(RI, Module, ModuleBkgColor);
	if (bHitTesting)
        RI->SetHitProxy(NULL);

	// Draw little 'send properties to graph' button.
	if (Module->ModuleHasCurves())
        DrawCurveButton(Emitter, Module, bHitTesting, RI);

	// Draw button for 3DDrawMode.
	if (Module->bSupported3DDrawMode)
        Draw3DDrawButton(Emitter, Module, bHitTesting, RI);

    // Restore the 2D draw origin
    RI->SetOrigin2D(Origin2D);
}

void FCascadeEmitterEdViewportClient::DrawModule(FRenderInterface* RI, UParticleModule* Module, FColor ModuleBkgColor)
{
	RI->DrawTile(-1, -1, EmitterWidth+1, ModuleHeight+2, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
	RI->DrawTile(0, 0, EmitterWidth-1, ModuleHeight, 0.f, 0.f, 1.f, 1.f, ModuleBkgColor);

	INT XL, YL;
	FString ModuleName = Module->GetClass()->GetDescription();

	// Postfix name with '+' if shared.
	if (Cascade->ModuleIsShared(Module))
		ModuleName = ModuleName + FString(TEXT("+"));

	RI->StringSize(GEngine->SmallFont, XL, YL, *(ModuleName));
	RI->DrawShadowedString(10, 3, *(ModuleName), GEngine->SmallFont, FLinearColor::White);

	// If module is shared, draw colored block down left to indicate where
	if (Cascade->ModuleIsShared(Module) || Module->IsDisplayedInCurveEd(Cascade->CurveEd->EdSetup))
	{
		RI->DrawTile(0, 0, 5, ModuleHeight, 0.f, 0.f, 1.f, 1.f, Module->ModuleEditorColor);
	}
}

void FCascadeEmitterEdViewportClient::DrawDraggedModule(UParticleModule* Module, FChildViewport* Viewport, FRenderInterface* RI)
{
	FIntPoint MousePos = FIntPoint(Viewport->GetMouseX(), Viewport->GetMouseY()) + Origin2D;

    // Draw indicator for where we would insert this module.
	UParticleEmitter* TargetEmitter = NULL;
	INT TargetIndex = INDEX_NONE;
	FindDesiredModulePosition(MousePos, TargetEmitter, TargetIndex);

	if (TargetEmitter && TargetIndex != INDEX_NONE)
	{
		FIntPoint DropPos = FindModuleTopLeft(TargetEmitter, NULL, Viewport);
		DropPos.Y = EmitterHeadHeight + (TargetIndex + 1) * ModuleHeight;

		RI->DrawTile(DropPos.X, DropPos.Y - 2, EmitterWidth, 5, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);
	}

	// When dragging, draw the module under the mouse cursor.
	FIntPoint DraggedModulePos = MousePos + MouseHoldOffset;
	if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
	{
		RI->SetOrigin2D(DraggedModulePos.X, DraggedModulePos.Y);
	}
	else
	{
		RI->SetOrigin2D(DraggedModulePos.X, DraggedModulePos.Y + ModuleHeight);
	}

	DrawModule(RI, DraggedModule, EmitterSelectedColor);
}

void FCascadeEmitterEdViewportClient::DrawCurveButton(UParticleEmitter* Emitter, UParticleModule* Module, UBOOL bHitTesting, FRenderInterface* RI)
{
	if (bHitTesting)
        RI->SetHitProxy(new HCascadeGraphButton(Emitter, Module));

	RI->DrawTile(10, 20, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);
	RI->DrawTile(11, 21, 6, 6, 0.f, 0.f, 1.f, 1.f, FColor(100,200,100));

	if (bHitTesting)
        RI->SetHitProxy(NULL);
}

void FCascadeEmitterEdViewportClient::Draw3DDrawButton(UParticleEmitter* Emitter, UParticleModule* Module, UBOOL bHitTesting, FRenderInterface* RI)
{
	if (bHitTesting)
		RI->SetHitProxy(new HCascade3DDrawModeButtonProxy(Emitter, Module));
	RI->DrawTile(10 + 12, 20, 8, 8, 0.f, 0.f, 1.f, 1.f, FColor(0,0,0));
	if (Module->b3DDrawMode)
		RI->DrawTile(11 + 12, 21, 6, 6, 0.f, 0.f, 1.f, 1.f, Module3DDrawModeEnabledColor);
	else
		RI->DrawTile(11 + 12, 21, 6, 6, 0.f, 0.f, 1.f, 1.f, Module3DDrawModeDisabledColor);
	if (bHitTesting)
		RI->SetHitProxy(NULL);

	if (Module->b3DDrawMode)
    {
        if (bHitTesting)
			RI->SetHitProxy(new HCascade3DDrawModeOptionsButtonProxy(Emitter, Module));
		RI->DrawTile(10 + 12, 20 + 10, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);
		RI->DrawTile(11 + 12, 21 + 10, 6, 6, 0.f, 0.f, 1.f, 1.f, FColor(100,200,100));
		if (bHitTesting)
			RI->SetHitProxy(NULL);
    }
}

void FCascadeEmitterEdViewportClient::DrawModuleDump(FChildViewport* Viewport, FRenderInterface* RI)
{
	if (Cascade->ModuleDumpList.Num() > 0)
	{
		INT iDummy = 0;
	}

	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	UBOOL bHitTesting = RI->IsHitTesting();
    INT XPos = ViewX - EmitterWidth - 1;
	FColor HeadColor = EmitterUnselectedColor;

    RI->SetOrigin2D(Origin2D);

	RI->DrawTile(XPos, 0, EmitterWidth, EmitterHeadHeight, 0.f, 0.f, 1.f, 1.f, HeadColor);
    RI->DrawTile(XPos, 0, 5, EmitterHeadHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black);

    FString ModuleDumpTitle = TEXT("Module Dump");
	
    RI->DrawShadowedString(XPos + 10, 5, *ModuleDumpTitle, GEngine->SmallFont, FLinearColor::White);

	// Draw column background
	RI->DrawTile(XPos, EmitterHeadHeight, EmitterWidth, ViewY - EmitterHeadHeight - Origin2D.Y, 0.f, 0.f, 1.f, 1.f, FColor(130, 130, 130));
    if (bHitTesting)
        RI->SetHitProxy(NULL);
/***
	// Draw button for closing the dump modules
	if (bHitTesting)
        RI->SetHitProxy(new HCascadeGraphButton(Emitter, NULL));

	RI->DrawTile(XPos + 11, 38, 8, 8, 0.f, 0.f, 1.f, 1.f, FColor(0,0,0));
	RI->DrawTile(XPos + 12, 39, 6, 6, 0.f, 0.f, 1.f, 1.f, FColor(100,200,100));
***/
	if (bHitTesting)
        RI->SetHitProxy(NULL);

	// Draw the dump module list...
	INT YPos = EmitterHeadHeight;
	for(INT i = 0; i < Cascade->ModuleDumpList.Num(); i++)
	{
		UParticleModule* Module = Cascade->ModuleDumpList(i);
		check(Module);
        DrawModule(XPos, YPos, NULL, Module, Viewport, RI);
        // Update Y position for next module.
		YPos += ModuleHeight;
	}

    RI->SetOrigin2D(Origin2D);
}

void FCascadeEmitterEdViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
{
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
				return;

			if (Key == KEY_LeftMouseButton)
			{
				MousePressPosition = MousePos;
				bMouseDown = true;
			}

			HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
			wxMenu* Menu = NULL;

			if (HitResult)
			{
				if (HitResult->IsA(TEXT("HCascadeEmitterProxy")))
				{
					UParticleEmitter* Emitter = ((HCascadeEmitterProxy*)HitResult)->Emitter;
					Cascade->SetSelectedEmitter(Emitter);

					if (Key == KEY_RightMouseButton)
					{
						Menu = new WxMBCascadeEmitterBkg(Cascade, WxMBCascadeEmitterBkg::EVERYTHING);
					}
				}
				else
				if (HitResult->IsA(TEXT("HCascadeDrawModeButtonProxy")))
				{
					UParticleEmitter* Emitter = ((HCascadeDrawModeButtonProxy*)HitResult)->Emitter;
					INT DrawMode = ((HCascadeDrawModeButtonProxy*)HitResult)->DrawMode;

					Cascade->SetSelectedEmitter(Emitter);
					Emitter->EmitterRenderMode = DrawMode;
				}
				else
				if (HitResult->IsA(TEXT("HCascadeModuleProxy")))
				{
					UParticleEmitter* Emitter = ((HCascadeModuleProxy*)HitResult)->Emitter;
					UParticleModule* Module = ((HCascadeModuleProxy*)HitResult)->Module;
					Cascade->SetSelectedModule(Emitter, Module);

					if (Key == KEY_RightMouseButton)
					{
						Menu = new WxMBCascadeModule(Cascade);
					}
					else
					{
						check(Cascade->SelectedModule);

						// We are starting to drag this module. Look at keys to see if we are moving/instancing
						if (bCtrlDown)
							CurrentMoveMode = CMMM_Instance;
						else
							CurrentMoveMode = CMMM_Move;

						// Figure out and save the offset from mouse location to top-left of selected module.
						MouseHoldOffset = FindModuleTopLeft(Emitter, Module, Viewport) - MousePressPosition;
					}
				}
				else
				if (HitResult->IsA(TEXT("HCascadeGraphButton")))
				{
					UParticleEmitter* Emitter = ((HCascadeModuleProxy*)HitResult)->Emitter;
					UParticleModule* Module = ((HCascadeModuleProxy*)HitResult)->Module;

					if (Module)
					{
						Cascade->SetSelectedModule(Emitter, Module);
					}
					else
					{
						Cascade->SetSelectedEmitter(Emitter);
					}

					Cascade->AddSelectedToGraph();
				}
				else
				if (HitResult->IsA(TEXT("HCascade3DDrawModeButtonProxy")))
				{
					UParticleModule* Module = ((HCascadeModuleProxy*)HitResult)->Module;
					check(Module);
					Module->b3DDrawMode = !Module->b3DDrawMode;
				}
                else
				if (HitResult->IsA(TEXT("HCascade3DDrawModeOptionsButtonProxy")))
				{
					UParticleModule* Module = ((HCascadeModuleProxy*)HitResult)->Module;
					check(Module);
                    // Pop up an options dialog??
                    appMsgf(0, TEXT("3DDrawMode Options Menu!"));
				}
			}
			else
			{
				Cascade->SetSelectedModule(NULL, NULL);

				if (Key == KEY_RightMouseButton)
					Menu = new WxMBCascadeBkg(Cascade);
			}

			if (Menu)
			{
				FTrackPopupMenu tpm(Cascade, Menu);
				tpm.Show();
				delete Menu;
			}
		}
		else 
		if (Event == IE_Released)
		{
			// If we were dragging a module, find where the mouse currently is, and move module there
			if ((CurrentMoveMode != CMMM_None) && bMouseDragging)
			{
				if (DraggedModule)
				{
					// Find where to move module to.
					UParticleEmitter* TargetEmitter = NULL;
					INT TargetIndex = INDEX_NONE;
					FindDesiredModulePosition(MousePos, TargetEmitter, TargetIndex);

					if (!TargetEmitter || TargetIndex == INDEX_NONE)
					{
						// If the target is the DumpModules area, add it to the list of dump modules
						if (bDrawModuleDump)
						{
							Cascade->ModuleDumpList.AddItem(DraggedModule);
							DraggedModule = NULL;
						}
						else
						// If target is invalid and we were moving it, put it back where it came from.
						if (CurrentMoveMode == CMMM_Move)
						{
							if (ResetDragModIndex != INDEX_NONE)
							{
								Cascade->InsertModule(DraggedModule, Cascade->SelectedEmitter, ResetDragModIndex);
								Cascade->SelectedEmitter->UpdateModuleLists();
								Cascade->RemoveModuleFromDump(DraggedModule);
							}
							else
							{
								Cascade->ModuleDumpList.AddItem(DraggedModule);
							}
						}
					}
					else
					{
						// Add dragged module in new location.
						if ((CurrentMoveMode == CMMM_Move) || (CurrentMoveMode == CMMM_Instance))
						{
							Cascade->InsertModule(DraggedModule, TargetEmitter, TargetIndex);
							TargetEmitter->UpdateModuleLists();
							Cascade->RemoveModuleFromDump(DraggedModule);
						}
					}
				}
			}

			bMouseDown = false;
			bMouseDragging = false;
			CurrentMoveMode = CMMM_None;
			DraggedModule = NULL;

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
			if (Cascade->SelectedModule)
			{
				Cascade->DeleteSelectedModule();
			}
			else
			{
				Cascade->DeleteSelectedEmitter();
			}
		}
		else
		if (Key == KEY_Left)
		{
			Cascade->MoveSelectedEmitter(-1);
		}
		else
		if (Key == KEY_Right)
		{
			Cascade->MoveSelectedEmitter(1);
		}
	}
}

void FCascadeEmitterEdViewportClient::MouseMove(FChildViewport* Viewport, INT X, INT Y)
{
	if (bPanning)
	{
		INT DeltaX = X - OldMouseX;
		OldMouseX = X;

		INT DeltaY = Y - OldMouseY;
		OldMouseY = Y;

		Origin2D.X += DeltaX;
		Origin2D.X = Min(0, Origin2D.X);
		
		Origin2D.Y += DeltaY;
		Origin2D.Y = Min(0, Origin2D.Y);

		Viewport->Invalidate();

		return;
	}

	// Update bMouseDragging.
	if (bMouseDown && !bMouseDragging)
	{
		// See how far mouse has moved since we pressed button.
		FIntPoint TotalMouseMove = FIntPoint(X,Y) - MousePressPosition;

		if (TotalMouseMove.Size() > 4)
			bMouseDragging = true;


		// If we are moving a module, here is where we remove it from its emitter.
		// Should not be able to change the CurrentMoveMode unless a module is selected.
		if (bMouseDragging && (CurrentMoveMode != CMMM_None))
		{		
			check(Cascade->SelectedModule);
//			check(Cascade->SelectedEmitter);

//			if (!Cascade->SelectedModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
			{
				DraggedModule = Cascade->SelectedModule;

				if (CurrentMoveMode == CMMM_Move)
				{
					// Remeber where to put this module back to if we abort the move.
					ResetDragModIndex = INDEX_NONE;
					if (Cascade->SelectedEmitter)
					{
						for(INT i=0; i < Cascade->SelectedEmitter->Modules.Num(); i++)
						{
							if (Cascade->SelectedEmitter->Modules(i) == Cascade->SelectedModule)
								ResetDragModIndex = i;
						}
						if (ResetDragModIndex == INDEX_NONE)
						{
							if (Cascade->SelectedModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
							{
								ResetDragModIndex = INDEX_TYPEDATAMODULE;
							}							
						}

						check(ResetDragModIndex != INDEX_NONE);
						Cascade->DeleteSelectedModule();
					}
					else
					{
						// Remove the module from the dump
						Cascade->RemoveModuleFromDump(Cascade->SelectedModule);
					}
				}
			}
		}
	}

	// If dragging a module around, update each frame.
	if (bMouseDragging && CurrentMoveMode != CMMM_None)
	{
		Viewport->Invalidate();
	}
}

void FCascadeEmitterEdViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{

}

// Given a screen position, find which emitter/module index it corresponds to.
void FCascadeEmitterEdViewportClient::FindDesiredModulePosition(const FIntPoint& Pos, class UParticleEmitter* &OutEmitter, INT &OutIndex)
{
	INT EmitterIndex = Pos.X / EmitterWidth;

	// If invalid Emitter, return nothing.
	if (EmitterIndex < 0 || EmitterIndex > Cascade->PartSys->Emitters.Num()-1)
	{
		OutEmitter = NULL;
		OutIndex = INDEX_NONE;
		return;
	}

	OutEmitter = Cascade->PartSys->Emitters(EmitterIndex);
	OutIndex = Clamp<INT>((Pos.Y - EmitterHeadHeight - ModuleHeight) / ModuleHeight, 0, OutEmitter->Modules.Num());
//	if (Cascade->PartSys->Emitters(EmitterIndex)->TypeDataModule)
//	{
//		OutIndex -= 1;
//	}
}

FIntPoint FCascadeEmitterEdViewportClient::FindModuleTopLeft(class UParticleEmitter* Emitter, class UParticleModule* Module, FChildViewport* Viewport)
{
	INT i;

	INT EmitterIndex = -1;
	for(i = 0; i < Cascade->PartSys->Emitters.Num(); i++)
	{
		if (Cascade->PartSys->Emitters(i) == Emitter)
			EmitterIndex = i;
	}

	INT ModuleIndex = 0;

	if (EmitterIndex != -1)
	{
		if (Module && Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			return FIntPoint(EmitterIndex*EmitterWidth, EmitterHeadHeight);
		}
		else
		{
			for(i = 0; i < Emitter->Modules.Num(); i++)
			{
				if (Emitter->Modules(i) == Module)
					ModuleIndex = i;
			}
		}

		return FIntPoint(EmitterIndex*EmitterWidth, EmitterHeadHeight + ModuleIndex*ModuleHeight);
	}

	// Must be in the module dump...
	check(Cascade->ModuleDumpList.Num());
	for (i = 0; i < Cascade->ModuleDumpList.Num(); i++)
	{
		if (Cascade->ModuleDumpList(i) == Module)
		{
			INT OffsetHeight = 0;
			if (!Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
			{
				// When we grab from the dump, we need to account for no 'TypeData'
				OffsetHeight = ModuleHeight;
			}
			return FIntPoint(Viewport->GetSizeX() - EmitterWidth, EmitterHeadHeight - OffsetHeight + i * EmitterHeadHeight);
		}
	}

	return FIntPoint(0.f, 0.f);
}

/*-----------------------------------------------------------------------------
	WxCascadeEmitterEd
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxCascadeEmitterEd, wxWindow)
	EVT_SIZE(WxCascadeEmitterEd::OnSize)
END_EVENT_TABLE()

WxCascadeEmitterEd::WxCascadeEmitterEd(wxWindow* InParent, wxWindowID InID, class WxCascade* InCascade )
: wxWindow(InParent, InID)
{
	EmitterEdVC = new FCascadeEmitterEdViewportClient(InCascade);
	EmitterEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(EmitterEdVC, (HWND)GetHandle());
	EmitterEdVC->Viewport->CaptureJoystickInput(false);
}

WxCascadeEmitterEd::~WxCascadeEmitterEd()
{
	GEngine->Client->CloseViewport(EmitterEdVC->Viewport);
	EmitterEdVC->Viewport = NULL;
	delete EmitterEdVC;
}

void WxCascadeEmitterEd::OnSize(wxSizeEvent& In)
{
	wxRect rc = GetClientRect();

	::MoveWindow((HWND)EmitterEdVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1);
}

