var APP_PATH = "file:///C:/Program%20Files/Scaleform/GFx%20SDK%202.1/Bin/Win32/Msvc90/GFxPlayer/D3D9_Release_Static_Player.exe";
var CONFIG_URI = fl.configURI + "scaleform/";
var EXPORT_URI = CONFIG_URI + "temp/";
var TEMP_FILE = EXPORT_URI + "movie.swf";
var sep = "$__%__$";

function showDialog(p_title, p_message, p_isConfirmation) {
	var xml = "<dialog title=\""+p_title+"\" buttons=\"accept"+((p_isConfirmation == "true")?", cancel":"")+"\">";
	xml += "<label value=\""+p_message+"\" />";
	xml += "</dialog>";
	if (FLfile.write(fl.configURI + "Scaleform/xml/prompt.xml", xml) == false) {
		//fl.trace("Error in LaunchPanel.showDialog(): cannot create the file 'prompt.xml'. Dialog has been skipped.");
		return true;
	}
	
	var dom = fl.getDocumentDOM();
	if (dom == null) { return true; }
	var result = dom.xmlPanel(fl.configURI + "Scaleform/xml/prompt.xml");
	if (result.dismiss == "accept") {
		return "true";
	} else {
		return "false";
	}
}

// Export a temp movie swf, and launch it in the GFX player
function launchSWF(p_appPath, p_commands) {
	var dom = fl.getDocumentDOM();
	if (dom == null) {
		showDialog("No Document", "Please first open up an FLA file.", "false");
		return;
	}
	
	// Create Temp folder
	if (!FLfile.exists(EXPORT_URI)) { FLfile.createFolder(EXPORT_URI); }
	
	// CHECK OUTPUT?  Maybe save to temp and read?
	var testPath = getPublishProfileProperty("Format", "flashFileName");
	var testing = getPublishProfileProperty("Format","html");
	
	var execPath = testPath;
	if (dom.path == null) { alert('Please save .SWF to your hard drive'); return; } 
	if (testPath.indexOf('/') == -1 ) {
		var t = dom.path.split('/');
		t.pop();
		execPath = t.join('/') +'/'+ testPath;
	}
	
	var app = (p_appPath == null || p_appPath =="") ? APP_PATH : p_appPath.split("|").join(":");
	
	//SD: check for OS
	
	if(getVersion().mac) {
		var playerPath = cleanPath(app);
		var winPath = execPath;
		//SD: %SWF_PATH%" will be what the panel gives me back... and Sub that out
		
		dom.publish();
		FLfile.runCommandLine('/'+playerPath+" "+ p_commands.split("%SWF_PATH%").join('"'+winPath+'" '));
	 }
	 if (getVersion().windows) {;
		 var winPath = toPath(dom.path+execPath);
		 var swfPath = getPublishProfileProperty("Format", "flashFileName");
		 var s = dom.path.split('\\');
		 s.pop();

		 if (swfPath.indexOf(":") != -1) { // Absolute path
			execPath = swfPath;
		 }
		 else {
			execPath = s.join('/')+'/'+swfPath;
		 }		 
		 
		 dom.publish();
	     FLfile.runCommandLine("start "+app+" \""+ p_commands.split("%SWF_PATH%").join(execPath+'" '));
	}
}

function getPublishProfileProperty(p_section, p_property) {
	var dom = fl.getDocumentDOM();
	if (!dom) { return; }
	
	var fileURI = fl.configURI + '_'+new Date().getTime()+'.tmp';
	dom.exportPublishProfile(fileURI);
	
	var pubContent = FLfile.read(fileURI);
	FLfile.remove(fileURI);
	return findPublishProfileProperty(pubContent, p_section, p_property);
}

function findPublishProfileProperty(p_content, p_section, p_property) {
	
	var secNodeName = 'Publish'+p_section+'Properties';
	
	var secExp = new RegExp('<'+secNodeName+'\\s*[^>]*>([\\s\\S]+)</'+secNodeName+'>', 'igm');
	
	var secMatch = secExp.exec(p_content);
	
	if (secMatch && secMatch[1]) {
		var propExp = new RegExp('<'+p_property+'\\s*[^>]*>([\\s\\S]+)</'+p_property+'>', 'igm');
		var propMatch = propExp.exec(secMatch[1]);
		if (propMatch && propMatch[1]) { return propMatch[1]; }
	}
	
	return;
	
}

function cleanPath(p_uri) {
	var tempPath =  toPath(p_uri).split('/');
	tempPath.shift();
	var path = '/'+tempPath.join('/');
	return path;
}

// Browse for a player, select a name, return to flash.
function selectPlayer() {
	var path = fl.browseForFileURL("select", "Select a Player");
	if (path == null) { return; }
	var parts = path.split("/");
	var app = parts[parts.length-1];
	parts = app.split(".");
	var ext = parts.pop();
	var name = parts.join("");
	disableMessage(false);
	var playerName= prompt("Player Name:", name);
	disableMessage(true);
	return path + sep + playerName;
}
//SD: disables warning for scripts that run too long
function disableMessage(p_value) {
	fl.showIdleMessage(p_value);
}

// External Commands
function runCommand(p_xml) {
	var dom = fl.getDocumentDOM();
	
	//if (dom.path == null) { alert('Please save .SWF to your hard drive'); return; } 
	if (dom == null  ) {
		var openDoc = true;
		dom = fl.createDocument();
	}
	var path = fl.configURI + "Scaleform/xml/" + p_xml + ".xml";
	var response = dom.xmlPanel(path);
	if (openDoc) { fl.closeDocument(dom); }
}

// Utility

function getVersion() {
        var ver = fl.version;
        var pieces = ver.split(",");
        var plat = pieces[0].split(" ");
        return {
                platform: plat[0],
                windows: (plat[0] == "WIN"),
                mac: (plat[0] == "MAC"),
                major: plat[1],
                minor: pieces[1],
                revision: pieces[2],
                build: pieces[3]
        };
}
function getMacHD() {
        // RM: hack attempt at finding HD name
        return unescape(fl.configURI.replace('file:///','').split('/').shift());
}
function toURI(p_path) {}
function toPath(p_path) {
	return p_path.split("file:///").join("").split("|").join(":").split("\\").join("/").split("%20").join(" ");
}