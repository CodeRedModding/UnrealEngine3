class MaterialExpressionTime extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() bool	Absolute;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}