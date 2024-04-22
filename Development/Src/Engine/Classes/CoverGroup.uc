/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CoverGroup extends Info
	native
	placeable
	ClassGroup(Cover)
	dependson(CoverLink);

/**
 * Defines a group of cover links they can be acted on as a single unit
 * (ie enable/disable)
 */
cpptext
{
	void AutoFillGroup( ECoverGroupFillAction CGFA, TArray<class ACoverLink*>& Links );

	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel);

	virtual void PostLoad();
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
	virtual void CheckForErrors();
#endif
}

enum ECoverGroupFillAction
{
	CGFA_Overwrite,
	CGFA_Add,
	CGFA_Remove,
	CGFA_Clear,
	CGFA_Cylinder,
};

/** List of cover links in the group */
var() array<ActorReference> CoverLinkRefs;

/** Radius around group actor to select nodes */
var() float	AutoSelectRadius;
/** Z distance below group actor to select nodes */
var() float AutoSelectHeight;

native function EnableGroup();
native function DisableGroup();
native function ToggleGroup();

simulated function OnToggle( SeqAct_Toggle Action )
{
	// On
	if( Action.InputLinks[0].bHasImpulse )
	{
		EnableGroup();
	}
	// Off
	if( Action.InputLinks[1].bHasImpulse )
	{
		DisableGroup();
	}
	// Toggle
	if( Action.InputLinks[2].bHasImpulse )
	{
		ToggleGroup();
	}
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorMaterials.CovergroupIcon'
		SpriteCategoryName="Cover"
	End Object

	Begin Object Class=CoverGroupRenderingComponent Name=CoverGroupRenderer
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
	End Object
	Components.Add(CoverGroupRenderer)

	AutoSelectRadius=0.f
	AutoSelectHeight=0.f

	bStatic=TRUE
}
