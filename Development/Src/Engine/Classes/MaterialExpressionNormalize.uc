class MaterialExpressionNormalize extends MaterialExpression
	native(Material);

var ExpressionInput	Vector;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const { return TEXT("Normalize"); }
}