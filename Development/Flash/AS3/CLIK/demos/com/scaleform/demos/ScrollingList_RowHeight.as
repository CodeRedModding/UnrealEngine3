/**************************************************************************

Filename    :   ScrollingList_RowHeight.as

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
    
    public class ScrollingList_RowHeight extends UIComponent {
    
    // Constants:
    
    // Public Properties:
    
    // Protected Properties:
    
    // UI Elements:
        public var List1:ScrollingList;
        
        public var rowHeightBtn:Button;
        
        public var rowHeightLabel:TextField;
        
    // Initialization:
        public function ScrollingList_RowHeight() {
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
            
            rowHeightBtn.label = "Toggle Row Height";
            rowHeightBtn.autoSize = TextFieldAutoSize.CENTER;
            rowHeightBtn.addEventListener(ButtonEvent.CLICK, handleRowHeightButtonClick, false, 0, true);
            rowHeightBtn.validateNow();
            
            rowHeightBtn.focused = 1;
            updateRowHeightLabel();
        }
        
        protected function handleRowHeightButtonClick(event:ButtonEvent):void {
            List1.rowHeight = (List1.rowHeight == 32) ? 36 : 32;
            
            // Invalidate the list's size.
            List1.invalidateSize();
            
            // Update the label that displays the List's rowHeight.
            updateRowHeightLabel();
        }
        
        protected function updateRowHeightLabel():void {
            rowHeightLabel.text = "List.rowHeight = " + List1.rowHeight;
        }
    }
}