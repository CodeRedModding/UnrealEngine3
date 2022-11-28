class MaterialExpressionLinearInterpolate extends MaterialExpression
	native(Material);

var ExpressionInput	A;
var ExpressionInput	B;
var ExpressionInput Alpha;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}