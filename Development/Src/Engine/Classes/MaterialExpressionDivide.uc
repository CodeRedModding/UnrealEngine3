class MaterialExpressionDivide extends MaterialExpression
	native(Material);

var ExpressionInput	A;
var ExpressionInput	B;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}