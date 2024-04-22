/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ImageBasedReflectionComponent extends StaticMeshComponent
	native(Mesh)
	hidecategories(StaticMeshComponent)
	placeable
	editinlinenew;

/** Whether to render the reflection. */
var() bool bEnabled;

/** Whether the reflection should be visible from the back. */
var() bool bTwoSided;

/** 
 * Texture that will be applied to this reflection. 
 * This texture will be used in a texture array and therefore must have the same size, number of mips, texture group settings and format 
 * As the ReflectionTexture of every ImageBasedReflectionComponent that can be loaded at the same time.
 */
var() Texture2D ReflectionTexture;

/** Color that will be multiplied against ReflectionTexture.  Alpha is a brightness control. */
var() interp LinearColor ReflectionColor;

cpptext
{
protected:
	// ActorComponent interface.
	virtual UBOOL IsValidComponent() const { return ReflectionTexture != NULL; }
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

/**
* Changes the enabled state of the image reflection component.
* @param bSetEnabled - The new value for bEnabled.
*/
native final function SetEnabled(bool bSetEnabled);

native final function UpdateImageReflectionParameters(); 

/** Called from matinee code when ReflectionColor property changes. */
function OnUpdatePropertyReflectionColor()
{
	UpdateImageReflectionParameters();
}

defaultproperties
{
	bEnabled=true
	ReflectionTexture=Texture2D'Engine_MI_Shaders.Textures.DefaultReflectionTexture_IBR'
	Materials(0)=Material'EditorMaterials.Utilities.ImageReflectionPreview'
	ReflectionColor=(R=1.0f, G=1.0f, B=1.0f, A=1.0f)
	StaticMesh=StaticMesh'EditorMeshes.TexPropPlane'
	bCastDynamicShadow=FALSE
	BlockRigidBody=FALSE
	CollideActors=FALSE
	bForceDirectLightMap=FALSE
	bAcceptsDynamicLights=FALSE
	bAcceptsLights=FALSE
	CastShadow=FALSE
	bUsePrecomputedShadows=FALSE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=FALSE
	bUseAsOccluder=FALSE
	HiddenGame=true
	WireframeColor=(R=100,G=100,B=200,A=255)
}
