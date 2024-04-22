//var prefDir = fl.configURI + "gskinner/preferences/";
var prefDir = fl.configURI + 'Scaleform/jsfl/';

function loadPreferences(p_file) {
        var prefURI = prefDir + p_file;
        if (FLfile.exists(prefURI)) {
                var prefs = FLfile.read(prefURI);
                return prefs;
        } else {
                fl.trace("Creating File");
                if (!FLfile.exists(prefDir)) { FLfile.createFolder(prefDir) };
                if (FLfile.write(prefURI, "</plist>")) {
                        return loadPreferences(p_file);
                } else {
                        return false;
                }
        }
}
function savePreferences(p_file, p_prefs) {
        var prefURI = prefDir + p_file;
        if (FLfile.write(prefURI, p_prefs)) {
                return true;
        } else {
                return false;
        }
}

function deleteFile(p_file) {
        if (p_file == "" || !p_file) { return "</error>"; }
        var prefURI = prefDir + p_file;
        if (FLfile.remove(prefURI)) {
                return true;
        } else {
                return false;
        }
}