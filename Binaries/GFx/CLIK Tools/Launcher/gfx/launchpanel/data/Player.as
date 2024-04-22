package gfx.launchpanel.data {
	
	import com.gskinner.util.ISerializable;
	
	public class Player extends Object implements ISerializable {
		
		public var path:String;
		public var name:String;
		
		// Just for compatibility:
		public var label:String;
		public var icon:String;
		
		public function Player(name:String, path:String) {
			this.path = path;
			this.name = name;
		}
		public function toString():String {
			return 'Path: ' + path + ' , ' + 'Name: ' + name;	
		}
		
		public function serialize():Object {
			return {name:name, path:path};
		}
		
	}
	
}