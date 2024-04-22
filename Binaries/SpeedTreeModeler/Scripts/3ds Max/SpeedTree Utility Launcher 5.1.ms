-----------------------------------------------------------------------------------------------
--	SpeedTree Utilities Launcher - 3D Studio MAX Version
--
--	For working with SpeedTree meshes
--
--
--	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
--
--	This software is supplied under the terms of a license agreement or
--	nondisclosure agreement with Interactive Data Visualization and may
--	not be copied or disclosed except in accordance with the terms of
--	that agreement.
--
--      Copyright (c) 2003-2009 IDV, Inc.
--      All Rights Reserved.
--
-----------------------------------------------------------------------------------------------

-- Initialization
global STlauncher -- Predefine rollout name
mx_ini = getMAXIniFile() -- Recall utility preferences
iniLoadAtStartup = getINISetting mx_ini "IDV" "stm_loadatstartup"
if iniLoadAtStartup == "" then iniLoadAtStartup = true
iniUtilityPath = getINISetting mx_ini "IDV" "stm_utilitypath"
if iniUtilityPath == undefined then iniUtilityPath = ""


-----------------------------------------------------------------------------------------------
-- Write Startup Script -------------------------------------------------------------------

fn WriteStartupScript = (
	arrExternalMaxscripts = getFiles (iniUtilityPath + "*.ms")
	tmpStartupPath = getdir #startupScripts
	arrStartupPath = filterString tmpStartupPath "\\"
	strStartupName = ""
	for p in arrStartupPath do
		strStartupName = strStartupName + p + "\\\\"
	strStartupName = strStartupName + "SpeedTreeUtilities_Startup.ms"
	startupFile = createfile strStartupName
	
	if STlauncher.cbLoadAtStartup.checked == true then (
		if arrExternalMaxscripts.count > 0 then (	
			format "ms = true\n" to:startupFile
			
			launcherFile = undefined
			bHasLauncherFile = false
			for s in arrExternalMaxscripts do (
				if (findString s "Launcher" != undefined) then (
					bHasLauncherFile = true
					launcherFile = s
					exit
				)
			)
			
			if bHasLauncherFile do format "ms = ms AND doesFileExist \"%\" == true\n" launcherFile to:startupFile	
				
			format "if ms == true then (\n" strStartupName to:startupFile
			
			if bHasLauncherFile do	format "\tfileIn \"%\" \n" launcherFile to:startupFile
			
			format ")" to:startupFile
			
		) else (
			strUtilityPath = getSavePath caption:"Please browse to the location of \"SpeedTree Utility Launcher 5.1.ms\"\nexample: \"[SpeedTree Modeler install dir]\\Scripts\\3ds Max\\\""
			if strUtilityPath != undefined then (
				setINISetting mx_ini "IDV" "stm_utilitypath" strUtilityPath
				iniUtilityPath = strUtilityPath
			) else (
				iniUtilityPath = undefined
				setINISetting mx_ini "IDV" "stm_utilitypath" ""
				STlauncher.cbLoadAtStartup.checked = false 
			)
		)
	) else format "" to:startupFile

	close startupFile 
)


-----------------------------------------------------------------------------------------------
-- Utility  -----------------------------------------------------------------------------------

utility STlauncher "SpeedTree Utilities 5.1" (
	
	checkBox cbLoadAtStartup "Load launcher at startup" offset:[-8,0] checked:(iniLoadAtStartup as BooleanClass)
	
	on cbLoadAtStartup changed optionState do (
		setINISetting mx_ini "IDV" "stm_loadatstartup" (optionState as string)
		WriteStartupScript()
	)
	
	group "Load Utilities" (	
		button btnMeshUtility "SpeedTree Mesh Utilities" width:142 height:20 
		button btnFbxUtility "SpeedTree Material Utilities" width:142 height:20 offset:[3,0] 
	)
	
	on btnMeshUtility pressed do (
		uPath = iniUtilityPath + "SpeedTree Mesh Utility 5.1.ms"
		if (doesFileExist uPath == true) then fileIn uPath
		else messagebox "The specified filename does not exist." title:"Missing Script" beep:false
	)
	on btnFbxUtility pressed do (
		uPath = iniUtilityPath + "SpeedTree Material Utilities 5.1.ms"
		if (doesFileExist uPath == true) then fileIn uPath
		else messagebox "The specified filename does not exist." title:"Missing Script" beep:false
	)
	
	on STlauncher open do (			
		if (STextras != undefined) AND STextras.Open do CloseUtility STextras	
		if (stm != undefined) AND stm.Open do CloseUtility stm
	)
)

if (STlauncher != undefined) AND STlauncher.Open do CloseUtility STlauncher
OpenUtility STlauncher

if iniUtilityPath == "" then (
	tmpUtilityPath = sysInfo.currentdir
	arrUtilityPath = filterString tmpUtilityPath "\\"
	strUtilityPath = ""
	for p in arrUtilityPath do
		strUtilityPath = strUtilityPath + p + "\\\\"
	
	setINISetting mx_ini "IDV" "stm_utilitypath" strUtilityPath
	iniUtilityPath = getINISetting mx_ini "IDV" "stm_utilitypath"
	WriteStartupScript()
)