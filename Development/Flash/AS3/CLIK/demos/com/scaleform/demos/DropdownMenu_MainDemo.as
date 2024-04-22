/**************************************************************************

Filename    :   DropdownMenu_MainDemo.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

﻿package com.scaleform.demos {
        
    import flash.events.Event;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.controls.DropdownMenu;
    import scaleform.clik.data.DataProvider;
    
    public class DropdownMenu_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var ddm:DropdownMenu;
        public var ddm2:DropdownMenu;
        public var ddm3:DropdownMenu;
        
    // Initialization:
        public function DropdownMenu_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            var itemsData:Array = new Array();
            for (var i:int = 1; i < 15; i++) {
                itemsData.push({label:"Item " + i, index:i});
            }
            var dp:DataProvider = new DataProvider(itemsData);
            
            stage.focus = ddm;
            
            ddm.dataProvider = dp;
            ddm.invalidateData();
            ddm.selectedIndex = 0;
            ddm.validateNow();
            
            ddm2.dataProvider = dp;
            ddm2.invalidateData();
            ddm2.selectedIndex = 0;
            ddm2.validateNow();
            
            ddm3.dataProvider = dp;
            ddm3.invalidateData();
            ddm3.selectedIndex = 0;	
            ddm3.validateNow();
        }
        
        override protected function draw():void {
            super.draw();
        }
        
    }
}