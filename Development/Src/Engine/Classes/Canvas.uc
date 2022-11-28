//=============================================================================
// Canvas: A drawing canvas.
// This is a built-in Unreal class and it shouldn't be modified.
//
//=============================================================================
class Canvas extends Object
	native
	noexport;

// Modifiable properties.
var font    Font;            // Font for DrawText.
var float   SpaceX, SpaceY;  // Spacing for after Draw*.
var float   OrgX, OrgY;      // Origin for drawing.
var float   ClipX, ClipY;    // Bottom right clipping region.
var float   CurX, CurY;      // Current position for drawing.
var float   CurYL;           // Largest Y size since DrawText.
var color   DrawColor;       // Color for drawing.
var bool    bCenter;         // Whether to center the text.
var bool    bNoSmooth;       // Don't bilinear filter.
var const int SizeX, SizeY;  // Zero-based actual dimensions.

// Internal.
var const int RenderInterface;
var const int SceneView;

var Plane ColorModulate;
var Texture2D DefaultTexture;

// native functions.
native(464) final function StrLen( coerce string String, out float XL, out float YL ); // Wrapped!
native(465) final function DrawText( coerce string Text, optional bool CR );
native(466) final function DrawTile( Texture2D Tex, float XL, float YL, float U, float V, float UL, float VL );
native(468) final function DrawTileClipped( Texture2D Tex, float XL, float YL, float U, float V, float UL, float VL );
native(469) final function DrawTextClipped( coerce string Text, optional bool bCheckHotKey );
native(470) final function TextSize( coerce string String, out float XL, out float YL ); // Clipped!

native final function WrapStringToArray(string Text, out array<string> OutArray, float dx, string EOL);

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
 */
native final function DrawMaterialTile
( 
				MaterialInstance	Mat, 
				float				XL, 
				float				YL, 
	optional	float				U, 
	optional	float				V, 
	optional	float				UL, 
	optional	float				VL 
);

// jmw - These are two helper functions.  The use the whole texture only.  If you need better support, use DrawTile
native final function DrawTileStretched(Texture2D Tex, float XL, float YL);
native final function DrawTileJustified(Texture2D Tex, byte Justification, float XL, float YL);
native final function DrawTileScaled(Texture2D Tex, float XScale, float YScale);
native final function DrawTextJustified(coerce string String, byte Justification, float x1, float y1, float x2, float y2);

// UnrealScript functions.
event Reset()
{
	Font        = Default.Font;
	SpaceX      = Default.SpaceX;
	SpaceY      = Default.SpaceY;
	OrgX        = Default.OrgX;
	OrgY        = Default.OrgY;
	CurX        = Default.CurX;
	CurY        = Default.CurY;
	DrawColor   = Default.DrawColor;
	CurYL       = Default.CurYL;
	bCenter     = false;
	bNoSmooth   = false;
}
final function SetPos( float X, float Y )
{
	CurX = X;
	CurY = Y;
}
final function SetOrigin( float X, float Y )
{
	OrgX = X;
	OrgY = Y;
}
final function SetClip( float X, float Y )
{
	ClipX = X;
	ClipY = Y;
}
final function DrawPattern( Texture2D Tex, float XL, float YL, float Scale )
{
	DrawTile( Tex, XL, YL, (CurX-OrgX)*Scale, (CurY-OrgY)*Scale, XL*Scale, YL*Scale );
}
final function DrawIcon( Texture2D Tex, float Scale )
{
	if ( Tex != None )
		DrawTile( Tex, Tex.SizeX*Scale, Tex.SizeY*Scale, 0, 0, Tex.SizeX, Tex.SizeY );
}
final function DrawRect(float RectX, float RectY, optional Texture2D Tex )
{
	if( Tex != None )
		DrawTile( Tex, RectX, RectY, 0, 0, Tex.SizeX, Tex.SizeY );
	else
		DrawTile( DefaultTexture, RectX, RectY, 0, 0, DefaultTexture.SizeX, DefaultTexture.SizeY );
}

final function SetDrawColor(byte R, byte G, byte B, optional byte A)
{
	local Color C;

	C.R = R;
	C.G = G;
	C.B = B;
	if ( A == 0 )
		A = 255;
	C.A = A;
	DrawColor = C;
}

// Draw a vertical line
final function DrawVertical(float X, float height)
{
    SetPos( X, CurY);
    DrawRect(2, height);
}

// Draw a horizontal line
final function DrawHorizontal(float Y, float width)
{
    SetPos(CurX, Y);
    DrawRect(width, 2);
}

// Draw Line is special as it saves it's original position

final function DrawLine(int direction, float size)
{
    local float X, Y;

    // Save current position
    X = CurX;
    Y = CurY;

    switch (direction)
    {
      case 0:
		  SetPos(X, Y - size);
		  DrawRect(2, size);
		  break;

      case 1:
		  DrawRect(2, size);
		  break;

      case 2:
		  SetPos(X - size, Y);
		  DrawRect(size, 2);
		  break;

	  case 3:
		  DrawRect(size, 2);
		  break;
    }
    // Restore position
    SetPos(X, Y);
}

final simulated function DrawBracket(float width, float height, float bracket_size)
{
    local float X, Y;
    X = CurX;
    Y = CurY;

	Width  = max(width,5);
	Height = max(height,5);

    DrawLine(3, bracket_size);
    DrawLine(1, bracket_size);
    SetPos(X + width, Y);
    DrawLine(2, bracket_size);
    DrawLine(1, bracket_size);
    SetPos(X + width, Y + height);
    DrawLine(0, bracket_size);
    DrawLine(2, bracket_size);
    SetPos(X, Y + height);
    DrawLine(3, bracket_size);
    DrawLine( 0, bracket_size);

    SetPos(X, Y);
}

final simulated function DrawBox(canvas canvas, float width, float height)
{
	local float X, Y;
	X = canvas.CurX;
	Y = canvas.CurY;
	canvas.DrawRect(2, height);
	canvas.DrawRect(width, 2);
	canvas.SetPos(X + width, Y);
	canvas.DrawRect(2, height);
	canvas.SetPos(X, Y + height);
	canvas.DrawRect(width+1, 2);
	canvas.SetPos(X, Y);
}

native final function vector Project(vector location);			// Convert a 3D vector to a 2D screen coords.
native final function vector DeProject(vector screenloc);		// Convert 2D screen coords to a 3D vector

defaultproperties
{
     DrawColor=(R=127,G=127,B=127,A=255)
	 Font="EngineFonts.SmallFont"
     ColorModulate=(X=1,Y=1,Z=1,W=1)
     DefaultTexture="EngineResources.WhiteSquareTexture"
}