/**************************************************************************

Filename    :   CheckBox_MainDemo.as

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
    import scaleform.clik.controls.CheckBox;
    import scaleform.clik.events.ButtonEvent;
    
    public class CheckBox_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to the first CheckBox which toggles the visibility of the second CheckBox. */
        public var CheckBox1:CheckBox;
        /** Reference to the second CheckBox. This CheckBox is added via ActionScript in configUI(). */
        public var CheckBox2:CheckBox;
        /** Reference to the third CheckBox. This CheckBox mirrors the selected property of the third CheckBox.*/
        public var CheckBox3:CheckBox;
        
        public var CheckBox2_Label:TextField;
        
    // Initialization:
        public function CheckBox_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Configure CheckBox1
            CheckBox1.label = "Hide CheckBox2";
            // Add weak referenced event listeners to Button1 and Button2 that listen for MouseEvent.CLICK.
            CheckBox1.addEventListener(ButtonEvent.CLICK, toggleCheckBox2Visibility, false, 0, true);
            CheckBox1.autoSize = "left";
            CheckBox1.validateNow();
            
            // Configure CheckBox2
            var ClassRef:Class = getDefinitionByName("CheckBoxSkinned") as Class;
            if (ClassRef != null) { CheckBox2 = new ClassRef() as CheckBox; }            
            
            CheckBox2.label = "Toggle CheckBox";
            CheckBox2.autoSize = "left";
            CheckBox2.addEventListener(ButtonEvent.CLICK, updateCheckBox3, false, 0, true);
            CheckBox2.x = CheckBox1.x;
            CheckBox2.y = 215;
            CheckBox2.validateNow();
            
            this.addChild(CheckBox2 as DisplayObject);
            
            // Configure CheckBox 3.
            CheckBox3.label = "Mirror of CheckBox2";
            CheckBox3.autoSize = "left";
            CheckBox3.validateNow();
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        protected function updateCheckBox3( event:Event ):void {
            CheckBox3.selected = CheckBox2.selected;
        }
        
        protected function toggleCheckBox2Visibility( event:Event ):void {
            CheckBox2.visible = !CheckBox1.selected;
        }
    }
}