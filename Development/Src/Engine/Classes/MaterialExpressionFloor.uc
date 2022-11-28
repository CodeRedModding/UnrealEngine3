class MaterialExpressionFloor extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Input;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}