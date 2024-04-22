/**************************************************************************

Filename    :   ScrollBar_TrackMode.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

﻿package com.scaleform.demos {
        
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.MouseEvent;
    import flash.events.KeyboardEvent;
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
    import flash.utils.getDefinitionByName;
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollIndicator;
    
    public class ScrollBar_TrackMode extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to the first ScrollBar for the textField that has its scrollMode set to "scrollPage". */
        public var ScrollBar1:ScrollBar;        
        /** Reference to the second ScrollBar for the textField that has its scrollMode set to "scrollToCursor". */
        public var ScrollBar2:ScrollBar;        
        
        /** Reference to the textField that the ScrollBars manipulate. */
        public var textField:TextField;
        
                
    // Initialization:
        public function ScrollBar_TrackMode() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Clear any existing text from the textField.
            textField.text = "";
            for (var i:uint = 0; i < 25; i++) {
                textField.appendText(i + "\n"); // Add enough text to the textField so that we have something to scroll toward.
            }
        }
        
        override protected function draw():void {
            super.draw();
        }
    }	
}