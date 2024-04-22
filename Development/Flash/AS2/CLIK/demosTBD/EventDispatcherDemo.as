/**
 * This sample demonstrates the EventDispatcher class, and its use.
 * 
 */

import gfx.controls.ScrollBar;
import gfx.controls.DropdownMenu;
import gfx.controls.Button;
import gfx.controls.TextArea;

class EventDispatcherDemo extends MovieClip {
	
	public var dm:DropdownMenu;
	public var results:TextArea;
	public var sb:ScrollBar;
	public var sb2:ScrollBar;
	public var btn:Button;
	public var btn2:Button;
	public var btn3:Button;
	public var eventLog:TextArea;
	
	public function EventDispatcherDemo() { } 
	
	public function onLoad():Void {		
		eventLog.text = "";
		
		// Populate some components.
		dm.labelField = "item";
		dm.dataProvider = [{item:"item 1",data:"info 1"},
		   {item:"item 2",data:"info 2"},
		   {item:"item 3",data:"info 3"},
		   {item:"item 4",data:"info 4"},
		   {item:"item 5",data:"info 5"},
		   {item:"item 6",data:"info 6"}
		];
		
		// Add events to components.
		dm.addEventListener("change",this,"onComboBoxChangeHandler");
		sb2.addEventListener("scroll",this,"onScroll");
		btn.addEventListener("click",this,"onButtonClickHandler");
		results.addEventListener("textChange",this,"onTextChangeHandler");
		btn2.addEventListener("click",this,"onRemoveAllListeners");
		btn3.addEventListener("click",this,"onAddListeners");
		
	}
	
	// ScrollBar scrolled
	public function onScroll(p_event:Object):Void {
		results.text = "You are Scrolling";
		displayMessage("Scroll Event from ScrollBar")
	}

	// Button was clicked
	public function onButtonClickHandler(p_event:Object):Void {
		//p_event is a reference to the button
		results.text = "You have clicked Button";
		displayMessage("Click Event from Button");
	}
	
	// Text changed.
	public function onTextChangeHandler(p_event:Object):Void {
		displayMessage("TextChange Event from <b>TextArea</b> ");
	}
	
	// ComboBox changed.
	public function onComboBoxChangeHandler(p_event:Object):Void {
		// p_event.target is a reference to the combobox component.
		var selectedItem:Object = p_event.target.selectedItem;
		results.text = "You have selected: " + selectedItem.data;
		displayMessage("Change Event from ComboBox \n");
	}
	
	// Show a message in the eventLog.
	public function displayMessage(p_message):Void {
		eventLog.htmlText += p_message;
	}
	
	// Remove listeners.
	public function onRemoveAllListeners(p_event:Object):Void {
		dm.removeEventListener("change",this,"onComboBoxChangeHandler");
		btn.removeEventListener("click",this,"onButtonClickHandler");
		results.removeEventListener("textChange",this,"onTextChangeHandler");
	}
	
	// Add back listeners.
	public function onAddListeners(p_event:Object):Void {
		dm.addEventListener("change",this,"onComboBoxChangeHandler");
		btn.addEventListener("click",this,"onButtonClickHandler");
		results.addEventListener("textChange",this,"onTextChangeHandler");	
	}
	
}