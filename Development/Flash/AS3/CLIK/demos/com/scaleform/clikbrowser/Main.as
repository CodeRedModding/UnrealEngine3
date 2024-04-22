/**************************************************************************

Filename    :   Main.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.clikbrowser {
    
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.net.URLRequest;
    import flash.display.Loader;
    import flash.system.LoaderContext;
    import flash.system.ApplicationDomain;
    import flash.display.StageScaleMode;
    import flash.display.StageAlign;
    
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.events.ListEvent;
    import scaleform.clik.controls.ListItemRenderer;
    
    import com.scaleform.clikbrowser.controls.TabBar;
    import com.scaleform.clikbrowser.controllers.*;    
    import com.scaleform.clikbrowser.events.TabEvent;
    
    public class Main extends UIComponent  {
        
    // Constants:
        
    // Public Properties:
        public var browserList:BrowserList;
        public var sbBrowserList:CLIKScrollBar;
        public var tabBar:TabBar;
        public var content:MovieClip;
        public var header:MovieClip;
        
        public var mainHeader:MovieClip;
    
    // Protected Properties:
        protected var _demoControllers:Array;
        protected var _currentDemo:String;
        protected var _loader:Loader;
    
    // UI Elements:
    
    // Initialization:
        public function Main() {
            super();
            
            stage.scaleMode = StageScaleMode.SHOW_ALL;
            stage.align = StageAlign.TOP_LEFT;
            
            _demoControllers = [];
            _demoControllers.push(new ButtonDemoController());
            _demoControllers.push(new ButtonBarDemoController());
            _demoControllers.push(new CheckBoxDemoController());
            _demoControllers.push(new DropdownMenuController());
            _demoControllers.push(new LayoutDemoController());
            _demoControllers.push(new MultiControllerDemoController());
            _demoControllers.push(new OptionStepperDemoController());
            _demoControllers.push(new RadioButtonDemoController());
            _demoControllers.push(new TextAreaDemoController());
            _demoControllers.push(new TextInputDemoController());
            _demoControllers.push(new TileListDemoController());
            _demoControllers.push(new ScrollBarDemoController());
            _demoControllers.push(new ScrollingListDemoController());
            _demoControllers.push(new SliderDemoController());
            _demoControllers.push(new WindowDemoController());
        }
        
    // Public getter / setters:
        
    // Public Methods:
        override public function toString():String {
            return "[CLIKBrowser Main]";
        }
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            
            browserList.focused = 1;
            browserList.addEventListener(ListEvent.INDEX_CHANGE, selectDemo, false, 0, true);
            tabBar.addEventListener(TabEvent.SELECT, handleTabSelect, false, 0, true);
            
            browserList.dataProvider = new DataProvider(_demoControllers);
            browserList.selectedIndex = 0;
            browserList.validateNow();
            
            browserList.tabEnabled = false;
            browserList.focusable = false;
            
            tabBar.focusable = false;
        }
        
        protected function selectDemo(e:Event):void {
            refreshTabBar();
        }
        
        protected function refreshTabBar():void {
            var demoController:DemoController = _demoControllers[browserList.selectedIndex] as DemoController;
            var data = demoController.getTabData();
            _currentDemo = demoController.toString();
            tabBar.dataProvider = data;
            tabBar.validateNow();
        }
        
        protected function handleTabSelect(e:TabEvent):void {
            header.textField.text = _currentDemo + ": " + e.data.label;
            
            if (_loader) {
                content.removeChild(_loader);
                _loader.unloadAndStop();
            }
            
            _loader = new Loader();
            content.addChild(_loader);
            var url:URLRequest = new URLRequest(e.data.path); 
            var context:LoaderContext = new LoaderContext(false, ApplicationDomain.currentDomain);
            _loader.load(url, context);
        }
    }
}