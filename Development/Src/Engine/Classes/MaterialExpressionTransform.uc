/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTransform extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** input expression for this transform */
var ExpressionInput	Input;

/** Source coordinate space of the vector */
var() const enum EMaterialVectorCoordTransformSource
{
	TRANSFORMSOURCE_World<DisplayName=World>,
	TRANSFORMSOURCE_Local<DisplayName=Local>,
	TRANSFORMSOURCE_Tangent<DisplayName=Tangent>,
	TRANSFORMSOURCE_View<DisplayName=View>,
} TransformSourceType<DisplayName=Source>;

/** Destination coordinate space of the vector */
var() const enum EMaterialVectorCoordTransform
{
	TRANSFORM_World<DisplayName=World>,
	TRANSFORM_View<DisplayName=View>,
	TRANSFORM_Local<DisplayName=Local>,
	TRANSFORM_Tangent<DisplayName=Tangent>
} TransformType<DisplayName=Destination>;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	MenuCategories(0)="VectorOps"
	TransformSourceType=TRANSFORMSOURCE_Tangent
}
