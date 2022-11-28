/*=============================================================================
	MaterialEditor.cpp: Material editing.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"

/*-----------------------------------------------------------------------------
	FExpressionViewportClient
-----------------------------------------------------------------------------*/

FExpressionViewportClient::FExpressionViewportClient( UMaterial* InMaterial, WxMaterialEditor_Upper* InUpperWindow, WxPropertyWindow* InLowerWindow ):
	HitExpression(NULL),
	HitOutputExpression(NULL),
	HitOutput(0),
	HitInput(NULL)
{
	Material = InMaterial;
	UpperWindow = InUpperWindow;
	LowerWindow = InLowerWindow;
	bRotating = bZooming = 0;
	ZoomPct = 1.f;
	bShowBackground = 0;

	GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
	GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );

	Realtime = 0;
}

FExpressionViewportClient::~FExpressionViewportClient()
{
	RefreshExpressionPreviews();
}

void FExpressionViewportClient::RefreshExpressionPreviews()
{
	ExpressionPreviews.Empty();
}

void FExpressionViewportClient::DrawConnection(FRenderInterface* RI,INT InputX,INT InputY,FExpressionInput* Input,UBOOL Tab)
{
	INT LabelPadding = Input->Expression ? Input->Expression->GetLabelPadding() : 0;

	if(Input->Expression && !RI->IsHitTesting())
	{
		INT							OutputY = Input->Expression->EditorY + ME_CAPTION_HEIGHT + 8;
		TArray<FExpressionOutput>	Outputs;
		Input->Expression->GetOutputs(Outputs);
		for(UINT OutputIndex = 0;OutputIndex < (UINT)Outputs.Num();OutputIndex++)
		{
			FExpressionOutput&	Output = Outputs(OutputIndex);
			if(	Output.Mask == Input->Mask
					&& Output.MaskR == Input->MaskR
					&& Output.MaskG == Input->MaskG
					&& Output.MaskB == Input->MaskB
					&& Output.MaskA == Input->MaskA )
				break;
			OutputY += ME_STD_TAB_HEIGHT;
		}

		#define DRAW_LINES( xoffset, yoffset )\
		{\
			RI->DrawLine2D(InputX+xoffset,InputY+8+yoffset,InputX+LINE_LEADER_SZ+8+xoffset,InputY+8+yoffset,FColor(0,0,0));\
			RI->DrawLine2D(InputX+LINE_LEADER_SZ+8+xoffset,InputY+8+yoffset,Input->Expression->EditorX-LINE_LEADER_SZ+xoffset-LabelPadding,OutputY+yoffset,FColor(0,0,0));\
			RI->DrawLine2D(Input->Expression->EditorX,OutputY+yoffset,Input->Expression->EditorX-LINE_LEADER_SZ-LabelPadding,OutputY+yoffset,FColor(0,0,0));\
		}

		DRAW_LINES(0,0);
		DRAW_LINES(1,0);
		DRAW_LINES(0,1);
	}

	if( Tab )
	{
		UTexture2D* tex = Input->Expression ? GApp->MaterialEditor_RightHandleOn : GApp->MaterialEditor_RightHandle;

		RI->SetHitProxy(new HInputHandle(Input));
		RI->DrawTile(InputX,InputY,tex->SizeX,tex->SizeY, 0.f,0.f,1.f,1.f, FLinearColor::White, tex );
		RI->SetHitProxy(NULL);
	}
}

void FExpressionViewportClient::DrawWindow(FRenderInterface* RI,UMaterialExpression* InExpression,FString InCaption,INT InX, INT InY, INT InWidth, INT InHeight,UBOOL InPadRight,UBOOL InHasErrors,UBOOL InSelected,UBOOL InShowX)
{
	FColor clr(0,0,0);
	INT Pad = 1;
	if( InSelected )
	{
		clr = FColor(255,255,0);
		Pad = 2;
	}
	if( InHasErrors )
	{
		clr = FColor(255,0,0);
		Pad = 2;
	}

	InWidth += 32;
	if( InPadRight )
		InWidth += 32;

	// Window body

	RI->DrawTile(InX,InY, InWidth,InHeight, 0.0f,0.0f,0.0f,0.0f, clr, NULL, 0 );

	RI->DrawTile(InX+Pad,InY+Pad, InWidth-(Pad*2),InHeight-(Pad*2), 0.0f,0.0f,0.0f,0.0f, FColor(140,140,140), NULL, 0 );
	RI->DrawTile(InX+Pad,InY+Pad, InWidth-(Pad*2),ME_CAPTION_HEIGHT, 0.0f,0.0f,0.0f,0.0f, FColor(112,112,112), NULL, 0 );
	RI->DrawTile(InX+Pad,InY+1+ME_CAPTION_HEIGHT, InWidth-(Pad*2),1, 0.0f,0.0f,0.0f,0.0f, FColor(0,0,0), NULL, 0 );

	// Caption text

	INT XL, YL;
	RI->StringSize( GEngine->SmallFont, XL, YL, *InCaption );
	RI->DrawShadowedString( InX+((InWidth-XL)/2),InY+((ME_CAPTION_HEIGHT-YL)/2)+2, *InCaption, GEngine->SmallFont, FColor(255,255,128) );

	// Delete button

	if( InShowX )
	{
		UTexture2D* tex = GApp->MaterialEditor_Delete;
		RI->SetHitProxy(new HDeleteExpression(InExpression));
		RI->DrawTile(InX+InWidth-tex->SizeX-4,InY+((ME_CAPTION_HEIGHT-tex->SizeY)/2), tex->SizeX,tex->SizeY, 0.0f,0.0f,1.0f,1.0f, FLinearColor::White, tex );
		RI->SetHitProxy(NULL);
	}

	// Level designer description below the window

	if( InExpression )
	{
		RI->StringSize( GEngine->SmallFont, XL, YL, *InExpression->Desc );
		INT x = InX+((InWidth-XL)/2), y = InY+InHeight+2;
		RI->DrawString( x,y, *InExpression->Desc, GEngine->SmallFont, FColor(64,64,192) );
	}
}

void FExpressionViewportClient::DrawExpression(FRenderInterface* RI,UMaterialExpression* Expression,UBOOL Selected)
{
	DrawWindow( RI,Expression,Expression->GetCaption(),Expression->EditorX,Expression->EditorY,Expression->GetWidth(),Expression->GetHeight(),0,Expression->Errors.Num(),Selected,1);

	FExpressionPreview*	Preview = NULL;
	for(UINT PreviewIndex = 0;PreviewIndex < (UINT)ExpressionPreviews.Num();PreviewIndex++)
		if(ExpressionPreviews(PreviewIndex).Expression == Expression)
		{
			Preview = &ExpressionPreviews(PreviewIndex);
			break;
		}
	if(!Preview)
		Preview = new(ExpressionPreviews) FExpressionPreview(Expression);

	RI->SetHitProxy(new HMaterialExpression(Expression));
	RI->DrawTile(Expression->EditorX+ME_STD_BORDER,Expression->EditorY+ME_CAPTION_HEIGHT+ME_STD_BORDER,ME_STD_THUMBNAIL_SZ,ME_STD_THUMBNAIL_SZ,0,0,1,1,Preview,Material->GetInstanceInterface());
	RI->SetHitProxy(NULL);
}

void FExpressionViewportClient::DrawMaterialPreview(FChildViewport* Viewport,FRenderInterface* RI,UMaterial* InMaterial,UBOOL Selected)
{
	RI->SetHitProxy(new HMaterial(Material));
	DrawWindow( RI,NULL,InMaterial->GetName(),InMaterial->EditorX,InMaterial->EditorY,InMaterial->GetWidth(),InMaterial->GetHeight(),1,0,Selected,0);
	RI->SetHitProxy(NULL);

	if(!RI->IsHitTesting())
		InMaterial->DrawThumbnail( UpperWindow->PrimType, InMaterial->EditorX+ME_STD_BORDER, InMaterial->EditorY+ME_CAPTION_HEIGHT+ME_STD_BORDER, InMaterial->EditorPitch,InMaterial->EditorYaw, Viewport, RI, .5f, bShowBackground, ZoomPct, 0 );
}

void FExpressionViewportClient::Draw(FChildViewport* Viewport,FRenderInterface* RI)
{
	RI->SetOrigin2D( Origin2D );

	// Erase background

	RI->Clear( FColor(197,197,197) );

	// Draw the preview expression window.

	DrawMaterialPreview(Viewport,RI,Material,GSelectionTools.IsSelected(Material));

	// Draw all expressions.

	for(TObjectIterator<UMaterialExpression> It;It;++It)
	{
		if(It->GetOuter() == Material)
		{
			RI->SetHitProxy(new HMaterialExpression(*It));
			DrawExpression(RI,*It,GSelectionTools.IsSelected(*It));
			RI->SetHitProxy(NULL);
		}
	}

	// Draw all connections.

	UStruct*	InputStruct = FindField<UStruct>(UMaterialExpression::StaticClass(),TEXT("ExpressionInput"));

	INT InputTabLength = 90,
		XPos = Material->EditorX+Material->GetWidth()+InputTabLength,
		XPosText = Material->EditorX+Material->GetWidth(),
		YPos = Material->EditorY+ME_CAPTION_HEIGHT+8;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->DiffuseColor,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Diffuse"), new HInputHandle(&UpperWindow->Material->DiffuseColor) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->EmissiveColor,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Emissive"), new HInputHandle(&UpperWindow->Material->EmissiveColor) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->SpecularColor,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Specular"), new HInputHandle(&UpperWindow->Material->SpecularColor) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->SpecularPower,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Sp.Power"), new HInputHandle(&UpperWindow->Material->SpecularPower) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->Opacity,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Opacity"), new HInputHandle(&UpperWindow->Material->Opacity) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->OpacityMask,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Op.Mask"), new HInputHandle(&UpperWindow->Material->OpacityMask) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->Distortion,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Distortion"), new HInputHandle(&UpperWindow->Material->Distortion) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->TwoSidedLightingMask,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Tmission mask"), new HInputHandle(&UpperWindow->Material->TwoSidedLightingMask) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->TwoSidedLightingColor,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Tmission color"), new HInputHandle(&UpperWindow->Material->TwoSidedLightingColor) );
	YPos += ME_STD_TAB_HEIGHT;

	DrawConnection(RI,XPos,YPos,&UpperWindow->Material->Normal,1);
	DrawLabel( RI, XPosText, YPos, InputTabLength, TEXT("Normal"), new HInputHandle(&UpperWindow->Material->Normal) );

	for(TObjectIterator<UMaterialExpression> It;It;++It)
	{
		INT XPos = It->EditorX + It->GetWidth() + 32;
		if(It->GetOuter() == Material)
		{
			INT	YPos = It->EditorY + ME_CAPTION_HEIGHT + 8;

			for(TFieldIterator<UStructProperty> InputIt(It->GetClass());InputIt;++InputIt)
			{
				if(InputIt->Struct == InputStruct)
				{
					FExpressionInput*	Input = (FExpressionInput*)((BYTE*)*It + InputIt->Offset);
					DrawConnection(RI,XPos,YPos,Input,1);
					DrawLabel( RI, XPos-32, YPos, 32, *FString(InputIt->GetName()).Left(5), new HInputHandle(Input) );
					YPos += ME_STD_TAB_HEIGHT;
				}
			}
		}
	}

	// Draw output tabs.

	UTexture2D *LeftHandle = GApp->MaterialEditor_LeftHandle, *LabelGraphic;

	for(TObjectIterator<UMaterialExpression> It;It;++It)
	{
		if(It->GetOuter() == Material)
		{
			TArray<FExpressionOutput>	Outputs;
			It->GetOutputs(Outputs);

			INT	Y = It->EditorY + ME_CAPTION_HEIGHT;
			for(UINT OutputIndex = 0;OutputIndex < (UINT)Outputs.Num();OutputIndex++)
			{
				RI->SetHitProxy(new HOutputHandle(*It,Outputs(OutputIndex)));

				INT XPos = It->EditorX;

				if(Outputs(OutputIndex).Mask)
				{
					if		( Outputs(OutputIndex).MaskR && !Outputs(OutputIndex).MaskG && !Outputs(OutputIndex).MaskB && !Outputs(OutputIndex).MaskA)
						LabelGraphic = GApp->MaterialEditor_R;
					else if	(!Outputs(OutputIndex).MaskR &&  Outputs(OutputIndex).MaskG && !Outputs(OutputIndex).MaskB && !Outputs(OutputIndex).MaskA)
						LabelGraphic = GApp->MaterialEditor_G;
					else if	(!Outputs(OutputIndex).MaskR && !Outputs(OutputIndex).MaskG &&  Outputs(OutputIndex).MaskB && !Outputs(OutputIndex).MaskA)
						LabelGraphic = GApp->MaterialEditor_B;
					else if	(!Outputs(OutputIndex).MaskR && !Outputs(OutputIndex).MaskG && !Outputs(OutputIndex).MaskB &&  Outputs(OutputIndex).MaskA)
						LabelGraphic = GApp->MaterialEditor_A;
					else
						LabelGraphic = GApp->MaterialEditor_RGBA;

					XPos -= LabelGraphic->SizeX;
					RI->DrawTile(XPos,Y,LabelGraphic->SizeX,LabelGraphic->SizeY, 0.f,0.f,1.f,1.f, FLinearColor::White, LabelGraphic );
				}

				XPos -= LeftHandle->SizeX;
				RI->DrawTile(XPos,Y,LeftHandle->SizeX,LeftHandle->SizeY, 0.f,0.f,1.f,1.f, FLinearColor::White, LeftHandle );

				RI->SetHitProxy(NULL);
				Y += ME_STD_TAB_HEIGHT;
			}
		}
	}

	// Draw the connection currently being dragged.

	if(HitOutputExpression)
	{
		FExpressionInput	Input;
		Input.Expression = HitOutputExpression;
		Input.Mask = HitOutput.Mask;
		Input.MaskR = HitOutput.MaskR;
		Input.MaskG = HitOutput.MaskG;
		Input.MaskB = HitOutput.MaskB;
		Input.MaskA = HitOutput.MaskA;
		DrawConnection(RI,NewX,NewY - 8,&Input,0);
	}

	RI->SetOrigin2D( 0, 0 );

	// Errors.

	for(UINT ErrorIndex = 0;ErrorIndex < (UINT)UpperWindow->MaterialEditor->Errors.Num();ErrorIndex++)
		RI->DrawShadowedString(0,Viewport->GetSizeY() - 10 - 20 * (UpperWindow->MaterialEditor->Errors.Num() - ErrorIndex),*UpperWindow->MaterialEditor->Errors(ErrorIndex).Description,GEngine->SmallFont,FLinearColor(1,0,0));
}

void FExpressionViewportClient::DrawLabel( FRenderInterface* RI, INT InX, INT InY, INT InWidth, FString InLabel, HHitProxy* HitProxy )
{
	RI->SetHitProxy(HitProxy);
	RI->DrawTile(InX-8,InY,InWidth+8,GApp->MaterialEditor_LabelBackground->SizeY, 0.f,0.f,1.f,1.f, FLinearColor::White, GApp->MaterialEditor_LabelBackground );
	RI->DrawShadowedString( InX, InY+2, *InLabel, GEngine->SmallFont, FLinearColor::White );
	RI->SetHitProxy(NULL);
}

void FExpressionViewportClient::UpdateSelectionInfo()
{
	SelectedObjects.Empty();

	for( TSelectedObjectIterator It(UMaterialExpression::StaticClass()) ; It ; ++It )
		if( It->GetOuter() == Material && GSelectionTools.IsSelected(*It) )
			SelectedObjects.AddItem( *It );

	if( GSelectionTools.IsSelected(Material) )
		SelectedObjects.AddItem( Material );

	LowerWindow->SetObjectArray( &SelectedObjects, 1,0,0 );
}

UBOOL FExpressionViewportClient::HaveSelections()
{
	return SelectedObjects.Num();
}

void FExpressionViewportClient::InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT)
{
	Viewport->CaptureMouseInput( Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_RightMouseButton) );

	if( Key == KEY_LeftMouseButton )
	{
		switch( Event )
		{
			case IE_Pressed:
			{
				INT			HitX = Viewport->GetMouseX(),
							HitY = Viewport->GetMouseY();				
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
				HitExpression = NULL;

				if( HitResult )
				{
					if( HitResult->IsA(TEXT("HMaterialExpression")) )
					{
						HitExpression = ((HMaterialExpression*)HitResult)->Expression;

						if( !Viewport->IsCtrlDown() )
						{
							GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
							GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
						}
						if( HitExpression )
							GSelectionTools.Select( HitExpression );
					}
					else if( HitResult->IsA(TEXT("HMaterial")) )
					{
						HitExpression = ((HMaterialExpression*)HitResult)->Expression;

						if( !Viewport->IsCtrlDown() )
						{
							GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
							GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
						}
						if( HitExpression )
						{
							GSelectionTools.Select( HitExpression );
							bZooming = 1;
						}
					}
					else if( HitResult->IsA(TEXT("HOutputHandle")) )
					{
						HitOutputExpression = ((HOutputHandle*)HitResult)->Expression;
						HitOutput = ((HOutputHandle*)HitResult)->Output;
						NewX = HitX - Origin2D.X;
						NewY = HitY - Origin2D.Y;
					}
					else if(HitResult->IsA(TEXT("HInputHandle")))
					{
						HitInput = ((HInputHandle*)HitResult)->Input;
					}
					else if(HitResult->IsA(TEXT("HDeleteExpression")))
					{
						HDeleteExpression* hde = ((HDeleteExpression*)HitResult);
						UMaterialExpression* obj = hde->Expression;

						// Eliminate UI references to the expression.

						HitExpression = NULL;
						ExpressionPreviews.Empty();
						Viewport->Invalidate();
						GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
						UpdateSelectionInfo();

						obj->PendingKill = 1;
						for(TObjectIterator<UMaterialExpression> ExpressionIt;ExpressionIt;++ExpressionIt)
							if(ExpressionIt->GetOuter() == Material)
								ExpressionIt->GetClass()->CleanupDestroyed((BYTE*)*ExpressionIt, *ExpressionIt);
						Material->GetClass()->CleanupDestroyed((BYTE*)Material, Material);
						Material->PostEditChange(NULL);
						obj->PendingKill = 0;

						if( UObject::IsReferenced( *(UObject**)&obj, RF_Native | RF_Public, 0 ) )
						{
							appMsgf( 0, TEXT("Expression has references.  Cannot delete.") );
							return;
						}

						delete obj;
					}
					else
					{
						GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
						GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
					}
				}
				else
				{
					if( !Viewport->IsCtrlDown() )
					{
						GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
						GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
					}
				}

				UpdateSelectionInfo();
			}
			break;

			case IE_Released:
			{
				if( HitOutputExpression )
				{
					HHitProxy*	HitResult = Viewport->GetHitProxy(NewX+Origin2D.X,NewY+Origin2D.Y);
					if(HitResult)
					{
						if(HitResult->IsA(TEXT("HInputHandle")))
						{
							FExpressionInput*	Input = ((HInputHandle*)HitResult)->Input;
							Input->Expression = HitOutputExpression;
							Input->Mask = HitOutput.Mask;
							Input->MaskR = HitOutput.MaskR;
							Input->MaskG = HitOutput.MaskG;
							Input->MaskB = HitOutput.MaskB;
							Input->MaskA = HitOutput.MaskA;
							Material->PostEditChange(NULL);
						}
					}

					HitOutputExpression = NULL;
				}
				HitExpression = NULL;
				if( HitInput )
				{
					HitInput->Expression = NULL;
					HitInput = NULL;
					Material->PostEditChange(NULL);
				}
			}

			bZooming = 0;

			break;
		}

		Viewport->Invalidate();
	}
	else if( Key == KEY_RightMouseButton )
	{
		switch( Event )
		{
			case IE_Pressed:
			{
				INT			HitX = Viewport->GetMouseX(),
							HitY = Viewport->GetMouseY();
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
				HitExpression = NULL;

				NewX = HitX;
				NewY = HitY;

				if( HitResult )
				{
					if( HitResult->IsA(TEXT("HMaterialExpression")) )
					{
						HitExpression = ((HMaterialExpression*)HitResult)->Expression;

						if( !Viewport->IsCtrlDown() )
						{
							GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
							GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
						}
						if( HitExpression )
							GSelectionTools.Select( HitExpression );
					}
					else if( HitResult->IsA(TEXT("HInputHandle")) )
					{
						HitInput = ((HInputHandle*)HitResult)->Input;
					}
					else if( HitResult->IsA(TEXT("HMaterial")) )
					{
						bRotating = 1;
					}
					else
					{
						GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
						GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
					}
				}
				else
				{
					if( !Viewport->IsCtrlDown() )
					{
						GSelectionTools.SelectNone( UMaterialExpression::StaticClass() );
						GSelectionTools.SelectNone( UMaterialInstance::StaticClass() );
					}
				}

				UpdateSelectionInfo();
			}
			break;

			case IE_Released:
			{
				if( HitExpression )
				{
					UMaterialExpressionTextureSample* mets = Cast<UMaterialExpressionTextureSample>( HitExpression );

					if( mets )
					{
						WxMBMaterialEditorContext_TextureSample* menu = new WxMBMaterialEditorContext_TextureSample();
						FTrackPopupMenu tpm( UpperWindow, menu );
						tpm.Show();
						delete menu;
					}

					HitExpression = NULL;
				}
				else
				{
					if( !bRotating || !Viewport->MouseHasMoved() )
					{
						WxMBMaterialEditorContext_Base* menu = new WxMBMaterialEditorContext_Base();
						FTrackPopupMenu tpm( UpperWindow, menu );
						tpm.Show();
						delete menu;
					}
				}
			}

			bRotating = 0;

			break;
		}

		Viewport->Invalidate();
	}
}

void FExpressionViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{
	if( bRotating )
	{
		Material->EditorPitch += (Key == KEY_MouseY) * (Delta*10);
		Material->EditorYaw += (Key == KEY_MouseX) * (Delta*10);

		Viewport->Invalidate();
	}
	else if( bZooming )
	{
		ZoomPct -= (Key == KEY_MouseY) * (-Delta/100);
		ZoomPct = Clamp<FLOAT>( ZoomPct, 1.0, 2.0 );

		Viewport->Invalidate();
	}
	else if( HitOutputExpression )
	{
		NewX += (Key == KEY_MouseX) * Delta;
		NewY -= (Key == KEY_MouseY) * Delta;

		Viewport->Invalidate();
	}
	else if( HaveSelections() )
	{
		for( INT x = 0 ; x < SelectedObjects.Num() ; ++x )
		{
			UMaterialExpression* me = Cast<UMaterialExpression>( SelectedObjects(x) );

			if( me )
			{
				me->EditorX += (Key == KEY_MouseX) * Delta;
				me->EditorY -= (Key == KEY_MouseY) * Delta;
			}
			else
			{
				Material->EditorX += (Key == KEY_MouseX) * Delta;
				Material->EditorY -= (Key == KEY_MouseY) * Delta;
			}
		}
				
		Viewport->Invalidate();
	}
	else
	{
		Origin2D.X += (Key == KEY_MouseX) * Delta;
		Origin2D.Y -= (Key == KEY_MouseY) * Delta;

		Viewport->Invalidate();
	}
}

EMouseCursor FExpressionViewportClient::GetCursor(FChildViewport* Viewport,INT X,INT Y)
{
	HHitProxy*	HitProxy = Viewport->GetHitProxy(X,Y);
	if(HitProxy)
		return MC_Cross;
	else
		return MC_Arrow;
}

/*-----------------------------------------------------------------------------
	WxMaterialEditor_Upper
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxMaterialEditor_Upper, wxWindow )
	EVT_SIZE( WxMaterialEditor_Upper::OnSize )
	EVT_MENU_RANGE( IDM_NEW_SHADER_EXPRESSION_START, IDM_NEW_SHADER_EXPRESSION_END, WxMaterialEditor_Upper::OnContextNewShaderExpression )
	EVT_MENU( IDM_USE_CURRENT_TEXTURE, WxMaterialEditor_Upper::OnUseCurrenTexture )
END_EVENT_TABLE()

WxMaterialEditor_Upper::WxMaterialEditor_Upper( wxWindow* InParent, WxMaterialEditor* InMaterialEditor, wxWindowID InID, UMaterial* InMaterial, WxPropertyWindow* InLowerWindow )
	: wxWindow( InParent, InID )
	, MaterialEditor(InMaterialEditor)
{
	Material = InMaterial;
	PrimType = TPT_Sphere;

	ExpressionViewportClient = new FExpressionViewportClient( Material, this, InLowerWindow );
	ExpressionViewportClient->Viewport = GEngine->Client->CreateWindowChildViewport(ExpressionViewportClient,(HWND)GetHandle());
	ExpressionViewportClient->Viewport->CaptureJoystickInput(false);
}

WxMaterialEditor_Upper::~WxMaterialEditor_Upper()
{
	GEngine->Client->CloseViewport(ExpressionViewportClient->Viewport);
	ExpressionViewportClient->Viewport = NULL;
	delete ExpressionViewportClient;
}

void WxMaterialEditor_Upper::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	::MoveWindow( (HWND)ExpressionViewportClient->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}

void WxMaterialEditor_Upper::OnContextNewShaderExpression( wxCommandEvent& In )
{
	UClass* Class = *GApp->ShaderExpressionMap.Find( In.GetId() );

	if( Class )
	{
		UMaterialExpression*	NewExpression = ConstructObject<UMaterialExpression>( Class, Material );

		if(ExpressionViewportClient->HitInput)
		{
			TArray<FExpressionOutput>	Outputs;
			NewExpression->GetOutputs(Outputs);

			ExpressionViewportClient->HitInput->Expression = NewExpression;
			ExpressionViewportClient->HitInput->Mask = Outputs(0).Mask;
			ExpressionViewportClient->HitInput->MaskR = Outputs(0).MaskR;
			ExpressionViewportClient->HitInput->MaskG = Outputs(0).MaskG;
			ExpressionViewportClient->HitInput->MaskB = Outputs(0).MaskB;
			ExpressionViewportClient->HitInput->MaskA = Outputs(0).MaskA;
			ExpressionViewportClient->HitInput = NULL;
		}

		NewExpression->EditorX = ExpressionViewportClient->NewX - ExpressionViewportClient->Origin2D.X;
		NewExpression->EditorY = ExpressionViewportClient->NewY - ExpressionViewportClient->Origin2D.Y;

		// If the user is adding a texture sample, automatically assign the currently selected texture to it

		UMaterialExpressionTextureSample* mets = Cast<UMaterialExpressionTextureSample>(NewExpression);
		if( mets )
		{
			mets->Texture = GSelectionTools.GetTop<UTexture>();
		}

		Material->PostEditChange(NULL);
	}
}

void WxMaterialEditor_Upper::OnUseCurrenTexture( wxCommandEvent& In )
{
	// Set the currently selected texture as the texture to use in all selected texture sample expressions.

	UMaterialExpressionTextureSample* mets = GSelectionTools.GetTop<UMaterialExpressionTextureSample>();

	mets->Texture = GSelectionTools.GetTop<UTexture>();

	Material->PostEditChange(NULL);
}

/*-----------------------------------------------------------------------------
	WxMaterialEditor
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxMaterialEditor, wxFrame )
	EVT_SIZE( WxMaterialEditor::OnSize )
	EVT_TOOL( IDM_SHOW_BACKGROUND, WxMaterialEditor::OnShowBackground )
	EVT_TOOL( ID_PRIMTYPE_PLANE, WxMaterialEditor::OnPrimTypePlane )
	EVT_TOOL( ID_PRIMTYPE_CYLINDER, WxMaterialEditor::OnPrimTypeCylinder )
	EVT_TOOL( ID_PRIMTYPE_CUBE, WxMaterialEditor::OnPrimTypeCube )
	EVT_TOOL( ID_PRIMTYPE_SPHERE, WxMaterialEditor::OnPrimTypeSphere )
	EVT_TOOL( ID_GO_HOME, WxMaterialEditor::OnCameraHome )
	EVT_TOOL( IDM_REALTIME, WxMaterialEditor::OnRealTime )
	EVT_UPDATE_UI( IDM_SHOW_BACKGROUND, WxMaterialEditor::UI_ShowBackground )
	EVT_UPDATE_UI( ID_PRIMTYPE_PLANE, WxMaterialEditor::UI_PrimTypePlane )
	EVT_UPDATE_UI( ID_PRIMTYPE_CYLINDER, WxMaterialEditor::UI_PrimTypeCylinder )
	EVT_UPDATE_UI( ID_PRIMTYPE_CUBE, WxMaterialEditor::UI_PrimTypeCube )
	EVT_UPDATE_UI( ID_PRIMTYPE_SPHERE, WxMaterialEditor::UI_PrimTypeSphere )
	EVT_UPDATE_UI( IDM_REALTIME, WxMaterialEditor::UI_RealTime )
END_EVENT_TABLE()

WxMaterialEditor::WxMaterialEditor( wxWindow* InParent, wxWindowID InID, UMaterial* InMaterial )
	: wxFrame( InParent, InID, TEXT("wxMaterial Editor"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	Material = InMaterial;

	ToolBar = new WxMaterialEditorToolBar( (wxWindow*)this, -1 );
	SetToolBar( ToolBar );

	wxBoxSizer* item2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(item2);
	SetAutoLayout(TRUE);

	SplitterWnd = new wxSplitterWindow(this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );

	LowerWindow = new WxPropertyWindow( (wxWindow*)SplitterWnd, GUnrealEd );
	UpperWindow = new WxMaterialEditor_Upper( (wxWindow*)SplitterWnd, this, -1, Material, LowerWindow );

	SplitterWnd->SplitHorizontally( UpperWindow, LowerWindow, 600 );
	item2->Add(SplitterWnd, 1, wxGROW|wxALL, 0);

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);
	Centre();

	GResourceManager->RegisterClient(this);
	Material->PostEditChange(NULL);
    wxCommandEvent eventCommand;
	OnCameraHome( eventCommand );

	FWindowUtil::LoadPosSize( TEXT("MaterialEditor"), this );
}

WxMaterialEditor::~WxMaterialEditor()
{
	FWindowUtil::SavePosSize( TEXT("MaterialEditor"), this );

	GResourceManager->DeregisterClient(this);
}

void WxMaterialEditor::RequestMips( FTextureBase* Texture, UINT RequestedMips, FTextureMipRequest* TextureMipRequest )
{
}

void WxMaterialEditor::FinalizeMipRequest( FTextureMipRequest* TextureMipRequest )
{
}

void WxMaterialEditor::FreeResource(FResource* Resource)
{
}

void WxMaterialEditor::UpdateResource(FResource* Resource)
{
	if(Resource == Material->MaterialResources[0])
	{
		URenderDevice*	RenderDevice = NULL;
		for(TObjectIterator<URenderDevice> It;It;++It)
		{
			RenderDevice = *It;
			break;
		}

		if(RenderDevice)
			Errors = RenderDevice->CompileMaterial(Material->MaterialResources[0]);
		else
			Errors.Empty();

		UpperWindow->ExpressionViewportClient->RefreshExpressionPreviews();

		if( UpperWindow->ExpressionViewportClient->Viewport )
			UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
	}
}

void WxMaterialEditor::OnSize( wxSizeEvent& In )
{
	if( SplitterWnd )
	{
		wxRect rc = GetClientRect();

		SplitterWnd->SetSize( rc );
	}

	In.Skip();
}

void WxMaterialEditor::OnShowBackground( wxCommandEvent& In )
{
	UpperWindow->ExpressionViewportClient->bShowBackground = !UpperWindow->ExpressionViewportClient->bShowBackground;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnPrimTypePlane( wxCommandEvent& In )
{
	UpperWindow->PrimType = TPT_Plane;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnPrimTypeCylinder( wxCommandEvent& In )
{
	UpperWindow->PrimType = TPT_Cylinder;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnPrimTypeCube( wxCommandEvent& In )
{
	UpperWindow->PrimType = TPT_Cube;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnPrimTypeSphere( wxCommandEvent& In )
{
	UpperWindow->PrimType = TPT_Sphere;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnCameraHome( wxCommandEvent& In )
{
	UpperWindow->ExpressionViewportClient->Origin2D = FIntPoint(-Material->EditorX,-Material->EditorY);
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::OnRealTime( wxCommandEvent& In )
{
	UpperWindow->ExpressionViewportClient->Realtime = !UpperWindow->ExpressionViewportClient->Realtime;
	UpperWindow->ExpressionViewportClient->Viewport->Invalidate();
}

void WxMaterialEditor::UI_ShowBackground( wxUpdateUIEvent& In )		{	In.Check( UpperWindow->ExpressionViewportClient->bShowBackground ? true : false );		}
void WxMaterialEditor::UI_PrimTypePlane( wxUpdateUIEvent& In )		{	In.Check( UpperWindow->PrimType == TPT_Plane );		}
void WxMaterialEditor::UI_PrimTypeCylinder( wxUpdateUIEvent& In )	{	In.Check( UpperWindow->PrimType == TPT_Cylinder );	}
void WxMaterialEditor::UI_PrimTypeCube( wxUpdateUIEvent& In )		{	In.Check( UpperWindow->PrimType == TPT_Cube );		}
void WxMaterialEditor::UI_PrimTypeSphere( wxUpdateUIEvent& In )		{	In.Check( UpperWindow->PrimType == TPT_Sphere );	}
void WxMaterialEditor::UI_RealTime( wxUpdateUIEvent& In )			{	In.Check( UpperWindow->ExpressionViewportClient->Realtime ? true : false ); }

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
