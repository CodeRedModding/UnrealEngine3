/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_Character extends SeqVar_Object
	abstract
	native(Sequence);

cpptext
{
	UObject** GetObjectRef( INT Idx );

	virtual FString GetValueStr()
	{
#if WITH_EDITORONLY_DATA
		return ObjName;
#else
		return FString( TEXT( "" ) );
#endif // WITH_EDITORONLY_DATA
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return FALSE;
	}

#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

/** Pawn class for the character we're looking for */
var class<Pawn> PawnClass;

defaultproperties
{
	ObjCategory="Player"
}
