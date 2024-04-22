/**
 * This is an actor which can be used to change its material based on whether Extreme Content is on or not  
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkeletalMeshActorBasedOnExtremeContent extends SkeletalMeshActor
	native(Anim)
	placeable;


/** This is set so the LD can map specific MaterialIndex on the target to some material **/
struct native SkelMaterialSetterDatum
{
	var() int MaterialIndex;
	var() MaterialInterface TheMaterial;
};

/** The material to use for ExtremeContent (e.g. blood and gore!) **/
var() array<SkelMaterialSetterDatum> ExtremeContent;

/** The material to use for NonExtremeContent (e.g. blackness everywhere there used to be blood and gore) **/
var() array<SkelMaterialSetterDatum> NonExtremeContent;



simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	SetMaterialBasedOnExtremeContent();
}


/** This will set the material of this Actor based on the ExtremeContent setting. **/
simulated function SetMaterialBasedOnExtremeContent()
{
	local int Idx;

	if( WorldInfo.GRI.ShouldShowGore() )
	{
		for( Idx = 0; Idx < ExtremeContent.Length; ++Idx )
		{
			//`log( "SKA ExtremeContent" @ ExtremeContent[Idx].MaterialIndex @ ExtremeContent[Idx].TheMaterial );
			SkeletalMeshComponent.SetMaterial( ExtremeContent[Idx].MaterialIndex, ExtremeContent[Idx].TheMaterial );
		}
	}
	else
	{
		for( Idx = 0; Idx < NonExtremeContent.Length; ++Idx )
		{
			//`log( "SKA NonExtremeContent" @ NonExtremeContent[Idx].MaterialIndex @  NonExtremeContent[Idx].TheMaterial );
			SkeletalMeshComponent.SetMaterial( NonExtremeContent[Idx].MaterialIndex, NonExtremeContent[Idx].TheMaterial );
		}
	}
}



DefaultProperties
{
}
