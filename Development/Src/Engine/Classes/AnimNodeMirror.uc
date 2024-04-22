/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNodeMirror extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

var()	bool	bEnableMirroring;

cpptext
{
	virtual void GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);
}

defaultproperties
{
	Children(0)=(Name="Child",Weight=1.0)
	bFixNumChildren=true

	bEnableMirroring=true

	CategoryDesc = "Mirror"
}
