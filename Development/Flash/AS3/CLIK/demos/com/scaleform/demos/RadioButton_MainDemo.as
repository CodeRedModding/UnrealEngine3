/**************************************************************************

Filename    :   RadioButton_MainDemo.as

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
    import scaleform.clik.controls.ButtonGroup;
    import scaleform.clik.controls.RadioButton;
    import scaleform.clik.events.ButtonEvent;
    
    public class RadioButton_MainDemo extends UIComponent {
        
    // Constants:
    
    // Public Properties:
        
    // Protected Properties:
    
    // UI Elements:
        /** Reference to the first RadioButton in Group1. */
        public var RadioButton1:RadioButton;
        /** Reference to the second RadioButton in Group1. */
        public var RadioButton2:RadioButton;
        /** Reference to the third RadioButton in Group1. */
        public var RadioButton3:RadioButton;
        
        /** Reference to the first RadioButton in Group2 that will be created dynamically during configUI(). */
        public var RadioButton4:RadioButton;
        /** Reference to the second RadioButton in Group2 that will be created dynamically during configUI(). */
        public var RadioButton5:RadioButton;
        /** Reference to the third RadioButton in Group2 that will be created dynamically during configUI(). */
        public var RadioButton6:RadioButton;
        
        /** Reference to the Group1 ButtonGroup that contains the left three RadioButtons. */
        public var Group1:ButtonGroup;
        /** Reference to the textField that displays the current selectedIndex for Group1. */
        public var txt_Group1Index:TextField;        
        
        /** Reference to the Group2 ButtonGroup that contains the right three RadioButtons. */
        public var Group2:ButtonGroup;
        /** Reference to the textField that displays the current selectedIndex for Group2. */
        public var txt_Group2Index:TextField;
        
    // Initialization:
        public function RadioButton_MainDemo() {
            super();
        }
        
    // Public getter / setters:
        
    // Public Methods:
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();     
            
            // Cache a reference to Group 1 and assign a listener for Event.CHANGE to update the txt_Group1Index
            // textField with the latest selectedIndex.
            Group1 = ButtonGroup.getGroup("Group1", this);
            Group1.addEventListener(Event.CHANGE, handleGroup1Change, false, 0, true);
            
            // Create a new RadioButton for Group2 and assign it to RadioButton4.
            RadioButton4 = configureButtonForGroup2( "RadioButton4" );
            RadioButton4.x = 380;
            RadioButton4.y = RadioButton1.y;
            
            // Create a new RadioButton for Group2 and assign it to RadioButton5.
            RadioButton5 = configureButtonForGroup2( "RadioButton5" );
            RadioButton5.x = RadioButton4.x;
            RadioButton5.y = RadioButton4.y + 25;
            
            // Create a new RadioButton for Group2 and assign it to RadioButton6.
            RadioButton6 = configureButtonForGroup2( "RadioButton6" );
            RadioButton6.x = RadioButton4.x;
            RadioButton6.y = RadioButton5.y + 25;
            
            // Cache a reference to Group 12and assign a listener for Event.CHANGE to update the txt_Group2Index
            // textField with the latest selectedIndex.
            Group2 = ButtonGroup.getGroup("Group2", this);
            Group2.addEventListener(Event.CHANGE, handleGroup2Change, false, 0, true);
            
            // Force an update of the textFields that display the current selectedIndex for Group1 and Group2.
            handleGroup1Change( null );
            handleGroup2Change( null );
        }
        
        override protected function draw():void {
            super.draw();
        }
        
        /** Creates, configures, and returns a new RadioButton for Group2. */
        protected function configureButtonForGroup2( label:String ):RadioButton {
            var NewRadioButton:RadioButton;
            var ClassRef:Class = getDefinitionByName("RadioButtonSkinned") as Class;
            if (ClassRef != null) { NewRadioButton = new ClassRef() as RadioButton; }
            
            // Configure the new RadioButton for Group2. 
            NewRadioButton.label = label;
            NewRadioButton.groupName = "Group2";
            NewRadioButton.allowDeselect = true; // Group2's RadioButtons can be deselected so none are selected.   
            NewRadioButton.validateNow(); // Validate the component since we've changed it's properties.
            
            this.addChild( NewRadioButton ); // Add the new RadioButton to the display list.
            return NewRadioButton;
        }
        
        /** Updates the txt_Group1Index textField with the latest selectedIndex for Group1. */
        protected function handleGroup1Change( event:Event ):void {
            trace("Group1's selectedIndex changed: " + Group1.selectedIndex);
            txt_Group1Index.text = "Group1.selectedIndex:\t" + Group1.selectedIndex;
        }
        
        /** Updates the txt_Group2Index textField with the latest selectedIndex for Group2. */
        protected function handleGroup2Change( event:Event ):void {
            trace("Group2's selectedIndex changed: " + Group2.selectedIndex);
            txt_Group2Index.text = "Group2.selectedIndex:\t" + Group2.selectedIndex;
        }
    }
}