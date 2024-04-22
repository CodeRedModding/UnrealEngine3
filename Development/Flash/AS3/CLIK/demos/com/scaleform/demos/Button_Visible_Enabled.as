/**************************************************************************

Filename    :   Button_Visible_Enabled.as

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
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.Button;
    
    public class Button_Visible_Enabled extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        
    // UI Elements:
        /** Reference to the first Button who's MouseEvents will be traced to the log.  */
        public var Button1:Button;
        /** Reference to the first Button who's ButtonEvents will be traced to the log.  */
        public var Button2:Button;
        /** Reference to the first Button who's FocusEvents will be traced to the log.  */
        public var TargetButton:Button;
        
    // Initialization:
        public function Button_Visible_Enabled() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            Button1.addEventListener(ButtonEvent.CLICK, toggleTargetButtonEnabled, false, 0, true);
            Button2.addEventListener(ButtonEvent.CLICK, toggleTargetButtonVisible, false, 0, true);
            
            // Assign listeners for Click, FocusIn, and FocusOut to show how these events are affected by "visible" and "enabled".
            TargetButton.addEventListener(ButtonEvent.CLICK, traceTargetButtonEvent, false, 0, true);
            TargetButton.addEventListener(FocusEvent.FOCUS_IN, traceTargetButtonEvent, false, 0, true);
            TargetButton.addEventListener(FocusEvent.FOCUS_OUT, traceTargetButtonEvent, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        /** Traces a Click, FocusIn, or FocusOut event dispatched by TargetButton to the log. */
        protected function traceTargetButtonEvent( event:Event ):void {
            trace("TargetButton dispatched a " + event.type + " event!");
        }
        
        protected function toggleTargetButtonEnabled( event:Event ):void {
            TargetButton.enabled = !TargetButton.enabled;
            trace("TargetButton.enabled toggled to: " + TargetButton.enabled);
        }
        
        protected function toggleTargetButtonVisible( event:Event ):void {
            TargetButton.visible = !TargetButton.visible;
            trace("TargetButton.visible toggled to: " + TargetButton.visible);
        }
    }
}