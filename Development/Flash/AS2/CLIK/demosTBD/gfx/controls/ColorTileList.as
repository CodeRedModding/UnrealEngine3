/**
 * This sample extends the TileList to set some ColorPicker properties directly on the class, rather than
 * externally.
 */

import gfx.controls.TileList

[InspectableList("disabled", "visible", "inspectableDropdown", "rowHeight", "columnWidth")]
class gfx.controls.ColorTileList extends TileList {
	
	public function ColorTileList() { 
		super(); 
	}
	
	private function configUI():Void {
		super.configUI();
		
		columnWidth = 10;
		direction = "veritcal";
		rowHeight = 10;
		itemRenderer = "ColorSwatch";
	}
	
}