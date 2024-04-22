/**************************************************************************

Filename    :   ScrollingList_MainDEmo.as

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
    import scaleform.clik.controls.Button;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.controls.ScrollingList;
    import scaleform.clik.controls.ListItemRenderer;
    import scaleform.clik.events.ButtonEvent;
    import scaleform.clik.events.ListEvent;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.utils.Padding;
    
    public class ScrollingList_MainDemo extends UIComponent {
    
    // Constants:
    
    // Public Properties:
    
    // Protected Properties:
    
    // UI Elements:
        public var List1:ScrollingList;
        public var List2:ScrollingList;
        
        public var Rend0:ListItemRenderer;
        public var Rend1:ListItemRenderer;
        public var Rend2:ListItemRenderer;
        public var Rend3:ListItemRenderer;
        public var Rend4:ListItemRenderer;
        
        public var ScrollBar2:ScrollBar;
        
        public var txt_L1SelectedIndex:TextField;
        public var txt_L1Label:TextField;
        
        public var txt_L2SelectedIndex:TextField;
        public var txt_L2Label:TextField;
        
        public var addButton:Button;
        public var removeButton:Button;
        
    // Initialization:
        public function ScrollingList_MainDemo() {
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
            
            List1.dataProvider = new DataProvider(itemsData);
            List1.selectedIndex = 0;
            List1.validateNow();
            
            addButton.label = "Add >>";
            addButton.addEventListener(ButtonEvent.CLICK, handleAddBtnClick, false, 0, true);
            addButton.validateNow();
            
            removeButton.label = "<< Remove";
            removeButton.addEventListener(ButtonEvent.CLICK, handleRemoveBtnClick, false, 0, true);
            removeButton.validateNow();
            
            List1.addEventListener(ListEvent.INDEX_CHANGE, handleListChange, false, 0, true);
            List1.addEventListener(ListEvent.ITEM_CLICK, handleListItemClick, false, 0, true);
            
            List2.addEventListener(ListEvent.INDEX_CHANGE, handleListChange, false, 0, true);
            List2.addEventListener(ListEvent.ITEM_CLICK, handleListItemClick, false, 0, true);
            
            updateListStatusText( List1 );
            updateListStatusText( List2 );
        }
    
        override protected function draw():void {
            super.draw();
        }
        
        function handleListChange( event:Event ):void {
            var targetList:ScrollingList = event.target as ScrollingList;
            if (targetList != null) { 
                updateListStatusText( targetList );
            }
        }
        
        function handleListItemClick( event:ListEvent ):void {
            if (event.isKeyboard) {
                if (event.target == List1) { addItemToList2();  }
                else if (event.target == List2) { removeItemFromList2(); }
            }
        }
        
        function updateListStatusText( list:ScrollingList ):void {
            var selectedLabel:String = "";
            var selectedIndex:int = list.selectedIndex;                      
            if (selectedIndex >= 0 && selectedIndex < list.dataProvider.length) { selectedLabel = list.dataProvider[selectedIndex].label; }

            if (list == List1) {
               txt_L1SelectedIndex.text = "SelectedIndex: \t" + selectedIndex;
               txt_L1Label.text = "Label: \t" + selectedLabel;
            }
            else if (list == List2) {
               txt_L2SelectedIndex.text = "SelectedIndex: \t" + selectedIndex;
               txt_L2Label.text = "Label: \t" + selectedLabel;
            }
        }
        
        function handleAddBtnClick(event:ButtonEvent):void {
            addItemToList2();
        }
        
        function addItemToList2():void {
            // Cache references to the dataProviders for the Lists.
            var dp_List1:Array = List1.dataProvider as Array;
            var dp_List2:Array = List2.dataProvider as Array;
            
            var selectedIndex:int = List1.selectedIndex;
            if (selectedIndex < 0 && selectedIndex >= dp_List1.length) { return; }
            
            // Retrieve the data and remove it from the dataProvider for List1. 
            var selectedData:Object = dp_List1.splice(selectedIndex, 1)[0];
            if (selectedData == null) { return; }
            var dataIndex:int = selectedData.index; // Retreive the data index for the selected renderer.

            var list2Length:int = dp_List2.length;
            for (var i:int = 0; i <= list2Length; i++) {
                // If we've reached the end of the list, just push the item on to the end of the dataProvider..
                if (i == list2Length) {
                    dp_List2.push(selectedData);
                    List2.selectedIndex = i;
                }
                // To keep the lists properly orderered, if we find an item with a higher dataProvider index, 
                // we know to place the new item before it in the list.
                else if (dataIndex < dp_List2[i].index) {
                    dp_List2.splice(i, 0, selectedData);
                    List2.selectedIndex = i; 
                    break;
                }
            }
            
            if (dp_List1.length > 0) {
                List1.selectedIndex = ((selectedIndex - 1) < 0) ? 0 : (selectedIndex - 1);
            } else {
                List1.selectedIndex = -1;
            }
            
            List1.invalidateData();
            List2.invalidateData();
        }

        function removeItemFromList2():void {
             // Cache references to the dataProviders for the Lists.
            var dp_List1:Array = List1.dataProvider as Array;
            var dp_List2:Array = List2.dataProvider as Array;
            
            // Ensure that the user hasn't selected an index that is out of bounds.
            var selectedIndex:int = List2.selectedIndex;
            if (selectedIndex < 0 && selectedIndex >= dp_List2.length) { return; }
            
            // Retrieve the data and remove it from the dataProvider for List2.            
            var selectedData:Object = dp_List2.splice(selectedIndex, 1)[0];
            if (selectedData == null) { return; }
            var dataIndex:int = selectedData.index; // Retreive the data index for the selected renderer.
            
            var list1Length:int = dp_List1.length;            
            for (var i:int = 0; i <= list1Length; i++) {
                // If we've reached the end of the list, attach it to the end.
                if (i == list1Length) {
                    dp_List1.push(selectedData);
                    List1.selectedIndex = i;
                }
                // To keep the lists properly orderered, we find an item with a higher dataProvider index, 
                // we know to place the new item before it in the list.
                else if (dataIndex < dp_List1[i].index) {
                    dp_List1.splice(i, 0, selectedData);
                    List1.selectedIndex = i;
                    break;
                }
            }
            
            // Set List2's selectedIndex to the selectedIndex - 1 assuming we have items in the list and selectedIndex - 1 > 0.
            if (dp_List2.length > 0) {
                List2.selectedIndex = ((selectedIndex - 1) < 0) ? 0 : (selectedIndex - 1);
            } else {
                List2.selectedIndex = -1;
            }
            
            List1.invalidateData();
            List2.invalidateData();
        }
        
        function handleRemoveBtnClick(event:Event):void {
            removeItemFromList2();
        }
    }
}


        
