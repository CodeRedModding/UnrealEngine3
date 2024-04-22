/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTime extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** This time continues advancing regardless of whether the game is paused. */
var() bool bIgnorePause;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;

	/**
	 * @return TRUE if the expression preview needs realtime update
     */
	virtual UBOOL NeedsRealtimePreview() { return TRUE; }
}

defaultproperties
{
	MenuCategories(0)="Utility"
	MenuCategories(1)="WorldPosOffset"
}
