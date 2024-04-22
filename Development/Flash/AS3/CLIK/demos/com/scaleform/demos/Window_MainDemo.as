/**************************************************************************

Filename    :   Window_MainDemo.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

﻿package com.scaleform.demos {
        
    import flash.display.MovieClip;
    import flash.utils.getDefinitionByName;
    
    import scaleform.clik.controls.Button;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.controls.Window;
    import scaleform.clik.utils.Padding;
    
    public class Window_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var window1:Window;
        public var resetBtn:Button;
        
    // Initialization:
        public function Window_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            resetBtn.addEventListener(ButtonEvent.CLICK, handleResetButtonClick, false, 0, true);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        protected function handleResetButtonClick(e:ButtonEvent):void {
            if (window1 != null && this.contains(window1)) {
                removeChild(window1);
                window1 = null;
            }
            
            var windowClass:Class = getDefinitionByName("WindowSkinned") as Class;
            var newWin:Window = new windowClass();
            newWin.name = "window2";
            newWin.x = 20;
            newWin.y = 15;
            newWin.width = 462;
            newWin.height = 356;
            newWin.title = "Dynamic Window";
            newWin.contentPadding = new Padding(48, 8, 8, 8);
            newWin.source = "Form1";
            
            window1 = newWin;
            addChild(window1);
        }
    }
}