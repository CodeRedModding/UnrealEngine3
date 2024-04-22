package com.scaleform.clikbrowser.events {
	
	import flash.events.Event;
	
	public class TabEvent extends Event {
		
	// Constants:
		public static const SELECT:String = "tabSelect";
		
	// Public Properties:
		public var data:Object;
		
	// Protected Properties:
		
	// Initialization:
		public function TabEvent(type:String, data:Object=null, bubbles:Boolean=true, cancelable:Boolean=false) {
			super(type, bubbles, cancelable);
			this.data = data;
		}	
		
	// Public getter / setters:
		
	// Public Methods:
		override public function clone():Event {
			return new TabEvent(type, data, bubbles, cancelable);
		}
		
		override public function toString():String {
			return formatToString("TabEvent", "type", "data", "bubbles", "cancelable");
		}
		
	// Protected Methods:
		
	}
	
}