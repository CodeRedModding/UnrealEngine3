/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CoverMeshComponent extends StaticMeshComponent
	native(AI);

cpptext
{
	void UpdateBounds();
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual UBOOL ShouldRecreateProxyOnUpdateTransform() const;
	virtual void UpdateMeshes() {};
};

struct native CoverMeshes
{
	var StaticMesh Base;
	var StaticMesh LeanLeft, LeanRight;
	var StaticMesh LeanLeftPref, LeanRightPref;
	var StaticMesh Climb, Mantle;
	var StaticMesh SlipLeft, SlipRight;
	var StaticMesh SwatLeft, SwatRight;
	var StaticMesh PopUp;
	var StaticMesh PlayerOnly;
};
var array<CoverMeshes> Meshes;

/** Base offset applied to all meshes */
var vector LocationOffset;

var StaticMesh AutoAdjustOn;
var StaticMesh AutoAdjustOff;
var StaticMesh Disabled;

/** Allows the LDs to show all cover in a level without editing the cover */
var editoronly transient bool bShowWhenNotSelected;

defaultproperties
{
	HiddenGame=true
	AlwaysLoadOnServer=FALSE
	AlwaysLoadOnClient=FALSE
	CollideActors=FALSE
	BlockActors=FALSE
	BlockZeroExtent=FALSE
	BlockNonZeroExtent=FALSE
	BlockRigidBody=FALSE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=FALSE
	bAcceptsLights=FALSE
	CastShadow=FALSE

	StaticMesh=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy__BASE_TALL'
	LocationOffset=(X=0,Y=0,Z=-60)

	Meshes(CT_None)=(Base=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy__BASE_TALL')
	Meshes(CT_Standing)={(
						  Base=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy__BASE_TALL',
						  LeanLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanLeftS',
						  LeanRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanRightS',
						  SlipLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_CoverSlipLeft',
						  SlipRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_CoverSlipRight',
						  SwatLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_SwatLeft',
						  SwatRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_SwatRight',
						  PlayerOnly=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_PlayerOnlyS'
						)}
	Meshes(CT_MidLevel)={(
						  Base=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy__BASE_SHORT',
						  LeanLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanLeftM',
						  LeanLeftPref=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanLeftMPref',
						  LeanRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanRightM',
						  LeanRightPref=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_LeanRightMPref',
						  Climb=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_Climb',
						  Mantle=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_Mantle',
						  SlipLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_CoverSlipLeft',
						  SlipRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_CoverSlipRight',
						  SwatLeft=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_SwatLeft',
						  SwatRight=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_SwatRight',
						  PopUp=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_PopUp',
						  PlayerOnly=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_PlayerOnlyM'
						)}

	AutoAdjustOn=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_AutoAdjust'
	AutoAdjustOff=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_AutoAdjustOff'
	Disabled=StaticMesh'NodeBuddies.3D_Icons.NodeBuddy_Enabled'
}
