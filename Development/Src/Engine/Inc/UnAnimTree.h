/*=============================================================================
UnAnimTree.h
Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by James Golding  

=============================================================================*/

//
// Defines.
//

#define ZERO_ANIMWEIGHT_THRESH (0.00001f)  // Below this weight threshold, animations won't be blended in.

FColor AnimDebugColorForWeight(FLOAT Weight);

//
// Animation tree element definitons and helper structs.
//

struct FBoneAtom
{
	FQuat	Rotation;
	FVector	Translation;
	FVector Scale;

	FBoneAtom() {};

	FBoneAtom(FQuat InRotation, FVector InTranslation, FVector InScale) : 
		Rotation(InRotation), 
		Translation(InTranslation), 
		Scale(InScale) {};

	void Blend(const FBoneAtom& Atom1, const FBoneAtom& Atom2, FLOAT Alpha);
	void DebugPrint();

	FORCEINLINE FBoneAtom operator+(const FBoneAtom& Atom)
	{
		return FBoneAtom(Rotation + Atom.Rotation, Translation + Atom.Translation, Scale + Atom.Scale);
	}

	FORCEINLINE FBoneAtom operator+=(const FBoneAtom& Atom)
	{
		Translation += Atom.Translation;

		Rotation.X += Atom.Rotation.X;
		Rotation.Y += Atom.Rotation.Y;
		Rotation.Z += Atom.Rotation.Z;
		Rotation.W += Atom.Rotation.W;

		Scale += Atom.Scale;

		return *this;
	}

	FORCEINLINE FBoneAtom operator*(FLOAT Mult)
	{
		return FBoneAtom(Rotation * Mult, Translation * Mult, Scale * Mult);
	}
};