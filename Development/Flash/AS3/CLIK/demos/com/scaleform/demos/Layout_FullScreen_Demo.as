/**************************************************************************

Filename    :   Layout_FullScreen_Demo.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

﻿package com.scaleform.demos {
    
    import flash.events.Event;
    import flash.display.MovieClip;
    import flash.display.StageScaleMode;
    import flash.display.StageAlign;
    
    import scaleform.gfx.Extensions;
    
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.layout.Layout;
    import scaleform.clik.layout.LayoutData;
    
    public class Layout_FullScreen_Demo extends UIComponent {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var topLeft:MovieClip;
        public var topRight:MovieClip;
        
        public var bottomLeft:MovieClip;
        public var bottomRight:MovieClip;
        
        public var layout:Layout;
        
    // Initialization:
        public function Layout_FullScreen_Demo() {
            super();
        }

    // Public Getter / Setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            Extensions.enabled = true;
            
            stage.scaleMode = StageScaleMode.NO_BORDER; // Works with StageScaleoMode.NO_BORDER and NO_SCALE.
            // stage.align = StageAlign.TOP; // Works with TOP, TOP_LEFT, and CENTER. Set to CENTER by default.
            
            stage.addEventListener( Event.RESIZE, handleStageResize, false, 0, true );
            
            // TOP RIGHT
            topRight.layoutData = new LayoutData();
            topRight.layoutData.alignV = "top";
            topRight.layoutData.alignH = "right";
            
            topRight.layoutData.offsetH = -70;
            topRight.layoutData.offsetHashH["16:9"] = -70; // Move the content out if it's widescreen.
            topRight.layoutData.offsetHashH["4:3"] = -40; // Will be set by default.
            
            // TopRight.layoutData.offsetV = 40; // Set automatically based on the original offset.
            // TopRight.layoutData.offsetHashV["16:9"] = 40; // Can use default instead.
            // TopRight.layoutData.offsetHashV["4:3"] = 65; // y-coord will be the same for 4:3 and 16:9.
            
            topRight.textField.text = "Top Right";
            
            // TOP LEFT            
            topLeft.layoutData = new LayoutData();
            topLeft.layoutData.alignV = "top";
            topLeft.layoutData.alignH = "left";
            
            topLeft.layoutData.offsetH = 70;
            topLeft.layoutData.offsetHashH["16:9"] = 70;
            topLeft.layoutData.offsetHashH["4:3"] = 40;
            
            // TopLeft.layoutData.offsetV = 40; // Set automatically based on the original offset.
            // TopLeft.layoutData.offsetHashV["16:9"] = 40; // Can use default instead.
            // TopLeft.layoutData.offsetHashV["4:3"] = 65; // y-coord will be the same for 4:3 and 16:9.
            
            topLeft.textField.text = "Top\nLeft";
            
            
            // BOTTOM RIGHT
            bottomRight.layoutData = new LayoutData();
            bottomRight.layoutData.alignV = "bottom";
            bottomRight.layoutData.alignH = "right";
            
            bottomRight.layoutData.offsetH = -70;
            bottomRight.layoutData.offsetHashH["16:9"] = -70;
            bottomRight.layoutData.offsetHashH["4:3"] = -40;
            
            // TopLeft.layoutData.offsetV = 40; // Set automatically based on the original offset.
            // TopLeft.layoutData.offsetHashV["16:9"] = 40; // Can use default instead.
            // TopLeft.layoutData.offsetHashV["4:3"] = 65; // y-coord will be the same for 4:3 and 16:9.
            
            bottomRight.textField.text = "Bottom Right";
            
            // BOTTOM LEFT 
            bottomLeft.layoutData = new LayoutData();
            bottomLeft.layoutData.alignV = "bottom";
            bottomLeft.layoutData.alignH = "left";
            
            bottomLeft.layoutData.offsetH = 70;
            bottomLeft.layoutData.offsetHashH["16:9"] = 70;
            bottomLeft.layoutData.offsetHashH["4:3"] = 40;
            
            // TopLeft.layoutData.offsetV = 40; // Set automatically based on the original offset.
            // TopLeft.layoutData.offsetHashV["16:9"] = 40; // Can use default instead.
            // TopLeft.layoutData.offsetHashV["4:3"] = 65; // y-coord will be the same for 4:3 and 16:9.
            
            bottomLeft.textField.text = "Bottom Left";
            
            // Add the Layout instance to the Stage. Since it is defined as "tiedToStageSize", it's rect
            // will match the Stage and it will be updated if the stage's size changes.
            layout = new Layout();
            layout.tiedToStageSize = true;
            addChild(layout);
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        function handleStageResize(e:Event):void {
            trace("\n > handleStageResize(), stage.visibleRect: " + Extensions.visibleRect);
        }
    }
}