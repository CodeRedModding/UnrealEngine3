/**
 * This component extends the ProgressBar, and draws the progress programmatically, rather than
 * using a keyframe approach. Additionally, it adds a direction property, which can be used to
 * change the direction of the bar animation.
 */
 
import gfx.controls.ProgressBar;
import gfx.motion.Tween;

class gfx.controls.ProgrammaticProgressBar extends ProgressBar {
	
// Public Properties
	public var direction:String;
	
// Private Properties
// UI Elements
	public var bar:MovieClip;
	public var loadedBar:MovieClip;
	
// Initialization
	public function ProgrammaticProgressBar() { super(); } 
	
// Private Properties
	private function configUI():Void {
		super.configUI();
		bar._width = 200;
		Tween.init();
	}
	
	private function updatePosition():Void {		
		var percent:Number = percentLoaded / 100;		
		if (percent >= 0.7) { 
			bar.gotoAndStop(2); 
		}
		if (direction == "pos") { 
			bar.tweenTo(.5,{_width:__width*percent});
			loadedBar.tweenTo(.5,{_x:__width*percent});
			loadedBar.nameField.text = percent*100 + " %";
		}else {
			bar.tweenFrom(.5,{_width:__width-(__width*percent)});
			loadedBar.tweenTo(.5,{_x:__width-(__width*percent)});
			loadedBar.nameField.text = 100-(Math.floor(percent*100)) + " %";
		}
		
	}
}
