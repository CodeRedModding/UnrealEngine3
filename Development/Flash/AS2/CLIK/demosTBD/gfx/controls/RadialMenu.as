/**
 * The RadialMenu draws a pie-shaped component with labels, and determines the mouse angle 
 * relative to the center of the component to determine the selected button.
 */
 
import gfx.controls.RadialMenuButton;
import gfx.core.UIComponent;

class gfx.controls.RadialMenu extends UIComponent {

// Public Properties
// Private Properties
	private var data:Array;
	private var radius:Number = 100;
	private var centerX:Number;
	private var centerY:Number;
	private var angle:Number;
	private var line:MovieClip;
	private var selectedItem:RadialMenuButton;
	private var isFirstMove:Boolean = false;
	private var rotationalOffset:Number = 22.5

// Initialization
	public function RadialMenu() {
		super();
	}

// Private Methods
	private function configUI():Void {
		super.configUI();
		// Move this to dataProvider approach?
		data = ["item 1", "item 2", "item 3", "item 4","item 5","item 6","item 7","item 8"];
		line._alpha = 50;
		createMenu();
	}
	
	// Initial draw
	private function createMenu():Void {
		// Currently draws at center stage with a specific size.
		centerX = Stage.width >> 1;
		centerY = Stage.height >> 1;
		line._x = centerX;
		line._y = centerY;
		var l:Number = data.length;
		var increment:Number = 360/l;
		for (var i:Number = 0; i<l; i++) {
			var menuItem:RadialMenuButton = RadialMenuButton(this.attachMovie("RadialMenuButton", "radialMenuButton_"+i, i));
			menuItem.addEventListener("click",this, "onMenuItemClick");
			menuItem.init(centerX,centerY,((90-increment)-i*increment)+rotationalOffset,increment,radius,0x0,0xFF9900,100,radius);
			var labelAngle:Number = increment*i+increment/2-90;
			var labelRad:Number = labelAngle*Math.PI/180;

			var h:MovieClip = menuItem.createEmptyMovieClip("label"+(2000+i), 2000+i);
			h.createTextField("segmentLabel",0,0,0,20,20);

			var lab:TextField = h.segmentLabel;

			lab.autoSize = "left";
			lab.text = data[i];

			var tf:TextFormat = new TextFormat();
			tf.font = "Verdana";
			tf.size = 10;
			tf.color = 0x0099FF;
			lab.setTextFormat(tf);
			lab.embedFonts = true;
			lab.selectable = false;

			lab._x -= lab._width/2;
			lab._y -= lab._height/2;

			h._x = Math.cos(labelRad)*(radius+rotationalOffset)+centerX;
			h._y = Math.sin(labelRad)*(radius+rotationalOffset)+centerY;

			// Position and rotate labels:
			/*if (labelAngle>=-90 && labelAngle<0) {
				h._rotation = 90+labelAngle;
			} else if (labelAngle>=0 && labelAngle<90) {
				h._rotation = -90+labelAngle;
			} else if (labelAngle>=90 && labelAngle<180) {
				h._rotation = labelAngle-90;
			} else {
				h._rotation = labelAngle-270;
			}*/
		}
	} 
	
	// An item has been clicked.
	private function onMenuItemClick(event:Object):Void {
		dispatchEvent({type:"itemClick"})
	}
	
	// The mouse moved, update the selection.
	private function onMouseMove(mouseIndex:Number):Void {
		var diffX:Number = centerX - _xmouse;
		var diffY:Number = centerY - _ymouse;
		var hyp:Number = Math.sqrt((diffX * diffX) + (diffY * diffY));
		var rad:Number = Math.asin(diffX/hyp);
		var degree = -(rad/(Math.PI/180));
		angle = (line._rotation < 0) ? (line._rotation + 360) << 0 : (line._rotation) << 0;
		line._rotation = (_ymouse > (Stage.height >> 1)) ? (rad/(Math.PI/180)) - 180 : -(rad/(Math.PI/180));
		if (!isFirstMove) {
			isFirstMove = true;
			return;
		}
		
		var distance:Number = hyp;
		selectItem(angle);
	}	
	
	// Select an item.
	private function selectItem(angle:Number):Void {		
		var targetIndex:Number = (((angle+rotationalOffset)/360) * data.length) << 0;
		var item:RadialMenuButton = this["radialMenuButton_" + targetIndex];
		
		if (selectedItem.selected) { 
			selectedItem.selected = false;			
		} else if (selectedItem != item) {			
			item.selected = true;
			item.focused = true;
			selectedItem = item;
		}
		
	}
}