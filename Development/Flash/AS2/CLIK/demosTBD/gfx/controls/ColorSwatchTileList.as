/**
 * This sample extends the TileList to add color rollOver support, as well as a TextField color preview. Note that this
 * sample is not complete.
 */

import gfx.controls.TileList;
import gfx.utils.Constraints;

class gfx.controls.ColorSwatchTileList extends TileList {
	
	public var textField:TextField;
	public var constraints:Constraints;
	
	
	public function ColorSwatchTileList() { super(); }
	
	private function configUI():Void {
		constraints = new Constraints(this, true);
		constraints.addElement(textField, Constraints.ALL);
		super.configUI();
		updateAfterStateChange();
	}
	
	private function updateAfterStateChange():Void {	
		//if (textField != null && _label != null) { textField.text = _label; }		
		if (constraints != null) { 
			constraints.update(width, height);
		}
	}
	
	
	private function createItemRenderer(index:Number):MovieClip {
		var clip:MovieClip = super.createItemRenderer(index);
		clip.addEventListener("rollOver", this, "handleItemRollOver");
		return clip;
	}
	
	private function handleItemRollOver(event:Object):Void {
		textField.text = validateColor(event.target.data);
		selectedIndex = event.target.index;
		var newEvent:Object = {
			type:"itemOver", 
			item:event.target.data, 
			renderer:event.target, 
			index:event.target.index
		};
		dispatchEvent(newEvent);
	}
	
	private function validateColor(value:Number):String {
		var num:String = value.toString(16);
		var prefix:String = '';
		var l:Number  = num.length;
		switch(l) {
			case 1:
				prefix = '00000';
				break;
			case 2: 
				prefix = '0000';
				break;
			case 4:
				prefix = '00';
		}
		return '#'+prefix+num;
	}
	
}