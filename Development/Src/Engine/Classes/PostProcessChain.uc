/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PostProcessChain extends Object
	native;

/** Post process effects active in this chain. Rendered in order */
var array<PostProcessEffect> Effects;

/**
 * Returns the index of the named post process effect, None if not found.
 */
final function PostProcessEffect FindPostProcessEffect(name EffectName)
{
	local int Idx;
	for (Idx = 0; Idx < Effects.Length; Idx++)
	{
		if (Effects[Idx] != None && Effects[Idx].EffectName == EffectName)
		{
			return Effects[Idx];
		}
	}
	// not found
	return None;
}

