/**************************************************************************

Filename    :   TextInput_MainDemo.as

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
    
    public class TextInput_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to the first TextInput with default settings. */
        public var TextInput1:TextInput;
        /** Reference to the Button that submits the text for TextInput1. */
        public var Button1:Button;
        
        /** Reference to the second TextInput that is set for "displayAsPassword" and "actAsButton". */
        public var TextInput2:TextInput;
        /** Reference to the Button that submits the text for TextInput2. */
        public var Button2:Button;
        
        /** Reference to the EventLog where the text from the TextInputs is added. */
        public var EventLog:TextField;        
        
    // Initialization:
        public function TextInput_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Configure Button1.
            Button1.autoSize = TextFieldAutoSize.LEFT;
            Button1.label = "Submit Text";
            // Button1.disableFocus = true;
            Button1.validateNow();
            
            // Configure Button2.
            Button2.autoSize = TextFieldAutoSize.LEFT;
            Button2.label = "Submit Password Text";
            // Button2.disableFocus = true;
            Button2.validateNow();
            
            // Add weak referenced event listeners to Button1 and Button2 that listen for MouseEvent.CLICK.
            Button1.addEventListener( ButtonEvent.CLICK, submitText, false, 0, true);   
            Button2.addEventListener( ButtonEvent.CLICK, submitText, false, 0, true);   
            
            // Add weak referenced event listeners to TextInput1 and TextInput2 that listen for KeyboardEvent.KEY_UP.
            TextInput1.addEventListener( KeyboardEvent.KEY_UP, checkInputKeyForSubmit, false, 0, true);
            TextInput2.addEventListener( KeyboardEvent.KEY_UP, checkInputKeyForSubmit, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        /** Submits the current text of the appropraite TextInput (TextInput1 / TextInput2) to the EventLog. */
        protected function submitText( event:Event ):void {
            var TargetTextInput:TextInput;
            var TargetButton:Button = event.target as Button;
            
            // Submit the text for the appropriate TextInput based on which Button was clicked.
            if ( TargetButton == Button1 ) {
                TargetTextInput = TextInput1;
            } else if ( TargetButton == Button2 ) {
                TargetTextInput = TextInput2;
            }
            
            if (TargetTextInput.text != "") { 
                EventLog.appendText( "\n" + TargetTextInput.text ); // Append the text to the log.
                updateEventLog(); // Update the EventLog(), scrolling it if necessary.
                TargetTextInput.text = ""; // Clear the textField.
            }
        }
        
        /** Checks whether the input key was "Enter". If it was, append the text for the TextInput to the EventLog and clear the TextInput. */
        protected function checkInputKeyForSubmit( event:KeyboardEvent ):void {            
            var TargetTextInput:TextInput = event.target.parent as TextInput; // Retrieve a reference to the textInput.            
            if ( event.keyCode ==  13 ) { 
                EventLog.appendText( "\n" + TargetTextInput.text ); // Append the text to the log.
                updateEventLog(); // Update the EventLog(), scrolling it if necessary.
                trace( TargetTextInput + ".text: " + TargetTextInput.text);
                TargetTextInput.text = ""; // Clear the textField.
            }
        }
        
        /** Scrolls the EventLog textField to its bottom-most line of text. */
        protected function updateEventLog():void {
            // Is the last line of text available the last line currently being displayed?
            if (EventLog.maxScrollV != EventLog.bottomScrollV) {
                // If not, scroll down to the last line of text available.
                EventLog.scrollV = EventLog.maxScrollV;
            }
        }
    }
}