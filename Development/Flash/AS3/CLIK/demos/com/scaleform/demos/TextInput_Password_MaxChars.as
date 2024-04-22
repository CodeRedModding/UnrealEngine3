/**************************************************************************

Filename    :   TextInput_Password_MaxChars.as

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
    
    public class TextInput_Password_MaxChars extends UIComponent {
        
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
        
        /** Reference to the EventLog where the text from the TextInputs is added. */
        public var EventLog:TextField;  
        
    // Initialization:
        public function TextInput_Password_MaxChars() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Add weak referenced event listeners to Button1 and Button2 that listen for MouseEvent.CLICK.
            Button1.addEventListener( ButtonEvent.CLICK, cancelLogin, false, 0, true);  
            Button2.addEventListener( ButtonEvent.CLICK, submitLogin, false, 0, true);
            
            // Add weak referenced event listeners to TextInput1 and TextInput2 that listen for KeyboardEvent.KEY_UP.
            TextInput1.addEventListener( KeyboardEvent.KEY_UP, checkInputKeyForSubmit, false, 0, true);
            TextInput2.addEventListener( KeyboardEvent.KEY_UP, checkInputKeyForSubmit, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        /** Toggles the "enabled" property of the appropriate TextInput based on the Button that dispatched the event. */
        protected function cancelLogin( event:Event ):void {
            // Clear the TextInputs.
            TextInput1.text = "";
            TextInput2.text = "";
            
            // Append a message to the EventLog that the user canceled the login.
            EventLog.appendText("User canceled login.\n");
            updateEventLog(); // Scroll the EventLog to its bottom-most line of text.
            
            // Reset focus to the first TextInput.
            stage.focus = TextInput1;
        }  
        
        /** Toggles the "enabled" property of the appropriate TextInput based on the Button that dispatched the event. */
        protected function submitLogin( event:Event ):void {            
            submitLoginImpl();
        }
        
        /** Checks whether the input key was "Enter". If it was, submit the login info. */
        protected function checkInputKeyForSubmit( event:KeyboardEvent ):void {            
            var TargetTextInput:TextInput = event.target.parent as TextInput; // Retrieve a reference to the textInput.            
            if ( event.keyCode ==  13 ) { 
                submitLoginImpl();
            }            
        }
        
        /** "Submits" the login information from the TextInputs'. Prints a message to the EventLog and clears the TextInputs. */
        protected function submitLoginImpl():void {
            // Generate the String to be added to the EventLog based on the TextInputs' "text" property.
            var submission:String = "\n\tUsername: " + TextInput1.text + "\n\tPassword: " + TextInput2.text;           
            EventLog.appendText("User submitted login:" + submission + "\n\n"); // Append the text to the log.
            
            // Clear the TextInputs.
            TextInput1.text = "";
            TextInput2.text = "";
            
            updateEventLog(); // Scroll the EventLog to its bottom-most line of text.
            stage.focus = TextInput1; // Reset focus to the first TextInput.
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