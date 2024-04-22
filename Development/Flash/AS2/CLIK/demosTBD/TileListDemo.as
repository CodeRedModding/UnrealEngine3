/**
 * This sample demonstrates the TileList component.
 */

import gfx.controls.TileList;
import gfx.controls.Button;
import gfx.controls.ScrollBar;
import gfx.controls.ListItemRenderer;
import gfx.controls.UILoader;
import gfx.motion.Tween;

import mx.transitions.easing.Strong;

class TileListDemo extends MovieClip {
	
	public var list:TileList;
	public var b:Button;
	
	public function TileListDemo() { }
	
	public function onLoad():Void {
		// Set the dataProvider. Note that the commented out items were for the TileListItemRenderer class, which is incomplete.
		list.dataProvider = [
			{label:'item 1',source:'Mario'},
			{label:'item 2',source:'Mario2'},
			//{label:'item 3',source:'images/image1.jpg'},
			//{label:'item 4',source:'images/image1.jpg'},
			{label:'item 5',source:'Mario3'},
			//{label:'item 6',source:'images/image1.jpg'},
			{label:'item 7',source:'Mario4'},			
			{label:'item 8',source:'Mario5'},
			1,2,3,4,5,6,7,8,9,10
		];
		
		Tween.init();
		b.addEventListener("click",this,"onClick");
	}
	
	private function onClick(e) {
		// Randomly set the size of the component.
		list.setSize(Math.random() * 400 + 80 >> 0, Math.random() * 400 + 30 >> 0);	
	}
	
}