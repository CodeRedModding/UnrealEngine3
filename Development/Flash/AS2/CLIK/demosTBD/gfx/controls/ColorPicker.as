/**
 * This sample adds a color swatch to the component, which is updated based on the dataProvider value. It also makes
 * the color array settable using component parameters. Note that this sample is not complete.
 */
 
import gfx.controls.DropdownMenu;

[InspectableList("disabled", "visible", ,"inspectableDropdown", "colors")]
class gfx.controls.ColorPicker extends DropdownMenu {
	
	public var icon:MovieClip;
	private var _colors:Array;
	private var isData:Boolean = true;
	
	public function ColorPicker() { super(); }
	
	[Inspectable(type="Array")]
	public function get colors():Array { return _colors; }
	public function set colors(colors:Array):Void {
		_colors = colors;
		isData = false;
	}
	
	private function setColor(value:Number):Void {
		var color:Color = new Color(icon);
		color.setRGB(value)
	}
	
	private function updateSelectedItem():Void {
		super.updateSelectedItem();
		
		if (dataProvider.length == 0) { dataProvider = _colors; }
		var color:Number = isData ? parseInt(selectedItem.toString()) : parseInt(selectedItem.split("#").pop(), 16); 
		setColor(color);
	}
	
}