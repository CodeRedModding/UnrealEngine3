class MaterialExpressionCosine extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Input;

var() float	Period;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual FString GetCaption() const;
}

defaultproperties
{
	Period=1.f
}