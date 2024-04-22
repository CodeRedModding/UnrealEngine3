/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class AnimNodeEditInfo_AimOffset extends AnimNodeEditInfo
	native;
	
	
var native const pointer	EditWindow{class WxAnimAimOffsetEditor};	
var	AnimNodeAimOffset		EditNode;

cpptext
{
	virtual void OnDoubleClickNode(UAnimNode* InNode, class WxAnimTreeEditor* InEditor);
	virtual void OnCloseAnimTreeEditor();
	virtual UBOOL ShouldDrawWidget();
	virtual UBOOL IsRotationWidget();
	virtual FMatrix GetWidgetTM();
	virtual void HandleWidgetDrag(const FQuat& DeltaQuat, const FVector& DeltaTranslate);
	virtual void Draw3DInfo(const FSceneView* View, FPrimitiveDrawInterface* PDI);
}

defaultproperties
{
	AnimNodeClass=class'Engine.AnimNodeAimOffset'
}