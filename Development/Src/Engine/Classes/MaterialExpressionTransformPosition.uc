/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTransformPosition extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** input expression for this transform */
var ExpressionInput	Input;

/** source format of the position that will be transformed */
var() const enum EMaterialPositionTransformSource
{
	TRANSFORMPOSSOURCE_Local<DisplayName=Local>,
	TRANSFORMPOSSOURCE_PostProjection<DisplayName=Post Projection>
}TransformSourceType<DisplayName=Source>;

/** type of transform to apply to the input expression */
var() const enum EMaterialPositionTransform
{
	// transform from post projection to world space
	TRANSFORMPOS_World<DisplayName=World>
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
	TransformSourceType=TRANSFORMPOSSOURCE_PostProjection
}
