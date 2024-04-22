/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdAgentSM extends GameCrowdAgent
	abstract
	native;

var(Rendering) StaticMeshComponent Mesh;
var(Rendering) MaterialInstanceConstant MeshColor;

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();

	MeshColor = Mesh.CreateAndSetMaterialInstanceConstant(0);
}

simulated function InitDebugColor()
{
	Super.InitDebugColor();
	ChangeDebugColor( DebugAgentColor );
}

simulated function ChangeDebugColor( Color InC )
{
	local LinearColor C;

	C.R = float(InC.R) / 255.f;
	C.G = float(InC.G) / 255.f;
	C.B = float(InC.B) / 255.f;
	MeshColor.SetVectorParameterValue( 'CrowdCylinderColor', C );	
}

function ActivateBehavior(GameCrowdAgentBehavior NewBehaviorArchetype, optional Actor LookAtActor )
{
	Super.ActivateBehavior( NewBehaviorArchetype, LookAtActor );

	if( CurrentBehavior != None )
	{
		ChangeDebugColor( CurrentBehavior.DebugBehaviorColor );
	}
	else
	{
		ChangeDebugColor( DebugAgentColor );
	}
}

function StopBehavior()
{
	Super.StopBehavior();

	if( CurrentBehavior == None )
	{
		ChangeDebugColor( DebugAgentColor );
	}
}  

defaultproperties
{
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		CollideActors=TRUE
		BlockActors=TRUE
		BlockZeroExtent=TRUE
		BlockNonZeroExtent=FALSE
		BlockRigidBody=FALSE
		RBChannel=RBCC_GameplayPhysics
		bCastDynamicShadow=FALSE
		RBCollideWithChannels=(Default=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE)
		bAcceptsDynamicDecals=FALSE // for crowds there are so many of them probably not going to notice not getting decals on them.  Each decal on them causes entire SkelMesh to be rerendered
	End Object
	Mesh=StaticMeshComponent0
	Components.Add(StaticMeshComponent0)
}
