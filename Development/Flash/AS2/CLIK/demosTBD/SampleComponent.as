/**
 * Test the enabled property in GFx.
 */

class SampleComponent extends MovieClip {
			
	private var _enabled:Boolean = true;
	
	private function SampleComponent() { 
		super();
	}
	
	// Uncomment this, and comment out the one on the timeline to make it work.
	/*function onRelease() {
		enabled = !enabled; // Toggle the enabled.  Should only work once, since it will become disabled.
	}*/
	
	function get enabled():Boolean { return _enabled; }
	function set enabled(value:Boolean):Void {
		super.enabled = _enabled = value; // Set the player-level and internal enabled parameters
		this.gotoAndStop(2); // Go to a frame to show the enabled state.
	}

}