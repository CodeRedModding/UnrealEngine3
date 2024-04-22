/**************************************************************************

Filename    :   DemoController.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.clikbrowser.controllers {
    
    import scaleform.clik.interfaces.IDataProvider;
    
    public class DemoController extends Object {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
    protected var _tabData:IDataProvider;
        
    // UI Elements:
    
    // Initialization:
        public function DemoController() {
            super();
        }
        
    // Public getter / setters:	
        public function getTabData():IDataProvider {
            return _tabData;
        }
    
    // Public Methods:
        
        public function toString():String {
            return "[DemoController Base]";
        }
        
    // Protected Methods:
        
    }
    
}