/**************************************************************************

Filename    :   TextInput_Editable_Enabled.as

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
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.TextInput;
    
    public class TextInput_Editable_Enabled extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to the first TextInput that is editable. */
        public var TextInput1:TextInput;
        /** Reference to the Button that toggles the "enabled" property of TextInput1. */
        public var Button1:Button;
        
        /** Reference to the second TextInput that is not editable.*/
        public var TextInput2:TextInput;
        /** Reference to the Button that toggles the "enabled" property of TextInput2. */
        public var Button2:Button;
        
    // Initialization:
        public function TextInput_Editable_Enabled() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Configure Button1.
            Button1.autoSize = TextFieldAutoSize.LEFT;            
            Button1.label = "Toggle Enabled";
            // Button1.disableFocus = true;
            Button1.validateNow();
            
            // Configure Button2.
            Button2.autoSize = TextFieldAutoSize.LEFT;            
            Button2.label = "Toggle Enabled";
            // Button2.disableFocus = true;
            Button2.validateNow();
            
            // Add weak referenced event listeners to Button1 and Button2 that listen for MouseEvent.CLICK.
            Button1.addEventListener( ButtonEvent.CLICK, toggleTextInputEditable, false, 0, true);  
            Button2.addEventListener( ButtonEvent.CLICK, toggleTextInputEditable, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        /** Toggles the "enabled" property of the appropriate TextInput based on the Button that dispatched the event. */
        protected function toggleTextInputEditable( event:Event ):void {            
            var TargetTextInput:TextInput;
            var TargetButton:Button = event.target as Button;
            
            // Submit the text for the appropriate TextInput based on which Button was clicked.
            if ( TargetButton == Button1 ) {
                TargetTextInput = TextInput1;
            } else if ( TargetButton == Button2 ) {
                TargetTextInput = TextInput2;
            }
                        
            TargetTextInput.enabled = !TargetTextInput.enabled; // Toggle the "enabled" property of the TextInput.
        }        
    }	
}