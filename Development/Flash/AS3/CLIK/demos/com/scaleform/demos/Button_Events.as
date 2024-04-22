/**************************************************************************

Filename    :   Button_Events.as

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
    import flash.events.FocusEvent;
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
    import flash.utils.Timer;    
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.Button;
    
    public class Button_Events extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        
    // UI Elements:
        /** Reference to the first Button on the left whose MouseEvents will be appended to the EventLog textField. */
        public var Button1:Button;
        /** Reference to the second Button on the left whose FocusEvents will be appended to the EventLog textField. */
        public var Button2:Button;
        /** Reference to the third Button on the left whose ButtonEvents will be appended to the EventLog textField. */
        public var Button3:Button;  
        
        /** Reference to the textField that will be used as our event log. All events will be appended to this textField. */
        public var EventLog:TextField;
        
    // Initialization:
        public function Button_Events() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Add listeners that will update the event log for relevant MouseEvents to Button1.
            Button1.addEventListener(MouseEvent.CLICK, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.DOUBLE_CLICK, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.MOUSE_OVER, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.MOUSE_DOWN, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.MOUSE_UP, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.MOUSE_OUT, handleMouseEvent, false, 0, true);
            Button1.addEventListener(MouseEvent.MOUSE_WHEEL, handleMouseEvent, false, 0, true);            
            
            // Add listeners that will update the event log for relevant FocusEvents to Button2.
            Button2.addEventListener(FocusEvent.FOCUS_IN, handleFocusEvent, false, 0, true);
            Button2.addEventListener(FocusEvent.FOCUS_OUT, handleFocusEvent, false, 0, true);
                        
            // Add listeners that will update the event log for relevant ButtonEvents to Button3.
            // Button3.addEventListener(ButtonEvent.BUTTON_REPEAT, handleButtonEvent, false, 0, true);
            Button3.addEventListener(ButtonEvent.PRESS, handleButtonEvent, false, 0, true);
            Button3.addEventListener(ButtonEvent.CLICK, handleButtonEvent, false, 0, true);
            Button3.addEventListener(ButtonEvent.DRAG_OVER, handleButtonEvent, false, 0, true);
            Button3.addEventListener(ButtonEvent.DRAG_OUT, handleButtonEvent, false, 0, true);
            Button3.addEventListener(ButtonEvent.RELEASE_OUTSIDE, handleButtonEvent, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }        
        
        /** Helper function to retrieve the number of elapsed seconds since the Flash runtime VM started. */
        protected function getElapsedSeconds():Number { return Math.round(flash.utils.getTimer() / 1000); }
        
        /** Handler for MouseEvents. Appends the event to the log with a timestamp. */
        protected function handleMouseEvent( event:Event ):void {
            EventLog.appendText("\n[" + getElapsedSeconds() +" sec] MouseEvent: " + event.type);
            updateEventLog();
        }
        
        /** Handler for FocusEvents. Appends the event to the log with a timestamp. */
        protected function handleFocusEvent( event:Event ):void {
            EventLog.appendText("\n[" + getElapsedSeconds() +" sec] FocusEvent: " + event.type);
            updateEventLog();
        }
        
        /** Handler for ButtonEvents. Appends the event to the log with a timestamp. */
        protected function handleButtonEvent( event:Event ):void {
            EventLog.appendText("\n[" + getElapsedSeconds() +" sec] ButtonEvent: " + event.type);
            updateEventLog();
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