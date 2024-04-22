/**
 * The ProgramaticButton class dynamically draws the states of the Button instead of relying on
 * keyframes. The setState method is overridden to handle this functionality.
 */
import gfx.controls.Button;

class gfx.controls.ProgrammaticButton extends Button {
	
// Static Properties
	public var WIDTH:Number = 200;
	public var HEIGHT:Number = 22;	
// Public Properties
// Private Properties
	private var depth:Number = 100;
	
// Initialization
	public function ProgrammaticButton() { super(); }
	
// Public Methods
// Private Methods
	private function configUI():Void {
		super.configUI();
		setState("up");
	}
	
	private function setState(state:String):Void {
		if (this["button"]) { this["button"].removeMovieClip(); }
		var button:MovieClip;
		
		switch(state) {
			case "up":
			case "out":
			case "selected_up":
			case "selected_out":
				button = drawButton(0x0066FF,WIDTH,HEIGHT,_label,depth);
				break
			case "over":
			case "selected_over":
				button = drawButton(0x00FF00,WIDTH,HEIGHT,_label,depth);
				break;
			case "down":
			case "selected_down":
				button = drawButton(0x0000FF,WIDTH,HEIGHT,_label,depth);
				break;
			case "release":
			case "selected_release":
				button = drawButton(0xFF9900,WIDTH,HEIGHT,_label,depth);
				break;
			case "disabled": 
			case "selected_disbled":
				button = drawButton(0xCCCCCC,WIDTH,HEIGHT,_label,depth);
				break;
		}
		depth++
	}
	
// Draw a single button state based on instructions.
	private function drawButton(p_color:Number,p_width:Number,p_height:Number,p_label:String,p_depth:Number):MovieClip {
		var button:MovieClip = this.createEmptyMovieClip("button",p_depth);
		
		var tf:TextFormat = new TextFormat();
		tf.font = "Verdana";
		
		var labelField:TextField = button.createTextField("label",200+p_depth,0,0,WIDTH,HEIGHT);
		labelField.text = p_label;
		labelField.setTextFormat(tf);
		labelField.embedFonts = true;
		labelField.selectable = false;
		labelField.border = true;
		
		button.beginFill(p_color,100);
		button.lineStyle(1,p_color,100);
		button.lineTo(p_width,0);
		button.lineTo(p_width,p_height);
		button.lineTo(0,p_height);
		button.endFill();
		return button;
	}
	
}