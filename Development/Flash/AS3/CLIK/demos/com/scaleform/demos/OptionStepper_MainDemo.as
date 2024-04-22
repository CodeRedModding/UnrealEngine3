/**************************************************************************

Filename    :   OptionStepper_MainDemo.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.demos {
    
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
        
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.controls.OptionStepper;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.events.IndexEvent;
    import scaleform.clik.interfaces.IDataProvider;
    
    public class OptionStepper_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
        
    // UI Elements:
        public var optStep1:OptionStepper;
        public var optStep2:OptionStepper;
        
        public var label1:TextField;
        public var label2:TextField;
        
    // Initialization:
        public function OptionStepper_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            var dp:Array = ["Item 1", "Item 2", "Item 3", "Item 4", "Item 5", "Item 6", "Item 7", "Item 8"];
            
            optStep1.dataProvider = new DataProvider(dp);
            optStep1.invalidateData(); // Invalidate the component so it reflects the new data.
            optStep1.validateNow();
            
            optStep2.dataProvider = new DataProvider(dp);
            optStep2.invalidateData(); // Invalidate the component so it reflects the new data.
            optStep2.validateNow();
            
            optStep1.addEventListener(IndexEvent.INDEX_CHANGE, handleStepper1Change, false, 0, true);
            
            // Add an event listener that will update optStep1's selectedIndex to match optStep2's whenever
            // optStep2's selectedIndex changes.
            optStep2.addEventListener(IndexEvent.INDEX_CHANGE, handleStepper2Change, false, 0, true);
        }
        
        protected function handleStepper1Change(e:IndexEvent):void {
            label1.text = (e.index + 1) + "";
        }
        
        protected function handleStepper2Change(e:IndexEvent):void {
            optStep1.selectedIndex = e.index;
            optStep1.validateNow();
            label2.text = (e.index + 1) + "";
        }
        
        override protected function draw():void {
            super.draw();
        }
    }	
}