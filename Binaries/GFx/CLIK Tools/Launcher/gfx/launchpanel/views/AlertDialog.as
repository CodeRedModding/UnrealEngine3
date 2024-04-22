package gfx.launchpanel.views {
	
	import fl.controls.Button;
	import fl.controls.TextInput;
	
	import flash.display.Sprite;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	
	import gfx.launchpanel.views.BaseDialog;
	
	public class AlertDialog extends BaseDialog {
		
		// UI Elements:
		
		public var messageInput:TextField;
		public var okBtn:Button;
		public var bg:Sprite;
	
		// Initialization:
		
		public function AlertDialog() { super(); }
		
		// Public Methods:
		
		public override function setLabels(itemsHash:Object):void {
			okBtn.label = itemsHash["LP_BTN_OK"];
		}
		
		// Private Methods:
		
		protected override function configUI():void {
			super.configUI();
			messageInput.text = props.message;
			messageInput.height = messageInput.textHeight * messageInput.numLines;
			messageInput.y = titleField.y + titleField.height;
			okBtn.addEventListener(MouseEvent.CLICK, handleOKClick, false, 0, true);
			okBtn.drawNow();
			okBtn.y = messageInput.y + messageInput.height;
			bg.height = okBtn.y + okBtn.height + 5; // 5 = padding.
		}
		
		protected function handleOKClick(event:MouseEvent):void {
			close();
		}
		
	}
	
}