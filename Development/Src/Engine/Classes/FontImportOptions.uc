/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FontImportOptions extends Object
	hidecategories( Object )
	transient
	native;


/** Font character set type for importing TrueType fonts */
enum EFontImportCharacterSet
{
	FontICS_Default,
	FontICS_Ansi,
	FontICS_Symbol
};


/** Font import options */
struct native FontImportOptionsData
{
	var() string FontName;  // Name of the typeface for the font to import
	var() float Height;  // Height of font (point size)
	var() bool bEnableAntialiasing;  // Whether the font should be antialiased or not.  Usually you should leave this enabled.
	var() bool bEnableBold;  // Whether the font should be generated in bold or not
	var() bool bEnableItalic;  // Whether the font should be generated in italics or not
	var() bool bEnableUnderline;  // Whether the font should be generated with an underline or not
	var() bool bAlphaOnly;	// if TRUE then forces PF_G8 and only maintains Alpha value and discards color
	var() EFontImportCharacterSet CharacterSet;  // Character set for this font

	var() string Chars;  // Explicit list of characters to include in the font
	var() string UnicodeRange;  // Range of Unicode character values to include in the font.  You can specify ranges using hyphens and/or commas (e.g. '400-900')
	var() string CharsFilePath;  // Path on disk to a folder where files that contain a list of characters to include in the font
	var() string CharsFileWildcard;  // File mask wildcard that specifies which files within the CharsFilePath to scan for characters in include in the font
	var() bool bCreatePrintableOnly;  // Skips generation of glyphs for any characters that are not considered 'printable'
	var() bool bIncludeASCIIRange;	// When specifying a range of characters and this is enabled, forces ASCII characters (0 thru 255) to be included as well

	var() LinearColor ForegroundColor;  // Color of the foreground font pixels.  Usually you should leave this white and instead use the UI Styles editor to change the color of the font on the fly
	var() bool bEnableDropShadow;  // Enables a very simple, 1-pixel, black colored drop shadow for the generated font

	var() int TexturePageWidth;  // Horizontal size of each texture page for this font in pixels
	var() int TexturePageMaxHeight;  // The maximum vertical size of a texture page for this font in pixels.  The actual height of a texture page may be less than this if the font can fit within a smaller sized texture page.
	var() int XPadding;  // Horizontal padding between each font character on the texture page in pixels
	var() int YPadding;  // Vertical padding between each font character on the texture page in pixels

	var() int ExtendBoxTop;  // How much to extend the top of the UV coordinate rectangle for each character in pixels
	var() int ExtendBoxBottom;  // How much to extend the bottom of the UV coordinate rectangle for each character in pixels
	var() int ExtendBoxRight;  // How much to extend the right of the UV coordinate rectangle for each character in pixels
	var() int ExtendBoxLeft;  // How much to extend the left of the UV coordinate rectangle for each character in pixels

	var() bool bEnableLegacyMode;  // Enables legacy font import mode.  This results in lower quality antialiasing and larger glyph bounds, but may be useful when debugging problems

	var() int Kerning;  // The initial horizontal spacing adjustment between rendered characters.  This setting will be copied directly into the generated Font object's properties.

	/** If TRUE then the alpha channel of the font textures will store a distance field instead of a color mask */
	var() bool bUseDistanceFieldAlpha;
	/** 
	* Scale factor determines how big to scale the font bitmap during import when generating distance field values 
	* Note that higher values give better quality but importing will take much longer.
	*/
	var() int DistanceFieldScaleFactor<EditCondition=bUseDistanceFieldAlpha>;
	/** Shrinks or expands the scan radius used to determine the silhouette of the font edges. */
	var() float DistanceFieldScanRadiusScale<ClampMin=0.0 | ClampMax=4.0>;

	structdefaultproperties
	{
		FontName = "Arial";
		Height = 16.0;
		bEnableAntialiasing = true;
		CharacterSet = FontICS_Default;

		bIncludeASCIIRange = true;

		ForegroundColor = ( R=1.0, G=1.0, B=1.0, A=1.0 );

		TexturePageWidth = 256;
		TexturePageMaxHeight = 256;
		XPadding = 1;
		YPadding = 1;

		DistanceFieldScaleFactor = 16;
		DistanceFieldScanRadiusScale = 1.0;
	}
};


/** The actual data for this object.  We wrap it in a struct so that we can copy it around between objects. */
var() FontImportOptionsData Data <FullyExpand=true>;
