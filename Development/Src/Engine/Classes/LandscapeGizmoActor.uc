/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class LandscapeGizmoActor extends Actor
	notplaceable
	native(Terrain);

var(Gizmo) editoronly float Width;
var(Gizmo) editoronly float Height;
var(Gizmo) editoronly float LengthZ;
var(Gizmo) editoronly float MarginZ;

var(Gizmo) editoronly float MinRelativeZ;
var(Gizmo) editoronly float RelativeScaleZ;

var(Gizmo) editoronly editconst transient LandscapeInfo TargetLandscapeInfo;

cpptext
{
#if WITH_EDITOR
	virtual void Duplicate(ALandscapeGizmoActor* Gizmo); 
	//virtual void EditorApplyTranslation(const FVector& DeltaTranslation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_DecalActorIcon'
		Scale=0.3
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Misc"
	End Object
	Components.Add(Sprite)

	bStatic=true
	bMovable=false
	Width=1280
	Height=1280
	LengthZ=1280
	MarginZ=512
	bEditable=false
	MinRelativeZ=0.0
	RelativeScaleZ=1.0
}
