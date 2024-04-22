/**
 * This sample adds a settable icon property to the RadioButton component.
 */

import gfx.controls.RadioButton;

class gfx.controls.IconRadioButton extends RadioButton {
	
// Public Properties
// UI Elements
	private var iconClip:MovieClip;

// Initialization
	public function IconRadioButton() { super(); }

// Public Properties
	[Inspectable(name="icon", default="")]
	public function get icon():String { return null; }
	public function set icon(value:String):Void {
		if (iconClip != null) { 
			iconClip.removeMovieClip(); 
			iconClip = null;
		}
		if (value == "") { return; }
		iconClip = this.attachMovie(value, "iconClip", 1000, {_x:5, _y:2});
		iconClip._xscale = 10000 / _xscale;
		iconClip._yscale = 10000 / _yscale;
	}
}