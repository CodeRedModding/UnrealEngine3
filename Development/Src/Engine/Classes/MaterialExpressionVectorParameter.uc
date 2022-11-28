class MaterialExpressionVectorParameter extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() name			ParameterName;
var() LinearColor	DefaultValue;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual void GetOutputs(TArray<FExpressionOutput>& Outputs) const;
	virtual FString GetCaption() const { return TEXT("Vector parameter"); }
}
