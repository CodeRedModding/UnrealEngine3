import gfx.controls.Button;

class com.scaleform.DataGridHeader extends Button {
	
// Constants:
// Public Properties:	
// Private Properties:	
	private var _descending:Boolean = false;
// UI Elements:
	public var sortArrow:MovieClip;	


// Initialization:
	private function DataGridHeader() { super(); }

// Public Methods:
	/**
	 * Determine if this button is set to descending.
	 */
	public function get descending():Boolean { return _descending; }
	public function set descending(value:Boolean):Void {
		_descending = value;
		sortArrow._rotation = _descending ? 180 : 0;
	}
	
	public function get selected():Boolean { return _selected; }
	public function set selected(value:Boolean):Void {
		super.selected = value;
		sortArrow._visible = _selected;
		if (!_selected) { 
			descending = false; // When deselected, reset the descending property.
		}
	}
	
// Private Methods:
	private function configUI():Void {
		super.configUI();
		sortArrow._visible = false; // Hide on start.
	}
	
	private function handleClick(controllerIdx:Number):Void {
		if (_selected) {
			descending = !_descending; // When clicked and already selected, toggle the descending property.
		} else {
			descending = false; // When first clicked, set to non-descending.
		}		
		var flags:Number = 0 | (_descending?Array.DESCENDING:0);
		group.dispatchEvent({type:"sort", field:data, flags:flags}); // Dispatch a sort event from the group rather than from the header. One event is better than multiple.
		
		super.handleClick(controllerIdx);
	}

}