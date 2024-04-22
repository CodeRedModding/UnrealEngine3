/**
 *	Encapsulates a RB_RadialImpulseComponent to let a level designer place one in a level.
 *	When toggled from Kismet, will apply a kick to surrounding physics objects within blast radius.
 *	@see AddRadialImpulse
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_RadialImpulseActor extends RigidBodyBase
	native(Physics)
	placeable;

var						DrawSphereComponent			RenderComponent;
var() const editconst	RB_RadialImpulseComponent	ImpulseComponent;
var repnotify byte ImpulseCount;

replication
{
	if (bNetDirty)
		ImpulseCount;
}

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		ImpulseComponent.FireImpulse( Location );
		ImpulseCount++;
		bForceNetUpdate = TRUE;
	}
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'ImpulseCount')
	{
		ImpulseComponent.FireImpulse(Location);
	}
}

cpptext
{
#if WITH_EDITOR
	// AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
		bDrawOnlyIfSelected=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
	End Object
	RenderComponent=DrawSphere0
	Components.Add(DrawSphere0)

	Begin Object Class=RB_RadialImpulseComponent Name=ImpulseComponent0
		PreviewSphere=DrawSphere0
	End Object
	ImpulseComponent=ImpulseComponent0
	Components.Add(ImpulseComponent0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadImpulse'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	bEdShouldSnap=true
	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true
}
