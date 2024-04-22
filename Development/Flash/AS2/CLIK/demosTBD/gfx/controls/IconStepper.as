/** 
 * An OptionStepper that steps between icons instead of a text label.
 */
 
import gfx.motion.Tween;
import gfx.controls.OptionStepper;
import mx.transitions.easing.Strong;

class gfx.controls.IconStepper extends OptionStepper {
	
// Constants
// Public Properties
	public var icon:MovieClip;
	public var oldItem:String;
	public var initScale:Number = 100;
// Private Properties
	private var lastIndex:Number = -1;
	private var removeClip:MovieClip;
	
// Initialization
	public function IconStepper() { 
		super();
		Tween.init();
	}
	
// Private Methods
	private function populateText(item:Object):Void {
		super.populateText(item);
		updateIcon(item);
	}
	
	// Attach the new icon, and tween the old one out.
	private function updateIcon(item:Object):Void {
		if (oldItem == item.icon) { return; }
		var dir:Number = (lastIndex < selectedIndex ) ? 1:-1
		var str:String = item.icon;
		
		icon = this.attachMovie(str,str+'_',1000,{_x:(dir==1) ? 30 : -90 ,_y:(dir==1) ? -10 :-10,_alpha:0,_xscale:initScale,_yscale:initScale});
		
		removeClip = this.attachMovie(oldItem,oldItem+'_',-1000,{_x:30,_y:-10,_alpha:100}); 
		removeClip.tweenTo(1.5,{_x:(dir==1) ? 90 : -30,_y:(dir==1) ? -10 :-10,_alpha:0},Strong.easeInOut); 
		
		icon.tweenTo(1.5,{_x:30,_y:-10,_alpha:100,_xscale:100,_yscale:100},Strong.easeInOut);
		
		lastIndex = selectedIndex;
		oldItem = str;
	}
}