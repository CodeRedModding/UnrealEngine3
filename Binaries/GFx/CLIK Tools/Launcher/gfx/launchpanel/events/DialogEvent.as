package gfx.launchpanel.events {
	
	import flash.events.Event;
	
	public class DialogEvent extends Event {
		
		public static var COMPLETE:String = "DialogEvent.complete";
		public static var CANCEL:String = "DialogEvent.cancel";
		
		public var data:Object;
		
		public function DialogEvent(type:String, data:Object = null, bubbles:Boolean = false, cancelable:Boolean = false):void{
			this.data = data;
			super(type,bubbles,cancelable);
		}
		
		public override function clone():Event {
			return new DialogEvent(type, data, bubbles, cancelable);
		}
		
		public override function toString():String {
			return formatToString("DialogEvent", "type", "bubbles", "cancelable");
		}
		
	}
	
}