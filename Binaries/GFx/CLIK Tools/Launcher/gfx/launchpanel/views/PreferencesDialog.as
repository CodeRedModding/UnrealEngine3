package gfx.launchpanel.views {
	
	import fl.controls.Button;
	import fl.controls.TextInput;
	
	import flash.events.MouseEvent;
	
	import gfx.launchpanel.views.BaseDialog;
	
	public class PreferencesDialog extends BaseDialog {
		
		// Private Properties:
		private var _command:String;
		
		// UI Elements:
		public var commandsInput:TextInput;
		public var submitBtn:Button;
		public var cancelBtn:Button;
	
		// Initialization:
		public function PreferencesDialog() { super(); }
		
		public function set commands(command:String):void {
			_command = command;
		}
		
		// Protected Methods:
		protected override function configUI():void {
			super.configUI();
			submitBtn.addEventListener(MouseEvent.CLICK, handleSubmitClick, false, 0, true);
			submitBtn.drawNow();
			cancelBtn.addEventListener(MouseEvent.CLICK, handleCancelClick, false, 0, true);
			cancelBtn.drawNow();
			commandsInput.text = _command;
			commandsInput.drawNow();
		}
		
		protected override function getSubmitData():Object {
			return {commands:commandsInput.text};
		}
		
		// Event Handlers:
		
		private function handleSubmitClick(event:MouseEvent):void {
			//submit();
		}
		
		private function handleCancelClick(event:MouseEvent):void {
			close();
		}
	
	}
	
}