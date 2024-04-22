/**************************************************************************

Filename    :   TabBar.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.clikbrowser.controls {
    
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.MouseEvent;
    
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.controls.Button;	
    import scaleform.clik.controls.ButtonGroup;
    import scaleform.clik.interfaces.IDataProvider;
    
    import com.scaleform.clikbrowser.controllers.DemoController;
    import com.scaleform.clikbrowser.events.TabEvent;
    
    public class TabBar extends UIComponent {
        
    // Constants:
        
    // Public Properties:
        public var btnPrev:Button;
        public var btnNext:Button;
        public var arrowMask:MovieClip;
        public var tabContainer:MovieClip;
        
    // Protected Properties:
        protected var _dataProvider:IDataProvider;
        protected var _tabs:Array;
        protected var _group:ButtonGroup;
        
    // UI Elements:
    
    // Initialization:
        public function TabBar() {
            super();
            
            _tabs = [];
        }
        
    // Public getter / setters:
        
    // Public Methods:
        public function get dataProvider():IDataProvider { return _dataProvider; }
        public function set dataProvider(value:IDataProvider):void {
            if (_dataProvider == value) { return; }
            if (_dataProvider != null) {
                _dataProvider.removeEventListener(Event.CHANGE, handleDataChange, false);
            }
            _dataProvider = value;
            if (_dataProvider == null) { return; }
            _dataProvider.addEventListener(Event.CHANGE, handleDataChange, false, 0, true);
            invalidateData();
        }
        
        override public function toString():String {
            return "[TabBar]";
        }
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            btnPrev.addEventListener(MouseEvent.CLICK, handlePrevClick, false, 0, true);
            btnNext.addEventListener(MouseEvent.CLICK, handleNextClick, false, 0, true);
            
            btnPrev.visible = false;
            btnNext.visible = false;
            arrowMask.visible = false;
        }
    
        override protected function draw():void {
            if (isInvalid(InvalidationType.DATA)) {
                refreshData();
            }
            if (isInvalid(InvalidationType.RENDERERS)) {
                // Select first tab
                if (_dataProvider && _dataProvider.length > 0) {
                    var firstTab:Button = _tabs[0] as Button;
                    firstTab.selected = true;
                    firstTab.validateNow();
                }
            }
        }
        
        protected function refreshData():void {
            if (_dataProvider) _dataProvider.requestItemRange(0, _dataProvider.length-1, populateData);
        }
        
        protected function populateData(data:Array):void {
            // TODO: Rebuild the tabs
            // trace("rebuilding the tabs..");
            while (tabContainer.numChildren > 0) {
                tabContainer.removeChildAt(0);
            }
            _group = new ButtonGroup(name + "Group", this);
            _tabs.length = 0;
            var tabx:Number = 0;
            for (var i:uint = 0; i < data.length; i++) {
                var tab:BrowserTab = new BrowserTab();
                tab.label = data[i].label;
                tab.data = data[i];
                tab.autoSize = "left";
                tab.addEventListener(Event.SELECT, handleTabSelect, false, 0, true);
                tab.x = tabx;
                tab.group = _group;
                tab.validateNow();
                tabx += tab.width;
                tabContainer.addChild(tab);
                _tabs.push(tab);
            }
            invalidate(InvalidationType.RENDERERS);
        }
        
        protected function handleTabSelect(event:Event):void {
            if (event.target.selected) {
                dispatchEvent(new TabEvent(TabEvent.SELECT, event.target.data));
            }
        }
        
        protected function handleDataChange(event:Event):void {
            invalidate(InvalidationType.DATA);
        }		
        
        protected function handlePrevClick(e:Event):void {
            // trace("tab prev clicked..");
        }
        protected function handleNextClick(e:Event):void {
            // trace("tab next clicked..");
        }
    }
    
}