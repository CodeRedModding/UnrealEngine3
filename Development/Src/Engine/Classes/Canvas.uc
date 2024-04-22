//=============================================================================
// Canvas: A drawing canvas.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Canvas extends Object
	native
	transient;

/**
 * Holds texture information with UV coordinates as well.
 */
struct native CanvasIcon
{
	/** Source texture */
	var() Texture Texture;
	/** UV coords */
	var() float U, V, UL, VL;
};


enum ECanvasBlendMode
{
	BLEND_CANVAS_Opaque,
	BLEND_CANVAS_Masked,
	BLEND_CANVAS_Translucent,
	BLEND_CANVAS_Additive,
	BLEND_CANVAS_Modulate,
	BLEND_CANVAS_ModulateAndAdd,
	BLEND_CANVAS_SoftMasked,
	BLEND_CANVAS_AlphaComposite,
	BLEND_CANVAS_DitheredTranslucent,
	BLEND_CANVAS_AlphaOnly,
};

/** info for glow when using depth field rendering */
struct native DepthFieldGlowInfo
{
	/** whether to turn on the outline glow (depth field fonts only) */
	var bool bEnableGlow;
	/** base color to use for the glow */
	var LinearColor GlowColor;
	/** if bEnableGlow, outline glow outer radius (0 to 1, 0.5 is edge of character silhouette)
	 * glow influence will be 0 at GlowOuterRadius.X and 1 at GlowOuterRadius.Y
	*/
	var vector2d GlowOuterRadius;
	/** if bEnableGlow, outline glow inner radius (0 to 1, 0.5 is edge of character silhouette)
	 * glow influence will be 1 at GlowInnerRadius.X and 0 at GlowInnerRadius.Y
	 */
	var vector2d GlowInnerRadius;

	structcpptext
	{
		FDepthFieldGlowInfo()
		{}
		FDepthFieldGlowInfo(EEventParm)
		{
			appMemzero(this, sizeof(FDepthFieldGlowInfo));
		}
		UBOOL operator==(const FDepthFieldGlowInfo& Other) const
		{
			if (Other.bEnableGlow != bEnableGlow)
			{
				return false;
			}
			else if (!bEnableGlow)
			{
				// if the glow is disabled on both, the other values don't matter
				return true;
			}
			else
			{
				return (Other.GlowColor == GlowColor && Other.GlowOuterRadius == GlowOuterRadius && Other.GlowInnerRadius == GlowInnerRadius);
			}
		}
		UBOOL operator!=(const FDepthFieldGlowInfo& Other) const
		{
			return !(*this == Other);
		}
	}
};


struct native MobileDistanceFieldParams
{
	/** Gamma value for distance field shader */
	var float Gamma;
	/** Alpha value to cull out*/
	var float AlphaRefVal;
	/** Width */
	var float SmoothWidth;
	/** Whether to shadow the font*/
	var BOOL EnableShadow;
	/** Which screen-space direction the shadows are in*/
	var vector2d ShadowDirection;
	/** Color of the font shadow*/
	var LinearColor ShadowColor;
	/** Shadow width*/
	var float ShadowSmoothWidth;
	/** Glow information structure*/
	var native DepthFieldGlowInfo GlowInfo;
	/** Blend mode*/
	var INT BlendMode;

	structcpptext
	{
		/** Constructors */
		FMobileDistanceFieldParams(
			FLOAT InGamma,
			FLOAT InAlphaRefVal,
			FLOAT InSmoothWidth,
			UBOOL InEnableShadow,
			FVector2D& InShadowDirection,
			FLinearColor& InShadowColor,
			FLOAT InShadowSmoothWidth,
			const FDepthFieldGlowInfo& InGlowInfo,
			INT InBlendMode
			)
			: Gamma(InGamma)
			, AlphaRefVal(InAlphaRefVal)
			, SmoothWidth(InSmoothWidth)
			, EnableShadow(InEnableShadow)
			, ShadowDirection(InShadowDirection)
			, ShadowColor(InShadowColor)
			, ShadowSmoothWidth(InShadowSmoothWidth)
			, GlowInfo(InGlowInfo)
			, BlendMode(InBlendMode)
		{
		}
	}
};

/** information used in font rendering */
struct native FontRenderInfo
{
	/** whether to clip text */
	var bool bClipText;
	/** whether to turn on shadowing */
	var bool bEnableShadow;
	/** depth field glow parameters (only usable if font was imported with a depth field) */
	var DepthFieldGlowInfo GlowInfo;
};

/** Simple 2d triangle with UVs */
struct native CanvasUVTri
{
	/** Position of first vertex */
	var()   vector2d    V0_Pos;
	/** UV of first vertex */
	var()   vector2d    V0_UV;

	/** Position of second vertex */
	var()   vector2d    V1_Pos;
	/** UV of second vertex */
	var()   vector2d    V1_UV;

	/** Position of third vertex */
	var()   vector2d    V2_Pos;
	/** UV of third vertex */
	var()   vector2d    V2_UV;
};

// Modifiable properties.
var font    Font;            // Font for DrawText.
var float   OrgX, OrgY;      // Origin for drawing.
var float   ClipX, ClipY;    // Bottom right clipping region.
var const float   CurX, CurY, CurZ;// Current position for drawing. Always use SetPos to set these
var float   CurYL;           // Largest Y size since DrawText.
var color   DrawColor;       // Color for drawing.
var bool    bCenter;         // Whether to center the text.
var bool    bNoSmooth;       // Don't bilinear filter.
var const int SizeX, SizeY;  // Zero-based actual dimensions.

// Internal.
var native const pointer Canvas{FCanvas};
var native const pointer SceneView{FSceneView};

var Plane ColorModulate;
var Texture2D DefaultTexture;


/**
 * General purpose data structure for grouping all parameters needed when sizing or wrapping a string
 */
struct native transient TextSizingParameters
{
	/** a pixel value representing the horizontal screen location to begin rendering the string */
	var		float				DrawX;

	/** a pixel value representing the vertical screen location to begin rendering the string */
	var		float				DrawY;

	/** a pixel value representing the width of the area available for rendering the string */
	var		float				DrawXL;

	/** a pixel value representing the height of the area available for rendering the string */
	var		float				DrawYL;

	/**
	 * A value between 0.0 and 1.0, which represents how much the width/height should be scaled,
	 * where 1.0 represents 100% scaling.
	 */
	var		Vector2D			Scaling;

	/** the font to use for sizing/wrapping the string */
	var		Font				DrawFont;

	/** Horizontal spacing adjustment between characters and vertical spacing adjustment between wrapped lines */
	var		Vector2D			SpacingAdjust;

	/** the current height of the viewport; needed to support multifont */
	var		float				ViewportHeight;


	structcpptext
	{
		FTextSizingParameters( FLOAT inDrawX, FLOAT inDrawY, FLOAT inDrawXL, FLOAT inDrawYL, UFont* inFont=NULL, FLOAT InViewportHeight=0.f )
		: DrawX(inDrawX), DrawY(inDrawY), DrawXL(inDrawXL), DrawYL(inDrawYL)
		, Scaling(1.f,1.f), DrawFont(inFont)
		, SpacingAdjust( 0.0f, 0.0f ), ViewportHeight(InViewportHeight)
		{
		}

		FTextSizingParameters( UFont* inFont, FLOAT ScaleX, FLOAT ScaleY, FLOAT InViewportHeight=0.f )
		: DrawX(0.f), DrawY(0.f), DrawXL(0.f), DrawYL(0.f)
		, Scaling(ScaleX,ScaleY), DrawFont(inFont)
		, SpacingAdjust( 0.0f, 0.0f ), ViewportHeight(InViewportHeight)
		{
		}
	}
};


/**
 * Used by UUIString::WrapString to track information about each line that is generated as the result of wrapping.
 */
struct native transient WrappedStringElement
{
	/** the string associated with this line */
	var	string		Value;

	/** the size (in pixels) that it will take to render this string */
	var Vector2D	LineExtent;

	structcpptext
	{
		/** Constructor */
		FWrappedStringElement( const TCHAR* InValue, FLOAT Width, FLOAT Height )
		: Value(InValue), LineExtent(Width,Height)
		{}
	}
};

cpptext
{
	// UCanvas interface.
	void Init();
	void Update();

	void DrawTile(UTexture* Tex, FLOAT X, FLOAT Y, FLOAT Z, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, const FLinearColor& Color,ECanvasBlendMode BlendMode);
	void DrawTile(UTexture* Tex, FLOAT X, FLOAT Y, FLOAT Z, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, const FLinearColor& Color,EBlendMode BlendMode=BLEND_Translucent);
	void DrawMaterialTile(UMaterialInterface* Tex, FLOAT X, FLOAT Y, FLOAT Z, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL);
	static void ClippedStrLen(UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Text);
	void VARARGS WrappedStrLenf(UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ...);
	INT WrappedPrint(UBOOL Draw, INT& XL, INT& YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text, const FFontRenderInfo& RenderInfo = FFontRenderInfo(EC_EventParm));
	void DrawText(const FString& Text);
	void DrawTileStretched(UTexture* Tex, FLOAT Left, FLOAT Top, FLOAT Depth, FLOAT AWidth, FLOAT AHeight, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FLinearColor DrawColor,UBOOL bStretchHorizontally=1,UBOOL bStretchVertically=1,FLOAT ScalingFactor=1.0);
	void DrawTimer(UTexture* Tex, FLOAT StartTime, FLOAT TotalTime, FLOAT X, FLOAT Y, FLOAT Z, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, const FLinearColor& Color,EBlendMode BlendMode=BLEND_Translucent);

	/**
	 * Calculates the size of the specified string.
	 *
	 * @param	Parameters	Used for various purposes
	 *							DrawXL:		[out] will be set to the width of the string
	 *							DrawYL:		[out] will be set to the height of the string
	 *							DrawFont:	[in] specifies the font to use for retrieving the size of the characters in the string
	 *							Scale:		[out] specifies the amount of scaling to apply to the string
	 * @param	pText		the string to calculate the size for
	 * @param	EOL			a pointer to a single character that is used as the end-of-line marker in this string
	 * @param	bStripTrailingCharSpace
	 *						whether the inter-character spacing following the last character should be included in the calculated width of the result string
	 */
	static void CanvasStringSize( FTextSizingParameters& Parameters, const TCHAR* pText, const TCHAR* EOL=NULL, UBOOL bStripTrailingCharSpace=TRUE );


	/**
	 * Parses a single string into an array of strings that will fit inside the specified bounding region.
	 *
	 * @param	Parameters		Used for various purposes:
	 *							DrawX:		[in] specifies the pixel location of the start of the horizontal bounding region that should be used for wrapping.
	 *							DrawY:		[in] specifies the Y origin of the bounding region.  This should normally be set to 0, as this will be
	 *										     used as the base value for DrawYL.
	 *										[out] Will be set to the Y position (+YL) of the last line, i.e. the total height of all wrapped lines relative to the start of the bounding region
	 *							DrawXL:		[in] specifies the pixel location of the end of the horizontal bounding region that should be used for wrapping
	 *							DrawYL:		[in] specifies the height of the bounding region, in pixels.  A input value of 0 indicates that
	 *										     the bounding region height should not be considered.  Once the total height of lines reaches this
	 *										     value, the function returns and no further processing occurs.
	 *							DrawFont:	[in] specifies the font to use for retrieving the size of the characters in the string
	 *							Scale:		[in] specifies the amount of scaling to apply to the string
	 * @param	CurX			specifies the pixel location to begin the wrapping; usually equal to the X pos of the bounding region, unless wrapping is initiated
	 *								in the middle of the bounding region (i.e. indentation)
	 * @param	pText			the text that should be wrapped
	 * @param	out_Lines		[out] will contain an array of strings which fit inside the bounding region specified.  Does
	 *							not clear the array first.
	 * @param	EOL				a pointer to a single character that is used as the end-of-line marker in this string
	 * @param	MaxLines		the maximum number of lines that can be created.
	 */
	static void WrapString( FTextSizingParameters& Parameters, FLOAT CurX, const TCHAR* pText, TArray<struct FWrappedStringElement>& out_Lines, const TCHAR* EOL = NULL, INT MaxLines = MAXINT);
}

/**
 * Draws a texture to an axis-aligned quad at CurX,CurY.
 *
 * @param	Tex - The texture to render.
 * @param	XL - The width of the quad in pixels.
 * @param	YL - The height of the quad in pixels.
 * @param	U - The U coordinate of the quad's upper left corner, in normalized coordinates.
 * @param	V - The V coordinate of the quad's upper left corner, in normalized coordinates.
 * @param	UL - The range of U coordinates which is mapped to the quad.
 * @param	VL - The range of V coordinates which is mapped to the quad.
 * @param	LColor - Color to colorize this texture.
 * @param	bClipTile - Whether to clip the texture (FALSE by default).
 */
native noexport final function DrawTile(Texture Tex, float XL, float YL, float U, float V, float UL, float VL, optional LinearColor LColor, optional bool ClipTile, optional EBlendMode Blend);


/**
 * Optimization call to pre-allocate vertices and triangles for future DrawTile() calls.
 *		NOTE: Num is number of subsequent DrawTile() calls that will be made in a row with the
 *			same Texture and Blend settings. If other draws (Text, different textures, etc) are
 *			done before the Num DrawTile calls, the optimization will not work and will only waste memory.
 *
 * @param	Num - The number of DrawTile calls that will follow this function call
 * @param	Tex - The texture that will be used to render tiles.
 * @param	Blend - The blend mode that will be used for tiles.
 */
native noexport final function PreOptimizeDrawTiles(INT Num, Texture Tex, optional EBlendMode Blend);

/**
 * Draws the emissive channel of a material to an axis-aligned quad at CurX,CurY.
 *
 * @param	Mat - The material which contains the emissive expression to render.
 * @param	XL - The width of the quad in pixels.
 * @param	YL - The height of the quad in pixels.
 * @param	U - The U coordinate of the quad's upper left corner, in normalized coordinates.
 * @param	V - The V coordinate of the quad's upper left corner, in normalized coordinates.
 * @param	UL - The range of U coordinates which is mapped to the quad.
 * @param	VL - The range of V coordinates which is mapped to the quad.
 * @param	bClipTile - Whether to clip the texture (FALSE by default).
 */
native noexport final function DrawMaterialTile
(
				MaterialInterface	Mat,
				float				XL,
				float				YL,
	optional	float				U,
	optional	float				V,
	optional	float				UL,
	optional	float				VL,
	optional	bool				bClipTile
);

native final function DrawRotatedTile( Texture Tex, rotator Rotation, float XL, float YL,  float U, float V,float UL, float VL,
									  optional float AnchorX=0.5f, optional float AnchorY=0.5f);

native final function DrawRotatedMaterialTile( MaterialInterface Mat, rotator Rotation, float XL, float YL,  optional float U=0.f, optional float V=0.f,
											  optional float UL=0.f, optional float VL=0.f, optional float AnchorX=0.5f, optional float AnchorY=0.5f);

/**
* Draws Draw a circular percentage of a texture - like drawing a pizza with slices missing.
*
* @param	Tex - The texture to render.
* @param	StartTime - 0 -> 12:00, 0.25 = 3:00, 0.5 = 6:00, 0.75 = 9:00. (negative goes other way)
* @param	TotalTime - % of circle to draw (positive is clockwise, neg is counter clockwise)
* @param	XL - The width of the quad in pixels.
* @param	YL - The height of the quad in pixels.
* @param	U - The U coordinate of the quad's upper left corner, in normalized coordinates.
* @param	V - The V coordinate of the quad's upper left corner, in normalized coordinates.
* @param	UL - The range of U coordinates which is mapped to the quad.
* @param	VL - The range of V coordinates which is mapped to the quad.
* @param	LColor - Color to colorize this texture.
* @param	Blend - The blend mode that will be used for tiles.
*/
native noexport final function DrawTimer(Texture Tex, float StartTime, float TotalTime, float XL, float YL, float U, float V, float UL, float VL, optional LinearColor LColor, optional EBlendMode Blend);

native noexport final function DrawTileStretched(Texture Tex, float XL, float YL,  float U, float V, float UL, float VL, optional LinearColor LColor/* = DrawColor*/, optional bool bStretchHorizontally/* = true*/, optional bool bStretchVertically/* = true*/, optional float ScalingFactor/* = 1.0*/);

/**
 *	Draw a number of triangles on the canvas
 *	@param Tex			Texture to apply to triangles
 *	@param Triangles	Array of triangles to render
 */
native final function DrawTris( Texture Tex, array<CanvasUVTri> Triangles, Color InColor );

/** constructor for FontRenderInfo */
static final function FontRenderInfo CreateFontRenderInfo(optional bool bClipText, optional bool bEnableShadow, optional LinearColor GlowColor, optional vector2D GlowOuterRadius, optional vector2D GlowInnerRadius)
{
	local FontRenderInfo Result;

	Result.bClipText = bClipText;
	Result.bEnableShadow = bEnableShadow;
	Result.GlowInfo.bEnableGlow = (GlowColor.A != 0.0);
	if (Result.GlowInfo.bEnableGlow)
	{
		Result.GlowInfo.GlowOuterRadius = GlowOuterRadius;
		Result.GlowInfo.GlowInnerRadius = GlowInnerRadius;
	}
	return Result;
}

native noexport final function StrLen(coerce string String, out float XL, out float YL); // Wrapped!
native noexport final function TextSize(coerce string String, out float XL, out float YL, optional float XScale = 1.0, optional float YScale = 1.0); // Clipped!

native noexport final function DrawText(coerce string Text, optional bool CR = true, optional float XScale = 1.0, optional float YScale = 1.0, optional const out FontRenderInfo RenderInfo);

/** Convert a 3D vector to a 2D screen coords. */
native noexport final function vector Project(vector location);

/** transforms 2D screen coordinates into a 3D world-space origin and direction
 * @param ScreenPos - screen coordinates in pixels
 * @param WorldOrigin (out) - world-space origin vector
 * @param WorldDirection (out) - world-space direction vector
 */
native noexport final function DeProject(vector2D ScreenPos, out vector WorldOrigin, out vector WorldDirection);

/**
 * Pushes a translation matrix onto the canvas.
 *
 * @param TranslationVector		Translation vector to use to create the translation matrix.
 */
native noexport final function PushTranslationMatrix(vector TranslationVector);

/** Pops the topmost matrix from the canvas transform stack. */
native noexport final function PopTransform();

// UnrealScript functions.
event Reset(optional bool bKeepOrigin)
{
	Font        = GetDefaultCanvasFont();

	if ( !bKeepOrigin )
	{
		OrgX        = Default.OrgX;
		OrgY        = Default.OrgY;
	}
	SetPos(Default.CurX, Default.CurY);
	DrawColor   = Default.DrawColor;
	CurYL       = Default.CurYL;
	bCenter     = false;
	bNoSmooth   = false;
}

/**
 * Override this function to change the default font used by the canvas
 *
 * @return the Font to use for this scene
 */
function Font GetDefaultCanvasFont()
{
	return class'Engine'.Static.GetSmallFont();
}


native final function SetPos(float PosX, float PosY, float PosZ=0.0);

final function SetOrigin(float X, float Y)
{
	OrgX = X;
	OrgY = Y;
}

final function SetClip(float X, float Y)
{
	ClipX = X;
	ClipY = Y;
}

native noexport final function PushMaskRegion(float X, float Y, float XL, float YL);
native noexport final function PopMaskRegion();

final function DrawTexture(Texture Tex, float Scale)
{
	if (Tex != None)
	{
		DrawTile(Tex, Tex.GetSurfaceWidth()*Scale, Tex.GetSurfaceHeight()*Scale, 0, 0, Tex.GetSurfaceWidth(), Tex.GetSurfaceHeight());
	}
}

/**
 * Draw a texture to the canvas using one of the special Canvas blend modes
 *
 * @param	Tex		The texture to draw onto the canvas
 * @param	XL/YL	Size on canvas (starting pos is CurX/CurY)
 * @param	U/V/UL/VL	Texture coordinates
 * @param	Blend	The ECanvasBlendMode to use for drawing the Texture
 *
 **/
native final function DrawBlendedTile(Texture Tex, float XL, float YL, float U, float V, float UL, float VL, ECanvasBlendMode Blend);

/**
 * Fake CanvasIcon constructor.
 */
final function CanvasIcon MakeIcon(Texture Texture, optional float U, optional float V, optional float UL, optional float VL)
{
	local CanvasIcon Icon;
	if (Texture != None)
	{
		Icon.Texture = Texture;
		Icon.U = U;
		Icon.V = V;
		Icon.UL = (UL != 0.f ? UL : Texture.GetSurfaceWidth());
		Icon.VL = (VL != 0.f ? VL : Texture.GetSurfaceHeight());
	}
	return Icon;
}

/**
 * Draw a CanvasIcon at the desired canvas position.
 */
final function DrawScaledIcon(CanvasIcon Icon, float X, float Y, Vector Scale)
{
	if (Icon.Texture != None)
	{
		// verify properties are valid
		if (VSize(Scale) <= 0.f)
		{
			Scale.X = 1.f;
			Scale.Y = 1.f;
		}
		if (Icon.UL == 0.f)
		{
			Icon.UL = Icon.Texture.GetSurfaceWidth();
		}
		if (Icon.VL == 0.f)
		{
			Icon.VL = Icon.Texture.GetSurfaceHeight();
		}
		// set the canvas position
		SetPos(CurX, Cury);
		// and draw the texture
		DrawTile(Icon.Texture, Abs(Icon.UL) * Scale.X, Abs(Icon.VL) * Scale.Y, Icon.U, Icon.V, Icon.UL, Icon.VL);
	}
}

/**
 * Draw a CanvasIcon at the desired canvas position.
 */
final function DrawIcon(CanvasIcon Icon, float X, float Y, optional float Scale)
{
	if (Icon.Texture != None)
	{
		// verify properties are valid
		if (Scale <= 0.f)
		{
			Scale = 1.f;
		}
		if (Icon.UL == 0.f)
		{
			Icon.UL = Icon.Texture.GetSurfaceWidth();
		}
		if (Icon.VL == 0.f)
		{
			Icon.VL = Icon.Texture.GetSurfaceHeight();
		}
		// set the canvas position
		SetPos(X, Y);
		// and draw the texture
		DrawTile(Icon.Texture, Abs(Icon.UL) * Scale, Abs(Icon.VL) * Scale, Icon.U, Icon.V, Icon.UL, Icon.VL);
	}
}

final function DrawRect(float RectX, float RectY, optional Texture Tex = DefaultTexture)
{
	DrawTile(Tex, RectX, RectY, 0, 0, Tex.GetSurfaceWidth(), Tex.GetSurfaceHeight());
}

final simulated function DrawBox(float width, float height)
{
	local int X, Y;

	X = CurX;
	Y = CurY;

	// normalize CurX, CurY (eliminate float precision errors)
	SetPos(X, Y);

	// draw the left side
	DrawRect(2, height);
	// then move cursor to top-right
	SetPos(X + width - 2, Y);

	// draw the right face
	DrawRect(2, height);
	// then move the cursor to the top-left (+2 to account for the line we already drew for the left-face)
	SetPos(X + 2, Y);

	// draw the top face
	DrawRect(width - 4, 2);
	// then move the cursor to the bottom-left (+2 to account for the line we already drew for the left-face)
	SetPos(X + 2, Y + height - 2);

	// draw the bottom face
	DrawRect(width - 4, 2);
	// move the cursor back to its original position
	SetPos(X, Y);
}

native final function SetDrawColor(byte R, byte G, byte B, optional byte A = 255);

/** Set the draw color using a color struct */
final function SetDrawColorStruct(color C)
{
	SetDrawColor(C.R, C.G, C.B, C.A);
}

native noexport final function Draw2DLine(float X1, float Y1, float X2, float Y2, color LineColor);

native final function DrawTextureLine(vector StartPoint, vector EndPoint, float Perc, float Width, color LineColor, Texture LineTexture, float U, float V, float UL, float VL );
native final function DrawTextureDoubleLine(vector StartPoint, vector EndPoint, float Perc, float Spacing, float Width,	color LineColor, color AltLineColor, Texture Tex, float U, float V, float UL, float VL);


/**
 * Draws a graph comparing 2 variables.  Useful for visual debugging and tweaking.
 *
 * @param Title		Label to draw on the graph, or "" for none
 * @param ValueX	X-axis value of the point to plot
 * @param ValueY	Y-axis value of the point to plot
 * @param UL_X		X screen coord of the upper-left corner of the graph
 * @param UL_Y		Y screen coord of the upper-left corner of the graph
 * @param W			Width of the graph, in pixels
 * @param H			Height of the graph, in pixels
 * @param RangeX	Range of values expressed by the X axis of the graph
 * @param RangeY	Range of values expressed by the Y axis of the graph
 */
function DrawDebugGraph(coerce string Title, float ValueX, float ValueY, float UL_X, float UL_Y, float W, float H, vector2d RangeX, vector2d RangeY)
{
	`define GRAPH_ICONSIZE		8

	local int X, Y;

	// draw graph box
	SetDrawColor(255, 255, 255, 255);
	SetPos(UL_X, UL_Y);
	DrawBox(W, H);

	// plot point
	SetDrawColor(255, 255, 0, 255);
	X = UL_X + GetRangePctByValue(RangeX, ValueX) * W - `GRAPH_ICONSIZE/2;
	Y = UL_Y + GetRangePctByValue(RangeY, ValueY) * H - `GRAPH_ICONSIZE/2;
	SetPos(X, Y);
	DrawRect(`GRAPH_ICONSIZE, `GRAPH_ICONSIZE);

	// plot lines
	SetDrawColor(128, 128, 0, 128);
	Draw2DLine(UL_X, Y, UL_X+W, Y, DrawColor);		// horiz
	Draw2DLine(X, UL_Y, X, UL_Y+H, DrawColor);		// vert

	// x value at bottom
	SetDrawColor(255, 255, 0, 255);
	SetPos(X, UL_Y+H+16);
	DrawText(ValueX);

	// y value on right
	SetPos(UL_X+W+8, Y);
	DrawText(ValueY);

	// title
	if (Title != "")
	{
		SetPos(UL_X, UL_Y-16);
		DrawText(Title);
	}
}

defaultproperties
{
	DrawColor=(R=127,G=127,B=127,A=255)
	ColorModulate=(X=1,Y=1,Z=1,W=1)
	DefaultTexture="EngineResources.WhiteSquareTexture"
}
