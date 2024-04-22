/**
 *	AnimNodeEditInfo
 *	Allows you to register extra editor functionality for a specific AnimNode class.
 *	One of each class of these will be instanced for each AnimTreeEditor context.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeEditInfo extends Object
	native
	abstract;
	
var		const class<AnimNode>		AnimNodeClass;

cpptext
{
	virtual void OnDoubleClickNode(UAnimNode* InNode, class WxAnimTreeEditor* InEditor) {}
	virtual void OnCloseAnimTreeEditor() {}
	virtual UBOOL ShouldDrawWidget() { return FALSE; }
	virtual UBOOL IsRotationWidget() { return TRUE; }
	virtual FMatrix GetWidgetTM() { return FMatrix::Identity; }
	virtual void HandleWidgetDrag(const FQuat& DeltaQuat, const FVector& DeltaTranslate) {}
	virtual void Draw3DInfo(const FSceneView* View, FPrimitiveDrawInterface* PDI) {}
}