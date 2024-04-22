/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_ConstraintDrawComponent extends PrimitiveComponent
	native(Physics);

cpptext
{
	// Primitive Component interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	
	/**
	 * Update the bounds of the component.
	 */
	virtual void UpdateBounds(); 

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const; 
}

var()	MaterialInterface	LimitMaterial;

defaultproperties
{
}
