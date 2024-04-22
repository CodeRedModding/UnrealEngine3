-------------------------------------------------------------------------------------------------
--	SpeedTree Mesh Exporter - 3D Studio MAX Version
--
--	For exporting an object for use with SpeedTree Modeler
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
-------------------------------------------------------------------------------------------------

-- Initialization
fn DeclareGlobals = (
	global stm, stmTools  -- Predefine rollout name
	global LastFileName, cObject
	global arrSelected = #()
	global strOldSystemScale = units.SystemScale
	global strOldSystemType = units.SystemType
	global fConversionScalar
)
DeclareGlobals()

mx_ini = getMAXIniFile() -- Recall utility preferences
iniUtilityPath = getINISetting mx_ini "IDV" "stm_utilitypath"
iniCustomNormals = getINISetting mx_ini "IDV" "stm_normals"
if iniCustomNormals == "" then iniCustomNormals = true
iniCenterOrigin = getINISetting mx_ini "IDV" "stm_origin"
if iniCenterOrigin == "" then iniCenterOrigin = false
iniConvertUnits = getINISetting mx_ini "IDV" "stm_convertunits"
if iniConvertUnits == "" then iniConvertUnits = true
iniStmFormat = getINISetting mx_ini "IDV" "stm_format"
if iniStmFormat == "" then iniStmFormat = 1
iniUseHelp = getINISetting mx_ini "IDV" "stm_usehelp"
if iniUseHelp == "" then iniUseHelp = true

fn stmRemoveScripts = ( 
	callbacks.removeScripts id:#stmChange
)

fn stmCallbacks = (
	callbacks.addScript #selectionSetChanged "UpdateList()" id:#stmChange
	callbacks.addScript #unitsChange "UpdateList()" id:#stmChange	
	callbacks.addScript #nodeRenamed "UpdateList()" id:#stmChange	
	callbacks.addScript #modPanelObjPostChange "UpdateList()" id:#stmChange
	callbacks.addScript #filePreOpen "if stm.Open do CloseUtility stm"
	callbacks.addScript #systemPreReset "if stm.Open do CloseUtility stm"
	callbacks.addScript #systemPreNew "if stm.Open do CloseUtility stm"
	callbacks.addScript #filePostMerge "OpenUtility stm"
	callbacks.addScript #filePostOpen "OpenUtility stm"
	callbacks.addScript #systemPostReset "OpenUtility stm"
	callbacks.addScript #systemPostNew "OpenUtility stm"
)

-----------------------------------------------------------------------------------------------
-- GetcObject ------------------------------------------------------------------------------

fn GetcObject = (
	global cObject = undefined
	if (selection.count > 0) then (
			
		arrSelected = #()
		for s in selection as array do (
			sClass = classOf s		
			sSuperClass = SuperClassOf s
			if (sSuperClass == GeometryClass AND s.modifiers["Hair and Fur"] == undefined AND sClass != SpeedTree_4 AND sClass != SpeedTree AND sClass != mr_Proxy) then (
				sTriMesh = snapshotAsMesh s
				if (sTriMesh != undefined) then (
					delete sTriMesh
					if cObject == undefined then cObject = s
					join arrSelected #(s)
				)
			)
		)
	)
)

-----------------------------------------------------------------------------------------------
-- Tools Group-------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------
-- Set Pivot (to scene origin) --------------------------------------------------------------

fn SetPivot obj loc = (
	undo on (
		if (obj != undefined) then (
			arrSelection = (selection as array)
			if (isGroupMember obj == true) then (
				isGrouped = true
				for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g true)
			) else isGrouped = false
			
			select obj
			if (loc == "origin") then obj.pivot = [0,0,0]
			else centerPivot obj
			if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
			select arrSelection
		)
		else 	messageBox message title:"Please select a mesh object." beep:false	
	)
)


-----------------------------------------------------------------------------------------------
-- Mirror Object for 4.1 compatability ---------------------------------------------------

fn MirrorObj = (
	undo on (
		GetcObject()
		if (cObject != undefined) then (
			origPivot = cObject.pivot
			cObject.pivot = [0,0,0]
			scale cObject [-1,1,1]
			ResetXForm cObject
			addModifier cObject (normalModifier flip:true)
			origPivot [1] = -(origPivot [1])
			cObject.pivot = origPivot
		)
	)
)


-----------------------------------------------------------------------------------------------
-- Flip Normals (over an Axis) -------------------------------------------------------------

fn FlipNormals = (
	stmRemoveScripts()
	undo on (
		GetcObject()
		if (cObject != undefined) then (
			arrSelection = (selection as array)
			
			if (isGroupMember cObject == true) then (
				isGrouped = true
				for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g true)
				
			) else isGrouped = false
			
			select cObject
			max modify mode
			if (cObject.modifiers[#editnormals] == undefined) then (addModifier cObject(Edit_Normals()))
			
			XVal = 1
			YVal = 1
			ZVal = 1
			if stm.stmTools.XSelect.checked == true then XVal = -1
			if stm.stmTools.YSelect.checked == true then YVal = -1
			if stm.stmTools.ZSelect.checked == true then ZVal = -1
			
			num = cObject.Edit_Normals.GetNumNormals()
			selectedNormals = cObject.Edit_Normals.GetSelection()
			if (selectedNormals == undefined OR (selectedNormals as array).count == 0) then
				selectedNormals = #{1..num}
		
			for i in selectedNormals do (
				cObject.Edit_Normals.SetSelection #{i}
				origNormal = cObject.Edit_Normals.GetNormal i
				
				if origNormal != undefined then (
					flippedNormal = (origNormal * [XVal,YVal,ZVal])
					cObject.Edit_Normals.SetNormal i flippedNormal
				)
				
				cObject.Edit_Normals.MakeExplicit()
			)
			
			cObject.Edit_Normals.SetSelection selectedNormals
			setCommandPanelTaskMode mode:#utility
			
			if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
			select arrSelection
		)
		else 	messageBox message title:"Please select a mesh object." beep:false	
	)
	stmCallbacks()
)


-----------------------------------------------------------------------------------------------
-- Project Normals -------------------------------------------------------------------------

fn ProjectNormals = (
	stmRemoveScripts()
	undo on (
		GetcObject()
		if (cObject != undefined) then (
			arrSelection = (selection as array)

			if (isGroupMember cObject == true) then (
				isGrouped = true
				for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g true)
				
			) else isGrouped = false
			
			select cObject
			
			if (classof cObject.baseObject != Editable_Mesh AND cObject.modifiers[#edit_mesh] == undefined) then (
				addModifier cObject(Edit_Mesh()) before:100
					
			) else if (cObject.modifiers[#meshsmooth] != undefined AND cObject.modifiers[#edit_mesh] != undefined) then (
				addModifier cObject(Edit_Mesh())
			)
			
			-- Match indeces of vertices and normals
			vertIdArr = #()
			for f = 1 to (getNumFaces cObject) do (
				for v = 1 to 3 do (
					append vertIdArr ((getFace cObject f)[v] as integer)
				)
			)
			for i = 1 to vertIdArr.count do (
				k = i + 1
				while (k <= vertIdArr.count) do (
					if vertIdArr[k] == vertIdArr[i] then deleteitem vertIdArr k
					else k = k + 1
				)
			)
			
			-- Select all normals
			max modify mode
			if (cObject.modifiers[#editnormals] == undefined) then (addModifier cObject(Edit_Normals()))
			num = cObject.Edit_Normals.GetNumNormals()
			allNormals = #{1..num}
			cObject.Edit_Normals.SetSelection allNormals
			cObject.Edit_Normals.Reset()
			cObject.Edit_Normals.Unify()
			
			rayStart = cObject.center
			objLength = cObject.max.y - cObject.min.y
			objLength = objLength*(stm.stmTools.projectBias.value*2)
			rayStart.y = rayStart.y + objLength
				
			objHeight = cObject.max.z - cObject.min.z
			objZOffset = objHeight*stm.stmTools.projectBias.value
			rayStart.z = rayStart.z - objZOffset
			
			-- Change normal to vector from object's center
			for n = 1 to vertIdArr.count do (
				getVector = (getVert cObject vertIdArr[n]) - rayStart
				objRotation = cObject.transform.rotationpart as matrix3
				objRotation = inverse objRotation
				getVector = getVector * objRotation
				
				cObject.Edit_Normals.SetNormal n getVector
				cObject.Edit_Normals.MakeExplicit()
			)
			
			cObject.Edit_Normals.SetSelection #{}
			
			if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
			select arrSelection
			
		) else 	messageBox message title:"Please select a mesh object." beep:false	
	)
	stmCallbacks()
	setCommandPanelTaskMode mode:#utility
)


-----------------------------------------------------------------------------------------------
-- Clean Up Faces ---------------------------------------------------------------------------

fn CleanUpFaces = (
	undo on (
		GetcObject()
		if (cObject != undefined) then (
			arrSelection = (selection as array)

			if (isGroupMember cObject == true) then (
				isGrouped = true
				for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g true)
				
			) else isGrouped = false
			
			select cObject	
					
			if (classof cObject.baseObject != Editable_Mesh) then (
				messageBox "This tool requires an editable mesh object." title:"Please select a mesh object." beep:false	
			) else (
				
				oldFaceCount = getNumFaces cObject
				oldVertCount = getNumVerts cObject
				
				faceSel = #()
				for f = 1 to cObject.numfaces do (
					if meshop.getFaceArea cObject #{f} < stm.stmTools.cleanBias.value then (
						append faceSel f
					)
				)
				
				meshop.deleteFaces cObject faceSel
				meshop.removeDegenerateFaces cObject	
				meshop.removeIllegalFaces cObject			
				isoVerts = meshop.getIsoVerts cObject
				meshop.deleteVerts  cObject isoVerts
		
				newFaceCount = getNumFaces cObject
				newVertCount = getNumVerts cObject
				
				message = (oldFaceCount - newFaceCount) as string + " triangles and " + (oldVertCount - newVertCount) as string + " vertices removed."
				messageBox message beep:false	
			)
			
			if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
			select arrSelection
		)
		else 	messageBox message title:"Please select a mesh object." beep:false	
	)
)

-- End Tools

-----------------------------------------------------------------------------------------------
-- The current selection set should be listed in the object list -------------------------

fn UpdateList = (
		disableSceneRedraw()
	
		if (units.displayType != #Generic) then	
				stm.ConvertUnitsOption.enabled = true
		else  stm.ConvertUnitsOption.enabled = false
	
		GetcObject()
		if (selection.count > 0 AND cObject != undefined) then (
			stm.objName.text = "- " + cObject.name + " -"
			stm.objFaces.text = cObject.mesh.numfaces as string + " triangles"
			stm.objFaces.enabled = true
			
			-- Get dimensions in display units
			oldTM = cObject.transform
			cObject.transform = matrix3 1
			mDimensions = cObject.max - cObject.min
			cObject.transform = oldTM
			xDim = substring ((mDimensions[1] * cObject.scale.x) as string) 1 7
			yDim = substring ((mDimensions[2] * cObject.scale.y) as string) 1 7
			zDim = substring ((mDimensions[3] * cObject.scale.z) as string) 1 7
				
			if (stm.ConvertUnitsOption.enabled == true AND stm.ConvertUnitsOption.checked == true) then (
				xDim = units.formatValue (mDimensions[1] * cObject.scale.x) 
				yDim = units.formatValue (mDimensions[2] * cObject.scale.y)
				zDim = units.formatValue (mDimensions[3] * cObject.scale.z)
			)

			stm.objDimensions.text = "(" + xDim + ", " + yDim + ", " + zDim + ")"
			stm.objDimensions.enabled = true
			
			stm.exportButton.enabled = true
			stm.stmTools.projectButton.enabled = true
			stm.stmTools.flipButton.enabled = true
			stm.stmTools.cleanButton.enabled = true
			stm.stmTools.pivotButton.enabled = true
			--	stm.stmToolsmirrorButton.enabled = true
			
			stm.stmUnits.btnConvertUnits.enabled = true		
			stm.stmUnits.pbChooseTarget.enabled = true
			
		) else (
			stm.objName.text = "- No mesh selected -"
			stm.objFaces.text = "0 triangles"
			stm.objFaces.enabled = false
			stm.objDimensions.text = "(0, 0, 0)"
			stm.objDimensions.enabled = false
			stm.exportButton.enabled = false
			stm.stmTools.projectButton.enabled = false
			stm.stmTools.flipButton.enabled = false
			stm.stmTools.cleanButton.enabled = false
			stm.stmTools.pivotButton.enabled = false
			--	stm.stmTools.mirrorButton.enabled = false

			stm.stmUnits.btnConvertUnits.enabled = false		
			stm.stmUnits.pbChooseTarget.enabled = false
		)
		enableSceneRedraw()
)

fn AddChangeHandler = (
	UpdateList()
	if (selection.count > 0 AND getCommandPanelTaskMode() == #utility) then (
	--	when parameters selection change id:#stmChangeHandler do UpdateList()
	--	when parameters selection change id:#stmChangeHandler handleAt:#redrawViews do UpdateList()

	)
)

------------------------------------------------------------------------------------------------
-- Convert Units -----------------------------------------------------------------------------

fn ConvertUnits obj = (
	if (units.displayType != #Generic AND units.displayType != units.SystemType) do (
		
		global strOldSystemScale = units.SystemScale
		global strOldSystemType = units.SystemType	
		
		i = 1
		testValStart = 0
		while (testValStart == 0) do (
			tmpVert = getVert obj i
			if (tmpVert[1] != 0) then testValStart = tmpVert[1]
			else if (tmpVert[2] != 0) then testValStart = tmpVert[2]
			else if (tmpVert[3] != 0) then testValStart = tmpVert[3]
			i = i + 1
		)
		
		testValFormatted = units.formatValue testValStart
		scalarVal = 1
		if (units.displayType == #US) then (
			units.SystemScale = 1.0
			units.SystemType = #Inches
			
			if (units.USType == #Frac_Ft OR 
				units.USType == #Dec_Ft OR 
				units.USType == #Ft_Frac_In OR 
				units.USType == #Ft_Dec_In) then
					scalarVal = 0.083333 --  (1 / 12)
				
		) else if (units.displayType == #Metric) then (
			units.SystemScale = 1.0
			units.SystemType = #Meters
			
			if (units.MetricType == #Millimeters) then (scalarVal = 0.001)
			if (units.MetricType == #Centimeters) then (scalarVal = 0.01)
			if (units.MetricType == #Meters) then (scalarVal = 1.0)
			if (units.MetricType == #Kilometers) then (scalarVal = 1000)
		
		) else if (units.displayType == #Custom) then (
			units.SystemScale = 1.0
			units.SystemType = units.CustomUnit
			
			scalarVal = (1 / units.CustomValue)
		) 
		
		testValDecoded = (units.decodeValue testValFormatted) * scalarVal
		sizeConversion = testValDecoded / testValStart

		SetPivot obj "origin"
		obj.scale = obj.scale * sizeConversion
		
		units.SystemScale = strOldSystemScale	-- revert back to old system scale
		units.SystemType = strOldSystemType	

	)
)


------------------------------------------------------------------------------------------------
-- ExportObjFile ----------------------------------------------------------------------------

fn ExportObjFile = (
	
	with redraw off
	
	GetcObject()
	if (cObject != undefined) then (
		arrSelection = (selection as array)
		global isGrouped = false
		if (isGroupMember cObject == true) then (
			isGrouped = true
			for obj in selection where (isGroupHead obj == true) do (setGroupOpen obj true)
		) else isGrouped = false
		
		tmpName = cObject.name
		strFilename = getSaveFileName "Export OBJ" types:"Wavefront Object (*.obj)|*.obj|All Files (*.*)|*.*"
		global LastFilename = (strFilename as string)
		
		cMesh = snapshot cObject
		select cMesh
		if (cObject.modifiers[#editnormals] == undefined) AND (stm.EditNormalsOption.checked == true) then (
			hasEditNormals = false
			addModifier cMesh(Edit_Mesh())
			addModifier cMesh(Edit_Normals())
				
		) else if (stm.EditNormalsOption.checked == false) then ( -- If NOT using "custom normals" option
			addModifier cMesh(Edit_Mesh())
			addModifier cMesh(Edit_Normals())
			cMesh.Edit_Normals.Reset()
		)
		addModifier cMesh(Edit_Normals())
		cMesh.Edit_Normals.RebuildNormals()
		cMesh.Edit_Normals.RecomputeNormals()
				
		if (stm.CenterOriginOption.checked == true) do (cMesh.pos = [0,0,0])
		if (stm.ConvertUnitsOption.checked == true) do (ConvertUnits cMesh)
		
		if (strFilename != undefined) then
			exportFile (strFilename) #noPrompt selectedOnly:true					
			
		if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
		delete cMesh
		cObject = getNodeByName(tmpName)
		select cObject
	)
)


------------------------------------------------------------------------------------------------
-- WriteVertex ------------------------------------------------------------------------------

fn WriteVertex cObject cMesh iFaceIndex iVertIndex pOutputFile iChannel matRotate = (	
	cFace = getFace cMesh iFaceIndex
	
	aOut = getVert cMesh (cFace[iVertIndex] as integer)
	format "% % % " aOut[1] aOut[2] aOut[3] to:pOutputFile 

	iNormalID = cObject.Edit_Normals.GetNormalID iFaceIndex iVertIndex;
	aOut = cObject.Edit_Normals.getNormal iNormalID
	aOut = aOut * matRotate
	format "% % % " aOut[1] aOut[2] aOut[3] to:pOutputFile
	
 	cTFace = meshop.getMapFace cMesh iChannel iFaceIndex 
	aOut = meshop.getMapVert cMesh iChannel (cTFace[iVertIndex] as integer)
	format "% %\n" aOut[1] aOut[2] to:pOutputFile 
)


------------------------------------------------------------------------------------------------
-- ExportSTMFile ---------------------------------------------------------------------------

fn ExportSTMFile = (
	GetcObject()
	if (cObject != undefined) then (
		
		if (LastFilename == undefined) then (
			strFilename = getSaveFileName "Export STM" types:"SpeedTree Mesh (*.stm)|*.stm|All Files (*.*)|*.*"
			global LastFilename = (strFilename as string)
		)
		
		-- Define variables		
		pOutputFile = createFile LastFilename
		iChannel = 1
		global hasEditNormals = true
			
		global arrSelection = (selection as array) -- Manipulate selection set and deal with grouped objects
		global isGrouped = false
		if (isGroupMember cObject == true) then (
			isGrouped = true
			for obj in selection where (isGroupHead obj == true) do (setGroupOpen obj true)
		) else isGrouped = false
		
		with redraw off (	
			select cObject	
			matRotate = cObject.transform.rotationpart as matrix3
					
			max modify mode
			if (cObject.modifiers[#editnormals] == undefined) AND (stm.EditNormalsOption.checked == true) then (
				hasEditNormals = false
				addModifier cObject(Edit_Mesh())
				addModifier cObject(Edit_Normals())
					
			) else if (stm.EditNormalsOption.checked == false) then ( -- If NOT using "custom normals" option
				addModifier cObject(Edit_Mesh())
				addModifier cObject(Edit_Normals())
				cObject.Edit_Normals.Reset()
			)
			
			addModifier cObject(Edit_Normals())
			cObject.Edit_Normals.RebuildNormals()
			cObject.Edit_Normals.RecomputeNormals()
			
			global cSnapObject = snapshot cObject
			global cMesh = getNodeByName(cSnapObject.name)
			addModifier cMesh (smoothModifier smoothingBits:1)
			ConvertToMesh cMesh
			
			if (stm.CenterOriginOption.checked == true) do (cMesh.pos = [0,0,0])
			if (stm.ConvertUnitsOption.checked == true) do (ConvertUnits cMesh)				
			if (meshop.getMapSupport cMesh iChannel == false) do (AbortScript "No mapping coordinates found")
				
			format "%\n" cMesh.numFaces to:pOutputFile -- Write mesh file 
			for iFace = 1 to cMesh.numFaces do  (
				WriteVertex cObject cMesh iFace 1 pOutputFile iChannel matRotate
				WriteVertex cObject cMesh iFace 2 pOutputFile iChannel matRotate
				WriteVertex cObject cMesh iFace 3 pOutputFile iChannel matRotate
			)
			delete cSnapObject
			deleteModifier cObject 1
					
			if (hasEditNormals == false) OR (stm.EditNormalsOption.checked == false) do (
				deleteModifier cObject 1
				deleteModifier cObject 1
			)

			if (isGrouped == true) then for g in arrSelection where (isGroupHead g == true) do (setGroupOpen g false)
			
			close pOutputFile
			select arrSelection -- Reset original selection and close groups
		)
	)
	else 	messageBox message title:"Please select a mesh object." beep:false
	
)


------------------------------------------------------------------------------------------------
-- ImportSTMFile ---------------------------------------------------------------------------

fn ImportSTMFile = (
	vert_array = #()
	face_array = #()
	normal_array = #()
	tex_array = #()

	stmFilename = getOpenFileName "Import STM" types:"SpeedTree Mesh (*.stm)|*.stm|All Files (*.*)|*.*"
	if stmFilename != undefined then (
		stmFile = openFile stmFilename
		if stmFile != undefined then (	
			stm_name = uniqueName (getFilenameFile stmFilename)
			num_faces = readValue stmFile
			num_verts = (num_faces * 3)
			
			for v = 1 to num_verts do (
				vert_x = readValue stmFile;
				vert_y = readValue stmFile
				vert_z = readValue stmFile
				append vert_array (point3 vert_x vert_y vert_z)
				
				normal_x = readValue stmFile
				normal_y = readValue stmFile
				normal_z = readValue stmFile
				append normal_array (point3 normal_x normal_y normal_z)
				
				tex_u = readValue stmFile
				tex_v = readValue stmFile
				append tex_array (point3 tex_u tex_v 0)
			)
			
			vert = 1
			for f = 1 to num_faces do (
				vert_x = vert; vert = vert + 1
				vert_y = vert; vert = vert + 1
				vert_z = vert; vert = vert + 1
				append face_array (point3 vert_x vert_y vert_z)
			)
			
			close stmFile
			stm_mesh = mesh vertices:vert_array tverts:tex_array faces:face_array
			stm_mesh.name = stm_name
			stm_mesh.wirecolor = (color 128 128 128)
			
			max modify mode
			buildTVFaces stm_mesh.mesh
			for i = 1 to stm_mesh.mesh.numfaces do (setTVFace stm_mesh.mesh i (getFace stm_mesh.mesh i))
			prevSelection = (selection as array)
			select stm_mesh
			ResetXForm stm_mesh
			ConvertToMesh stm_mesh
			addModifier stm_mesh(Edit_Normals())
			num = stm_mesh.Edit_Normals.GetNumNormals()
			stm_mesh.Edit_Normals.SetSelection #{1..num}
			for i = 1 to num_verts do (stm_mesh.Edit_Normals.setNormal i normal_array[i])
			stm_mesh.Edit_Normals.displayLength = 1
			stm_mesh.Edit_Normals.SetSelection #{}
			stm_mesh.Edit_Normals.SelLevel = 0
			if prevSelection.count > 0 then (select prevSelection)
			else clearSelection()
		)
	)
)


-------------------------------------------------------------------------------------------------
-- AbortScript --------------------------------------------------------------------------------

fn AbortScript message = (
	delete cSnapObject
	deleteModifier cObject 1
	if (hasEditNormals == false) OR (stm.EditNormalsOption.checked == false) then (
		deleteModifier cObject 1
		deleteModifier cObject 1
	)

	units.SystemScale = strOldSystemScale	-- revert back to old system scale
	units.SystemType = strOldSystemType
	
	select arrSelection -- Reset original selection and close groups
	if (isGrouped == true) do for obj in selection where (isGroupHead obj == true) do (setGroupOpen obj false)
	
	messageBox message title:"Mesh Export Error" beep:false	
	enableSceneRedraw()
	
	exit
)


------------------------------------------------------------------------------------------------
-- Convert Scene Units ----------------------------------------------------------------------

fn ConvertSceneUnits = (
	
	strFrom = stm.stmUnits.ddConvertFrom.selected
	case of (
		(strFrom == "millimeters") : valFrom = 0.001
		(strFrom == "centimeters"): valFrom = 0.01
		(strFrom == "meters"): valFrom = 1.0
		(strFrom == "kilometers") : valFrom = 1000
		(strFrom == "inches"): valFrom = 0.0254
		(strFrom == "feet"): valFrom = 0.3048
		(strFrom == "miles"): valFrom = 1609.344
		default: valFrom = 1.0
	)
	
	if (stm.stmUnits.ddConvertTo.selected == "scene units") then strTo = units.SystemType as string
	else strTo = stm.stmUnits.ddConvertTo.selected
	case of (
		(strTo == "millimeters") : valTo = 0.001
		(strTo == "centimeters"): valTo = 0.01
		(strTo == "meters"): valTo = 1.0
		(strTo == "kilometers") : valTo = 1000
		(strTo == "inches"): valTo = 0.0254
		(strTo == "feet"): valTo = 0.3048
		(strTo == "miles"): valTo = 1609.344
		default: valTo = 1.0
	)
	if (stm.stmUnits.ddConvertTo.selected == "Scene units") then valTo = valTo * units.SystemScale
	
	fConversionScalar =  valTo / valFrom
	stm.stmUnits.lblConversionRatio.text = "Scale -> " + substring((ValFrom / valTo) as string) 1 7
)


------------------------------------------------------------------------------------------------
-- SpeedTree Mesh Utility ------------------------------------------------------------------

utility stm "SpeedTree Mesh Utility 5.1" (
	group "Mesh Export" (
		label objName "- No mesh selected -"
		label objFaces "0 triangles" offset:[0,-3] enabled:false
		label objDimensions "(0, 0, 0)" offset:[0,-3] enabled:false
		checkBox ConvertUnitsOption "Use display units" offset:[0,3] checked:(iniConvertUnits as BooleanClass)
		checkBox EditNormalsOption "Use custom normals" offset:[0,-4] checked:(iniCustomNormals as BooleanClass)
		checkBox CenterOriginOption "Override offset" offset:[0,-4] checked:(iniCenterOrigin as BooleanClass)
		label lblModelFormat "Model format:" offset:[0,0] align:#left
		dropDownList ddModelFormat width:60 height:20 items:#(".stm", ".obj") selection:(iniStmFormat as integer) offset:[2,-22] align:#right		
		button exportButton "Export Mesh" width:140 height:20 offset:[-1,2] enabled:false
	)
	button importButton "Import Mesh (.stm)" width:140 height:20 offset:[0,3]
	
	-- set initial states
	on EditNormalsOption changed optionState do setINISetting mx_ini "IDV" "stm_normals" (optionState as string)
	on helpSelect changed optionState do setINISetting mx_ini "IDV" "stm_usehelp" (optionState as string)
	on ddModelFormat selected selection do setINISetting mx_ini "IDV" "stm_format" (ddModelFormat.selection as string)
	on EditNormalsOption changed optionState do setINISetting mx_ini "IDV" "stm_normals" (optionState as string)
	on ConvertUnitsOption changed optionState do (
		setINISetting mx_ini "IDV" "stm_convertunits" (optionState as string)
		UpdateList()
	)	
	
	on importButton pressed do (ImportSTMFile())
	on exportButton pressed do (
		if (selection.count > 0) then (
			if (stm.ddModelFormat.selected == ".stm") then (
				strFilename = getSaveFileName "Export STM" types:"SpeedTree Mesh (*.stm)|*.stm|All Files (*.*)|*.*"
				global LastFilename = (strFilename as string)
		
				if (strFilename != undefined) then (
					clearListener()
					ExportSTMFile()
					EnableSceneRedraw()
					setCommandPanelTaskMode mode:#utility
				)
				
			) else (	-- OBJ
				ExportOBJFile()
			)
			
		) else (
			messageBox "Please select an object to export." title:"Mesh Export Error" beep:false
		)
	)
	
	
	rollout stmTools "SpeedTree Mesh Tools" (
		button projectButton "Project normals" width:94 enabled:false offset:[-23,0]
		spinner projectBias range:[-1,1,0.5] fieldwidth:32 offset:[2,-23]
		button flipButton "Flip normals" across:4 offset:[16,3] enabled:false
		checkButton XSelect "X" width:18 offset:[32,3]
		checkButton YSelect "Y" width:18 offset:[21,3]
		checkButton ZSelect "Z" width:18 offset:[10,3] checked:true
		button cleanButton "Remove faces <" width:88 offset:[-26,2] enabled:false
		spinner cleanBias range:[0.0,0.04,0.002] scale:0.001 fieldwidth:38 offset:[2,-22] 	
		button pivotButton "Set pivot to:" align:#left offset:[-2,3] enabled:false
		dropDownList ddPivotLocation width:63 height:20 items:#("origin", "center") selection:1 offset:[3,-26] align:#right	
		--	button mirrorButton "Mirror over Y-axis (Legacy)" width:140 enabled:false
		-- button resetButton "Reset callbacks"
		-- checkButton helpSelect " ?  " width:18 offset:[0,-26] align:#right toolTip:"Toggle tooltip help" checked:(iniUseHelp as BooleanClass)
		
		on projectButton pressed do (
			print "fun!"
			if (selection.count > 0) then (
				ProjectNormals()
			) else (
				messageBox "No mesh object(s) selected." title:"Error" beep:false
			)
		)
		on flipButton pressed do (
			if (selection.count > 0) then (
				FlipNormals()
			) else (
				messageBox "No mesh object(s) selected." title:"Error" beep:false
			)
		)
		on cleanButton pressed do (
			if (selection.count > 0) then (
				CleanUpFaces()
			) else (
				messageBox "No mesh object(s) selected." title:"Error" beep:false
			)
		)
		on pivotButton pressed do (
			if (selection.count > 0) then (
				SetPivot cObject stm.stmTools.ddPivotLocation.selected
			) else (
				messageBox "No mesh object(s) selected." title:"Error" beep:false
			)
		)
		--	on mirrorButton pressed do MirrorObj()
		on resetButton pressed do (
			stmRemoveScripts()
			stmCallbacks()
		)
		on helpSelect changed state do (
			ToggleHelp state
		)
	)
	

	rollout stmUnits "Convert Units & Transforms" (
		group "Unit Converter" (		
			checkbox cbConvertSize "Size" checked:true
			checkbox cbConvertPos "Position" checked:false offset:[50,-20]
			label lblConvertFrom "From" offset:[0,4] align:#left
			dropDownList ddConvertFrom width:80 height:20 items:#("millimeters", "centimeters", "meters", "kilometers", "inches", "feet", "miles") offset:[-25,-22] align:#right selection:6	--selection:(iniStmFormat as integer) 
			label lblConvertTo "To" offset:[8,3] align:#left
			dropDownList ddConvertTo width:80 height:20 items:#("scene units", "millimeters", "centimeters", "meters", "kilometers", "inches", "feet", "miles") offset:[-25,-22] align:#right selection:1	--selection:(iniStmFormat as integer) 
			label lblConversionRatio "Scale -> 0.25 original" offset:[0,0] align:#left
			button btnConvertUnits "Convert Selected" width:140 height:20 offset:[0,0]
		)
		
		on ddConvertFrom selected selection do ConvertSceneUnits()
		on ddConvertTo selected selection do ConvertSceneUnits()
		on btnConvertUnits pressed do (
			undo on (	
				GetcObject()
				if (cObject != undefined) then (
					for s in arrSelected do (
						if stm.stmUnits.cbConvertSize.checked == true then s.scale = s.scale / fConversionScalar

						if stm.stmUnits.cbConvertPos.checked == true then (
							s.pos.x = s.pos.x / fConversionScalar
							s.pos.x = s.pos.x / fConversionScalar
							s.pos.x = s.pos.x / fConversionScalar
						)
					)
				)
			)
		)
		
		group "Copy Transforms" (
			checkbox copyPos_cb "Copy Position" checked:true
			checkbox copyRot_cb "Copy Rotation" checked:true
			checkbox copyScale_cb "Copy Scale" checked:true
			checkbox hideTarget_cb "Hide target after copy" offset:[0,10]
			pickbutton pbChooseTarget "- Select target -"  width:140 height:20 offset:[0,0]		
		)
		
		on pbChooseTarget picked targetObj do (
			undo on (
				objPosX = targetObj.pos.x
				objPosY = targetObj.pos.y
				objPosZ = targetObj.pos.z
				if targetObj.rotation != undefined then objRot = targetObj.rotation
				else objRot = (quat 0 0 0 1)
				objScale = targetObj.scale
				
				GetcObject()
				if (cObject != undefined) then (
					for s in arrSelected do (
						if targetObj != s then (
							if stm.stmUnits.copyRot_cb.checked == true then
								s.rotation = objRot
							
							if stm.stmUnits.copyPos_cb.checked == true then
								s.position = [objPosX, objPosY, objPosZ]
							
							if stm.stmUnits.copyScale_cb.checked == true then
								s.scale = objScale
							
							if stm.stmUnits.hideTarget_cb.checked == true then
								hide targetObj
						)
					)
				)
			)
		)
	)
	
	on stm open do (
		addRollout stmTools
		addRollout stmUnits
		stm.stmUnits.open = false		
		if (STextras != undefined) AND STextras.Open do CloseUtility STextras
		stmCallbacks()
		ConvertSceneUnits()
		UpdateList()
	)
	
	on stm close do (
		removeRollout stmTools
		removeRollout stmUnits
		stmRemoveScripts()
	)
)

UpdateList()

if (stm != undefined) AND stm.Open do CloseUtility stm

-- Force the launcher if it's around
LauncherPath = iniUtilityPath + "SpeedTree Utility Launcher 5.1.ms"
if (doesFileExist LauncherPath == true) then fileIn LauncherPath

OpenUtility stm -- rolledUp:true

macroScript QuickSaveSTM category:"SpeedTree" ( ExportSTMFile() )