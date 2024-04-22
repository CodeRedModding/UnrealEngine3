/**************************************************************************

Filename    :   Window_Form1.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.demos.extras {
        
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.utils.Constraints;
    import scaleform.clik.constants.ConstrainMode;
    
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollingList;
    
    import scaleform.clik.data.DataProvider;
        
    public class Window_Form1 extends UIComponent {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        public var list:ScrollingList;
        public var sb:ScrollBar;

        
    // Initialization:
        public function Window_Form1() {
            super();
        }
        
    // Public Methods:
        
    // Protected Methods:
        override protected function preInitialize():void {
            constraints = new Constraints(this, ConstrainMode.REFLOW);
        }
        
        override protected function initialize():void {
            super.initialize();
        }
        
        override protected function configUI():void {
            constraints.addElement("list", list, Constraints.ALL);
            constraints.addElement("sb", sb, Constraints.TOP | Constraints.BOTTOM | Constraints.RIGHT);
            
            var itemsData:Array = new Array();
            for (var i:int = 1; i < 15; i++) {
                itemsData.push({label:"Item " + i, index:i});
            }
            var dp:DataProvider = new DataProvider(itemsData);
            
            list.dataProvider = dp;
        }
        
        override protected function draw():void {
            // Resize and update constraints
            if (isInvalid(InvalidationType.SIZE)) {
                constraints.update(_width, _height);
            }
        }
    }
}