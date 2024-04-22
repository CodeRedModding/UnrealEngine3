import gfx.controls.RadioButton;
import gfx.ui.InputDetails;
import gfx.ui.NavigationCode;

class gfx.controls.RadialMenuItem extends RadioButton {
	
	public function RadialMenuItem() { super(); }
	
	public function handleInput(details:InputDetails, pathToFocus:Array):Boolean {
		switch(details.navEquivalent) {
			case NavigationCode.ENTER:
				handleClick();
				return true;
		}
		return false;
	}
}