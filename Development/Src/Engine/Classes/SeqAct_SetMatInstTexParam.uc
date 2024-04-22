/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetMatInstTexParam extends SequenceAction
	deprecated;

var() MaterialInstanceConstant	MatInst;
var() Texture					NewTexture;
var() Name						ParamName;

defaultproperties
{
	ObjName="Set TextureParam"
	ObjCategory="Material Instance"
	VariableLinks.Empty()
}
