/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DominantPointLightComponent extends PointLightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

cpptext
{
	/**
	* Called after property has changed via e.g. property window or set command.
	*
	* @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
    virtual ELightComponentType GetLightType() const;
    /** Returns the distance to the nearest dominant shadow transition, in world space units, starting from the edge of the bounds. */
    FLOAT GetDominantShadowTransitionDistance(const FBoxSphereBounds& Bounds, FLOAT MaxSearchDistance, UBOOL bDebugSearch, TArray<class FDebugShadowRay>& DebugRays, UBOOL& bLightingIsBuilt) const;
}

defaultproperties
{
	LightShadowMode=LightShadow_Normal
}
