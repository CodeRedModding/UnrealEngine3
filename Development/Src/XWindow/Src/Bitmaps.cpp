
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	WxBitmap.
-----------------------------------------------------------------------------*/

WxBitmap::WxBitmap()
{
}

WxBitmap::WxBitmap( const wxImage& img, int depth )
	: wxBitmap( img, depth )
{
}

WxBitmap::WxBitmap(int width, int height, int depth)
	: wxBitmap( width, height, depth)
{
}

// InFilename - should be the base name of the bitmap ( "wxres\\New.bmp" would be passed in as "New" );

WxBitmap::WxBitmap( FString InFilename )
{
	Load( InFilename );
}

UBOOL WxBitmap::Load( FString InFilename )
{
	// FIXME : this is where we need to insert any OS specific loading code
	UBOOL bRet = LoadFile( *FString::Printf( TEXT("%swxres\\%s.bmp"), appBaseDir(), *InFilename ), wxBITMAP_TYPE_BMP );

	check( bRet );	// Loading bitmaps should never fail
	return bRet;
}

/**
 Loads the bitmap from a literal path rather than assuming it's in wxres
 */

UBOOL WxBitmap::LoadLiteral( FString InFilename )
{
	// FIXME : this is where we need to insert any OS specific loading code
	UBOOL bRet = LoadFile( *FString::Printf( TEXT("%s\\%s.bmp"), appBaseDir(), *InFilename ), wxBITMAP_TYPE_BMP );

	check( bRet );	// Loading bitmaps should never fail
	return bRet;
}

WxBitmap::~WxBitmap()
{
}

/*-----------------------------------------------------------------------------
	WxMaskedBitmap.
-----------------------------------------------------------------------------*/

WxMaskedBitmap::WxMaskedBitmap()
{
}

WxMaskedBitmap::WxMaskedBitmap( FString InFilename )
	: WxBitmap( InFilename )
{
}

WxMaskedBitmap::~WxMaskedBitmap()
{
}

UBOOL WxMaskedBitmap::Load( FString InFilename )
{
	UBOOL bRet = WxBitmap::Load( InFilename );

	SetMask( new wxMask( *this, wxColor(192,192,192) ) );

	return bRet;
}

/*-----------------------------------------------------------------------------
	WxAlphaBitmap.
-----------------------------------------------------------------------------*/

WxAlphaBitmap::WxAlphaBitmap()
{
}

WxAlphaBitmap::WxAlphaBitmap( FString InFilename, UBOOL InBorderBackground )
	: WxBitmap( LoadAlpha( InFilename, InBorderBackground ) )
{
}

WxAlphaBitmap::~WxAlphaBitmap()
{
}

wxImage WxAlphaBitmap::LoadAlpha( FString InFilename, UBOOL InBorderBackground )
{
	// Import the TGA file into a temporary texture

	InFilename = *FString::Printf( TEXT("wxres\\%s.tga"), *InFilename );
	UTexture2D* Tex = ImportObject<UTexture2D>( GEngine->GetLevel(), GEngine, TEXT("TempAlphaBitmap_DeleteMe"), RF_Public|RF_Standalone, *InFilename, NULL, NULL, TEXT("CREATEMIPMAPS=0 NOCOMPRESSION=1") );
	if( !Tex )
		check(0);		// Not found!

	// Get the background color we need to blend with

	wxColour clr = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);

	if( InBorderBackground )
		clr.Set( 140,190,190 );

	// Create a wxImage of the alpha'd image data combined with the background color

	wxImage img( Tex->SizeX, Tex->SizeY );

	INT Toggle = 0;
	for( UINT x = 0 ; x < Tex->SizeX ; ++x )
	{
		for( UINT y = 0 ; y < Tex->SizeY ; ++y )
		{
			INT idx = ((y * Tex->SizeX) + x) * 4;
			FLOAT B = Tex->Mips(0).Data(idx);
			FLOAT G = Tex->Mips(0).Data(idx+1);
			FLOAT R = Tex->Mips(0).Data(idx+2);
			FLOAT SrcA = Tex->Mips(0).Data(idx+3) / 255.f;

			if( InBorderBackground )
				if( x == 0 || x == 1 || x == Tex->SizeX-1 || x == Tex->SizeX-2 || y == 0 || y == 1 || y == Tex->SizeY-1 || y == Tex->SizeY-2 )
				{
					R = 170;
					G = 220;
					B = 220;
					SrcA = 1.f;
				}

			FLOAT DstA = 1.f - SrcA;

			if( InBorderBackground )
			{
				Toggle++;
				if( Toggle%3 )
					clr.Set( 140,190,190 );
				else
					clr = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
			}

			BYTE NewR = ((R*SrcA) + (clr.Red()*DstA));
			BYTE NewG = ((G*SrcA) + (clr.Green()*DstA));
			BYTE NewB = ((B*SrcA) + (clr.Blue()*DstA));

			img.SetRGB( x, y, NewR, NewG, NewB );
		}
	}

	// Clean up

	delete Tex;
	
	return img;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
