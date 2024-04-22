/**************************************************************************

Filename    :   Button_AutoSize.as

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
    
    public class Button_AutoSize extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        protected var labelCounter:uint = 0;
        
    // UI Elements:
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.NONE. */
        public var Button1:Button;
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.LEFT. */
        public var Button2:Button;
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.CENTER. */
        public var Button3:Button;
        /** Reference to the first Button with autoSize set to TextFieldAutoSize.RIGHT. */
        public var Button4:Button;
        
        public var ToggleAutoSizeBtn:Button;
        public var ChangeLabelBtn:Button;
        public var TargetButton:Button;
        
        public var txt_autoSize:TextField;
        public var txt_labelState:TextField;
        
    // Initialization:
        public function Button_AutoSize() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            ToggleAutoSizeBtn.addEventListener(ButtonEvent.CLICK, toggleAutoSize, false, 0, true);
            ChangeLabelBtn.addEventListener(ButtonEvent.CLICK, changeLabel, false, 0, true);
            
            TargetButton.label = "Target ";
            TargetButton.validateNow();
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        protected function toggleAutoSize( event:Event ):void {
            if (event.target.selected) { 
                TargetButton.autoSize = TextFieldAutoSize.LEFT;
                txt_autoSize.text = "TargetButton.autoSize: LEFT";
            }
            else {
                TargetButton.autoSize = TextFieldAutoSize.NONE;
                txt_autoSize.text = "TargetButton.autoSize: NONE";
            }
            
            TargetButton.validateNow();
        }
        
        protected function changeLabel( event:Event ):void {
            if (labelCounter < 13) {
                TargetButton.label = (TargetButton.label + labelCounter++);
            }
            else {
                TargetButton.label = "Target ";
                labelCounter = 0;
            }
            
            txt_labelState.text = "TargetButton.label: " + TargetButton.label;
            TargetButton.validateNow();
        }
    }
}