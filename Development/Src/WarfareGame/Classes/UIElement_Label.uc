class UIElement_Label extends UIElement
	native;

cpptext
{
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};

/** Font to use for DrawLabel */
var(Label) Font DrawFont;

/** String to draw */
var(Label) string DrawLabel;

defaultproperties
{
	DrawLabel="TEXT"
	DrawFont=Font'MediumFont'
}
