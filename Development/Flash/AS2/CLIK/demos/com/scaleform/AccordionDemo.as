/**
 * A sample of the components working together to create a " Horizontal Accordion". The buttons all belong to a group, which 
 * dispatches change events when one of the button is selected, and deselects the other button. When the selected
 * button changes, the buttons and the view are all repositioned.  This ample can not be animated, as the same ViewStack 
 * is used for each view.
 *
 * Each button's "data" property is a string value representing a linkage in the library. 
 * Only a single event is necessary - a CHANGE event on the "group" of the first button.
 * The view is a ViewStack, which caches each view once it has been loaded, and only displays a single view at once.
 *
 * The headers have a labelFunction that converts the string label into a vetical label by adding line breaks.
 */

import gfx.controls.Button;
import gfx.controls.ViewStack;
import com.scaleform.IconRadioButton;

class com.scaleform.AccordionDemo extends MovieClip {
	
// Public Properties
// Private Properties
	private var buttons:Array;
// UI Elements
	public var h1:IconRadioButton;
	public var h2:IconRadioButton;
	public var h3:IconRadioButton;
	public var contentView:ViewStack;
	
// Initialization
	public function AccordionDemo() { }
	private function onLoad():Void {
		buttons = [h1,h2,h3]; // Create a list of Buttons.
		h1.group.addEventListener("change", this, "handleChange"); // Listen for group changes.
		convertLabels();
		h1.selected = true;
	}
	
// Private Methods
	// Since there is no labelFunction on the Button class, we have to do this ourselves manually, one time.
	// Add line breaks to each label.
	private function convertLabels():Void {
		var l:Number = buttons.length;
		for(var i:Number=0;i<l;i++) {
			var str:String = buttons[i].label.split("").join("\n");
			buttons[i].label = str;
		}
	}
	
	// The group has changed, reposition the buttons
	private function handleChange(e:Object):Void {
		var xPosition:Number = h1._x; // Start at the top of the first button.
		for (var i:Number=0; i<buttons.length; i++) {
			var b:Button = buttons[i];
			b._x = xPosition;
			b._y = h1._y; // Align the y position to the first button
			xPosition += b._width;
			if (h1.group.selectedButton == b) { // If the current button is selected
				contentView._x = xPosition; // Re-position the view
				contentView._y = b._y;
				xPosition += contentView._width;
			}
		}
	}
	
}