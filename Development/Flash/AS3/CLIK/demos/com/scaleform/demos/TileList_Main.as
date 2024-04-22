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
        
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.TileList;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollIndicator;
    import scaleform.clik.data.DataProvider;
    
    public class TileList_Main extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var tileList1:TileList;
        public var tileList2:TileList;
        
        public var si1:ScrollIndicator;
        public var sb1:ScrollBar;
        
    // Initialization:
        public function TileList_Main() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            var dataArray:Array = [];
            for (var i:uint = 0; i < 20; i++) {
                dataArray.push( ("Item" + i) );
            }
            
            tileList1.dataProvider = new DataProvider(dataArray);
            tileList2.dataProvider = new DataProvider(dataArray);
            
            tileList1.validateNow();
            tileList2.validateNow();
        }
        
        override protected function draw():void {
            super.draw();
        }
    }
}