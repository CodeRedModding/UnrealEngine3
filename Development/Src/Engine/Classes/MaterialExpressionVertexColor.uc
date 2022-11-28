class MaterialExpressionVertexColor extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
	virtual void GetOutputs(TArray<FExpressionOutput>& Outputs) const;
}