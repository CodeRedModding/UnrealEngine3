/**
 * The ProgrammaticProgressBarGuage extends the ProgressBar, and uses rotation to indicate progress loaded.
 * The updatePosition method is overridden to do the rotation instead of a keyframe approach.
 */
 
import gfx.controls.ProgressBar;

class gfx.controls.ProgrammaticProgressBarGauge extends ProgressBar {

// Private Properties
	private var vRange:Number = 0;
	private var degRatio:Number = 0;
	private var targetRot:Number;
	
// Initialization
	public function ProgrammaticProgressBarGauge() { super(); }
	
// Private Properties	
	private function updatePosition():Void {
		var percent:Number = (_value - _minimum) / (_maximum - _minimum);
		vRange = _maximum-_minimum;
		degRatio = 240/vRange;
		targetRot = -140+degRatio*((percent*100)-_minimum);
		_rotation = targetRot;
	}
}
