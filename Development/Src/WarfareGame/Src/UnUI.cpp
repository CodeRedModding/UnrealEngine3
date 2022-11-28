
#include "WarfareGame.h"

IMPLEMENT_CLASS(UUIElement);
IMPLEMENT_CLASS(UUIElement_Icon);
IMPLEMENT_CLASS(UUIElement_Label);

IMPLEMENT_CLASS(UUIContainer);
//IMPLEMENT_CLASS(UGenericBrowserType_UIContainer);

FThumbnailDesc UUIContainer::GetThumbnailDesc(FRenderInterface *InRI, FLOAT InZoom, INT InFixedSz)
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_Sequence"), NULL, LOAD_NoFail, NULL);
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width	= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height = InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT UUIContainer::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	return InLabels->Num();
}

void UUIContainer::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_Sequence"), NULL, LOAD_NoFail, NULL);
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

/** UUIContainer */

void UUIContainer::execDraw(FFrame &Stack,RESULT_DECL)
{
	P_GET_OBJECT(UCanvas,inCanvas);
	P_FINISH;
	DrawElement(inCanvas);
}

void UUIContainer::DrawElement(UCanvas *inCanvas)
{
	checkSlow(inCanvas != NULL);
	INT svdOrgX = inCanvas->OrgX, svdOrgY = inCanvas->OrgY, svdClipX = inCanvas->ClipX, svdClipY = inCanvas->ClipY;
	inCanvas->OrgX += DrawX;
	inCanvas->OrgY += DrawY;
	inCanvas->ClipX = inCanvas->OrgX + DrawW;
	inCanvas->ClipY = inCanvas->OrgY + DrawH;
	for (INT idx = 0; idx < Elements.Num(); idx++)
	{
		if (Elements(idx) != NULL &&
			Elements(idx)->bEnabled)
		{
			Elements(idx)->DrawElement(inCanvas);
		}
	}
	inCanvas->OrgX = svdOrgX;
	inCanvas->OrgY = svdOrgY;
	inCanvas->ClipX = svdClipX;
	inCanvas->ClipY = svdClipY;
	Super::DrawElement(inCanvas);
}

UUIElement* UUIContainer::FindElement(FName searchName)
{
	UUIElement *element = NULL;
	for (INT idx = 0; idx < Elements.Num() && element == NULL; idx++)
	{
		if (Elements(idx) != NULL)
		{
			if (Elements(idx)->GetFName() == searchName)
			{
				element = Elements(idx);
			}
			else
			if (Elements(idx)->IsA(UUIContainer::StaticClass()))
			{
				element = ((UUIContainer*)Elements(idx))->FindElement(searchName);
			}
		}
	}
	return element;
}

void UUIContainer::execFindElement(FFrame &Stack,RESULT_DECL)
{
	P_GET_NAME(searchName);
	P_FINISH;
	*(UUIElement**)Result = FindElement(searchName);
}

/** UUIElement */

void UUIElement::DrawElement(UCanvas *inCanvas)
{
	checkSlow(inCanvas != NULL);
	// draw a border if specified
	if (bDrawBorder &&
		BorderIcon != NULL &&
		BorderSize != 0)
	{
		INT drawX, drawY, drawW = inCanvas->ClipX, drawH = inCanvas->ClipY;
		// calculate the actual draw width/height (scaled)
		GetDimensions(drawX,drawY,drawW,drawH);
		BorderUSize = BorderUSize == -1 ? BorderIcon->SizeX : BorderUSize;
		BorderVSize = BorderVSize == -1 ? BorderIcon->SizeY : BorderVSize;
		// top bar
		inCanvas->DrawTile(BorderIcon,
						   inCanvas->OrgX+drawX,inCanvas->OrgY+drawY,
						   drawW,BorderSize,
						   BorderU,BorderV,
						   BorderUSize,BorderVSize,BorderColor);
		// bottom bar
		inCanvas->DrawTile(BorderIcon,
						   inCanvas->OrgX+drawX,inCanvas->OrgY+drawY+drawH-BorderSize,
						   drawW,BorderSize,
						   BorderU,BorderV,
						   BorderUSize,BorderVSize,BorderColor);
		// left bar
		inCanvas->DrawTile(BorderIcon,
						   inCanvas->OrgX+drawX,inCanvas->OrgY+drawY+BorderSize,
						   BorderSize,drawH-(BorderSize*2),
						   BorderU,BorderV,
						   BorderUSize,BorderVSize,BorderColor);
		// right bar
		inCanvas->DrawTile(BorderIcon,
						   inCanvas->OrgX+drawX+drawW-BorderSize,inCanvas->OrgY+drawY+BorderSize,
						   BorderSize,drawH-(BorderSize*2),
						   BorderU,BorderV,
						   BorderUSize,BorderVSize,BorderColor);
	}
}

void UUIElement::GetDimensions(INT &X, INT &Y, INT &W, INT &H)
{
	X = DrawX;
	Y = DrawY;
	W = DrawW;
	H = DrawH;
}

/** UUIElement_Icon */

void UUIElement_Icon::DrawElement(UCanvas *inCanvas)
{
	checkSlow(inCanvas != NULL);
	INT drawX, drawY, drawW = inCanvas->ClipX, drawH = inCanvas->ClipY;
	// calculate the actual draw width/height (scaled)
	GetDimensions(drawX,drawY,drawW,drawH);
	// draw the icon if specified
	if (DrawIcon != NULL)
	{
		// and draw the bastard
		DrawUSize = DrawUSize == -1 ? DrawIcon->SizeX : DrawUSize;
		DrawVSize = DrawVSize == -1 ? DrawIcon->SizeY : DrawVSize;
		inCanvas->DrawTile(DrawIcon,
						   inCanvas->OrgX+drawX,inCanvas->OrgY+drawY,
						   drawW,drawH,
						   DrawU,DrawV,
						   DrawUSize,DrawVSize,DrawColor);
	}
	Super::DrawElement(inCanvas);
}

void UUIElement_Icon::GetDimensions(INT &X, INT &Y, INT &W, INT &H)
{
	INT parentW = W, parentH = H, parentX = 0, parentY = 0;
	if (ParentElement != NULL)
	{
		ParentElement->GetDimensions(parentX,parentY,parentW,parentH);
	}
	W = (INT)(DrawW * DrawScale);
	H = (INT)(DrawH * DrawScale);
	// calculate the draw coordinates, handling centering as necessary
	X = parentX + DrawX;
	Y = parentY + DrawY;
	if (bCenterX)
	{
		X -= (INT)(W/2.f);
	}
	if (bCenterY)
	{
		Y -= (INT)(H/2.f);
	}
}

/** UUIElement_Label */

void UUIElement_Label::DrawElement(UCanvas *inCanvas)
{
	checkSlow(inCanvas != NULL);
	// draw the label if specified
	if (DrawLabel != TEXT("") &&
		DrawFont != NULL)	
	{
		INT drawX, drawY, drawW = inCanvas->ClipX, drawH = inCanvas->ClipY;
		GetDimensions(drawX,drawY,drawW,drawH);
		inCanvas->DrawColor = DrawColor;
		INT W, H;
		inCanvas->CurX = drawX + inCanvas->OrgX;
		inCanvas->CurY = drawY + inCanvas->OrgY;
		inCanvas->WrappedPrint(1,W,H,DrawFont,1.f,1.f,0,*DrawLabel);
	}
	Super::DrawElement(inCanvas);
}

void UUIElement_Label::GetDimensions(INT &X, INT &Y, INT &W, INT &H)
{
	INT parentX = 0, parentY = 0, parentW = W, parentH = H;
	if (ParentElement != NULL)
	{
		ParentElement->GetDimensions(parentX,parentY,parentW,parentH);
	}
	UCanvas::ClippedStrLen(DrawFont,1.f,1.f,W,H,*DrawLabel);
	// calculate the draw coordinates, handling centering as necessary
	X = parentX + DrawX;
	Y = parentY + DrawY;
	if (bCenterX)
	{
		X -= (INT)(W/2.f);
	}
	if (bCenterY)
	{
		Y -= (INT)(H/2.f);
	}
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_UIContainer
------------------------------------------------------------------------------*/

/*
void UGenericBrowserType_UIContainer::Init()
{
	FColor classColor(255,128,0);
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UUIContainer::StaticClass(), classColor, NULL ) );
}

UBOOL UGenericBrowserType_UIContainer::ShowObjectEditor(UObject* InObject)
{
	WxUIEditor* uiEditor = new WxUIEditor( (wxWindow*)GApp->EditorFrame, -1, Cast<UUIContainer>(InObject) );
	uiEditor->SetSize(1024,768);
	uiEditor->Show();
	return 1;
}
*/
