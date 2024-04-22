/**
	A Simple Color Picker Class
	
	The class uses a prebuilt bitmap image to display the color swatches.  The colors are generated with an algorithm.
 
 	The Base Component includes support for optional UI buttons including:
	<ul>
 	<li>Color Swatch Button: An instance named "colorSwatch" that will trigger a "click" event when it is clicked.</li>
	<li>Palette Button: An instance named "colorPalette" that will trigger a "click" event when it is clicked.</li>
	<li>Hex Color Text Field: An instance named "colorText" that displays the hex value for the current color selection.</li>
	</ul>
*/

import gfx.controls.Button;
import gfx.core.UIComponent;
import gfx.controls.Label;
import gfx.events.EventDispatcher;
import gfx.utils.Delegate;


class com.scaleform.ColorPicker extends UIComponent {
	
// PRIVATE PROPERTIES:

	private var numColors:Number;
	private var tilesAcross:Number;
	private var tileSize:Number;
	private var pendingColor:Number;
	private var pendingHexColor:String;
	private var selectedColor:Number;
	private var selectedHexColor:String;
	private var swatchColor:Color
	
// PUBLIC PROPERTIES:

	public var bOpen:Boolean;
	
// UI ELEMENTS:

	private var colorPalette:Button;
	private var colorText:TextField;
	private var colorSwatch:Button;

// PUBLIC METHODS:

	public function ColorPickerSimple() {
		super();
	}
	
	public function toString():String {
		return "[Scaleform Color Picker " + _name + "]";
	}

	public function getColor():String {
		return selectedHexColor;
	}
	
// PRIVATE METHODS:
	
	private function configUI():Void {
		super.configUI();
		bOpen = false;
		swatchColor = new Color(colorSwatch["colorPickerBg"]);
		numColors = 216;
		tilesAcross = 18;
		tileSize = colorPalette._width / tilesAcross;
		colorPalette._visible = colorText._visible = false;
		colorSwatch.addEventListener("click",this,"onSwatchClick");
	}
	
	private function onSwatchClick(event:Object):Void {
		if (!bOpen) {
			showSwatches();
		}else {
			hideSwatches();
		}
	}

	private function showSwatches():Void {
		bOpen = colorPalette._visible = colorText._visible = true;
		if (selectedHexColor == null) {
			colorText.text = "";
		}
		else {
			colorText.text = selectedHexColor;
		}
		colorPalette.addEventListener("rollOver", this,"handlePaletteOver");
		colorPalette.addEventListener("rollOut", this, "handlePaletteOut");
		colorPalette.addEventListener("click", this, "handleSetColor");
		onMouseDown = handleStageClick;		
	}

	private function hideSwatches():Void {
		bOpen = colorPalette._visible = colorText._visible = false;
		colorPalette.removeEventListener("rollOver", this, "handlePaletteOver");
		colorPalette.removeEventListener("rollOut", this, "handlePaletteOut");
		colorPalette.removeEventListener("click", this, "handleSetColor");
		delete onMouseDown;
	}
	
	private function handlePaletteOver(event:Object):Void {
		onMouseMove = showPendingColor;
	}
	
	private function handlePaletteOut(event:Object):Void {
		colorText.text = "";
		delete onMouseMove;
		swatchColor.setRGB(selectedColor);
	}
	
	private function handleSetColor(event:Object):Void {
		// on mouse click set the current color of the main swatch
		trace("color picked: " + pendingHexColor);
		selectedColor = pendingColor;
		colorText.text = selectedHexColor = pendingHexColor;
		hideSwatches();	
		delete onMouseMove;
		dispatchEvent({type: "colorSet"});
	}
	
	private function handleStageClick(event:Object):Void {
		if (colorPalette.hitTest(_root._xmouse, _root._ymouse, true) || hitTest(_root._xmouse, _root._ymouse, true)) { return; }
		handlePaletteOut();
		hideSwatches();
	}
		
	private function showPendingColor():Void {
		// displays mouse-over color of palette in label & on main swatch
		
		var idX:Number = Math.ceil(this.colorPalette._xmouse / tileSize);
		var idY:Number = Math.ceil(this.colorPalette._ymouse / tileSize);
		var i:Number = (idY - 1) * tilesAcross + idX - 1;
		pendingColor = ((i/6%3<<0)+((i/108)<<0)*3)*0x33<<16 | i%6*0x33<<8  | (i/18<<0)%6*0x33;
		colorText.text = pendingHexColor = validateColor(pendingColor);
		swatchColor.setRGB(pendingColor);
	}
	
	private function validateColor(value:Number):String {
		// convert pendingColor to hexadecimal format
		
		var num:String = value.toString(16);
		var prefix:String = '';
		var l:Number  = num.length;
		switch(l) {
			case 1:
				prefix = '00000';
				break;
			case 2: 
				prefix = '0000';
				break;
			case 4:
				prefix = '00';
		}
		return '0x'+prefix+num;
	}
}
