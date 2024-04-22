import gfx.controls.ListItemRenderer;

class com.scaleform.SDKBrowserItemRenderer extends ListItemRenderer {
	
// Constants:
// Public Properties:	
// Private Properties:	
// UI Elements:
	public var content:MovieClip;	

// Initialization:
	private function SDKBrowserItemRenderer() { super(); }
	
// Public Methods:	
	public function setData(data:Object):Void {
		this.data = data;
		content.title.text = data.title;
		content.description.text = data.description;
		if (data.type == "tutorials") {
			content.attachMovie("tutorialImage" + data.id, "tutorialImage" + data.id, getNextHighestDepth(), {_x:16, _y:16});
		}
		else if (data.type == "demos") {
			content.attachMovie("demoImage" + data.id, "demoImage" + data.id, getNextHighestDepth(), {_x:16, _y:16});
		}
		else if (data.type == "documents") {
			content.attachMovie("docImage" + data.id, "docImage" + data.id, getNextHighestDepth(), {_x:16, _y:16});
		}
	}
	
// Private Methods:

}