import gfx.core.UIComponent;
import gfx.controls.ViewStack;
import gfx.ui.InputDetails;
import gfx.ui.NavigationCode;
import gfx.utils.Delegate;
import gfx.motion.Tween;

class gfx.controls.RadialMenu2 extends UIComponent {
	
	public var segments:Number = 4//8;
	public var offset:Number = -45//-23;
	
	public var b0:MovieClip;
	public var b1:MovieClip;
	public var b2:MovieClip;
	public var b3:MovieClip;
	public var b4:MovieClip;
	public var b5:MovieClip;
	public var b6:MovieClip;
	public var b7:MovieClip;
	
	private var vs:ViewStack;
	private var selectedButton:MovieClip;
	
	private var but1:MovieClip;
	private var but2:MovieClip;
	private var but3:MovieClip;
	private var but4:MovieClip;
	
	private var old:MovieClip;
	private var angle:Array;
	
	public function RadialMenu2() { 
		super(); 
		Tween.init();
		angle = [90,180,-90,0];
		
	}
	
	private function configUI():Void {
		super.configUI();
		var tf:TextFormat = new TextFormat();
		tf.font = "Verdana";
		tf.size = 10;
		tf.color = 0x0099FF;
		trace('first???');
		var increment:Number = 360/segments;
		var centerX = this._width >> 1;
		var centerY = this._height >> 1;
		for(var i:Number=1;i<=segments;i++) {
			var labelAngle:Number = increment*i+increment/2-135;
			var labelRad:Number = labelAngle*Math.PI/180;

			var h:MovieClip = this.createEmptyMovieClip("label"+(2000+i), 2000+i);
			trace(h);
			h.createTextField("segmentLabel",0,0,0,20,20);

			var lab:TextField = h.segmentLabel;

			lab.autoSize = "left";
			lab.text = 'label' + i;

			var tf:TextFormat = new TextFormat();
			tf.font = "Verdana";
			tf.size = 12;
			tf.color = 0x0099FF;
			lab.setTextFormat(tf);
			lab.embedFonts = true;
			lab.selectable = false;

			lab._x -= lab._width/2;
			lab._y -= lab._height/2;

			h._x = Math.cos(labelRad)*(150)+centerX;
			h._y = Math.sin(labelRad)*(150)+centerY;
			
		}
	}
	
	public function setAngle(p_angle:Number):Void {
		var index:Number = Math.floor((p_angle-offset)%360/(360/segments));
		if ( old == selectedButton) {
			selectedButton.tweenTo(.5,{_xscale:100,_yscale:100,_alpha:100});
		}
		selectedButton = this['but'+(index+1)];
		selectedButton.selected = true;
		selectedButton.focused = true;
		selectedButton.tweenTo(.5,{_xscale:130,_yscale:130,_alpha:50});
		this['label200'+(index+1)].tweenTo(.5,{_xscale:130,_yscale:130});
		//trace(this['label200'+index+1])
		old = selectedButton;
		selectedButton.onTweenComplete = Delegate.create(this,onComplete);
	}
	
	public function onComplete(p_event:Object):Void {
		//trace('complete');
	}
	
	/*public function handleInput(details:InputDetails, pathToFocus:Array):Boolean {
		selectedButton.selected = false;
		selectedButton.focused = false;
		super.handleInput(details,pathToFocus);
		switch(details.code) {
			case 49:
				selectedButton = this['but1'];
				selectedButton.selected = true;
				selectedButton.focused = true;
				return true;
				break;
			case 50:
				selectedButton = this['but2'];
				selectedButton.selected = true;
				selectedButton.focused = true;
				return true;
				break;
			case 51:
				selectedButton = this['but3'];
				selectedButton.selected = true;
				selectedButton.focused = true;
				return true;
				break;
			case 52:
				selectedButton = this['but4'];
				selectedButton.selected = true;
				selectedButton.focused = true;
				return true;
				break;

		}
		return false;
	}*/
	
}