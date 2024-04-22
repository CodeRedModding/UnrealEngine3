/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialEditorMeshComponent extends StaticMeshComponent
	native;

cpptext
{
protected:
	// ActorComponent interface.
	virtual void Attach();
	virtual void Detach( UBOOL bWillReattach = FALSE );
}

var transient native const pointer	MaterialEditor;
