/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class of all Mobile sequence events.  
 */
class SeqEvent_HudRender extends SequenceEvent
	native
	abstract;

/** List of objects to call the handler function on */
var() array<Object> Targets;
var(HUD) bool bIsActive;

/** This is the scale factor you are authoring for. 2.0 is useful for Retina display resolution (960x640), 1.0 for iPads and older iPhones */
var(HUD) float AuthoredGlobalScale;

/**
 * Whenever a SeqEvent_MobileBase sequence is created, it needs to find the PlayerInput that is assoicated with it and 
 * add it'self to the list of Kismet sequences looking for input 
 */
event RegisterEvent()
{
	local int i;
	local GamePlayerController GPC;
	local MobileHUD TargetHud;

	for (i=0;i<Targets.Length;i++)
	{
		GPC = GamePlayerController(Targets[i]);
		if (GPC != none)
		{
			TargetHud = MobileHud(GPC.MyHud);
			if (TargetHud != none)
			{
				TargetHud.AddKismetRenderEvent(self);
				break;
			}
		}
	}
}

/** 
 * Perform the actual rendering
 */
function Render(Canvas TargetCanvas, Hud TargetHud)
{
}


defaultproperties
{
	AuthoredGlobalScale=2.0
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Active",PropertyName=bIsActive,MaxVars=1)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",PropertyName=Targets)
}