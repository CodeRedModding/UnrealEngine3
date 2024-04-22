/**************************************************************************

Filename    :   Button_MainDemo.as

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
    import flash.text.TextFieldAutoSize;
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.Button;
    
    public class Button_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        /** Whether the icon is its default color. true if it is, false if it is the alternate color because it was toggled. */
        protected var bIsIconDefaultColor;        
        
    // UI Elements:
        /** Reference to the first Button with a Mouse.CLICK listener that causes the icon to flash. */
        public var Button1:Button;
        /** Reference to the second Button with a ButtonEvent.SELECTED listener that toggles the icon's color. */
        public var Button2:Button;
        /** Reference to the third Button with a Mouse.DOUBLE_CLICK listener that toggles the icon's rotation. */
        public var Button3:Button;
        /** Reference to the icon that is maniuplated byt the three buttons. */
        public var targetIcon:MovieClip;
        
    // Initialization:
        public function Button_MainDemo() {
            super();            
            
            bIsIconDefaultColor = true;
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            Button1.addEventListener(ButtonEvent.CLICK, playIconPingAnimation, false, 0, true);
            Button2.addEventListener(ButtonEvent.CLICK, toggleIconRotation, false, 0, true);           
            
            Button3.doubleClickEnabled = true;
            Button3.addEventListener(MouseEvent.DOUBLE_CLICK, toggleIconColor, false, 0, true);            
        }
        
        override protected function draw():void {
            super.draw();
        }        
        
        /** Play the "flash" animation for the targetIcon using gotoAndPlay() with frame labels. */
        protected function playIconPingAnimation(event:Event):void {            
            targetIcon.gotoAndPlay("ping");
        }
        
        protected function toggleIconColor(event:Event):void {
            var TargetButton:Button = event.target as Button;
            
            // Cache the updated selected state for the target so we don't have to retrieve it multiple times.
            bIsIconDefaultColor = !bIsIconDefaultColor;            
            
            // Toggle the color of the targetIcon's shape MovieClip by using gotoAndPlay() with frame labels.
            targetIcon.shape.gotoAndPlay( (bIsIconDefaultColor) ? "blue" : "alt" );
        }
        
        protected function toggleIconRotation(event:Event):void {
            var TargetButton:Button = event.target as Button;
            var bIsTargetSelected:Boolean = TargetButton.selected;
            
            // Update the label for the button.
            if (bIsTargetSelected) {        
                TargetButton.label = "Stop Rotation";
                
                // Start a tween rotation of the object.
                targetIcon.addEventListener(Event.ENTER_FRAME, rotateCLIKLogo, false, 0, true);
            }
            else {                
                TargetButton.label = "Start Rotation";

                // Start a tween rotation of the object.
                targetIcon.removeEventListener(Event.ENTER_FRAME, rotateCLIKLogo);
                targetIcon.rotation = 0;
            }            
        }
        
        protected function rotateCLIKLogo( event:Event ):void {
            targetIcon.rotation++;
            if (targetIcon.rotation == 360) { targetIcon.rotation = 0; }
        }
    }	
}