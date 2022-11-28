class MaterialExpressionPanner extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Coordinate;
var ExpressionInput	Time;

var() float	SpeedX,
			SpeedY;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}