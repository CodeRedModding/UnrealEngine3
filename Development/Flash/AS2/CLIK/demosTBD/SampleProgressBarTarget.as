/**

 * Behave as a poll or event target for the ProgressBar. Dispatch events and update the "bytesLoaded" and "bytesTotal" properties.

 */



import gfx.core.UIComponent;



class SampleProgressBarTarget extends UIComponent {

	

// Constants:

// Public Properties:

	public var bytesLoaded:Number = 0;

	public var bytesTotal:Number = 0;

// Private Properties:

	private var mode:String = "manual";

// UI Elements:

	



	

// Initialization:

	public function SampleProgressBarTarget() { super(); }



// Public Methods:

	public function setMode(value:String):Void { 

		mode = value;		

		switch (mode) {

			case "manual":

				delete onEnterFrame;

				break;

			case "polled":

			case "event":

				onEnterFrame = change;

				break;

		}

	}



	private function change():Void {

		bytesLoaded = bytesLoaded+1%100;

		bytesTotal = 100;

		dispatchEvent({type:"progress", bytesLoaded:bytesLoaded, bytesTotal:bytesTotal});

	}

	

// Private Methods:

	private function configUI():Void { super.configUI(); }

}