
/**
 * AnimNodeBlendByBase.uc
 * Looks at the base of the Pawn that owns this node and blends accordingly.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeBlendByBase extends AnimNodeBlendList
		native(Anim);

cpptext
{
	virtual	void TickAnim(FLOAT DeltaSeconds);
}

enum EBaseBlendType
{
	BBT_ByActorTag,
	BBT_ByActorClass,
};

/** Type of comparison to do */
var() EBaseBlendType	Type;
/** Actor tag that will match the base */
var() Name				ActorTag;
/** Actor class that will match the base */
var() class<Actor>		ActorClass<AllowAbstract>;
/** Duration of blend */
var() float				BlendTime;
/** Cached Base Actor */
var	transient Actor		CachedBase;

defaultproperties
{
	bFixNumChildren=TRUE
	Children(0)=(Name="Normal")
	Children(1)=(Name="Based")
	BlendTime=0.2f
}
