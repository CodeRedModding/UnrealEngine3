/**********************************************************************

Filename    :   GFxClikWidget.uc
Content     :   Unreal Scaleform GFx integration

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

class GFxClikWidget extends GFxObject;

// Common event data from CLIK components. You can add or remove members from this struct depending on
// which properties you use. If you don't need Target, remove it as storing it performs memory allocation.

struct EventData
{
    var  GFxObject _this;
	var  GFxObject target;
	var  string   type;
	var  int      data;
	var  int      mouseIndex;
	var  int      button;

	var  int      index;
	var  int      lastIndex;
};

delegate EventListener(EventData data);

/** 
 *  Adds an event listener to the EventDispatcher for events on CLIK widgets.  To add an event, pass in the name of the CLIK event, prefaced with "CLIK_" (e.g. 'CLIK_press')
 *  For a full list of valid CLIK events, see EventTypes.as in the CLIK widget source.
 */
function AddEventListener(name type, delegate<EventListener> listener, bool useCapture=false, int listenerPriority=0, bool useWeakReference=false)
{
	local GFxObject o, funcobj;
	local string TypeString;

	TypeString = GetEventStringFromTypename(type);
	if( TypeString != "" )
	{
		o = Outer.CreateObject("Object");
		SetListener(o,TypeString,listener);
		if (GetAVMVersion() == 2)
		{
			funcobj = o.GetObject(TypeString);
			AS3AddEventListener(TypeString, funcobj, useCapture, listenerPriority, useWeakReference);
		}
		else
			ASAddEventListener(TypeString,o,TypeString);
	}
}

function RemoveAllEventListeners(string event)
{
	ASRemoveAllEventListeners(event);
}

/** Checks for a "CLIK_" prefix to the name (to avoid name collisions), and returns the string equivalent with the prefix removed.  If prefix isn't found, errors and returns a null string */
private final function string GetEventStringFromTypename(name Typename)
{
 	local string Typestring;
 
 	Typestring = string(Typename);
 	
	// Check for the "CLIK_" prefix on the name, to make sure it's in our "namespace"
 	if( InStr(Typestring, "CLIK_") >= 0 )
 	{
 		return Split(Typestring, "CLIK_", TRUE);
 	}
	else
	{
		`log("Improper CLIK callback name!  All callback names must start with CLIK_ (e.g. 'CLIK_press')");
		return "";
	}
}

private function SetListener(GFxObject o, string member, delegate<EventListener> listener) { ActionScriptSetFunctionOn(o,member); }
private function ASAddEventListener(string type, GFxObject o, string func) { ActionScriptVoid("addEventListener"); }
private function AS3AddEventListener(string type, GFxObject o, bool useCapture=false, int listenerPriority=0, bool useWeakReference=true) { ActionScriptVoid("addEventListener"); }
private function ASRemoveAllEventListeners(string event) { ActionScriptVoid("removeAllEventListeners"); }