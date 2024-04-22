/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class SplineLoftActor extends SplineActor
	placeable
	native(Spline);
	

/** Size that mesh should be along its local X axis at this node on spline. */
var()   interp float   ScaleX;

/** Size that mesh should be along its local Y axis at this node on spline. */
var()   interp float   ScaleY;

/** Components used to render deformed meshes along each connection spline to other nodes */
var     array<SplineMeshComponent>  SplineMeshComps;

/** 
 *  Mesh to deform along spline 
 *  This must be set to have a visual mesh viewable in the world
 *  @TODO: make a map check for errors for this being None
 */
var() const StaticMesh                  DeformMesh;

/** Materials to override with for this instance */
var() const array<MaterialInterface>	DeformMeshMaterials;


/** Roll around spline at this node, in degrees */
var()   interp float   Roll;

/** Axis (in world space) used to determine the X axis for the mesh along the spline */
var()   Vector  WorldXDir;

/** Offset in X and Y for the mesh along the spline.  Note the offset is applied BEFORE scaling and roll */
var()   Vector2D Offset;

/** If TRUE, will use smooth interpolation (ease in/out) for Scale and Roll along this section of spline. If FALSE, uses linear */
var()   bool    bSmoothInterpRollAndScale;

/** If TRUE, generated SplineMeshComponents will accept lights */
var()   bool    bAcceptsLights;

/** Light environment used to light dynamically moving spline */
var() const editconst DynamicLightEnvironmentComponent MeshLightEnvironment;

/** The maximum distance at which these meshes will be drawn */
var()   float   MeshMaxDrawDistance;

cpptext
{
	virtual void PostLoad();		
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);

    virtual void UpdateSplineComponents();	
}

/** Clear any static mesh assigned to this spline actor */
native function ClearLoftMesh();

/** Quick function that updates params/positions of the spline mesh components  */
native function UpdateSplineParams();

defaultproperties
{
	WorldXDir=(X=1)
	Offset=(X=0.f,Y=0.f)

	ScaleX=1.0
	ScaleY=1.0

	bAcceptsLights=TRUE
	
	bSmoothInterpRollAndScale=TRUE
	
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.Spline.T_Loft_Spline'
	End Object

	bEdShouldSnap=true
	bStatic=true
	bMovable=false
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=true
	bGameRelevant=true
	bCollideWhenPlacing=false	
}