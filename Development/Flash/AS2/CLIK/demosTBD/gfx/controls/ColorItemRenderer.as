/**
 * This sample sdds a swatch to the ListItemRenderer, which is colored using the data property as a color.
 */

import gfx.controls.ListItemRenderer;

class gfx.controls.ColorItemRenderer extends ListItemRenderer {
	
	public var icon:MovieClip;
	
	public function ColorItemRenderer() { super(); }
	
	public function setData(data:Object):Void {
		super.setData(data);
		var color:Number = parseInt(data.split("#").pop().toString(), 16);
		setColor(color);		
	}
	
	private function setColor(value:Number):Void {
		var color:Color = new Color(icon);
		color.setRGB(value);
	}
}