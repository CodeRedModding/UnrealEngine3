
/**
 * AnimNotify_PawnMaterialParam
 * 
 * Control MaterialInstanceConstant Scalar parameters through AnimNotifies
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNotify_PawnMaterialParam extends AnimNotify_Scripted
	native(Anim);

var() Array<ScalarParameterInterpStruct> ScalarParameterInterpArray;

cpptext
{
	virtual FString GetEditorComment() { return TEXT("MatParam"); }
}

event Notify(Actor Owner, AnimNodeSequence AnimSeqInstigator)
{
	local Pawn P;
	local INT i;
	local ScalarParameterInterpStruct ScalarParam;

	P = Pawn(Owner);
	if( P != None )
	{
		for(i=0; i<ScalarParameterInterpArray.Length; i++)
		{
			ScalarParam = ScalarParameterInterpArray[i];
			P.SetScalarParameterInterp(ScalarParam);
		}
	}
}

