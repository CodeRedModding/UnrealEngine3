//! @file SubstanceAirGraphActor.uc
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief Utility class designed to allow you to connect a FGraphInstance to a Matinee action.

class SubstanceAirGraphActor extends Actor
	native(Actor)
	placeable
	hidecategories(Movement)
	hidecategories(Advanced)
	hidecategories(Collision)
	hidecategories(Display)
	hidecategories(Actor)
	hidecategories(Attachment);

/** Pointer to SubstanceAirGraphInstance that we want to control parameters of using Matinee. */
var()	SubstanceAirGraphInstance	GraphInst;

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Actor'
		HiddenGame=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Materials"
	End Object
	Components.Add(Sprite)

	bNoDelete=true
}
