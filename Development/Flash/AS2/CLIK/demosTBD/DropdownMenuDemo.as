import gfx.controls.DropdownMenu;

class DropdownMenuDemo extends MovieClip {
	
	public var dm:DropdownMenu;
	
	public function DropdownMenuDemo() {}
	
	public function onLoad():Void {
		dm.dataProvider = ["Grant", "Lanny", "Ryan", "Wes", "Michael", "Sebastian", "Nick", "Eddie", "Maurice", "Oscar"];
		dm.addEventListener("change", this, "onChange");
		
	}
	
	private function onChange(e:Object):Void {
		trace(e.target.selectedItem);
	}
}
