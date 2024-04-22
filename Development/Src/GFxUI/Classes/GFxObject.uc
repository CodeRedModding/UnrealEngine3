/**********************************************************************

Filename    :   GFxObject.uc
Content     :   Unreal Scaleform GFx integration

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

GFxObject is GFxValue in Scaleform code

**********************************************************************/

class GFxObject extends Object within GFxMoviePlayer
	dependsOn(GFxMoviePlayer)
    native;

/** Stores reference information for the GFx-side GFxValue that this GFxObject refers to */
var const private native int Value[12];

/** Struct for storing properties of display objects for easy and quick manipulation at runtime using the Complex Object Interface */
struct native ASDisplayInfo
{
	var() float  X, Y, Z;
	var() float  Rotation;
	var() float  XRotation, YRotation;
	var() float  XScale, YScale, ZScale;
	var() float  Alpha;
	var() bool   Visible;
	var() bool   hasX, hasY, hasZ, hasRotation, hasXRotation, hasYRotation, hasXScale, hasYScale, hasZScale, hasAlpha, hasVisible;
};

/** Struct for storing color transformation information for manipulation using the Complex Object Interface */
struct native ASColorTransform
{
	var() LinearColor multiply;
	var() LinearColor add;
	
	structdefaultproperties
	{
		multiply=(R=1,G=1,B=1,A=1)
		add=(R=0,G=0,B=0,A=0)
	}
};

/** Array of WidgetBindings for widgets that are forwarded to this widget for their WidgetInitialized() callback.  See GFxMoviePlayer's WidgetBindings for details */
var array<GFxWidgetBinding> SubWidgetBindings;

cpptext
{
	virtual void Clear();
	virtual void BeginDestroy();

	void SetValue(void* GFxObject);
	
	void SetDisplayMatrix(const FMatrix& m);
	void SetDisplayMatrix3D(const FMatrix& m);
	void SetElementDisplayMatrix(INT Index, const FMatrix& m);
}

/**
 *  Accessors for ActionScript / GFx Objects
 *  
 *  If you know the type of the variable or object you're accessing, it is best to use one of the type specific accessor functions, as they are significantly faster.
 *  Avoid using the slower ASValue functions if possible.
 */
native final function ASValue Get(string Member);
native final function bool GetBool(string Member);
native final function float GetFloat(string Member);
native final function int GetInt(string Member);
native final function string GetString(string Member);
/** 
 *  Returns a GFxObject for the specified member.  If the type parameter is specified, the returned object will be of the specified class.  Note the return value is
 *  not coerced though, so if you specify a type, you must manually cast the result
 */
native final function GFxObject GetObject(string Member, optional class<GFxObject> type = class'GFxObject');

native final function Set(string Member, ASValue Arg);
native final function SetBool(string Member, bool b);
native final function SetFloat(string Member, float f);
native final function SetInt(string Member, int i);
/**
 * Set the field specified by 'member' on this object.
 * 
 * @param Member      The member of set object to set; e.g. "text" or "htmlText" or "foo"
 * @param s           Value of the string that is to be assigned to the member
 * @param InContext   The TranslationContext to use when resolving any tags enocuntered in the text.
 */
native final function SetString(string Member, string s, optional TranslationContext InContext);
native final function SetObject(string Member, GFxObject val);
native final function SetFunction(string Member, Object context, name fname);

/**
 * Translate a string for handling markup
 *
 * @param StringToTranslate - The text to set.
 * @param TranslationContext - The TranslationContext to use when resolving any tags enocountered in the text.
 */
native static function String TranslateString(String StringToTranslate, optional TranslationContext InContext);

/**
 *  Complex Object Interface functions
 *  
 *  These functions are the preferred way modify the parameters of display objects (i.e. widgets).  When possible, use these functions to view and change the display parameters of widgets
 *  over setting the parameters individually via Set() functions
 */
native final function ASDisplayInfo GetDisplayInfo();
native final function bool GetPosition(out float x, out float y);
native final function ASColorTransform GetColorTransform();
native final function Matrix GetDisplayMatrix();
native final function Matrix GetDisplayMatrix3D();
native final function SetDisplayInfo(ASDisplayInfo d);
native final function SetPosition(float x, float y);
native final function SetColorTransform(ASColorTransform cxform);
native noexport final function SetDisplayMatrix(Matrix m);
native noexport final function SetDisplayMatrix3D(Matrix m);

/** Toggles visibility of this object, if it is a display object */
native final function SetVisible(bool visible);

/** Text field accessor functions */
native final function string GetText();
/**
 * Set the text field on this object.
 *
 * @param text        The text to set.
 * @param InContext   The TranslationContext to use when resolving any tags enocuntered in the text.
 */
native final function SetText(coerce string text, optional TranslationContext InContext);

/**
 *  Array accessor functions
 *  
 *  As with the normal member accessor functions, it is always preferable to use the accessor for the specific type, rather than the generic GetElement() / SetElement()
 *  functions.
 */
native final function ASValue GetElement(int index);
native final function GFxObject GetElementObject(int index, optional class<GFxObject> type = class'GFxObject');
native final function bool GetElementBool(int index);
native final function float GetElementFloat(int index);
native final function int GetElementInt(int index);
native final function string GetElementString(int index);
native final function SetElement(int index, ASValue Arg);
native final function SetElementObject(int index, GFxObject val);
native final function SetElementBool(int index, bool b);
native final function SetElementFloat(int index, float f);
native final function SetElementInt(int index, int i);
native final function SetElementString(int index, string s);

/** Array accessors for display objects */
native final function ASDisplayInfo GetElementDisplayInfo(int index);
native final function Matrix GetElementDisplayMatrix(int index);
native final function SetElementDisplayInfo(int index, ASDisplayInfo d);
native noexport final function SetElementDisplayMatrix(int index, Matrix m);
native final function SetElementVisible(int index, bool visible);
native final function SetElementPosition(int index, float x, float y);
native final function SetElementColorTransform(int index, ASColorTransform cxform);

/** Array accessors for general element types */
native final function ASValue GetElementMember(int index, string Member);
native final function GFxObject GetElementMemberObject(int index, string Member, optional class<GFxObject> type = class'GFxObject');
native final function bool GetElementMemberBool(int index, string Member);
native final function float GetElementMemberFloat(int index, string Member);
native final function int GetElementMemberInt(int index, string Member);
native final function string GetElementMemberString(int index, string Member);
native final function SetElementMember(int index, string Member, ASValue arg);
native final function SetElementMemberObject(int index, string Member, GFxObject val);
native final function SetElementMemberBool(int index, string Member, bool b);
native final function SetElementMemberFloat(int index, string Member, float f);
native final function SetElementMemberInt(int index, string Member, int i);
native final function SetElementMemberString(int index, string Member, string s);

/**
 *  Function property setters
 *  
 *  Use these functions to set function properties in ActionScript to UnrealScript delegates, using the delegate from the calling UnrealScript function.
 *  This is a useful method for getting callbacks from ActionScript into UnrealScript.
 *  
 *  Examples:       
 *      // Sets a ActionScript function property "onClick" to call back to the delegate specified in f
 *      function SetOnClick(delegate<OnClick> f)
 *      {
 *          ActionScriptSetFunction("onClick");
 *      }
 *      
 *      // Sets OtherObject's "onClick" function object to the delegate specified in f
 *      function SetOnEvent(GFxObject OtherObject, delegate<OnEvent> f)
 *      {
 *          ActionScriptSetFunctionOn(OtherObject, "onClick");
 *      }
 */
protected native noexport final function ActionScriptSetFunction(string Member);
protected native noexport final function ActionScriptSetFunctionOn(GFxObject target, string Member);

/** 
 *  Calls an ActionScript function on this GFxObject, with the values from the args array as its parameters.  This is slower than creating a wrapper function to call the ActionScript method
 *  using one of the ActionScript*() methods below, but does not require a subclass of GFxObject to implement.  Use this for one-off functions, or functions with variable length arguments
 */
native final function ASValue Invoke(string Member, array<ASValue> args);

/**
 *  ActionScript function call wrappers
 *  
 *  These functions, when called from within a UnrealScript function, invoke an ActionScript function with the specified method name, with the parameters of the wrapping UnrealScript 
 *  function.  This is the preferred method for calling ActionScript functions from UnrealScript, as it is faster than Invoke, with less overhead.
 *  
 *  Example:    To call the following ActionScript function from UnrealScript -
 *  
 *                  function MyActionScriptFunction(Param1:String, Param2:Number, Param3:Object):Void;
 *                  
 *              Use the following UnrealScript code -
 *              
 *                  function CallMyActionScriptFunction(string Param1, float Param2, GFxObject Param3)
 *                  {
 *                      ActionScriptVoid("MyActionScriptFunction");
 *                  }
 */
native noexport final function ActionScriptVoid(string method);
native noexport final function int ActionScriptInt(string method);
native noexport final function float ActionScriptFloat(string method);
native noexport final function string ActionScriptString(string method);
native noexport final function GFxObject ActionScriptObject(string path);
native noexport final function array<GFxObject> ActionScriptArray(string path);

/**
 *  Movie flow control functions
 *  
 *  These functions are used for controlling movie playback and skipping around on the timeline.  The string functions take a (case-sensitive) frame label to jump to,
 *  while the integer functions jump to a frame number.  If the label or frame isn't found, a warning will be sent to the DevGFxUI logging channel, but the movie can still
 *  potentially jump frames (usually to the end of the timeline for the object)
 */
native final function GotoAndPlay(string frame);
native final function GotoAndPlayI(int frame);
native final function GotoAndStop(string frame);
native final function GotoAndStopI(int frame);

/** Creates an empty MovieClip in the movie.  This can then be manipulated like any other MovieClip using the above functions */
native final function GFxObject CreateEmptyMovieClip(string instancename, optional int depth = -1, optional class<GFxObject> type = class'GFxObject');
/** Attaches a symbol to specified movie instance.  If no instance is found in this object's scope with the InstanceName, a new instance is created and returned */
native final function GFxObject AttachMovie(string symbolname, string instancename, optional int depth = -1, optional class<GFxObject> type = class'GFxObject');

/** 
 *  Callback when a child widget is initialized within the path bound to this widget via GFxMoviePlayer::SetWidgetPathBinding().  Allows for GFxObject subclasses that encapsulate
 *  functionality to handle their own initialization for child widgets, instead of the GFxMoviePlayer.  Returns TRUE if the widget was handled, FALSE if not.   
 */
event bool WidgetInitialized(name WidgetName, name WidgetPath, GFxObject Widget);

/**
 *  Callback when a child widget with enableInitCallback set to TRUE is unloaded within the path bound to this widget via GFxMoviePlayer::SetWidgetPathBinding().  
 *  Returns TRUE if the widget was handled, FALSE if not.
 */
event bool WidgetUnloaded(name WidgetName, name WidgetPath, GFxObject Widget);
