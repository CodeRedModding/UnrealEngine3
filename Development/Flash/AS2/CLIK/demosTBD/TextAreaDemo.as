/**
 * This sample demonstrates the TextArea component.
 */
 
import gfx.controls.TextArea;
import gfx.controls.ScrollBar

class TextAreaDemo extends MovieClip {
	
	public var ta:TextArea;
	public var sb:ScrollBar;
	public var pos:TextField;
	
	public function TextAreaDemo() { }
	
	public function onLoad():Void {
		ta.addEventListener("textChange",this,"changeHandler");
		ta.text = [1,2,3,4,5,6,7,8,9,10].join("\n");
	}
	
	private function changeHandler(e:Object):Void {
		trace("Text Change event");
	}
}