class MaterialExpressionRotator extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Coordinate;
var ExpressionInput	Time;

var() float	CenterX,
			CenterY,
			Speed;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}

defaultproperties
{
	CenterX=0.5
	CenterY=0.5
	Speed=0.25
}