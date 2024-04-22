/**
 * This sample shows the Button usage:
 *  - Buttons added on stage
 *  - Buttons added via ActionScript
 *  - Labels set via ActionScript, and using component parameters
 *  - Properties set via ActionScript, and using component parameters
 *  - Event handling from Button click events.
 *  - Creating a Button that uses animated states (AnimatedButton in the library)
 */

import gfx.controls.Button;

class com.scaleform.ButtonDemo extends MovieClip {
	
	public var btn1:Button;
	public var btn2:Button;
	public var btn3:Button;
	public var btn:Button;
	
	public function ButtonDemo() { }
	
	public function onLoad():Void {
		configUI();
	}
	
	private function configUI():Void {
		btn1.addEventListener("click", this, "onClick");
		btn2.addEventListener("click", this, "onClick");
		
		btn1.doubleClickEnabled = true;
		btn1.addEventListener("doubleClick", this, "handleDoubleClick");
		btn2.addEventListener("doubleClick", this, "handleDoubleClick");
		btn2.autoRepeat = true;
		
		btn3 = Button(attachMovie((_name == "skinnedMain") ? "ButtonSkinned" : "Button", "btn3", 1, {_x:24, _y:105}));
		btn3.width = 200;
		btn3.label = "Change Toggle Property";
		btn3.addEventListener("click", this, "onClick");
		btn3.toggle = true;
		
		Selection.setFocus(btn1);
	}
	
	private function onClick(event:Object):Void {
		switch (event.target) {
			case btn1:
				btn.label = "Selected has been toggled!";
				btn.selected = !btn.selected;
				break;
			case btn2:
				btn.label = "Disabled has been toggled!";
				btn.disabled = !btn.disabled;
				break;
			case btn3:
				btn.label = "Toggle has been toggled!"
				btn.toggle = !btn.toggle;
				break;
		}
	}
	
	private function handleDoubleClick(event:Object):Void {
		trace("[Double Click]");
	}
}