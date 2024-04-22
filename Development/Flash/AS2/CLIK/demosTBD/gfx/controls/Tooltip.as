/**
 * The Tooltip class displays a popUp control with a text property in the TOOLTIP_DEPTH of the PopUpManager. A delay can be specified after which the Tooltip is displayed.
 */
import gfx.core.UIComponent;
import gfx.managers.PopUpManager;
import gfx.utils.Delegate;

class gfx.controls.Tooltip extends Object {
	
// Public Properties
	public static var currentTooltip:MovieClip;
	
// Private Properties
	private static var open:Boolean = false;
	private static var delayInterval:Number;
	
	
// Public Methods	
	public static function show(linkage:String, text:String, props:Object, relativeTo:MovieClip, delay:Number):MovieClip {
		hide();
		
		// Create new Tooltip
		var tooltip:MovieClip = PopUpManager.createPopUp(linkage, props, relativeTo, PopUpManager.TOOLTIP_DEPTH);
		if (tooltip == null) { return null; }
		currentTooltip = tooltip;
		currentTooltip.text = text;
		
		// Reposition tooltup
		tooltip._y -= tooltip._height;
		
		// Show the Tooltip
		if (delay == 0 || delay == null || isNaN(delay)) {
			open = true;
		} else if (delay == -1) { // Set to -1 to not open.			
			currentTooltip._visible = false;
		} else {
			currentTooltip._visible = false;
			delayInterval = setInterval(Delegate.create(Tooltip, showTooltip), delay); // Need delegate.. seems callback won't work on static class?
		}
		
		return tooltip;
	}
	
// Public
	public static function hide():Void {
		clearInterval(delayInterval);
		delete delayInterval;
		if (currentTooltip != null) { 
			currentTooltip.removeMovieClip();
			delete currentTooltip;
		}
	}
	
// Private Methods
	private static function showTooltip():Void {
		clearInterval(delayInterval);
		open = true;
		currentTooltip._visible = true;
	}
	
}