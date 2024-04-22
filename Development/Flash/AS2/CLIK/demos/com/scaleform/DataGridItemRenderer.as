import gfx.controls.ListItemRenderer;

class com.scaleform.DataGridItemRenderer extends ListItemRenderer {
	
// Constants:
// Public Properties:	
// Private Properties:	
// UI Elements:
	public var field1:TextField;	// NAME
	public var field2:TextField;	// REGISTERED DATE
	public var field3:TextField;	// AGE
	public var field4:TextField;	// ID
	public var field5:TextField;	// GRADE
	// Maybe a button or something?
	


// Initialization:
	private function DataGridItemRenderer() { super(); }
	
// Public Methods:	
	public function setData(data:Object):Void {
		this.data = data;
		field1.text = data.name;
		var date:Date = new Date(data.date);
		field2.text = ((date.getMonth()+1)+"/"+date.getDate()+"/"+date.getFullYear());
		field3.text = data.age.toString();
		field4.text = data.id.toString();
		field5.text = data.grade;
	}
	
// Private Methods:

}