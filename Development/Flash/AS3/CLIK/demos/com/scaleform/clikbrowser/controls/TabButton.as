/**************************************************************************

Filename    :   TabButton.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.clikbrowser.controls {
    
    import flash.display.MovieClip;
    
    import scaleform.clik.controls.RadioButton;
    import scaleform.clik.utils.Constraints;
    
    public class TabButton extends RadioButton {
        
    // Constants:
        
    // Public Properties:
        public var arrow:MovieClip;
    
    // Protected Properties:
        
    // UI Elements:
    
    // Initialization:
        public function TabButton() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:	
        override public function toString():String {
            return "[TabButton]";
        }
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            constraints.addElement("arrow", arrow, Constraints.CENTER_H);
        }
        
        override protected function draw():void {
            super.draw();
            
            arrow.visible = _selected;
        }
    
    }
    
}