/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryCoverLink extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

defaultproperties
{
	MenuName="Add CoverLink"
	NewActorClass=class'Engine.CoverLink'
	bShowInEditorQuickMenu=true
}
