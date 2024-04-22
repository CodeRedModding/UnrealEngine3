package gfx.launchpanel.events {
	
	import flash.events.Event;
	
	public class EditEvent extends Event {
		
		public static var ADD_PLAYER:String = "EditEvent.addPlayer";
		public static var REMOVE_PLAYER:String = "EditEvent.removePlayer";
		public static var PLAYER_CHANGE:String = "EditEvent.playerChange";
		
		public var data:Object;
		
		public function EditEvent(type:String, data:Object = null, bubbles:Boolean = false, cancelable:Boolean = false):void{
			this.data = data;
			super(type,bubbles,cancelable);
		}
		
		public override function clone():Event {
			return new EditEvent(type, data, bubbles, cancelable);
		}
		
		public override function toString():String {
			return formatToString("EditEvent", "type", "data", "bubbles", "cancelable");
		}
		
	}
	
}