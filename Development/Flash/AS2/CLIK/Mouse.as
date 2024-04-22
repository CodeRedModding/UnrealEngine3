/**************************************************************************

Filename    :   Mouse.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

/**
 * This intrinsic class replaces the built-in intrinsic class, and adds the Scaleform GFx methods and Properties of the Mouse class, making the GFx properties compile-safe so they can be typed using dot-notation instead of bracket-access.
 */
 
import flash.geom.Point;

intrinsic class Mouse {
	
	static function addListener(listener:Object):Void;
	static function hide():Number;
	static function removeListener(listener:Object):Boolean;
	static function show():Number;
	
	// GFx Extensions
	static var HAND:Number;
	static var ARROW:Number;
	static var IBEAM:Number;
	static var LEFT:Number;
	static var RIGHT:Number;
	static var MIDDLE:Number;
	static var mouseIndex:Number;
	static function getButtonsState(mouseIndex:Number):Number;
	static function getTopMostEntity(arg1:Object,arg2:Number,arg3:Boolean):Object;
	static function getPosition(mouseIndex:Number):Point;
	static function setCursorType(cursorType:Number,mouseIndex:Number):Void;
	
}