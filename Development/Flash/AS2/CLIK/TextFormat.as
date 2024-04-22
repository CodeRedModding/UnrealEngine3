/**************************************************************************

Filename    :   TextFormat.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

/**
 * This intrinsic class replaces the built-in intrinsic class, and adds the Scaleform GFx methods and Properties of the TextFormat class, making the GFx properties compile-safe so they can be typed using dot-notation instead of bracket-access.
 */
intrinsic class TextFormat {
	
	var align:String;
	var blockIndent:Number;
	var bold:Boolean;
	var bullet:Boolean;
	var color:Number;
	var font:String;
	var indent:Number;
	var italic:Boolean;
	var kerning:Boolean;
	var leading:Number;
	var leftMargin:Number;
	var letterSpacing:Number;
	var rightMargin:Number;
	var size:Number;
	var tabStops:Array;
	var target:String;
	var underline:Boolean;
	var url:String;

	function TextFormat(font:String,size:Number,color:Number,
                    	bold:Boolean,italic:Boolean,underline:Boolean,
                    	url:String,target:String,align:String,
                    	leftMargin:Number,rightMargin:Number,indent:Number,leading:Number);

	function getTextExtent(text:String, width:Number):Object;
	
	// GFx Extensions
	var alpha:Number;
}