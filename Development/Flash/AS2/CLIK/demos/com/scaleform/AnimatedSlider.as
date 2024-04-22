/**
 * This sample adds animation to the basic Slider class found in the Skin component set.
 * This component overrides some of the methods in the Slider class, and tweens the position
 * of the thumb instead of snapping it.
 * 
 */
import gfx.controls.Slider;
import gfx.motion.Tween;

import mx.transitions.easing.Strong;

class com.scaleform.AnimatedSlider extends Slider {

// Public Properties
// Private Properties
	private var speed:Number = .5;
	private var _islive:Boolean = true;
	
// Initialization
	public function AnimatedSlider() { 
		super(); 
		Tween.init();
	}
	
// Public Methods
// Private Methods
	private function updateThumb(newValue:Number):Void {
		if (newValue == null) { newValue = _value; }
		if (_islive) {
			var val:Number = ((_value - _minimum) / (_maximum - _minimum) * __width) - thumb._width/2;
			MovieClip(thumb).tweenTo(speed,{_x:val},Strong.easeInOut);
			dispatchEvent({type:"change"});
		} else {
			thumb._x = ((_value - _minimum) / (_maximum - _minimum) * __width) - thumb._width/2;
		}
	}
	
	private function beginDrag(event:Object):Void {
		super.beginDrag();
		_islive = false;
	}
	
	private function doDrag():Void {
		var thumbPosition:Number = _xmouse - dragOffset.x;
		var newValue:Number = lockValue(thumbPosition / __width * (_maximum-_minimum) + _minimum);
		updateThumb(newValue);
		if (value == newValue){ return; }
		_value = newValue;
		if (liveDragging) { dispatchEvent({type:"change"}); }
	}
	
	private function endDrag():Void {
		super.endDrag();
		_islive = true;
	}
	
	private function trackPress(e:Object):Void {
		Selection.setFocus(track);
		var newValue:Number = lockValue(_xmouse / __width * (_maximum-_minimum) + _minimum);
		if (value == newValue) { return; }		
		value = newValue;
	}	
}