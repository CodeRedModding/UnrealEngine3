/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * This UV node generates texture coordinates in view space centered on the particle system's MacroUVPosition, with tiling controlled by the particle system's MacroUVRadius.
 * It is useful for mapping a 'macro' noise texture in a continuous manner onto all particles of a particle system.
 */
class MaterialExpressionParticleMacroUV extends MaterialExpression
	native(Material);

/** Whether to calculate the UVs in View space, which gives a slightly different UV position based on depth and creates a parallax effect, at the cost of texture swimming. */
var() bool bUseViewSpace;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Particles"
}
