/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkyLightComponent extends LightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

/** The brightness for the lower hemisphere of the sky light. */
var() const float LowerBrightness;

/** The color of the lower hemisphere of the sky light. */
var() const color LowerColor;

cpptext
{
	/**
	 * Called when a property is being changed.
	 *
	 * @param PropertyThatChanged	Property that changed or NULL if unknown or multiple
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	/**
	 * Called after data has been serialized.
	 */
	virtual void PostLoad();

	// ULightComponent interface.
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual FVector4 GetPosition() const;
	virtual ELightComponentType GetLightType() const;
}

defaultproperties
{
	CastShadows=False
	LowerColor=(R=255,G=255,B=255)
}
