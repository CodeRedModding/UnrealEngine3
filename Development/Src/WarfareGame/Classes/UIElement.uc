class UIElement extends Object
	editinlinenew
	abstract
	native;

cpptext
{
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};

/** == Common properties == */

/** Is this element enabled? */
var(Element) bool bEnabled;

/** Other element that this element is relative to */
var(Element) UIElement ParentElement;

/** XY position of the element */
var(Element) int DrawX;
var(Element) int DrawY;

/** WH of the element */
var(Element) int DrawW;
var(Element) int DrawH;

/** Should DrawX be treated as the center point for drawing? */
var(Element) bool bCenterX;

/** Should DrawY be treated as the center point for drawing? */
var(Element) bool bCenterY;

/** Draw color to use for the element */
var(Element) color DrawColor;

var(Control) bool bAcceptsFocus;
var(Control) int FocusId;


/** == Border specific == */

/** Draw a border on this icon? */
var(Border) bool bDrawBorder;

/** Border material to use */
var(Border) Texture2D BorderIcon;

/** Thickness of the border */
var(Border) int BorderSize;

/** Border draw color */
var(Border) color BorderColor;

/** Border UVs */
var(Border) int BorderU;
var(Border) int BorderV;
var(Border) int BorderUSize;
var(Border) int BorderVSize;


/** == Editor specific properties == */

/** Is this element currently selected? */
var transient bool bSelected;

/** Does the element support width/height drag */
var const bool bSizeDrag;

defaultproperties
{
	bEnabled=true
	DrawColor=(R=255,G=255,B=255,A=255)
	BorderIcon=Texture2D'WhiteSquareTexture'
	BorderColor=(A=255)
	DrawW=64
	DrawH=64
}
