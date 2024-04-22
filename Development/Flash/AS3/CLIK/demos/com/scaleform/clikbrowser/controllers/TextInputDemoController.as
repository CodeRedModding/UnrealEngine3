/**************************************************************************

Filename    :   TextInputDemoController.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package com.scaleform.clikbrowser.controllers {
    
    import scaleform.clik.data.DataProvider;
    
    public class TextInputDemoController extends DemoController {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
        
    // UI Elements:
    
    // Initialization:
        public function TextInputDemoController() {
            super();
            
            _tabData = new DataProvider([ { label:"Demo", path:"TextInput_MainDemo.swf" },
                                          { label:"Editable & Enabled", path:"TextInput_Editable_Enabled.swf" },
                                          { label:"Password & MaxChars", path:"TextInput_Password_MaxChars.swf" },
                                          { label:"ActAsButton", path:"TextInput_ActAsButton.swf" } ]);
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
        override public function toString():String {
            return "TextInput";
        }
        
    // Protected Methods:
        
    }
    
}