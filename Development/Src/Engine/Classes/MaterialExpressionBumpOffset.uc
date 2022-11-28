class MaterialExpressionBumpOffset extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

// Outputs: Coordinate + Eye.xy * (Height - ReferencePlane) * HeightRatio

var ExpressionInput	Coordinate;
var ExpressionInput	Height;
var() float			HeightRatio;	// Perceived height as a fraction of width.
var() float			ReferencePlane;	// Height at which no offset is applied.

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}

defaultproperties
{
	HeightRatio=0.05
	ReferencePlane=0.5
}