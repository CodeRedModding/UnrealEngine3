/**************************************************************************

Filename    :   ScrollingList_Margin_Padding.as

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
    import flash.utils.getDefinitionByName;
    
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollingList;
    import scaleform.clik.controls.ListItemRenderer;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.events.ListEvent;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.utils.Padding;
    import scaleform.clik.controls.Button;
    
    public class ScrollingList_Margin_Padding extends UIComponent {
    
    // Constants:
    
    // Public Properties:
    
    // Protected Properties:
    
    // UI Elements:
        public var List1:ScrollingList;
        
        public var marginBtn:Button;
        public var paddingBtn:Button;
        public var resetBtn:Button;
        
    // Initialization:
        public function ScrollingList_Margin_Padding() {
            super();
        }
    
    // Public Getter / Setters:
    
    // Public Methods:
    
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            var itemsData:Array = new Array();
            for (var i:int = 1; i < 15; i++) {
                itemsData.push({label:"Item " + i, index:i});
            }
            
            List1.dataProvider = new DataProvider(itemsData);
            List1.selectedIndex = 0;
            List1.validateNow();
            
            marginBtn.label = "Toggle Margin";
            marginBtn.autoSize = TextFieldAutoSize.CENTER;
            marginBtn.addEventListener(ButtonEvent.CLICK, handleMarginButtonClick, false, 0, true);
            marginBtn.validateNow();
            
            paddingBtn.label = "Toggle Padding";
            paddingBtn.autoSize = TextFieldAutoSize.CENTER;
            paddingBtn.addEventListener(ButtonEvent.CLICK, handlePaddingButtonClick, false, 0, true);
            paddingBtn.validateNow();
            
            resetBtn.label = "Reset Padding / Margin";
            resetBtn.autoSize = TextFieldAutoSize.CENTER;
            resetBtn.addEventListener(ButtonEvent.CLICK, handleResetButtonClick, false, 0, true);
            resetBtn.validateNow();
        }
        
        protected function handleMarginButtonClick(event:ButtonEvent):void {
            switch (List1.margin) {
                case (0):
                    List1.margin = 12;
                    break;
                case (12):
                    List1.margin = 0;
                    break;
                default:
                    break;
            }
            
            List1.invalidateSize();
        }
        
        protected function handlePaddingButtonClick(event:ButtonEvent):void {
            var newPadding:Padding;
            
            var currentPadding:Padding = List1.padding;
            if (currentPadding.top == 10) {
                newPadding = new Padding(0, 0, 0, 0);
            }
            else {
                newPadding = new Padding(10, 10, 10, 10);
            }
            
            List1.padding = newPadding;
            List1.invalidateSize();
        }
        
        protected function handleResetButtonClick(event:ButtonEvent):void {
            // Reset the padding.
            var padding:Padding = new Padding(0, 0, 0, 0);
            List1.padding = padding;
            
            // Reset the margin.
            List1.margin = 0;
            
            // Invalidate the list's size.
            List1.invalidateSize();
        }
    }
}