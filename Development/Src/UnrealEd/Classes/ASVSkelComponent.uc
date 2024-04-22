/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ASVSkelComponent extends SkeletalMeshComponent
	transient
	native;

var native const pointer	AnimSetViewerPtr;

/** If TRUE, render a wireframe skeleton of the mesh animated with the raw (uncompressed) animation data. */
var bool		bRenderRawSkeleton;
/** If TRUE, render softbody tetrahedra */
var bool		bShowSoftBodyTetra;

/** Holds onto the bone color that will be used to render the bones of its skeletal mesh */
var Color		BoneColor;

/** If TRUE then the skeletal mesh associated with the component is drawn. */
var bool		bDrawMesh;

/** Bone influences viewing */
var transient bool bDrawBoneInfluences;

/** Color render mode enum value - 0 - none, 1 - tangent, 2 - normal, 3 - mirror, 4 - bone weighting */
var native transient int   ColorRenderMode;
/** Array of bone's to render bone weights for */
var transient array<int> BonesOfInterest;
/** Array of materials to restore when not rendering blend weights */
var transient array<MaterialInterface> SkelMaterials;

cpptext
{
	// UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

    /**
     * Function that returns whether or not CPU skinning should be applied
     * Allows the editor to override the skinning state for editor tools
     */
	virtual UBOOL ShouldCPUSkin();

	/** 
	 * Function to operate on mesh object after its created, 
	 * but before it's attached.
	 * @param MeshObject - Mesh Object owned by this component
	 */
	virtual void PostInitMeshObject(class FSkeletalMeshObject* MeshObject);

	/**
	 * Update material information depending on color render mode 
	 * Refresh/replace materials 
	 */
	void ApplyColorRenderMode(INT InColorRenderMode);
}

defaultproperties
{
	bDrawMesh = true
	bShowSoftBodyTetra = true
	BoneColor = (R=230, G=230, B=255, A=255)
	bUseOnePassLightingOnTranslucency=true
}
