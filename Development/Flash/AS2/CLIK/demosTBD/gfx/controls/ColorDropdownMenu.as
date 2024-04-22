/**
 * This sample extends a DropdownMenu to dispatch color events on itemRollOver, so the sample can
 * change the swatch preview as the user rolls over colors. Note that this will be easier once the 
 * native lists dispatch itemRollOver and itemRollOut events.
 */
 
import gfx.controls.DropdownMenu;
import gfx.managers.PopUpManager;

[InspectableList("disabled", "visible", "inspectableDropdown", "dropdownWidth")]
class gfx.controls.ColorDropdownMenu extends DropdownMenu {
	
// Public Properties
// UI Elements
	public var bg:MovieClip;
	
// Initialization
	public function ColorDropdownMenu() { super(); }
	
// Public Methods
	public function get dropdown():Object { return _dropdown; }
	public function set dropdown(value:Object):Void { 
		super.dropdown = value;
		_dropdown.addEventListener("itemRollOver",this,"handleColorChange");
	}
	
// Private Methods
	private function handleColorChange(event:Object):Void {
		var color:Number = event.item;
		dispatchEvent({type:"colorChange", data:color});
	}
	
}