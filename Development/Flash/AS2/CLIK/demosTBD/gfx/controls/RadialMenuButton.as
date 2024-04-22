import gfx.controls.Button;
import flash.geom.ColorTransform;

class gfx.controls.RadialMenuButton extends Button {
	
	private var _clip:MovieClip;
	private var _xPosition:Number;
	private var _yPosition:Number;
	private var _startAngle:Number;
	private var _arc:Number;
	private var _radius:Number;
	private var _lineColor:Number;
	private var _fillColor:Number;
	private var _fillAlpha:Number;
	private var _yRadius:Number;
	private var _depth:Number;
	private var _lineThickness:Number;
	private var ct:ColorTransform
	private var ct2:ColorTransform
	
	public function RadialMenuButton() {
		super();
	}
	
	public function init(x, y, startAngle, arc, radius, lineColor, fillColor, fillAlpha, yRadius):Void {
		_xPosition = x;
		_yPosition = y;
		_startAngle = startAngle;
		_arc = arc;
		_radius = radius;
		_lineColor = lineColor;
		_fillColor = fillColor;
		_fillAlpha = fillAlpha;
		_yRadius = yRadius;
		_lineThickness = 1
		drawWedge();
		
		 ct = this.transform.colorTransform;
		 ct2 = this.transform.colorTransform;
	}
	
	private function drawWedge() {
		this.clear();
		// move to x,y position 
		this.lineStyle(_lineThickness,_lineColor,100);
		this.beginFill(_fillColor,_fillAlpha);
		this.moveTo(_xPosition,_yPosition);
		// if yRadius is undefined, yRadius = radius
		if (_yRadius == undefined) { _yRadius = _radius; }
		// Init vars  
		var segAngle, theta, angle, angleMid, segs, ax, ay, bx, by, cx, cy;
		// limit sweep to reasonable numbers
		if (Math.abs(_arc)>360) { _arc = 360; }
		// Flash uses 8 segments per circle, to match that, we draw in a maximum  
		// of 45 degree segments. First we calculate how many segments are needed
		// for our arc.
		segs = Math.ceil(Math.abs(_arc)/45);

		// Now calculate the sweep of each segment.
		segAngle = _arc/segs;
		// The math requires radians rather than degrees. To convert from degrees
		// use the formula (degrees/180)*Math.PI to get radians.
		theta = -(segAngle/180)*Math.PI;
		// convert angle startAngle to radians
		angle = -(_startAngle/180)*Math.PI;
		// draw the curve in segments no larger than 45 degrees.
		if (segs>0) {
			// draw a line from the center to the start of the curve
			ax = _xPosition+Math.cos(_startAngle/180*Math.PI)*_radius;
			ay = _yPosition+Math.sin(-_startAngle/180*Math.PI)*_yRadius;
			if (_arc == 360) {
				this.moveTo(ax,ay);
			} else {
				this.lineTo(ax,ay);
			}
			// Loop for drawing curve segments
			for (var i = 0; i<segs; i++) {
				angle += theta;
				angleMid = angle-(theta/2);
				bx = _xPosition+Math.cos(angle)*_radius;
				by = _yPosition+Math.sin(angle)*_yRadius;
				cx = _xPosition+Math.cos(angleMid)*(_radius/Math.cos(theta/2));
				cy = _yPosition+Math.sin(angleMid)*(_yRadius/Math.cos(theta/2));
				this.curveTo(cx,cy,bx,by);
			}
			// close the wedge by drawing a line to the center
			if (_arc == 360) {
				this.moveTo(_xPosition,_yPosition);
			} else {
				this.lineTo(_xPosition,_yPosition);
			}
		}
	}
	private function setState(state:String):Void {
		this.state = state;
		
		
		
		switch(state) {
			case 'up':
			case 'out':
			case 'selected_up':
			case 'selected_out':
				this.transform.colorTransform = ct2;
				//changeColor(0,0,0,1,0,120,0,0)
				break
			case 'over':
			case 'selected_over':
				changeColor(-1,-1,-1,1,255,120,0,1)
				break;
			case 'down':
			case 'selected_down':
				changeColor(1,-1,1,1,255,120,0,1)
				break;
			case 'release':
			case 'selected_release':
				changeColor(1,-1,1,1,255,120,0,1)
				break;
			case 'disabled': 
			case 'selected_disbled':
				changeColor(1,-1,1,1,255,120,0,1)
				break;
		}
		
	}
	
	private function changeColor(p_rMulti,p_gMulti,p_bMulti,p_aMulti,p_rOffset,p_gOffset,p_bOffset,p_aOffset):Void {
		ct.redMultiplier = p_rMulti;
		ct.greenMultiplier = p_gMulti;
		ct.blueMultiplier = p_bMulti;
		ct.alphaMultiplier = p_aMulti;
		
		ct.redOffset = p_rOffset;
		ct.greenOffset = p_gOffset;
		ct.blueOffset = p_bOffset;
		ct.alphaOffset = p_aOffset;
		
		this.transform.colorTransform = ct;
	}
}