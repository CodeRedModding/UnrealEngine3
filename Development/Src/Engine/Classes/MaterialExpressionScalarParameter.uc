class MaterialExpressionScalarParameter extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() name	ParameterName;
var() float	DefaultValue;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const { return TEXT("Scalar parameter"); }
}
