/*
	@2010 ScaleForm / gskinner.com
	Commands > Publish in GFX Player
*/
var swfPanels = fl.swfPanels;
for (var i = 0; i < swfPanels.length; i++) {
	if (swfPanels[i].name == "Scaleform Launcher") {
		swfPanels[i].call("publish")
		break;
	}
}

// Old code:
/*var path = fl.configURI + "Scaleform/jsfl/LaunchPanel.jsfl";
fl.runScript(path, "runCommand", "publish");*/