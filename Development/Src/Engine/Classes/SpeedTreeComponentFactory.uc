/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
 
class SpeedTreeComponentFactory extends PrimitiveComponentFactory
	native(SpeedTree)
	hidecategories(Object)
	collapsecategories
	editinlinenew;

var() SpeedTreeComponent SpeedTreeComponent;

cpptext
{
	virtual UBOOL FactoryIsValid( ) 
	{ 
		return SpeedTreeComponent != NULL && Super::FactoryIsValid( ); 
	}
	virtual UPrimitiveComponent* CreatePrimitiveComponent(UObject* InOuter);
}

defaultproperties
{
	Begin Object Class=SpeedTreeComponent Name=SpeedTreeComponent0
	End Object
	SpeedTreeComponent = SpeedTreeComponent0;
	
	CollideActors		= TRUE
	BlockActors			= TRUE
	BlockZeroExtent		= TRUE
	BlockNonZeroExtent	= TRUE
	BlockRigidBody		= TRUE
}