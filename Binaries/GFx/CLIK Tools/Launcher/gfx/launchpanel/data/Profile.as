package gfx.launchpanel.data {
	
	import com.gskinner.util.ISerializable;
	
	public class Profile extends Object implements ISerializable {
		
		public var name:String;
		public var command:String;
		public var playerPath:String;
		public var playerName:String;
		
		// Just for compatibility:
		public var icon:String;
		public var data:String;
		public var label:String;
		
		public function Profile(p_name:String, p_command:String, p_path:String, p_playerName:String) {
			name = p_name;
			command = p_command;
			playerPath = p_path;
			playerName = p_playerName;
		}
		
		public function toString():String {
			return 'name: '+ name + ' , command: ' + command + ' , playerPath: ' + playerPath + ' , PlayerName: ' + playerName ;
		}
		
		public function serialize():Object {
			return {name:name, command:command, playerPath:playerPath, playerName:playerName};
		}
		
	}
	
}