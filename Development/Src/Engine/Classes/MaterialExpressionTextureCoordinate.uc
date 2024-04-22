/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTextureCoordinate extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** Texture coordinate index */
var() int	CoordinateIndex;

/** Controls how much the texture tiles horizontally, by scaling the U component of the vertex UVs by the specified amount. */
var() float UTiling;

/** Controls how much the texture tiles vertically, by scaling the V component of the vertex UVs by the specified amount. */
var() float VTiling;

/** Would like to unmirror U or V 
 *  - if the texture is mirrored and if you would like to undo mirroring for this texture sample, use this to unmirror */
var() bool          UnMirrorU;
var() bool          UnMirrorV;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	UTiling=1.0
	VTiling=1.0
	MenuCategories(0)="Coordinates"
	MenuCategories(1)="WorldPosOffset"
}
