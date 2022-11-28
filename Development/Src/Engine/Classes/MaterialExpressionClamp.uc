class MaterialExpressionClamp extends MaterialExpression
	native(Material);

var ExpressionInput	Input;
var ExpressionInput	Min;
var ExpressionInput Max;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}