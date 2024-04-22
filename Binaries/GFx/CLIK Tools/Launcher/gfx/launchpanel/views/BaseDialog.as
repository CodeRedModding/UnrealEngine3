package gfx.launchpanel.views {
	
	import flash.display.MovieClip;
	import flash.events.Event;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	
	import gfx.launchpanel.events.DialogEvent;
	
	public class BaseDialog extends MovieClip {
		
		public static const PADDING:Number = 5;
		
		public var titleField:TextField;
		public var props:Object;
	
		// Initialization:
		public function BaseDialog() {
			addEventListener(Event.ADDED_TO_STAGE, handleAddedToStage, false, 0, true);
		}
		
		public function setLabels(itemsHash:Object):void {}
		
		// Protected Methods:
		protected function configUI():void {
			if (titleField != null) {
				titleField.text = props.title || "Dialog";
				titleField.height = titleField.textHeight * titleField.numLines;
			}
		}
		
		protected function close():void {
			dispatchEvent(new Event(Event.CLOSE));
		}
		
		/*protected function submit():void {
			if (!validate()) { return; }
			dispatchEvent(new DialogEvent(DialogEvent.SUBMIT));
		}*/
		
		protected function validate():Boolean {
			return true;
		}
		
		protected function getSubmitData():Object {
			return null;
		}
		
		// Event Handlers:
		protected function handleAddedToStage(event:Event):void {
			configUI();
		}
	
	}
	
}