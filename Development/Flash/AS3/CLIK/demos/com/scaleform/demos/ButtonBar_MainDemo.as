/**************************************************************************

Filename    :   ButtonBar_MainDemo.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

﻿package com.scaleform.demos {
    
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
    
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.ButtonBar;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.events.IndexEvent;
    import scaleform.clik.events.ButtonBarEvent;
    import scaleform.clik.events.ButtonEvent;
    
    public class ButtonBar_MainDemo extends UIComponent {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
        var _content:Array = [  { label:"Home" },
                                { label:"Forums" },
                                { label:"Scaleform" },
                                { label:"CLIK" } ];
        
    // UI Elements:
        /** Reference to the Button that randomizes the dataProvider shared by the ButtonBars.*/
        public var ChangeDP_Button:Button;
        
        /** Reference to the horizontal ButtonBar that creates autoSize "left" Buttons. */
        public var ButtonBar1:ButtonBar;
        /** Reference to the vertical ButtonBar that creates Buttons of fixed width. */
        public var ButtonBar2:ButtonBar;
        
        public var tf_Bar1SelectedIndex:TextField;
        public var tf_Bar2SelectedIndex:TextField;
        
        public var tf_Description:TextField;
        public var tf_Bar1Info:TextField;
        public var tf_Bar2Info:TextField;
        
    // Initialization:
        public function ButtonBar_MainDemo() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
    
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            ButtonBar1.dataProvider = new DataProvider(_content);
            ButtonBar1.selectedIndex = 0;
            
            ButtonBar1.addEventListener(IndexEvent.INDEX_CHANGE, handleIndexChange, false, 0, true);
            ButtonBar1.addEventListener(ButtonBarEvent.BUTTON_SELECT, handleButtonSelect, false, 0, true);
            
            ButtonBar2.dataProvider = new DataProvider(_content);
            ButtonBar2.selectedIndex = 0;
            
            ButtonBar2.addEventListener(IndexEvent.INDEX_CHANGE, handleIndexChange, false, 0, true);
            ButtonBar2.addEventListener(ButtonBarEvent.BUTTON_SELECT, handleButtonSelect, false, 0, true);
            
            ButtonBar1.focused = 1; // Set the focus to the ButtonBar.
            ButtonBar1.validateNow();
            ButtonBar2.validateNow();
            
            ChangeDP_Button.addEventListener( ButtonEvent.CLICK, handleChangeDP_ButtonClick, false, 0, true);
            
            tf_Bar1SelectedIndex.text = ("selectedIndex: " + ButtonBar1.selectedIndex);
            tf_Bar2SelectedIndex.text = ("selectedIndex: " + ButtonBar2.selectedIndex);
            
            ChangeDP_Button.autoSize = TextFieldAutoSize.LEFT;
            ChangeDP_Button.label = "Randomize Data";
            ChangeDP_Button.validateNow();
            
            tf_Description.text =   "This sample demonstrates the capabilities of the ButtonBar component.\n\n" +
                                    "The ButtonBar is similar to the ButtonGroup, but it has a visual representation. It can also create Button instances on the fly, based on a dataProvider. The ButtonBar is useful for creating dynamic tab-bar-like UI elements.\n\n"+
                                    "The ButtonBars in this demo exist on the Stage and were configured via their inspectable properties.\n\n"+
                                    "The Button at the top left will randomize the dataProvider shared by both ButtonBars, causing them to dynamically reflow their Buttons and update their selectedIndex.\n\n";
                                    
            tf_Bar1Info.text = "This horizontal ButtonBar creates Buttons with autoSize set to 'left'. These properties are configured via the inspectable. The dataProvider is set via ActionScript.";
            tf_Bar2Info.text = "This vertical ButtonBar creates Buttons with a fixed width of 180. These properties are configured via the inspectable. The dataProvider is set via ActionScript.";
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        function randomizeDataProvider():void {
            var list:Array = _content.slice(0); // Copy the list
            var l:Number = Math.random() * _content.length >> 0; // Determine a random number of items to REMOVE from the list.
            for (var i:Number = 0; i < l; i++) {
                var num:Number = Math.random() * list.length >> 0; // Determine a random index to remove from the list.
                list.splice(num, 1); // Remove it.
            }
            
            ButtonBar1.dataProvider = new DataProvider(list); // Set the new dataProvider to the ButtonBar.
            ButtonBar1.focused = 1; // Set the focus to the ButtonBar.
            
            ButtonBar2.dataProvider = new DataProvider(list);
        }
        
        function handleChangeDP_ButtonClick(e:ButtonEvent):void {
            randomizeDataProvider();
        }
        
        function handleIndexChange(e:IndexEvent):void {
            // trace("\n" + e.target + " :: selectedIndex changed: " + e.index);
            if (e.target == ButtonBar1) { 
                tf_Bar1SelectedIndex.text = ("selectedIndex: " + e.index);
            } else {
                tf_Bar2SelectedIndex.text = ("selectedIndex: " + e.index);
            }
        }
        
        function handleButtonSelect(e:ButtonBarEvent):void {
            // trace("\n" + e.target + " :: Button" + e.index + " selected.");
        }
      
    }
}