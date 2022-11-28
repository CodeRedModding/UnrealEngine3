/*=============================================================================
	TTFontImport.cpp: True-type Font Importing
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	UTrueTypeFontFactory.
------------------------------------------------------------------------------*/

INT FromHex( TCHAR Ch )
{
	if( Ch>='0' && Ch<='9' )
		return Ch-'0';
	else if( Ch>='a' && Ch<='f' )
		return 10+Ch-'a';
	else if( Ch>='A' && Ch<='F' )
		return 10+Ch-'A';
	appErrorf(TEXT("Expecting digit, got character %i"),Ch);
	return 0;
}
void UTrueTypeFontFactory::StaticConstructor()
{
	SupportedClass		= UFont::StaticClass();
	bCreateNew			= 1;
	AutoPriority        = -1;
	Description			= TEXT("Font Imported From TrueType");
	FontName			= TEXT("MS Sans Serif");
	Height				= 16.0;
	USize				= 256;
	VSize				= 256;
	XPad				= 1;
	YPad				= 1;
	Gamma				= 0.7f;
	Count				= 256;
	AntiAlias			= 0;
	Chars				= TEXT("");
	Wildcard			= TEXT("");
	Path				= TEXT("");
	Style	            = FW_NORMAL;
	Italic	            = 0;
	Underline           = 0;
	new(GetClass(),TEXT("Style"),             RF_Public)UIntProperty  (CPP_PROPERTY(Style            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Italic"),            RF_Public)UBoolProperty (CPP_PROPERTY(Italic           ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Underline"),         RF_Public)UBoolProperty (CPP_PROPERTY(Underline        ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("FontName"),          RF_Public)UStrProperty  (CPP_PROPERTY(FontName         ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Height"),            RF_Public)UFloatProperty(CPP_PROPERTY(Height           ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("USize"),             RF_Public)UIntProperty  (CPP_PROPERTY(USize            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("VSize"),             RF_Public)UIntProperty  (CPP_PROPERTY(VSize            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("XPad"),              RF_Public)UIntProperty  (CPP_PROPERTY(XPad             ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("YPad"),              RF_Public)UIntProperty  (CPP_PROPERTY(YPad             ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Count"),             RF_Public)UIntProperty  (CPP_PROPERTY(Count            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Gamma"),             RF_Public)UFloatProperty(CPP_PROPERTY(Gamma            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Chars"),             RF_Public)UStrProperty  (CPP_PROPERTY(Chars            ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("AntiAlias"),         RF_Public)UBoolProperty (CPP_PROPERTY(AntiAlias        ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("UnicodeRange"),      RF_Public)UStrProperty  (CPP_PROPERTY(UnicodeRange     ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Path"),              RF_Public)UStrProperty  (CPP_PROPERTY(Path             ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Wildcard"),          RF_Public)UStrProperty  (CPP_PROPERTY(Wildcard         ), TEXT(""), CPF_Edit );
    Kerning = 0;
	new(GetClass(),TEXT("Kerning"),           RF_Public)UIntProperty  (CPP_PROPERTY(Kerning          ), TEXT(""), CPF_Edit );
    DropShadowX = 0;
    DropShadowY = 0;
	new(GetClass(),TEXT("DropShadowX"),       RF_Public)UIntProperty  (CPP_PROPERTY(DropShadowX      ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("DropShadowY"),       RF_Public)UIntProperty  (CPP_PROPERTY(DropShadowY      ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ExtendBoxTop"),       RF_Public)UIntProperty  (CPP_PROPERTY(ExtendBoxTop     ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ExtendBoxBottom"),    RF_Public)UIntProperty  (CPP_PROPERTY(ExtendBoxBottom  ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ExtendBoxRight"),     RF_Public)UIntProperty  (CPP_PROPERTY(ExtendBoxRight   ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ExtendBoxLeft"),      RF_Public)UIntProperty  (CPP_PROPERTY(ExtendBoxLeft    ), TEXT(""), CPF_Edit );

	new(GetClass()->HideCategories) FName(TEXT("Object"));

}
UTrueTypeFontFactory::UTrueTypeFontFactory()
{
}
UObject* UTrueTypeFontFactory::FactoryCreateNew
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	FFeedbackContext*	Warn
)
{
	check(Class==UFont::StaticClass());

	// Create font and its texture.
	UFont* Font = new( InParent, Name, Flags )UFont;

	Font->Kerning = Kerning;
	Font->IsRemapped = 0;
	TMap<TCHAR,TCHAR> InverseMap;

	UBOOL UseFiles = Wildcard!=TEXT("") && Path!=TEXT("");
	UBOOL UseRange = UnicodeRange != TEXT("");

	if( UseFiles || UseRange )
	{
		Font->IsRemapped = 1;
		// Map (ASCII)
		for( TCHAR c=0;c<256;c++ )
		{
			Font->CharRemap.Set( c, c );
			InverseMap.Set( c, c );
		}

		TArray<BYTE> Chars;
		Chars.AddZeroed(65536);

		if( UseFiles )
		{
			// find all characters in specified path/wildcard
			TArray<FString> Files;
			GFileManager->FindFiles( Files, *(Path*Wildcard),1,0 );
			for( TArray<FString>::TIterator it(Files); it; ++it )
			{
				FString S;
				verify(appLoadFileToString(S,*(Path * *it)));
				for( INT i=0; i<S.Len(); i++ )
					Chars((*S)[i]) = 1;
			}
			warnf(TEXT("Checked %d files"), Files.Num() );
		}

		if( UseRange )
		{
			Warn->Logf(TEXT("UnicodeRange <%s>:"),*UnicodeRange);
			INT From = 0;
			INT To = 0;
			UBOOL HadDash = 0;
			for( const TCHAR* C=*UnicodeRange; *C; C++ )
			{
				if( (*C>='A' && *C<='F') || (*C>='a' && *C<='f') || (*C>='0' && *C<='9') )
				{
					if( HadDash )
						To = 16*To + FromHex(*C);
					else
						From = 16*From + FromHex(*C);
				}
				else
				if( *C=='-' )
					HadDash = 1;
				else
				if( *C==',' )
				{
					warnf(TEXT("Adding unicode character range %x-%x (%d-%d)"),From,To,From,To);
					for( INT i=From;i<=To&&i>=0&&i<65536;i++ )
						Chars(i) = 1;
					HadDash=0;
					From=0;
					To=0;
				}
			}
			warnf(TEXT("Adding unicode character range %x-%x (%d-%d)"),From,To,From,To);
			for( INT i=From;i<=To&&i>=0&&i<65536;i++ )
				Chars(i) = 1;

		}

		INT j=256;
		INT Min=65536, Max=0;
		for( INT i=256; i<65536; i++ )
			if( Chars(i) )
			{
				if( i < Min )
					Min = i;
				if( i > Max )
					Max = i;
				Font->CharRemap.Set( i, j );
				InverseMap.Set( j++, i );
			}

		// Add space for characters.
		Font->Characters.AddZeroed(j);
		warnf(TEXT("Importing %d characters (unicode range %04x-%04x)"), j, Min, Max);
	}
	else
		Font->Characters.AddZeroed(256);
    
    // If all upper case chars have lower case char counterparts no mapping is required.   
    if( !Font->IsRemapped )
    {
        bool NeedToRemap = false;
        
        for( const TCHAR* p = *Chars; *p; p++ )
        {
            TCHAR c;
            
            if( !appIsAlpha( *p ) )
                continue;
            
            if( appIsUpper( *p ) )
                c = appToLower( *p );
            else
                c = appToUpper( *p );

            if( appStrchr(*Chars, c) )
                continue;
            
            NeedToRemap = true;
            break;
        }
        
        if( NeedToRemap )
        {
            Font->IsRemapped = 1;

            for( const TCHAR* p = *Chars; *p; p++ )
		    {
                TCHAR c;

                if( !appIsAlpha( *p ) )
                {
			        Font->CharRemap.Set( *p, *p );
			        InverseMap.Set( *p, *p );
                    continue;
                }
                
                if( appIsUpper( *p ) )
                    c = appToLower( *p );
                else
                    c = appToUpper( *p );

			    Font->CharRemap.Set( *p, *p );
			    InverseMap.Set( *p, *p );

                if( !appStrchr(*Chars, c) )
			        Font->CharRemap.Set( c, *p );
		    }
        }
    }

	// Create the Windows font.
	HDC tempDC = CreateCompatibleDC( NULL );
	INT nHeight = -appRound(Height * (FLOAT)GetDeviceCaps(tempDC, LOGPIXELSY) / 72.0);
	DeleteDC( tempDC );

	HFONT F = TCHAR_CALL_OS(
		CreateFont(nHeight, 0, 0, 0, Style, Italic, Underline, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, AntiAlias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, VARIABLE_PITCH, *FontName),
		CreateFontA(nHeight, 0, 0, 0, Style, Italic, Underline, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, AntiAlias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, VARIABLE_PITCH, TCHAR_TO_ANSI(*FontName)) );
	if( !F )
	{
		Warn->Logf( NAME_Error, TEXT("CreateFont failed: %s"), appGetSystemErrorMessage() );
		return NULL;
	}

	// Create DC
	HDC dc = CreateCompatibleDC( NULL );
	if( !dc )
	{
		Warn->Logf( NAME_Error, TEXT("CreateDC failed: %s"), appGetSystemErrorMessage() );
		return NULL;
	}

	// Create bitmap
	HBITMAP B;
	if( AntiAlias )
	{
		BITMAPINFO* pBI = (BITMAPINFO*)appMalloc(sizeof(BITMAPINFO));

		pBI->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
		pBI->bmiHeader.biWidth         = USize;
		pBI->bmiHeader.biHeight        = VSize;
		pBI->bmiHeader.biPlanes        = 1;      //  Must be 1
		pBI->bmiHeader.biBitCount      = 24;
		pBI->bmiHeader.biCompression   = BI_RGB; 
		pBI->bmiHeader.biSizeImage     = 0;      
		pBI->bmiHeader.biXPelsPerMeter = 0;      
		pBI->bmiHeader.biYPelsPerMeter = 0;      
		pBI->bmiHeader.biClrUsed       = 0;      
		pBI->bmiHeader.biClrImportant  = 0;      

		void* pvBmp;
		B = CreateDIBSection((HDC)NULL, 
								pBI,
								DIB_RGB_COLORS,
								&pvBmp,
								NULL,
								0);  
		appFree( pBI );
	}
	else
		B = CreateBitmap( USize, VSize, 1, 1, NULL);

	if( !B )
	{
		Warn->Logf( NAME_Error, TEXT("CreateBitmap failed: %s"), appGetSystemErrorMessage() );
		return NULL;
	}

	SelectObject( dc, F );
	SelectObject( dc, B );
	SetTextColor( dc, 0x00ffffff );
	SetBkColor( dc, 0x00000000 );

	// clear the bitmap
	HBRUSH Black = CreateSolidBrush(0x00000000);
	RECT   r     = {0, 0, USize, VSize};
	FillRect( dc, &r, Black );

	INT X=0, Y=0, RowHeight=0;
	INT CurrentTexture=0;
	for( INT Ch=0; Ch<Font->Characters.Num(); Ch++ )
	{
		// Skip if this ASCII character if it isn't in the list of characters to import.
		if( Ch<256 && Chars!=TEXT("") && (!Ch || !appStrchr(*Chars, Ch)) )
			continue;

		// Get text size.
		SIZE Size;
		TCHAR Tmp[5];
		if( Font->IsRemapped )
		{
			TCHAR *p = InverseMap.Find(Ch);
			Tmp[0] = p ? *p : 32;
		}
		else
			Tmp[0] = Ch;
		Tmp[1] = 0;
		GetTextExtentPoint32( dc, Tmp, 1, &Size );

		// dropshadow
        Size.cx += DropShadowX;
        Size.cy += DropShadowY;

		// If it doesn't fit right here, advance to next line.
		if( Size.cx + X + 2 > USize)
		{
			X         = 0;
			Y         = Y + RowHeight + YPad;
			RowHeight = 0;
		}
		INT OldRowHeight = RowHeight;
		if( Size.cy > RowHeight )
			RowHeight = Size.cy;

		// new page
		if( Y+RowHeight>VSize )
		{
			Font->Textures.AddItem( CreateTextureFromDC( Font, (DWORD)dc, Y+OldRowHeight, CurrentTexture ) );
			CurrentTexture++;

			// blank out DC
			HBRUSH Black = CreateSolidBrush(0x00000000);
			RECT   r     = {0, 0, USize, VSize};
			FillRect( dc, &r, Black );

			X = 0;
			Y = 0;
			RowHeight = 0;
		}

		// Set font character information.
		Font->Characters(Ch).StartU = Clamp<INT>(X-ExtendBoxLeft, 0, USize-1);
		Font->Characters(Ch).StartV = Clamp<INT>(Y+1-ExtendBoxTop, 0, VSize-1);
		Font->Characters(Ch).USize  = Clamp<INT>(Size.cx+ExtendBoxLeft+ExtendBoxRight, 0, USize-Font->Characters(Ch).StartU);
		Font->Characters(Ch).VSize  = Clamp<INT>(Size.cy+ExtendBoxTop+ExtendBoxBottom, 0, VSize-Font->Characters(Ch).StartV);
		Font->Characters(Ch).TextureIndex = CurrentTexture;		

		// Draw character into font and advance.
		TextOut( dc, X, Y, Tmp, 1 );
		X = X + Size.cx + XPad;
	}
	// save final page
	Font->Textures.AddItem( CreateTextureFromDC( Font, (DWORD)dc, Y+RowHeight, CurrentTexture ) );
	warnf(TEXT("Font used %d textures"), CurrentTexture+1);

	DeleteDC( dc );
	DeleteObject( B );

	// Success.
	return Font;

}

UTexture2D* UTrueTypeFontFactory::CreateTextureFromDC( UFont* Font, DWORD Indc, INT Height, INT TextureNum )
{
	HDC dc = (HDC)Indc;

	// Create texture for page.
	UTexture2D* Texture = new(Font)UTexture2D;
	Texture->Init( USize, 1<<appCeilLogTwo(Height), PF_A8R8G8B8 );
	Texture->PostLoad();

    FString TextureString = FString::Printf( TEXT("%s_Page"), Font->GetName() );
	if( TextureNum < 26 )
		TextureString = TextureString + FString::Printf(TEXT("%c"), 'A'+TextureNum);
	else
		TextureString = TextureString + FString::Printf(TEXT("%c%c"), 'A'+TextureNum/26, 'A'+TextureNum%26 );

    if( !StaticFindObject( NULL, Texture->GetOuter(), *TextureString ) )
        Texture->Rename( *TextureString );
    else
		warnf( TEXT("A texture named %s already exists!"), *TextureString );

	// Copy bitmap data to texture page.

	if( !AntiAlias )
    {
		for( INT i=0; i<(INT)Texture->SizeX; i++ )
		{
			for( INT j=0; j<(INT)Texture->SizeY; j++ )
			{
				INT CharAlpha = GetRValue( GetPixel( dc, i, j ) );
				INT DropShadowAlpha;

				if( (i >= DropShadowX) && (j >= DropShadowY) )
					DropShadowAlpha = GetRValue( GetPixel( dc, i - DropShadowX, j - DropShadowY ) );
				else
					DropShadowAlpha = 0;

				if( CharAlpha )
				{
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 0) = 0xFF;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 1) = 0xFF;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 2) = 0xFF;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 3) = 0xFF;
				}
				else if( DropShadowAlpha )
				{
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 0) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 1) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 2) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 3) = 0xFF;
				}
				else
				{
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 0) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 1) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 2) = 0x00;
					Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 3) = 0x00;
                }
			}
		}
	}
	else
	{
		for( INT i=0; i<(INT)Texture->SizeX; i++ )
		{
			for( INT j=0; j<(INT)Texture->SizeY; j++ )
			{
                INT CharAlpha = GetRValue( GetPixel( dc, i, j ) );
                float fCharAlpha = float( CharAlpha ) / 255.0f;

                INT DropShadowAlpha;

                if( (i >= DropShadowX) && (j >= DropShadowY) )
                    DropShadowAlpha = GetRValue( GetPixel( dc, i - DropShadowX, j - DropShadowY ) );
                else
                    DropShadowAlpha = 0;

                // Dest = (White * fCharAlpha) + (1.0 - fCharAlpha) * (Black * fDropShadowAlpha)                        // Dest = (White * CharAlpha) + (1.0 - CharAlpha) * (Black * DropShadowAlpha)
                // --------------------------------------------------------------------------
                // Dest.[RGB] = (255 * fCharAlpha)
                // Dest.A = (255 * fCharAlpha) + (1.0 - fCharAlpha) * (255 * fDropShadowAlpha)

				Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 0) = CharAlpha;
				Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 0) = CharAlpha;
				Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 1) = CharAlpha;
				Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 2) = CharAlpha;

				Texture->Mips(0).Data(4 * (i + j * Texture->SizeX) + 3)
                    = (BYTE)( CharAlpha + ( 1.0f - fCharAlpha ) * ( (float)DropShadowAlpha ) );
			}
		}
	}

	// PNG Compress.
	FPNGHelper PNG;
	PNG.InitRaw( &Texture->Mips(0).Data(0), Texture->Mips(0).Data.Num(), Texture->SizeX, Texture->SizeY );
	TArray<BYTE> CompressedData = PNG.GetCompressedData();
	check( CompressedData.Num() );

	// Store source art.
	Texture->SourceArt.Empty( CompressedData.Num() );
	Texture->SourceArt.Add( CompressedData.Num() );
	appMemcpy( &Texture->SourceArt(0), &CompressedData(0), CompressedData.Num() );

	// Detach lazy loader.
	Texture->SourceArt.Detach();

	Texture->CompressionNoMipmaps = 1;
	Texture->Compress();

	return Texture;
}


IMPLEMENT_CLASS(UTrueTypeFontFactory);

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/

