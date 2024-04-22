/**************************************************************************

Filename    :   Stage.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

/**
 * This intrinsic class replaces the built-in intrinsic class, and adds the Scaleform GFx methods and Properties of the Stage class, making the GFx properties compile-safe so they can be typed using dot-notation instead of bracket-access.
 */
import flash.geom.Point;
import flash.geom.Rectangle;

intrinsic class Stage {
	
	static var align:String;
	static var height:Number;
	static var scaleMode:String;
	static var showMenu:Boolean;
	static var width:Number;

	static function addListener(listener:Object):Void;
	static function removeListener(listener:Object):Boolean;
	
	// GFx Extensions
	static var visibleRect:Rectangle;
	static var safeRect:Rectangle;
	static var originalRect:Rectangle;
	
	static function translateToScreen(pt:Object):Point;
}