/**************************************************************************

Filename    :   TextFieldEx.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package scaleform.gfx
{
   import flash.text.TextField

   public final class TextFieldEx
   {   	
        public static const VALIGN_NONE:String          = "none";        
        public static const VALIGN_TOP:String 			= "top";
		public static const VALIGN_CENTER:String 		= "center";
		public static const VALIGN_BOTTOM:String 		= "bottom";
        
        public static const TEXTAUTOSZ_NONE:String      = "none";
        public static const TEXTAUTOSZ_SHRINK:String    = "shrink";
        public static const TEXTAUTOSZ_FIT:String       = "fit";
	   
		// The flash.text.TextField specification includes appendText.
		static public function appendHtml(textField:TextField, newHtml:String) : void  { }
        
		static public function setIMEEnabled(textField:TextField, isEnabled:Boolean): void { }

        // Sets the vertical alignment of the text inside the textfield.
        // Valid values are "none", "top" (same as none), "bottom" and "center"
		static public function setVerticalAlign(textField:TextField, valign:String) : void { }
        static public function getVerticalAlign(textField:TextField) : String { return "none"; }
	  
        // Enables automatic resizing of the text's font size to shrink or fit the textfield.
        // Valid values are "none", "shrink", and "fit"
        static public function setTextAutoSize(textField:TextField, autoSz:String) : void { }
        static public function getTextAutoSize(textField:TextField) : String { return "none"; }
   }
}
