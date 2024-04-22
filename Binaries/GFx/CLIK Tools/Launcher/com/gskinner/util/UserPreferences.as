package com.gskinner.util {
	
	import adobe.utils.MMExecute;
	
	import com.gskinner.util.PList;
	
	public class UserPreferences {
			// JSFL Stuff
			//public static var JSFL_FILE = "gskinner/jsfl/UserPreferences.jsfl";
			public static var JSFL_FILE = 'Scaleform/jsfl/UserPreferences.jsfl'
			public static var SCRIPT_URL:String = "fl.runScript(fl.configURI + \"" + JSFL_FILE + "\", \"%FUNCTION%\", \"%URL%\", \"%XML%\");";
			
			// Instance props
			private var tempModel:Object;
			public var model:Object;
			public var xmlSource:XML;
			public var file:String;
			public var status:Boolean = false;
	
	// Initialization:
	
			public function UserPreferences(p_url:String) {
				if (p_url == null) { return; }
				var dotPosition:Number = p_url.indexOf(".");
				if (dotPosition == -1) {
						p_url += ".plist";
				} else if (dotPosition == p_url.length-1) {
						p_url += "plist";
				}
				file = p_url;
				
				model = {};
				
				var url:String = compileURL("loadPreferences");
				
				var prefs:String = MMExecute(url);
				if (prefs) {
						xmlSource = new XML();
						xmlSource.ignoreWhitespace = true;
						try {
							xmlSource = XML(prefs);
						} catch (error:*) {
							trace("UserPreferences.UserPreferences() Error parsing XML: " + error);
						}
						fromXML(xmlSource);
				} else {
						model = {};
				}
				model = (model == null) ? {} : model;
			}
	
	// Public Methods:
	
			public function getPref(p_key:String):Object {
				return model[p_key];
			}
			
			public function setPref(p_key:String, p_val, p_save:Boolean):Boolean {
				model[p_key] = p_val;
				if (p_save == false) { return false; }
				return save();
			}
			
			public function removePref(p_key:String, p_save:Boolean):Boolean {
				delete model[p_key];
				if (p_save == false) { return false; }
				return save();
			}
			
			public function remove():Boolean {
				var deleteURL:String = compileURL("deleteFile");
				var success:String = MMExecute(deleteURL);
				if (success == "true") {
						model = null;
						xmlSource = null;
				}
				status = false;
				return (success == "true") ? true : false;
			}
			
			public function save():Boolean {
				var xmlStr:String = PList.serialize(model).toString();
				xmlStr = xmlStr.split("\"").join("\\\"");
				xmlStr = xmlStr.split("\n").join("\\n");
				
				var saveURL:String = compileURL("savePreferences", xmlStr);
			   
				var success:String = MMExecute(saveURL);
				return success == "true";
			}
			
	// Protected Methods:
	
			protected function fromXML(xml:XML):void {
				model = PList.deserialize(xml);
				status = true;
			}
			
			protected function compileURL(p_function:String, p_xml:String = null):String {
				var url:String = SCRIPT_URL.split("%FUNCTION%").join(p_function);
				url = url.split("%URL%").join(file);
				var x:String = (p_xml != null) ? p_xml : "";
				url = url.split("%XML%").join(x);
				return url;
			}
	}
	
}