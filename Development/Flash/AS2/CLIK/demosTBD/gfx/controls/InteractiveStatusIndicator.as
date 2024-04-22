/**
 * This sample adds keyboard/controller interactivity to a StatusIndicator so that it can update
 * its status via user input. There is no focus on this component, so it must be a container component
 * in its own handleInput() method.
 */

import gfx.controls.StatusIndicator;

class gfx.controls.InteractiveStatusIndicator extends StatusIndicator {
	
// Constants:	
	public static var CLASS_REF = demos.gfx.controls.InteractiveStatusIndicator;
	public static var LINKAGE_ID:String = "InteractiveStatusIndicator";

// Public Properties:
// Private Properties:
// UI Elements:

// Initialization:
	private function InteractiveStatusIndicator() { super(); }

// Public Methods:	
	public function handleInput(details:InputDetails, pathToFocus:Array):Boolean {
		switch (details.navEquivalent) {
			case NavigationCode.LEFT:
				value = _value - 1;
				return true;
			case NavigationCode.RIGHT:
				value = _value + 1;
				return true;
		}
		return false;
	}
	
// Private Methods:

}