class MaterialExpressionConstant extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() float	R;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}