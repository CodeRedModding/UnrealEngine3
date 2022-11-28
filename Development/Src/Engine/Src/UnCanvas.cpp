/*=============================================================================
	UnCanvas.cpp: Unreal canvas rendering.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* 31/3/99 Updated revision history - Jack Porter
	* 04/4/03 Hi - Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UCanvas scaled sprites.
-----------------------------------------------------------------------------*/

//
// Draw arbitrary aligned rectangle.
//
void UCanvas::DrawTile
(
	UTexture2D*			Tex,
	FLOAT				X,
	FLOAT				Y,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				U,
	FLOAT				V,
	FLOAT				UL,
	FLOAT				VL,
	const FLinearColor&	Color
)
{
    if ( !RenderInterface || !Tex ) 
        return;

	FLOAT w = X + XL > ClipX ? ClipX - X : XL;
	FLOAT h = Y + YL > ClipY ? ClipY - Y : YL;
	if (XL > 0.f &&
		YL > 0.f)
	{
		RenderInterface->DrawTile(X,Y,w,h,U/Tex->SizeX,V/Tex->SizeY,UL/Tex->SizeX * w/XL,VL/Tex->SizeY * h/YL,Color,Tex);
	}
}

void UCanvas::DrawMaterialTile(
	UMaterialInstance*	Material,
	FLOAT				X,
	FLOAT				Y,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				U,
	FLOAT				V,
	FLOAT				UL,
	FLOAT				VL
	)
{
    if ( !RenderInterface || !Material ) 
        return;

	RenderInterface->DrawTile(X,Y,XL,YL,U,V,UL,VL,Material->GetMaterialInterface(0),Material->GetInstanceInterface());
}

//
// Draw titling pattern.
//
void UCanvas::DrawPattern
(
	UTexture2D*			Tex,
	FLOAT				X,
	FLOAT				Y,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				Scale,
	FLOAT				OrgX,
	FLOAT				OrgY,
	const FLinearColor&	Color
)
{
	DrawTile( Tex, X, Y, XL, YL, (X-OrgX)*Scale + Tex->SizeX, (Y-OrgY)*Scale + Tex->SizeY, XL*Scale, YL*Scale, Color );
}

//
// Draw a scaled sprite.  Takes care of clipping.
// XSize and YSize are in pixels.
//
void UCanvas::DrawIcon
(
	UTexture2D*			Tex,
	FLOAT				ScreenX, 
	FLOAT				ScreenY, 
	FLOAT				XSize, 
	FLOAT				YSize,
	const FLinearColor&	Color
)
{
	DrawTile( Tex, ScreenX, ScreenY, XSize, YSize, 0, 0, 1, 1, Color );
}

/*-----------------------------------------------------------------------------
	Clip window.
-----------------------------------------------------------------------------*/

void UCanvas::SetClip( INT X, INT Y, INT XL, INT YL )
{
	CurX  = 0;
	CurY  = 0;
	OrgX  = X;
	OrgY  = Y;
	ClipX = XL;
	ClipY = YL;

}

/*-----------------------------------------------------------------------------
	UCanvas basic text functions.
-----------------------------------------------------------------------------*/


// Notice: This will actually append to the array for continous wrapping, does not support tabs yet
void UCanvas::WrapStringToArray( const TCHAR *Text, TArray<FString> *OutArray, float Width, UFont *pFont, TCHAR EOL)
{
	check(Text != NULL);
	check(OutArray != NULL);

	const TCHAR *LastText;

	if (*Text == '\0')
		return;

	if (pFont == NULL)
		pFont = this->Font;

	do
	{
		while (*Text == EOL)
		{
		INT iAdded = OutArray->AddZeroed();

			(*OutArray)(iAdded) = FString(TEXT(""));
			Text++;
		}
		if (*Text==0)
			break;

		LastText = Text;

		INT iCleanWordEnd=0, iTestWord;
		INT TestXL=0, CleanXL=0;
		UBOOL GotWord=0;
		for( iTestWord=0; Text[iTestWord]!=0 && Text[iTestWord]!=EOL; )
		{
			INT ChW, ChH;
			pFont->GetCharSize(Text[iTestWord], ChW, ChH);
			TestXL              += (INT) ((FLOAT)(ChW + SpaceX /*+ pFont->Kerning*/)); 
			if( TestXL>Width )
				break;
			iTestWord++;
			UBOOL WordBreak = Text[iTestWord]==' ' || Text[iTestWord]==EOL || Text[iTestWord]==0;
			if( WordBreak || !GotWord )
			{
				iCleanWordEnd = iTestWord;
				CleanXL       = TestXL;
				GotWord       = GotWord || WordBreak;
			}
		}
		if( iCleanWordEnd==0 )
		{
			if (*Text != 0)
			{
			INT iAdded = OutArray->AddZeroed();

				if (Text == LastText)
				{
					(*OutArray)(iAdded) = FString::Printf(TEXT("%c"), Text);
					Text++;
				}
				else
					(*OutArray)(iAdded) = FString(TEXT(""));
			}
		}
		else
		{
		FString TextLine(Text);
		INT iAdded = OutArray->AddZeroed();

			(*OutArray)(iAdded) = TextLine.Left(iCleanWordEnd);
		}
		Text += iCleanWordEnd;

		// Skip whitespace after word wrap.
		while( *Text==' ' )
			Text++;

		if (*Text == EOL)
			Text++;
	}
	while( *Text );
}

void UCanvas::execWrapStringToArray( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_TARRAY_REF(OutArray, FString);
	P_GET_FLOAT(Width);
	P_GET_STR_OPTX(EOL, TEXT("|"));
	P_FINISH;

	WrapStringToArray(*InText, OutArray, Width, NULL, EOL[0]);

}
IMPLEMENT_FUNCTION( UCanvas, -1, execWrapStringToArray );

//
// Compute size and optionally print text with word wrap.
//!!For the next generation, redesign to ignore CurX,CurY.
//
void UCanvas::WrappedPrint( UBOOL Draw, INT& XL, INT& YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text ) 
{
	// FIXME: Wrapped Print is screwed which kills the hud.. fix later

	if( ClipX<0 || ClipY<0 )
		return;
	check(Font);

	// Process each word until the current line overflows.
	XL = YL = 0;
	do
	{
		INT iCleanWordEnd=0, iTestWord;
		INT TestXL=(INT)CurX, CleanXL=0;
		INT TestYL=0,    CleanYL=0;
		UBOOL GotWord=0;
		for( iTestWord=0; Text[iTestWord]!=0 && Text[iTestWord]!='\n'; )
		{
			INT ChW, ChH;
			Font->GetCharSize(Text[iTestWord], ChW, ChH);
			TestXL              += (INT) ((FLOAT)(ChW + SpaceX /*+ Font->Kerning*/) * ScaleX ); 
			TestYL               = (INT) ((FLOAT)(Max( TestYL, ChH + (INT)SpaceY)) * ScaleY); 
			if( TestXL>ClipX )
				break;
			iTestWord++;
			UBOOL WordBreak = Text[iTestWord]==' ' || Text[iTestWord]=='\n' || Text[iTestWord]==0;
			if( WordBreak || !GotWord )
			{
				iCleanWordEnd = iTestWord;
				CleanXL       = TestXL;
				CleanYL       = TestYL;
				GotWord       = GotWord || WordBreak;
			}
		}
		if( iCleanWordEnd==0 )
			break;

		// Sucessfully split this line, now draw it.
		if( Draw && OrgY+CurY<SizeY && OrgY+CurY+CleanYL>0 )
		{
			FString TextLine(Text);
			INT LineX = Center ? (INT) (CurX+(ClipX-CleanXL)/2) : (INT) (CurX);
			LineX += RenderInterface->DrawString(LineX,(INT)CurY,*(TextLine.Left(iCleanWordEnd)),Font,DrawColor);
			CurX = LineX;
		}

		// Update position.
		CurX  = 0;
		CurY += CleanYL;
		YL   += CleanYL;
		XL    = Max(XL,CleanXL);
		Text += iCleanWordEnd;

		// Skip whitespace after word wrap.
		while( *Text==' ' )
			Text++;
	}
	while( *Text );

}

/*-----------------------------------------------------------------------------
	UCanvas derived text functions.
-----------------------------------------------------------------------------*/

//
// Calculate the size of a string built from a font, word wrapped
// to a specified region.
//
void VARARGS UCanvas::WrappedStrLenf( UFont* Font, INT& XL, INT& YL, const TCHAR* Fmt, ... )
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	WrappedPrint( 0, XL, YL, Font, 1.0F, 1.0F, 0, Text ); 
}

//
// Wrapped printf.
//
void VARARGS UCanvas::WrappedPrintf( UFont* Font, UBOOL Center, const TCHAR* Fmt, ... )
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	INT XL=0, YL=0;
	WrappedPrint( 1, XL, YL, Font, 1.0F, 1.0F, Center, Text ); 
}

//
// Calculate the size of a string built from a font, word wrapped
// to a specified region.
//
void VARARGS UCanvas::WrappedStrLenf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ... ) 
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	WrappedPrint( 0, XL, YL, Font, ScaleX, ScaleY, 0, Text ); 
}

//
// Wrapped printf.
//
void VARARGS UCanvas::WrappedPrintf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Fmt, ... ) 
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	INT XL=0, YL=0;
	WrappedPrint( 1, XL, YL, Font, ScaleX, ScaleY, Center, Text ); 
}

void UCanvas::ClippedStrLen( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Text )
{
	XL = 0;
	YL = 0;
	if (Font != NULL)
	{
		for( INT i=0; Text[i]; i++)
		{
			INT W, H;
			Font->GetCharSize( Text[i], W, H );
			if (Text[i + 1])
			{
				W = (INT)((FLOAT)(W /*+ SpaceX + Font->Kerning*/) * ScaleX);
			}
			else
			{
				W = (INT)((FLOAT)(W) * ScaleX);
			}
			H = (INT)((FLOAT)(H) * ScaleY);
			XL += W;
			if(YL < H)
			{
				YL = H;	
			}
		}
	}
}

void UCanvas::ClippedPrint( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text )
{
	RenderInterface->DrawString((INT)(OrgX+CurX),(INT)(OrgY+CurY),Text,Font,DrawColor);
}

/*-----------------------------------------------------------------------------
	UCanvas object functions.
-----------------------------------------------------------------------------*/

void UCanvas::Init()
{
#if 0
	// Load default fonts
	TinyFont	= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *TinyFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !TinyFont )
	{
		warnf(TEXT("Could not load stock Tiny font %s"), *TinyFontName);
		TinyFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
	SmallFont	= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *SmallFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !SmallFont )
	{
		warnf(TEXT("Could not load stock Small font %s"), *SmallFontName);
		SmallFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
	MedFont		= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *MedFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !MedFont )
	{
		warnf(TEXT("Could not load stock Medium font %s"), *MedFontName);
		MedFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
#endif
}
void UCanvas::Update()
{
	// Call UnrealScript to reset.
	eventReset();

	// Copy size parameters from viewport.
	ClipX = SizeX;
	ClipY = SizeY;

}

/*-----------------------------------------------------------------------------
	UCanvas natives.
-----------------------------------------------------------------------------*/

void UCanvas::execStrLen( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_FINISH;

	INT XLi, YLi;
	INT OldCurX, OldCurY;
	OldCurX = (INT) CurX;
	OldCurY = (INT) CurY;
	CurX = 0;
	CurY = 0;
	WrappedStrLenf( Font, 1.f, 1.f, XLi, YLi, TEXT("%s"), *InText ); 
	CurY = OldCurY;
	CurX = OldCurX;
	*XL = XLi;
	*YL = YLi;

}
IMPLEMENT_FUNCTION( UCanvas, 464, execStrLen );

void UCanvas::execDrawText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_UBOOL_OPTX(CR,1);
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( NAME_Warning, TEXT("DrawText: No font") ); 
		return;
	}
	INT XL=0, YL=0, OldCurX=CurX, OldCurY=CurY;
	WrappedPrint( 1, XL, YL, Font, 1.f, 1.f, bCenter, *InText ); 
    
	CurX += XL;
	CurYL = Max(CurYL,(FLOAT)YL);
	if( CR )
	{
		CurX	= OldCurX;
		CurY	= OldCurY + CurYL;
		CurYL	= 0;
	}

}
IMPLEMENT_FUNCTION( UCanvas, 465, execDrawText );

void UCanvas::execDrawTile( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture2D,Tex);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_FINISH;
	if( !Tex )
		return;
	DrawTile
	(
		Tex,
		OrgX+CurX,
		OrgY+CurY,
		XL,
		YL,
		U,
		V,
		UL,
		VL,
		DrawColor
	);
	CurX += XL + SpaceX;
	CurYL = Max(CurYL,YL);
}
IMPLEMENT_FUNCTION( UCanvas, 466, execDrawTile );

void UCanvas::execDrawMaterialTile( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UMaterialInstance,Material);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT_OPTX(U,0.f);
	P_GET_FLOAT_OPTX(V,0.f);
	P_GET_FLOAT_OPTX(UL,1.f);
	P_GET_FLOAT_OPTX(VL,1.f);
	P_FINISH;
	if(!Material)
		return;
	DrawMaterialTile
	(
		Material,
		OrgX+CurX,
		OrgY+CurY,
		XL,
		YL,
		U,
		V,
		UL,
		VL
	);
	CurX += XL + SpaceX;
	CurYL = Max(CurYL,YL);
}
IMPLEMENT_FUNCTION( UCanvas, INDEX_NONE, execDrawMaterialTile );

void UCanvas::execDrawTileClipped( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture2D,Tex);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_FINISH;

	if( !Tex )
	{
		Stack.Logf( TEXT("DrawTileClipped: Missing Material") );
		return;
	}


	// Clip to ClipX and ClipY
	if( XL > 0 && YL > 0 )
	{		
		if( CurX<0 )
			{FLOAT C=CurX*UL/XL; U-=C; UL+=C; XL+=CurX; CurX=0;}
		if( CurY<0 )
			{FLOAT C=CurY*VL/YL; V-=C; VL+=C; YL+=CurY; CurY=0;}
		if( XL>ClipX-CurX )
			{UL+=(ClipX-CurX-XL)*UL/XL; XL=ClipX-CurX;}
		if( YL>ClipY-CurY )
			{VL+=(ClipY-CurY-YL)*VL/YL; YL=ClipY-CurY;}
	
		DrawTile
		(
			Tex,
			OrgX+CurX,
			OrgY+CurY,
			XL,
			YL,
			U,
			V,
			UL,
			VL,
			DrawColor
		);

		CurX += XL + SpaceX;
		CurYL = Max(CurYL,YL);
	}

}
IMPLEMENT_FUNCTION( UCanvas, 468, execDrawTileClipped );

void UCanvas::execDrawTextClipped( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_UBOOL_OPTX(CheckHotKey, 0);
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( TEXT("DrawTextClipped: No font") ); 
		return;
	}

	check(Font);

	RenderInterface->DrawString((INT)CurX,(INT)CurY,*InText,Font,DrawColor);

}
IMPLEMENT_FUNCTION( UCanvas, 469, execDrawTextClipped );

void UCanvas::execTextSize( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_FINISH;

    INT XLi, YLi;

	if( !Font )
	{
		Stack.Logf( TEXT("TextSize: No font") ); 
		return;
	}

	ClippedStrLen( Font, 1.f, 1.f, XLi, YLi, *InText );
	
	*XL = XLi;
	*YL = YLi;

}
IMPLEMENT_FUNCTION( UCanvas, 470, execTextSize );

void UCanvas::execDrawTileStretched( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture2D,Tex);
	P_GET_FLOAT(AWidth);
	P_GET_FLOAT(AHeight);
	P_FINISH;

	DrawTileStretched(Tex,CurX, CurY, AWidth, AHeight);

}
IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileStretched );

void UCanvas::DrawTileStretched(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight)
{

	CurX = Left;
	CurY = Top;

	if( !Tex )
		return;

	// Get the size of the image

	FLOAT mW = Tex->SizeX;
	FLOAT mH = Tex->SizeY;

	// Get the midpoints of the image

	FLOAT MidX = appFloor(mW/2);
	FLOAT MidY = appFloor(mH/2);

	// Grab info about the scaled image

	FLOAT SmallTileW = AWidth - mW;
	FLOAT SmallTileH = AHeight - mH;
	FLOAT fX, fY;		// Used to shrink

	// Draw the spans first

		// Top and Bottom

	if (mW<AWidth)
	{
		fX = MidX;

		if (mH>AHeight)
			fY = AHeight/2;
		else
			fY = MidY;

		DrawTile(Tex, CurX+fX,	CurY,					SmallTileW,		fY,		MidX,	0,			1,		fY,	DrawColor);
		DrawTile(Tex, CurX+fX,	CurY+AHeight-fY,		SmallTileW,		fY,		MidX,	mH-fY,		1,		fY,	DrawColor);
	}
	else
		fX = AWidth / 2;

		// Left and Right

	if (mH<AHeight)
	{

		fY = MidY;

		DrawTile(Tex, CurX,				CurY+fY,	fX,	SmallTileH,		0,		fY, fX,	1, DrawColor);
		DrawTile(Tex, CurX+AWidth-fX,	CurY+fY,	fX,	SmallTileH,		mW-fX,	fY, fX,	1, DrawColor);

	}
	else
		fY = AHeight / 2; 

		// Center

	if ( (mH<AHeight) && (mW<AWidth) )
		DrawTile(Tex, CurX+fX, CurY+fY, SmallTileW, SmallTileH, fX, fY, 1, 1, DrawColor);

	// Draw the 4 corners.

	DrawTile(Tex, CurX,				CurY,				fX, fY, 0,		0,		fX, fY, DrawColor);
	DrawTile(Tex, CurX+AWidth-fX,	CurY,				fX, fY,	mW-fX,	0,		fX, fY, DrawColor);
	DrawTile(Tex, CurX,				CurY+AHeight-fY,	fX, fY, 0,		mH-fY,	fX, fY, DrawColor);
	DrawTile(Tex, CurX+AWidth-fX,	CurY+AHeight-fY,	fX, fY,	mW-fX,	mH-fY,	fX, fY, DrawColor);

}
void UCanvas::execDrawTileScaled( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture2D,Tex);
	P_GET_FLOAT(NewXScale);
	P_GET_FLOAT(NewYScale);
	P_FINISH;

	DrawTileScaled(Tex,CurX,CurY,NewXScale,NewYScale);

}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileScaled );


void UCanvas::DrawTileBound(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height)
{
	if( Tex )
	{
		DrawTile( Tex, Left, Top, Width, Height, 0, 0, Width, Height, DrawColor );
	}
	else
	{
		debugf(TEXT("DrawTileBound: Missing texture"));
	}
}

void UCanvas::DrawTileScaleBound(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height)
{
	if( Tex )
	{
		FLOAT mW = Tex->SizeX;
		FLOAT mH = Tex->SizeY;
		DrawTile( Tex, Left, Top, Width, Height, 0, 0, mW, mH, DrawColor);
	}
	else
	{
		debugf(TEXT("DrawTileScaleBound: Missing Material"));
	}
}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileJustified );

void UCanvas::execDrawTileJustified( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture2D,Tex);
	P_GET_BYTE(Just);
	P_GET_FLOAT(AWidth);
	P_GET_FLOAT(AHeight);
	P_FINISH;

	DrawTileJustified(Tex, CurX, CurY, AWidth, AHeight, Just);
}


// Justification is: 0 = left/top, 1 = Center, 2 = bottom/right
void UCanvas::DrawTileJustified(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height, BYTE Justification)
{
	if( Tex )
	{
		FLOAT mW = Tex->SizeX;
		FLOAT mH = Tex->SizeY;

		if (mW <= 0.0 || mH <= 0.0)
			return;

		// Find scaling proportions
		FLOAT MatProps = mH/mW;
		FLOAT BndProps = Height/Width;

		if (MatProps == BndProps)
		{
			DrawTile( Tex, Left, Top, Width, Height, 0, 0, mW, mH, DrawColor);
		}
		else if (MatProps > BndProps)	// Stretch to fit Height
		{
			FLOAT NewWidth = Width * BndProps / MatProps;
			FLOAT NewLeft = Left;

			if (Justification == 1)			// Centered
			{
				NewLeft += ((Width - NewWidth) / 2.0);
			}
			else if (Justification == 2)	// RightBottom
			{
				NewLeft += Width - NewWidth;
			}
			DrawTile( Tex, NewLeft, Top, NewWidth, Height, 0, 0, mW, mH, DrawColor);
		}
		else							// Stretch to fit Width
		{
			FLOAT NewHeight = Height * MatProps / BndProps;
			FLOAT NewTop = Top;

			if (Justification == 1)			// Centered
			{
				NewTop += ((Height - NewHeight) / 2.0);
			}
			else if (Justification == 2)	// RightBottom
			{
				NewTop += Height - NewHeight;
			}
			DrawTile( Tex, Left, NewTop, Width, NewHeight, 0, 0, mW, mH, DrawColor);
		}
	}
	else
	{
		debugf(TEXT("DrawTileScaleBound: Missing texture"));
	}
}

void UCanvas::DrawTileScaled(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT NewXScale, FLOAT NewYScale)
{
	if( Tex )
	{
		FLOAT mW = Tex->SizeX;
		FLOAT mH = Tex->SizeY;
		DrawTile( Tex, Left, Top, mW*NewXScale, mH*NewYScale, 0, 0, mW, mH, DrawColor);
	}
	else
	{
		debugf(TEXT("DrawTileScaled: Missing texture"));
	}
}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTextJustified );

void UCanvas::execDrawTextJustified( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_BYTE(Just);
	P_GET_FLOAT(X1);
	P_GET_FLOAT(Y1);
	P_GET_FLOAT(X2);
	P_GET_FLOAT(Y2);
	P_FINISH;

	DrawTextJustified(Just,X1,Y1,X2,Y2, TEXT("%s"),*InText);

}


void VARARGS UCanvas::DrawTextJustified(BYTE Justification, FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, const TCHAR* Fmt, ... )
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	INT XL,YL;
	CurX = 0;
	CurY = 0;
	ClippedStrLen(  Font, 1.0f,1.0f, XL, YL, Text );

	// Adjust the Y so the Font is centered in the bounding box
	CurY = ((y2-y1) / 2) - (YL/2);

	if (Justification == 0)						// Left
		CurX = 0;
	else if (Justification == 1)				// Center
	{
		if( XL > x2-x1 ) // align left when there's no room
			CurX = 0;
		else
			CurX = ((x2-x1) / 2) - (XL/2);
	}
	else if (Justification == 2)				// Right
		CurX = (x2-x1) - XL;

	INT OldClipX = ClipX;
	INT OldClipY = ClipY;
	INT OldOrgX = OrgX;
	INT OldOrgY = OrgY;

	// Clip to box
	OrgX = x1;
	OrgY = y1;
	ClipX = x2-x1;
	ClipY = y2-y1;

	ClippedPrint(Font, 1.0f, 1.0f, false, Text);

	ClipX = OldClipX;
	ClipY = OldClipY;
	OrgX = OldOrgX;
	OrgY = OldOrgY;
}

void UCanvas::execProject( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Location);
	P_FINISH;

	FPlane V(0,0,0);

	if (SceneView!=NULL)
		V = SceneView->Project(Location);

	FVector resultVec(V);
	resultVec.X = (ClipX/2.f) + (resultVec.X*(ClipX/2.f));
	resultVec.Y *= -1.f;
	resultVec.Y = (ClipY/2.f) + (resultVec.Y*(ClipY/2.f));
	*(FVector*)Result =	resultVec;

}
IMPLEMENT_FUNCTION( UCanvas, -1, execProject);

void UCanvas::execDeProject( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(ScreenLoc);
	P_FINISH;

	FPlane v(0,0,0,0);
	FPlane p=ScreenLoc;

	if (SceneView!=NULL)
		v = SceneView->Deproject(p);

	*(FVector*)Result =	v;

}
IMPLEMENT_FUNCTION( UCanvas, -1, execDeProject);



IMPLEMENT_CLASS(UCanvas);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
