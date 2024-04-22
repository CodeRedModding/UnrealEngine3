package com.gskinner.util {
	
	public class PList {
			
	// Public Methods
		public static function deserialize(data:XML):Object {
			// Assume the first Node is the <plist> node.  Parse starting below that...
			return parseNode(data);
		}
		
		public static function serialize(data:Object):XML {
			return buildXML(data);
		}
	
	/* 
		DESERIALIZATION:
	*/
	
		protected static function parseNode(data:XML):Object {
			var type:String = data.name();
			
			var node:Object;
			switch(type) {
				case "plist":
					if (data.children().length() == 0) { data.appendChild(<dict />); }
					node = parseNode(data.children()[0]);
					break;
				case "dict": node = parseObject(data); break; 
				case "array": node = parseArray(data); break;
				case "integer": node = int(data); break;
				case "real": node = Math.abs(data as int); break;
				case "number": node = Number(data); break;
				case "string": 
				case "data": node = data.toString().split("\n").join(""); break; // Remove extra linefeed
				case "true": node = true; break;
				case "false": node = false; break;
			}
			
			return node;
		}
		
		protected static function parseObject(data:XML):Object {
			var obj:Object = {};
			var l:Number = data.children().length();
			for (var i:int = 0; i < l; i++) {
				var node:XML = data.children()[i];
				var name:String = node.name();
				var label:String = node.children()[0].toString();
				i++; // Key/Value Pairs need next item...
				
				if (data.children()[i].name() == "key") {
					obj[label] = null;
					i--;
				} else {
					obj[label] = parseNode(data.children()[i]); // Parse Name/Key Pair
				}
			}
			return obj;
		}
		
		protected static function parseArray(data:XML):Array {
			var arr:Array = [];
			var l:Number = data.children().length();
			for (var i:Number=0; i < l; i++) {
				arr.push(parseNode(data.children()[i]));
			}
			return arr;
		}
			
	/*
		SERIALIZATION:
	*/
	
		protected static function buildXML(data:Object):XML {
			var newData:XML = new XML(<plist version="1.0"><dict></dict></plist>);
			
			for (var i:String in data) {
				buildNode(newData.children()[0], data[i], i);
			}
			return newData;
		}
		
		protected static function buildNode(data:XML, obj:*, name:String = null):void {
			// Create Key
			if (name != null) { createNode(data, "key", name); }
			
			// Create Values
			var node:XML;
			if (obj is Boolean) { // Boolean
				createNode(data, (obj == true) ? "true" : "false");
			} else if (!isNaN(obj) && obj != "") { // Number or int (Flash can't tell the difference between Numbers and ints).
				createNode(data, "number", obj);
			} else if (obj is String || (typeof obj) == "string") { // String
				createNode(data, "string", obj);
			} else if (obj is Array) {
				node = createNode(data, "array");
				var l:Number = obj.length;
				for (var i:Number = 0; i < l; i++) { buildNode(node, obj[i]); }
			} else if (obj is Object) {
				node = createNode(data, "dict");
				var newObj:Object = (obj is ISerializable) ? obj.serialize() : obj;
				for (var s:String in newObj) { buildNode(node, newObj[s], s); }
			}
		}
		
		protected static function createNode(data:XML, key:String, value:String = null):XML {
			var node:XML;
			if (value != null) {
				node = new XML("<" + key + ">" + value + "</" + key + ">");
				data.appendChild(node);
				return null;
			} else {
				node = new XML("<" + key + " />");
				data.appendChild(node);
				return node;
			}
		}
			
	}
	
}