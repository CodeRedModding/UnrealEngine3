import gfx.controls.ButtonBar;
import gfx.controls.Button;
import gfx.controls.ViewStack;

class ButtonBarDemo extends MovieClip {
	
	public var bb:ButtonBar;
	public var b:Button;
	public var contentView:ViewStack;
	
	public function ButtonBarDemo() {}
	
	public function onLoad():Void {
		bb.dataProvider = [{label:'view 1',data:'NumberOne'},
						   {label:'view 2',data:'NumberTwo'},
						   {label:'view 3',data:'NumberThree'}
						   ];
		b.label = "Selects Random TAB";
		b.addEventListener("click", this, "handleClick");
		bb.addEventListener("change",this,"itemClick");
		bb.focused = true;
		contentView._width = 300;
		bb.selectedIndex = 1;
		contentView.show(bb.dataProvider[1].data)
	}
	
	private function itemClick(e:Object):Void {
		var selectedView:String =  e.target.selectedItem.data;
		contentView.show(selectedView);
	}
	
	private function handleClick(event):Void {
		var n:Number = Math.random() * bb.length >> 0;
		bb.selectedIndex = n;
		var selectedView:String = bb.dataProvider[n].data;
		contentView.show(selectedView);
	}
}