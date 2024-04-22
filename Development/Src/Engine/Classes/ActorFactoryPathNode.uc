/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryPathNode extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

defaultproperties
{
	MenuName="Add PathNode"
	NewActorClass=class'Engine.PathNode'
	bShowInEditorQuickMenu=true
}
