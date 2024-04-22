package gfx.launchpanel.managers {
	
	import flash.external.ExternalInterface;
	
	public class ExternalInterfaceManager {
		
		protected var owner:Object;
		
		public function ExternalInterfaceManager(scope:Object) {
			owner = scope;
			addHandlers();
		}
		
		protected function addHandlers():void {
			ExternalInterface.addCallback("publish", publish);
		}
		
		protected function publish():void {
			if (owner != null) { owner.publish(); }
		}
		
	}
	
}