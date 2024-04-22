package gfx.launchpanel.views {
	
	import flash.display.MovieClip;
	import flash.display.Sprite;
	import flash.events.Event;
	
	import gfx.launchpanel.events.DialogEvent;
	import gfx.launchpanel.views.BaseDialog;
	
	public class Dialog extends Sprite {
	
		private static var instance:Dialog;
		public static function getInstance():Dialog {
			return instance;
		}
		public static function setSize(p_width:Number, p_height:Number):void {
			getInstance().iSetSize(p_width, p_height);
		}
		public static function show(p_linkage:Class, p_props:Object):BaseDialog {
			return getInstance().iShow(p_linkage, p_props);
		}
		public static function hide():void {
			getInstance().iHide();
		}
		
		// Private Properties:
		protected var content:BaseDialog;
		
		// UI Elements:
		public var bg:MovieClip;
	
		// Initialization:
		public function Dialog() {
			if (instance != null) { return; }
			instance = this;
			addEventListener(Event.ADDED_TO_STAGE, handleAddedToStage, false, 0, true);
		}
		
		// Private Methods:
		protected function configUI():void {
			visible = false;
			bg.useHandCursor = false;
		}
		
		protected function iSetSize(p_width:Number, p_height:Number):void {
			bg.width = p_width;
			bg.height = p_height;
			if (content) {
				content.x = (bg.width >> 1) - (content.width >> 1);
				content.y = (bg.height >> 1) - (content.height >> 1);
			}
		}
		
		protected function iShow(p_linkage:Class, p_props:Object):BaseDialog {
			iHide();
			
			content = new p_linkage();
			if (p_props) { content.props = p_props; }
			content.addEventListener(Event.CLOSE, close, false, 0, true);
			content.addEventListener(DialogEvent.COMPLETE, close, false, 0, true);
			addChild(content);
			content.x = (bg.width >> 1) - (content.width >> 1);
			content.y = (bg.height >> 1) - (content.height >> 1);
			
			visible = true;
			return content;
		}
		
		protected function iHide():void {
			if (content != null) {
				removeChild(content);
				content.removeEventListener(Event.CLOSE, close);
				content.removeEventListener(DialogEvent.COMPLETE, close);
				content = null;
			}
			visible = false;
		}
		
		protected function close(event:Event):void {
			iHide();
		}
		
		// Event Handlers:
		
		private function handleAddedToStage(event:Event):void {
			configUI();
		}
	}
	
}