/**************************************************************************

Filename    :   TileList_Main.as

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
    
    import scaleform.gfx.FocusManager;
    import scaleform.gfx.Extensions;
        
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.TileList;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollIndicator;
    import scaleform.clik.data.DataProvider;
    
    public class MultipleControllers_Main extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var panel1:MovieClip;
        public var panel2:MovieClip;
        public var panel3:MovieClip;
        
        public var tf1:TextField;
        public var tf2:TextField;
        public var tf3:TextField;
        
    // Initialization:
        public function MultipleControllers_Main() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            // Setup two focus groups
            FocusManager.setControllerFocusGroup(0, 0); // controller id 0
            FocusManager.setControllerFocusGroup(1, 1); // controller id 1
            
            FocusManager.setFocusGroupMask( panel1, 0x1 );
            FocusManager.setFocusGroupMask( panel2, 0x2 );
            FocusManager.setFocusGroupMask( panel3, 0x1 | 0x2 );
            
            // Set the initial focuses to appropriate focus group (value is a bitmask)
            panel1.r1.focused = 0x01; // Focus group 0
            panel2.r1.focused = 0x02; // Focus group 1
            
            panel3.btn1.label = "Restrict Focus";
            panel3.btn1.addEventListener( ButtonEvent.CLICK, handleButton1Click, false, 0, true );
            
            panel3.btn2.label = "Unrestrict Focus";
            panel3.btn2.addEventListener( ButtonEvent.CLICK, handleButton2Click, false, 0, true );
            
            panel3.btn3.label = "Reset Focus";
            panel3.btn3.addEventListener( ButtonEvent.CLICK, handleButton3Click, false, 0, true );
        }
        
        
        protected function handleButton1Click( be:ButtonEvent ):void {
            restrictFocus();
        }
        protected function handleButton2Click( be:ButtonEvent ):void {
            unrestrictFocus();
        }
        protected function handleButton3Click( be:ButtonEvent ):void {
            resetFocus();
        }
        
        protected function restrictFocus():void {
            FocusManager.setFocusGroupMask( panel1, 0x1 );
            FocusManager.setFocusGroupMask( panel2, 0x2 );
            
            tf1.text = "Controller 0 Only";
            tf2.text = "Controller 1 Only";
        }
        
        protected function unrestrictFocus():void {
            FocusManager.setFocusGroupMask( panel1, 0 );
            FocusManager.setFocusGroupMask( panel2, 0 );
            
            tf1.text = "Both Controllers";
            tf2.text = "Both Controllers";
        }
        
        protected function resetFocus():void {
            panel1.r1.focused = 0x01; // Focus group 0
            panel2.r1.focused = 0x02; // Focus group 1
        }
        
        override protected function draw():void {
            super.draw();
        }
    }
}