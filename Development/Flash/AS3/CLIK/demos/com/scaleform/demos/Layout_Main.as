/**************************************************************************

Filename    :   Layout_Main.as

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
    import flash.display.StageScaleMode;
    
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.Window;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.layout.LayoutData;
    import scaleform.clik.layout.Layout;
    
    public class Layout_Main extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        
    // Initialization:
        public function Layout_Main() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
        public var window:MovieClip;
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            stage.scaleMode = StageScaleMode.NO_SCALE;
            
            var s1:MovieClip = window.s1;
            s1.layoutData = new LayoutData();
            s1.layoutData.alignH = "right";
            s1.layoutData.alignV = "bottom";
            s1.textField.text = "Bottom Right";
            
            var s2:MovieClip = window.s2;
            s2.layoutData = new LayoutData();
            s2.layoutData.alignH = "left";
            s2.layoutData.alignV = "bottom";
            s2.textField.text = "Bottom Left";
            
            var s3:MovieClip = window.s3;
            s3.layoutData = new LayoutData();
            s3.layoutData.alignH = "center";
            s3.layoutData.alignV = "center";
            s3.layoutData.offsetV = 0;
            s3.textField.text = "Center";
            
            var s4:MovieClip = window.s4;
            s4.layoutData = new LayoutData();
            s4.layoutData.alignH = "left";
            s4.layoutData.alignV = "top";
            s4.textField.text = "Top\n Left";
            
            var s5:MovieClip = window.s5;
            s5.layoutData = new LayoutData();
            s5.layoutData.alignH = "right";
            s5.layoutData.alignV = "top";
            s5.textField.text = "Top Right";
            
            var layout:Layout = new Layout();
            window.addChild(layout);
        }
        
        override protected function draw():void {
            super.draw();
        }
    }
}