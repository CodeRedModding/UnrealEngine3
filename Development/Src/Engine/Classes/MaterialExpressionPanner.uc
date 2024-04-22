/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionPanner extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Coordinate;
var ExpressionInput	Time;

var() float	SpeedX,
			SpeedY;

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

	/**
	 * @return TRUE if the expression preview needs realtime update
     */
	virtual UBOOL NeedsRealtimePreview() { return Time.Expression==NULL && (SpeedX != 0.f || SpeedY != 0.f); }
}

defaultproperties
{
	MenuCategories(0)="Coordinates"
	MenuCategories(1)="WorldPosOffset"
}
