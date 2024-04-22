/**************************************************************************

Filename    :   TextArea_Main.as

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
        
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.TextInput;
    import scaleform.clik.controls.TextArea;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollIndicator;
    
    public class TextArea_Main extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to textArea1. */
        public var textArea1:TextArea;
        /** Reference to textArea2.*/
        public var textArea2:TextArea;
        
        /** Reference to the ScrollIndicator for textArea3. */
        public var si1:ScrollIndicator;
        /** Reference to the ScrollBar for textArea3. */
        public var sb1:ScrollBar;
        
    // Initialization:
        public function TextArea_Main() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            var lorem:String = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas faucibus, sapien ac dapibus adipiscing, justo erat cursus nisi, a euismod tortor massa eu sem. Aliquam pharetra iaculis sem quis semper. Suspendisse quis tempus est. Aliquam et scelerisque ante. Sed posuere pretium ipsum at commodo. Nullam est metus, suscipit et adipiscing vitae, viverra vitae tellus. Nam eget tellus odio."
            textArea1.text = lorem + lorem;
            textArea2.defaultText = "This textField has defaultText. If you give this textField focus, the defaultText will be removed.";
            
            textArea1.validateNow();
            textArea2.validateNow();
        }
        
        override protected function draw():void {
            super.draw();
        }
    }
}