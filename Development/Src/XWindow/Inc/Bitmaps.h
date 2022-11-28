/*=============================================================================
	Bitmaps.h: Classes based on standard bitmaps

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

/*-----------------------------------------------------------------------------
	WxBitmap.

	A bitmap that has OS specific loading hooks.
-----------------------------------------------------------------------------*/

class WxBitmap : public wxBitmap
{
public:
	WxBitmap();
	WxBitmap(const wxImage& img, int depth = -1);
	WxBitmap(int width, int height, int depth = -1);
	WxBitmap( FString InFilename );
	~WxBitmap();

	virtual UBOOL Load( FString InFilename );
	virtual UBOOL LoadLiteral( FString InFilename );
};

/*-----------------------------------------------------------------------------
	WxMaskedBitmap.

	A bitmap that automatically generates a wxMask for itself.
-----------------------------------------------------------------------------*/

class WxMaskedBitmap : public WxBitmap
{
public:
	WxMaskedBitmap();
	WxMaskedBitmap( FString InFilename );
	~WxMaskedBitmap();

	virtual UBOOL Load( FString InFilename );
};

/*-----------------------------------------------------------------------------
	WxAlphaBitmap.

	A bitmap that automatically generates a wxBitmap for itself based on
	the alpha channel of the source TGA file.
-----------------------------------------------------------------------------*/

class WxAlphaBitmap : public WxBitmap
{
public:
	WxAlphaBitmap();
	WxAlphaBitmap( FString InFilename, UBOOL InBorderBackground );
	~WxAlphaBitmap();

private:

	wxImage LoadAlpha( FString InFilename, UBOOL InBorderBackground );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
