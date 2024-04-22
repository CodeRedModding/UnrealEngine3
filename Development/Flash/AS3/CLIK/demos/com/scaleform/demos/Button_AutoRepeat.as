/**************************************************************************

Filename    :   Button_AutoRepeat.as

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
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.Button;
    
    public class Button_AutoRepeat extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        protected var count:uint;
        
    // UI Elements:
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.NONE. */
        public var Button1:Button;
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.LEFT. */
        public var Button2:Button;
        
        /** Reference to the textField that is incremented with each CLICK / PRESS event. */
        public var txt_Counter:TextField;
        
    // Initialization:
        public function Button_AutoRepeat() {
            super();
            
            count = 0;
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            Button1.addEventListener(ButtonEvent.CLICK, incrementCounter, false, 0, true);
            Button2.addEventListener(ButtonEvent.CLICK, incrementCounter, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        protected function incrementCounter(event:Event):void {            
            txt_Counter.text = (++count) + "";
        }
    }	
}