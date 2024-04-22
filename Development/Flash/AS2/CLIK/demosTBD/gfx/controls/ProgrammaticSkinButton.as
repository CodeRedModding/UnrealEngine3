/**
 * The ProgrammaticSkinButton takes programmatic skinning further, applying a color transform
 * depending on the state instead of relying on keyframes, with a settable array of colors, 
 * which can be defined in the component parameters as well as via ActionScript.
 */
import flash.geom.ColorTransform;
import gfx.controls.Button;

[InspectableList("disabled","visible","labelID","toggle","autoSize","stateColors")]
class gfx.controls.ProgrammaticSkinButton extends Button {
	
// Public properties
// Private Properties
	private var ratio:Number;
	private var steps:Number = 50;
	private var baseColor:ColorTransform;
	private var _colors:Array;
// UI Elements
	public var box:MovieClip;
	
// Initialization
	public function ProgrammaticSkinButton() { super(); }
	
// Public Methods
	[Inspectable(type="Array")]
	public function get stateColors():Array { return _colors; }
	public function set stateColors(colors:Array):Void {
		_colors = colors;
	}
	
// Private Methods
	private function configUI():Void {
		super.configUI();
		baseColor = new ColorTransform();
	}
	
	private function setState(state:String):Void {
		switch(state) {
			case "up":
			case "out":
			case "selected_up":
			case "selected_out":
				fade(box,_colors[0]);
				break
			case "over":
			case "selected_over":
				fade(box,_colors[1]);
				break;
			case "down":
			case "selected_down":
				fade(box,_colors[2]);
				break;
			case "release":
			case "selected_release":
				fade(box,_colors[3]);
				break;
			case "disabled": 
			case "selected_disbled":
				fade(box,_colors[4]);
				break;
		}
	}
	
	private function fade(p_target:MovieClip,p_colorTo:String):Void {
		baseColor = p_target.transform.colorTransform;
		var target:Number = Number(p_colorTo.split("#").join("0x"));
		var r:Number = target>>16&0xFF;
		var g:Number = target>>8&0xFF;
		var b:Number = target&0xFF;
		var count:Number=0;
		
		onEnterFrame = function() {
			count++;
			ratio = count/steps;
			baseColor.redOffset += (r - baseColor.redOffset)*ratio
			baseColor.greenOffset += (g - baseColor.greenOffset)*ratio;
			baseColor.blueOffset += (b - baseColor.blueOffset)*ratio;
			
			baseColor.redMultiplier += (0 - baseColor.redMultiplier)*ratio;
			baseColor.greenMultiplier += (0 - baseColor.greenMultiplier)*ratio;
			baseColor.blueMultiplier += (0 - baseColor.blueMultiplier)*ratio;
			
			if (baseColor.redOffset== r && baseColor.greenOffset == g && 
				baseColor.blueOffset == b && baseColor.redMultiplier == 0) { delete this.onEnterFrame;}
			p_target.transform.colorTransform = baseColor;
		}
	}

}