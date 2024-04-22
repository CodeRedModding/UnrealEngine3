/**
 * This sample extends the ListItemRender to colorize a swatch portion of the itemRenderer. This class
 * is used as an itemRenderer for the ColorPicker.
 */

import gfx.controls.ListItemRenderer;

[InspectableList("disabled", "visible")]
class gfx.controls.ColorSwatch extends ListItemRenderer {
	
	public var swatch:MovieClip;
	
	public function ColorSwatch() { super(); }
	
	public function setData(data:Object):Void {
		super.setData(data);
		setColor(parseInt(data.toString()));
	}
	
	private function setColor(value:Number):Void {
		var color:Color = new Color(swatch.bg);
		color.setRGB(value)
	}
	
	
}