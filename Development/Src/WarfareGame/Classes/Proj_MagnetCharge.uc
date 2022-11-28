/** 
 * MagnetGun Magnet Charge Projectile
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Proj_MagnetCharge extends Projectile;

var	PrimitiveComponent	HitPrimComp;
var name				HitBoneName;

simulated singular event Touch( Actor Other, vector HitLocation, vector HitNormal );
simulated function HitWall(vector HitNormal, actor Wall);

defaultproperties
{
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
    	StaticMesh=StaticMesh'COG_MagnetGun.Mesh.MagnetSpike'
		Materials(0)=Material'COG_MagnetGun.Materials.NegativeChargeMat'
        HiddenGame=false
        CastShadow=false
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
    End Object

	Begin Object Class=TransformComponent Name=TransformComponent0
		TransformedComponent=StaticMeshComponent0
		Scale=0.2
	End Object

	Physics=PHYS_None
	RemoteRole=ROLE_SimulatedProxy
	Components.Add(TransformComponent0)
	LifeSpan=0.0
	bHardAttach=true
}