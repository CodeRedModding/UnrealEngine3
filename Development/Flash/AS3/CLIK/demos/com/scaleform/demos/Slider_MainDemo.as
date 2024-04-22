/**************************************************************************

Filename    :   Slider_MainDemo.as

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
	import scaleform.clik.controls.Slider;
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.data.DataProvider;
    import scaleform.clik.events.IndexEvent;
    import scaleform.clik.events.ButtonBarEvent;
    import scaleform.clik.events.ButtonEvent;
	import scaleform.clik.events.SliderEvent;
	import flash.utils.getDefinitionByName;
	
	import flash.utils.Timer;
	import flash.events.TimerEvent;
    
    public class Slider_MainDemo extends UIComponent {
        
    // Constants:
        
    // Public Properties:
		
	// Protected Properties:
		/** true if the thumb for Slider4 is currently pressed. false if not. */
		protected var _slider4ThumbPressed:Boolean = false;
		/** Timer for Slider4 that increments the Slider automatically when it is not being manipulated by the user. */
		protected var _slider4Timer:Timer 
        
    // UI Elements:
		/** References to the TextFields that display the current value of the Sliders. */
		public var tf_Slider1Value:TextField;
		public var tf_Slider2Value:TextField;
		public var tf_Slider3Value:TextField;
		public var tf_Slider4Value:TextField;
		
		/** References to the five Slider components. */
		public var Slider1:Slider;
		public var Slider2:Slider;
		public var Slider3:Slider;
		public var Slider4:Slider;
		public var Slider5:Slider;
		
        /** Reference to the Button that randomizes the dataProvider shared by the ButtonBars.*/
        public var btnEnable:Button;
		/** Reference to the description TextField on the right side of the demo. */
		public var tf_Description:TextField;
		
        
    // Initialization:
        public function Slider_MainDemo() {
            super();
        }
        
    // Public Getter / Setters:
        
    // Public Methods:
    
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
			
			var SliderClass:Class = getDefinitionByName("Slider") as Class;
			if (SliderClass) { 
				Slider3 = new SliderClass() as Slider;
				Slider3.liveDragging = true;
				Slider3.x = 60;
				Slider3.y = 212;
				addChild(Slider3);
				Slider3.validateNow();
				
				Slider3.addEventListener( SliderEvent.VALUE_CHANGE, handleSlider3ValueChange, false, 0, true );
				tf_Slider3Value.text = String(Slider3.value);
			}
			
			// Add VALUE_CHANGE listeners on Sliders that will update the textFields that display their value
			Slider1.addEventListener( SliderEvent.VALUE_CHANGE, handleSlider1ValueChange, false, 0, true );
			Slider2.addEventListener( SliderEvent.VALUE_CHANGE, handleSlider2ValueChange, false, 0, true );
			Slider4.addEventListener( SliderEvent.VALUE_CHANGE, handleSlider4ValueChange, false, 0, true );
			
			Slider4.thumb.addEventListener( ButtonEvent.PRESS, handleSlider4ThumbPress, false, 0, true );
			Slider4.thumb.addEventListener( ButtonEvent.CLICK, handleSlider4ThumbRelease, false, 0, true );
			Slider4.thumb.addEventListener( ButtonEvent.RELEASE_OUTSIDE, handleSlider4ThumbRelease, false, 0, true );

			_slider4Timer = new Timer(100);
			_slider4Timer.addEventListener( TimerEvent.TIMER, slider4Timer, false, 0, true );
			_slider4Timer.start();
			
			// Add CLICK listener to enable/disable Slider5.
			btnEnable.addEventListener( ButtonEvent.CLICK, handleBtnEnablePress, false, 0, true);
			
			// Refresh label values on load
			tf_Slider1Value.text = String(Slider1.value);
			tf_Slider2Value.text = String(Slider2.value);
			tf_Slider3Value.text = String(Slider3.value);
			tf_Slider4Value.text = String(Slider4.value);
			
            tf_Description.text =   "This sample demonstrates the capabilities of the Slider component.\n\n" +
                                    "The ButtonBar is similar to the ButtonGroup, but it has a visual representation. It can also create Button instances on the fly, based on a dataProvider. The ButtonBar is useful for creating dynamic tab-bar-like UI elements.\n\n"+
                                    "The ButtonBars in this demo exist on the Stage and were configured via their inspectable properties.\n\n"+
                                    "The Button at the top left will randomize the dataProvider shared by both ButtonBars, causing them to dynamically reflow their Buttons and update their selectedIndex.\n\n";
                                    
        }

		// Slider 1
		protected function handleSlider1ValueChange(event:SliderEvent):void {
			tf_Slider1Value.text = String(Slider1.value);
		}

		protected function handleSlider2ValueChange(event:SliderEvent):void {
			tf_Slider2Value.text = String(Slider2.value);
		}
		
		protected function handleSlider3ValueChange(event:SliderEvent):void {
			Slider2.value = Slider3.value;	
			tf_Slider3Value.text = String(Slider3.value);
		}

		protected function handleSlider4ValueChange(event:SliderEvent):void {
			tf_Slider4Value.text = String(Slider4.value);
		}
		
		protected function slider4Timer(e:TimerEvent) {
			if (!_slider4ThumbPressed) { 
				Slider4.value++; 
			}
		}
		
		protected function handleSlider4ThumbPress(e:Event):void {
			_slider4ThumbPressed = true;
		}
		protected function handleSlider4ThumbRelease(e:Event):void {
			_slider4ThumbPressed = false;
		}

		protected function handleBtnEnablePress(e:Event):void {
			Slider5.enabled = !Slider5.enabled
		}
		
        override protected function draw():void {
            super.draw();
        }
    }
}