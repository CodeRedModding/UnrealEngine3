package gfx.launchpanel.views {
	
	import fl.controls.Button;
	import fl.controls.TextInput;
	
	import flash.events.MouseEvent;
	
	import gfx.launchpanel.views.BaseDialog;
	import gfx.launchpanel.events.DialogEvent;
	
	public class ProfileDialog extends BaseDialog {
		
		public var profileInput:TextInput;
		public var saveBtn:Button;
		public var cancelBtn:Button;
		
		public function ProfileDialog() {
			super();
		}
		
		protected override function configUI():void {
			super.configUI();
			saveBtn.addEventListener(MouseEvent.CLICK, handleSaveClick, false, 0, true);
			saveBtn.drawNow();
			cancelBtn.addEventListener(MouseEvent.CLICK, handleCancelClick, false, 0, true);
			cancelBtn.drawNow();
			profileInput.drawNow();
		}
		
		protected function handleSaveClick(event:MouseEvent):void {
			dispatchEvent(new DialogEvent(DialogEvent.COMPLETE, {profileName:profileInput.text}));
		}
		
		protected function handleCancelClick(event:MouseEvent):void {
			close();
		}
		
	}

}