class UIElement_Icon extends UIElement
	native;

cpptext
{
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};

/** == Icon specific == */

/** Texture to use in drawing */
var(Icon) Texture2D DrawIcon;

/** UV information for DrawIcon */
var(UV) float DrawU;
var(UV) float DrawV;

/** If these are set to -1, the full UVs for DrawIcon are used */
var(UV) int DrawUSize;
var(UV) int DrawVSize;

/** Overall scale to use on DrawW/DrawH */
var(Icon) float DrawScale;

defaultproperties
{
	DrawIcon=Texture2D'WhiteSquareTexture'
	DrawScale=1.f
	DrawUSize=-1
	DrawVSize=-1
	bSizeDrag=true
}
