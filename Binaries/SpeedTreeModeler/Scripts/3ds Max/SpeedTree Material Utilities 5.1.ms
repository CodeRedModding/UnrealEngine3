-------------------------------------------------------------------------------------------------
--	SpeedTree Extras Utilities - 3D Studio MAX Version
--
--	Tools for working with imported SpeedTree meshes
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
	global STextras -- Predefine rollout name
	global fConversionScalar
	global preNodes = for o in objects where o.parent == undefined collect o
	global arrSelected = global postNodes = global postMats = global postMulti = #()
	global bImporting = false
	global materialCount = 1	
	global globalMaterial	
	global bPromptWeld
	global arrCapturedValues = global arrDefaultValues = global arrValueComparison = #()
)
DeclareGlobals()

mx_ini = getMAXIniFile() -- Recall utility preferences
iniUtilityPath = getINISetting mx_ini "IDV" "stm_utilitypath"
iniProcessImported = getINISetting mx_ini "IDV" "ste_processimported"
if iniProcessImported == "" then iniProcessImported = true
iniComposite= getINISetting mx_ini "IDV" "ste_composite"
if iniComposite == "" then iniComposite = true
iniUncPaths= getINISetting mx_ini "IDV" "ste_uncpaths"
if iniUncPaths == "" then iniUncPaths = false
iniIncludeTranslucency = getINISetting mx_ini "IDV" "ste_IncludeTranslucency"
if iniIncludeTranslucency == "" then iniIncludeTranslucency = true
iniIncludeAO= getINISetting mx_ini "IDV" "ste_includeao"
if iniIncludeAO == "" then iniIncludeAO = false
iniMentalRay= getINISetting mx_ini "IDV" "ste_mentalray"
if iniMentalRay == "" then iniMentalRay = false
iniSwapTwoSided= getINISetting mx_ini "IDV" "ste_swaptwosided"
if iniSwapTwoSided == "" then iniSwapTwoSided = false
iniDisplacement = getINISetting mx_ini "IDV" "ste_displacement"
if iniDisplacement == "" then iniDisplacement = false

fn steRemoveScripts = ( 
	callbacks.removeScripts id:#stmChange
)

fn steCallbacks = (
	callbacks.addScript #selectionSetChanged "UpdateSTextras()" id:#stmChange
	callbacks.addScript #postRendererChange "UpdateSTextras()" id:#stmChange
	callbacks.addScript #preImport "preImport()"
	callbacks.addScript #filePreMerge "preNodes = for o in objects where o.parent == undefined collect o"
	callbacks.addScript #filePreOpen "if STextras.Open do CloseUtility STextras"
	callbacks.addScript #systemPreReset "if STextras.Open do CloseUtility STextras"
	callbacks.addScript #systemPreNew "if STextras.Open do CloseUtility STextras"
	callbacks.addScript #filePostMerge "OpenUtility STextras"
	callbacks.addScript #filePostOpen "OpenUtility STextras"
	callbacks.addScript #systemPostReset "OpenUtility STextras"
	callbacks.addScript #systemPostNew "OpenUtility STextras"
)

fn PreImport = (
	preNodes = for o in objects where o.parent == undefined collect o
	bImporting = true
)

fn ProcessImported = (
	if (iniProcessImported as BooleanClass) == true then 
		callbacks.addScript #postImport "PostImport()" id:#postImport
	else callbacks.removeScripts id:#postImport
)

fn UpdateSTextras = (
	
	arrSelected = #()
	if (selection.count > 0) then (
		for s in selection as array do (
			sClass = classOf s		
			sSuperClass = SuperClassOf s
			if (sSuperClass == GeometryClass AND s.modifiers["Hair and Fur"] == undefined AND sClass != SpeedTree_4 AND sClass != SpeedTree AND sClass != mr_Proxy) then (
				join arrSelected #(s)
			)
		)
	)
	-- arrSelected = for s in Geometry where (s.isSelected == true) collect s
	
	if arrSelected.count == 0 then (
		STextras.spVolumeInfluence.enabled = false
		STextras.btnShowVolume.enabled = false
		STextras.btnHideVolume.enabled = false
		STextras.btnClearVolume.enabled = false
		STextras.btnCalculateVolume.enabled =false
		STextras.btnProcessMaterial.enabled = false
		STextras.btnAssign.enabled = false
	) else (
		STextras.spVolumeInfluence.enabled = true
		STextras.btnShowVolume.enabled = true
		STextras.btnHideVolume.enabled = true
		STextras.btnClearVolume.enabled = true
		STextras.btnCalculateVolume.enabled =true
		STextras.btnProcessMaterial.enabled = true
		STextras.btnAssign.enabled = true		
	)
	
	if classof renderers.current != mental_ray_renderer then
		STextras.cbGammaCorrect.enabled = false
	else STextras.cbGammaCorrect.enabled = true
	
	if STextras.cbMentalRay.checked == false then (
	--	STextras.cbSwapTwoSided.enabled = false	
		STextras.cbDisplacement.enabled = false
	) else (
		if STextras.cbDisplacement.checked == true then
			STextras.spVolumeInfluence.enabled = true
		else if (arrSelected.count == 0) then 
			STextras.spVolumeInfluence.enabled = false
	)
	ProcessImported()
)


------------------------------------------------------------------------------------------------
-- Volume Displacement ---------------------------------------------------------------------

fn SetVolumeDisplacement obj = ( 
	local bConvertBack = false
	if classof obj == Editable_Poly then (
		ConvertToMesh obj
		bConvertBack = true
	)
	
	if classof obj == Editable_Mesh then (
		-- clear volume scalar first		
		meshop.setVertAlpha obj 99 #{1..obj.numverts}  1.0
		
		local numFaces = obj.numfaces
		-- need to find the max volume first
		local maxVolume = 0.0
		local i = j = 1
		while i <= numFaces do (
			elementFaces = meshop.getElementsUsingFace obj i
			lastFace = (elementFaces as array)[(elementFaces as array).count]
			
			elementVolume = 0.0
			for f in elementFaces do (
				Face= getFace obj f
				vert2 = getVert obj Face.z 
				vert1 = getVert obj Face.y 
				vert0 = getVert obj Face.x 
				dV = Dot (Cross (vert1 - vert0) (vert2 - vert0)) vert0
				ElementVolume+= dV
			) 
			elementVolume /= 6 
				
			if elementVolume > maxVolume then 
				maxVolume = elementVolume
			
			i = lastFace + 1
		)
		
		while j <= numFaces do (
			elementFaces = meshop.getElementsUsingFace obj j
			lastFace = (elementFaces as array)[(elementFaces as array).count]
			
			elementVolume = 0.0		
			for f in elementFaces do (
				Face= getFace obj f
				vert2 = getVert obj Face.z 
				vert1 = getVert obj Face.y 
				vert0 = getVert obj Face.x 
				dV = Dot (Cross (vert1 - vert0) (vert2 - vert0)) vert0
				ElementVolume+= dV 
			) 
			elementVolume /= 6 
			
			volumePercent = (elementVolume / maxVolume) * (1 / STextras.spVolumeInfluence.value)
			if volumePercent > 1.0 then volumePercent = 1.0

			elementVerts = meshop.getVertsUsedOnlyByFaces obj elementFaces
			meshop.setVertAlpha obj 99 elementVerts volumePercent	
			
			j = lastFace + 1
		)
	)
	
	if bConvertBack then convertTo obj Editable_Poly 
)


------------------------------------------------------------------------------------------------
-- Catch Material ----------------------------------------------------------------------------

fn CatchMaterial testMat = (
	
	-- need to test all maps for existence	
	bDiffuseMap = bSpecMap = bBumpMap = false
	bAlphaTest = bTransTest = bBumpTest = false
	strComputerName = sysInfo.computername
	
	-- diffuse map
	if classof testMat.diffuseMap == BitmapTexture then (
		bDiffuseMap = doesFileExist testMat.diffuseMap.filename
		if bDiffuseMap == false then (		-- checking local directory
			tmpFile = testMat.diffuseMap.filename as string
			arrSlashes = filterString tmpFile "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			tmpFilename = sysInfo.currentdir + "\\" + tmpFile
			
			if doesfileexist tmpFilename then (
				testMat.diffuseMap.filename = tmpFilename
				bDiffuseMap = true
			)			
		)
		
		if bDiffuseMap AND STextras.cbUncPaths.checked then (
			tmpPath = testMat.diffuseMap.filename as string
			arrSlashes = filterString tmpPath "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			fileInt = findString tmpPath tmpFile
			tmpPath = substring tmpPath 1 (fileInt - 1)
			
			if (tmpPath.count > 0 AND pathConfig.isUncPath tmpPath == false) then (
				arrSlashes = filterString tmpPath ":"
				tmpUncFilename = "\\\\" + strComputerName + "\\" + arrSlashes[1] + "$" + arrSlashes[2] + tmpFile
				
				testMat.diffuseMap.filename = tmpUncFilename
			)
		)
	)
	
	if bDiffuseMap then (
		testMat.diffuseMap.coordinates.mapChannel = 1
		bAlphaTest = testMat.diffuseMap.bitmap.hasAlpha
		
	) else if (classof testMat.diffuseMap == CompositeTexturemap) then (
		
		numProperties = (getPropNames testMat.diffuseMap).count
		if  numProperties >= 8 then (	-- catch the version of 3DS Max; this property changed in Max 2009
			local i = 1
			while i <= testMat.diffuseMap.mapList.count do (
				if classof testMat.diffuseMap.mapList[i] == BitmapTexture then (
					
					if doesFileExist testMat.diffuseMap.mapList[i].filename then
						bDiffuseMap = true
					else bDiffuseMap = false

					if bDiffuseMap == false then (		-- checking local directory
						tmpFile = testMat.diffuseMap.mapList[i].filename as string
						arrSlashes = filterString tmpFile "\\"
						tmpFile = arrSlashes[arrSlashes.count]
						tmpFilename = sysInfo.currentdir + "\\" + tmpFile
						
						if doesfileexist tmpFilename then (
							testMat.diffuseMap.mapList[i].filename = tmpFilename
							bDiffuseMap = true
						)
					)
					
					if bDiffuseMap AND STextras.cbUncPaths.checked then (
						tmpPath = testMat.diffuseMap.mapList[i].filename as string
						arrSlashes = filterString tmpPath "\\"
						tmpFile = arrSlashes[arrSlashes.count]
						fileInt = findString tmpPath tmpFile
						tmpPath = substring tmpPath 1 (fileInt - 1)
						
						if (tmpPath.count > 0 AND pathConfig.isUncPath tmpPath == false) then (
							arrSlashes = filterString tmpPath ":"
							tmpUncFilename = "\\\\" + strComputerName + "\\" + arrSlashes[1] + "$" + arrSlashes[2] + tmpFile
							
							testMat.diffuseMap.mapList[i].filename = tmpUncFilename
						)
					)
				
					if (bAlphaTest OR i == 1) then bAlphaTest = testMat.diffuseMap.mapList[i].bitmap.hasAlpha	
					
					-- do the mask if there's an alpha
					if (bDiffuseMap AND testMat.diffuseMap.mapList[i].bitmap.hasAlpha) then (
						testMat.diffuseMap.mask[i] = copy testMat.diffuseMap.mapList[i]
						testMat.diffuseMap.mask[i].monoOutput = 1
						testMat.diffuseMap.mask[i].RGBOutput = 1
						testMat.diffuseMap.mask[i].alphasource = 0
					)
				)
				
				i = i+1
			)
			showTextureMap testMat testMat.diffuseMap on	
		) else showTextureMap testMat testMat.diffuseMap.mapList[1] on -- older version of max
	)
	
	-- bump/normal map
	if classof testMat.bumpMap == Normal_Bump then
		testMat.bumpMap = testMat.bumpMap.normal_map
	
	if classof testMat.bumpMap == Utility_Gamma___Gain__mi then
		testMat.bumpMap = testMat.bumpMap.input_shader

	if classof testMat.bumpMap == BitmapTexture then (
		bBumpMap = doesFileExist testMat.bumpMap.filename
		
		if bBumpMap == false then (		-- checking local directory
			tmpFile = testMat.bumpMap.filename as string
			arrSlashes = filterString tmpFile "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			tmpFilename = sysInfo.currentdir + "\\" + tmpFile
			
			if doesfileexist tmpFilename then (
				testMat.bumpMap.filename = tmpFilename
				bBumpMap = true
			)
		)

		if bBumpMap AND STextras.cbUncPaths.checked then (
			tmpPath = testMat.bumpMap.filename as string
			arrSlashes = filterString tmpPath "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			fileInt = findString tmpPath tmpFile
			tmpPath = substring tmpPath 1 (fileInt - 1)
			
			if (tmpPath.count > 0 AND pathConfig.isUncPath tmpPath == false) then (
				arrSlashes = filterString tmpPath ":"
				tmpUncFilename = "\\\\" + strComputerName + "\\" + arrSlashes[1] + "$" + arrSlashes[2] + tmpFile
				
				testMat.bumpMap.filename = tmpUncFilename
			)
		)
	)
	
	if bBumpMap then (
		bBumpTest = testMat.bumpMap.bitmap.hasAlpha
	) else if (classof testMat.bumpMap == CompositeTexturemap) then (
		
		numProperties = (getPropNames testMat.bumpMap).count
		if  numProperties >= 8 then (	-- catch the version of 3DS Max; this property changed in Max 2009
			local i = 1
			while i <= testMat.bumpMap.mapList.count do (
				if (classof testMat.bumpMap.mapList[i] == Normal_Bump) then
					testMat.bumpMap.mapList[i] = testMat.bumpMap.mapList[i].normal_map
				if (classof testMat.bumpMap.mapList[i] == Utility_Gamma___Gain__mi) then
					testMat.bumpMap.mapList[i] = testMat.bumpMap.mapList[i].input_shader
				
				if (classof testMat.bumpMap.mapList[i] == BitmapTexture) then (
					if doesFileExist testMat.bumpMap.mapList[i].filename then
						bBumpMap = true
					else bBumpMap = false
				
					if bBumpMap == false then (		-- checking local directory
						tmpFile = testMat.bumpMap.mapList[i].filename as string
						arrSlashes = filterString tmpFile "\\"
						tmpFile = arrSlashes[arrSlashes.count]
						tmpFilename = sysInfo.currentdir + "\\" + tmpFile
						
						if doesfileexist tmpFilename then (
							testMat.bumpMap.mapList[i].filename = tmpFilename
							bBumpMap = true
						)
					)
					
					if bBumpMap AND STextras.cbUncPaths.checked then (
						tmpPath = testMat.bumpMap.mapList[i].filename as string
						arrSlashes = filterString tmpPath "\\"
						tmpFile = arrSlashes[arrSlashes.count]
						fileInt = findString tmpPath tmpFile
						tmpPath = substring tmpPath 1 (fileInt - 1)
						
						if (tmpPath.count > 0 AND pathConfig.isUncPath tmpPath == false) then (
							arrSlashes = filterString tmpPath ":"
							tmpUncFilename = "\\\\" + strComputerName + "\\" + arrSlashes[1] + "$" + arrSlashes[2] + tmpFile
							
							testMat.bumpMap.mapList[i].filename = tmpUncFilename
						)
					)
					
					if bBumpMap then (
						if (bBumpTest OR i == 1) then bBumpTest = testMat.bumpMap.mapList[i].bitmap.hasAlpha

						-- create mask if applicable
						if (bDiffuseMap AND testMat.diffuseMap.mapList[i].bitmap.hasAlpha) then (
							testMat.bumpMap.mask[i] = copy testMat.diffuseMap.mapList[i]
							testMat.bumpMap.mask[i].monoOutput = 1
							testMat.bumpMap.mask[i].RGBOutput = 1
							testMat.bumpMap.mask[i].alphasource = 0
						)
					)
				)
				
				i = i+1
			)
		)
	)
	
	--specular map
	if classof testMat.specularMap == BitmapTexture then (
		bSpecMap = doesFileExist testMat.specularMap.filename
		
		if bSpecMap == false then (		-- checking local directory
			tmpFile = testMat.specularMap.filename as string
			arrSlashes = filterString tmpFile "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			tmpFilename = sysInfo.currentdir + "\\" + tmpFile
			
			if doesfileexist tmpFilename then (
				testMat.specularMap.filename = tmpFilename
				bSpecMap = true
			)
		)

		if bSpecMap AND STextras.cbUncPaths.checked then (
			tmpPath = testMat.specularMap.filename as string
			arrSlashes = filterString tmpPath "\\"
			tmpFile = arrSlashes[arrSlashes.count]
			fileInt = findString tmpPath tmpFile
			tmpPath = substring tmpPath 1 (fileInt - 1)
			
			if (tmpPath.count > 0 AND pathConfig.isUncPath tmpPath == false) then (
				arrSlashes = filterString tmpPath ":"
				tmpUncFilename = "\\\\" + strComputerName + "\\" + arrSlashes[1] + "$" + arrSlashes[2] + tmpFile
				
				testMat.specularMap.filename = tmpUncFilename
			)
		)
	)
	
	if bSpecMap then bTransTest = testMat.specularMap.bitmap.hasAlpha
	arrTextures = #(bDiffuseMap,  bAlphaTest, bBumpMap, bBumpTest, bSpecMap, bTransTest)
	return arrTextures
)


------------------------------------------------------------------------------------------------
-- Process FBX Material --------------------------------------------------------------------

fn DoProcess testMat obj id = (
	if id == undefined then id = 1
	oldName = testMat.name
	bReplaceMaterial = false
	arrDefaultValues = #(0.5, 0.9, 0.75, 1.0, 0.5,  true, (color 170 200 110), 1.5, 3.0, 0.1, 0.9, 0.5, true, 0.15)
	arrValueComparison = #(true, true, true, true, true, true, true, true, true, true, true, true, true,true)
	
	-- catch mental ray materials
	if classof testMat == Arch___Design__mi then (
		tmpMat = StandardMaterial()
		if testMat.mapM0 != undefined then tmpMat.diffuseMap = testMat.mapM0
		if classof testMat.mapM2 == Two_Sided__base then
			if testMat.mapM2.front_shader  != undefined then tmpMat.specularMap = testMat.mapM2.front_shader 
		if testMat.bump_map != undefined then tmpMat.bumpMap = testMat.bump_map
		if classof tmpMat.bumpMap == Normal_Bump then tmpMat.bumpMap = tmpMat.bumpMap.normal_map
		if classof tmpMat.bumpMap == Utility_Gamma___Gain__mi then tmpMat.bumpMap = tmpMat.bumpMap.input_shader
			
		if STextras.cbMentalRay.checked then (
			arrCapturedValues[1] = testMat.diff_rough
			arrCapturedValues[2] = testMat.refl_weight
			arrCapturedValues[3] = testMat.refl_gloss
			arrCapturedValues[4] = testMat.refr_weight
			arrCapturedValues[5] = testMat.refr_gloss
			arrCapturedValues[6] = testMat.refr_trans_on
			arrCapturedValues[7] = testMat.refr_transc -- (color 170 200 110)
			if (classof testMat.mapM8 == Two_Sided__base AND classof testMat.mapM8.front_shader == BitmapTexture)  then (
				arrCapturedValues[8] = testMat.mapM8.front_shader.output.Output_Amount
				arrCapturedValues[9] = testMat.mapM8.back_shader.output.Output_Amount
			) else (
				arrCapturedValues[8] = 1.0
				arrCapturedValues[9] = 2.0
			)
			arrCapturedValues[10] = testMat.refl_func_low
			arrCapturedValues[11] = testMat.refl_func_high
			arrCapturedValues[12] = testMat.bump_map_amt
			arrCapturedValues[13] = testMat.displacement_map_on
			arrCapturedValues[14] = testMat.displacement_map_amt
		) else (
			for v = 1 to 14 do arrCapturedValues[v] = arrDefaultValues[v]
		)
		
		for v = 1 to 14 do (
			if (arrCapturedValues[v] != arrDefaultValues[v]) then arrValueComparison[v] = false
		)
		
		testMat = tmpMat
		bReplaceMaterial = true
	)
		
	if classof testMat == StandardMaterial then (
		testMat.shaderType = 1
		testMat.opacity = 100.0
		testMat.diffuseMapEnable = true
		
		-- catch prior AO inclusion
		if classof testMat.diffuseMap == RGB_Multiply then (
			if (classof testMat.diffuseMap.map1 == BitmapTexture OR classof testMat.diffuseMap.map1 == CompositeTexturemap) then
				testMat.diffuseMap = testMat.diffuseMap.map1
		)
		
		arrTextures = (CatchMaterial testMat)
		bDiffuseMap = arrTextures[1]
		bAlphaTest = arrTextures[2]
		bBumpMap = arrTextures[3]
		bBumpTest = arrTextures[4]
		bSpecMap = arrTextures[5]
		bTransTest = arrTextures[6]

		-- mental ray test
		bMentalRayTest = false			
		if classof renderers.current != mental_ray_renderer AND STextras.cbMentalRay.checked == true then (
			if queryBox "the current renderer must be set to mental ray before continuing. Assign mental ray renderer now?" beep:false then (
				renderers.current = mental_ray_renderer()
				renderers.production = mental_ray_renderer()
				bMentalRayTest = true
			) else (
				MessageBox "Mental ray materials have been disabled."
				STextras.cbMentalRay.checked = false
			)
		) else if STextras.cbMentalRay.checked == true then bMentalRayTest = true
		
		testMat.opacityMap = none
		if bAlphaTest then (
			
			if (classof testMat.diffuseMap == BitmapTexture) then (
				-- this is to prevent opocity processing if there happens to be an alpha channel and a detail texture (invalid for this process)
				testMat.twoSided = true
				testMat.diffuseMap.alphaSource = 2
				testMat.diffuseMap.monoOutput = 0
				testMat.opacityMap = copy testMat.diffuseMap
				testMat.opacityMapEnable = true
				testMat.opacityMap.alphasource = 0
				testMat.opacityMap.monoOutput = 1
				testMat.opacityMap.RGBOutput = 1
				testMat.opacityMap.preMultAlpha = false
			)
			
			if bSpecMap then (
				testMat.specularMap.coordinates.mapChannel = 1
				testMat.specularLevelMap = copy testMat.opacityMap
				testMat.specularLevelMap.output.Output_Amount = 0.6
				testMat.Glossiness = 50			
			) else
				testMat.specular = (color 0 0 0)
			
			if bTransTest AND STextras.cbIncludeTranslucency.checked AND bMentalRayTest == false then (
				testMat.shaderType = 7
				testMat.backsideSpecular = false
				testMat.translucentColorMapEnable = true
				testMat.translucentColorMap = RGB_Multiply()
				testMat.translucentColorMap.alphaFrom = 1
				testMat.translucentColorMap.map1 = copy testMat.diffuseMap
				testMat.translucentColorMap.map1.RGBOutput = 1
				testMat.translucentColorMap.map2	= Falloff()	
				testMat.translucentColorMap.map2.type = 2		-- fresnel effect
				testMat.translucentColorMap.map2.ior = 0.5
				testMat.translucentColorMap.map2.color2 = [0,0,0]
				testMat.translucentColorMap.map2.color1 = [255,255,255]
				testMat.translucentColorMap.map2.map2on = false
				testMat.translucentColorMap.map2.map1on = true		
				testMat.translucentColorMap.map2.map1 = RGB_Multiply()
				testMat.translucentColorMap.map2.map1.map1 = copy testMat.specularMap
				testMat.translucentColorMap.map2.map1.map1.RGBOutput = 1
				testMat.translucentColorMap.map2.map1.map2 = testMat.diffuseMap
				testMat.translucentColorMap.map2.map1.alphaFrom = 1
			)
		)
		
		if STextras.cbIncludeAO.checked == true AND classof testMat.diffuseMap != RGB_Multiply then (
			tmpDiffuse = testMat.diffuseMap
			testMat.diffuseMap = RGB_Multiply()
			testMat.diffuseMap.map1 = tmpDiffuse
			testMat.diffuseMap.map2 = Vertex_Color()
			if (classof testMat.diffuseMap.map1 == BitmapTexture) then 
				testMat.diffuseMap.map1.coordinates.mapChannel = 1
			else if (classof testMat.diffuseMap.map1 == CompositeTexturemap) then
				testMat.diffuseMap.map1.mapList[1].coordinates.mapChannel = 1
			showTextureMap testMat off
			showTextureMap testMat testMat.diffuseMap.map1 on
		) else
			testMat.showInViewport = true

		if testMat.bumpMap != undefined then (
			testMat.bumpMapEnable = true
			testMat.bumpMapAmount = 80
			
			if (Normal_Bump != undefined) then (
				if (classof testMat.bumpMap == BitmapTexture) then (
					originalBump = testMat.bumpMap
					testMat.bumpMap = Normal_Bump()
					testMat.bumpMap.flipgreen = true
					testMat.bumpMap.normal_map = originalBump
					testMat.bumpMap.normal_map.alphasource = 2
					testMat.bumpMap.normal_map.monoOutput = 0
					testMat.bumpMap.normal_map.RGBOutput = 0
					testMat.bumpMap.normal_map.coordinates.mapChannel = 1
								
					if (STextras.cbGammaCorrect.enabled AND STextras.cbGammaCorrect.checked AND bMentalRayTest == false) then (
						originalNormal= testMat.bumpMap.normal_map
						testMat.bumpMap.normal_map = Utility_Gamma___Gain__mi()
						testMat.bumpMap.normal_map.gamma = fileInGamma
						testMat.bumpMap.normal_map.reverse = false
						testMat.bumpMap.normal_map.input_connected = true
						testMat.bumpMap.normal_map.input_shader = originalNormal
					)
					
				) else if (classof testMat.bumpMap == CompositeTexturemap) then (
					numProperties = (getPropNames testMat.bumpMap).count
					if  numProperties >= 8 then (	-- catch the version of 3DS Max; this property changed in Max 2009
						local i = 1
						while i <= testMat.bumpMap.mapList.count do (
							local originalBump= testMat.bumpMap.mapList[i]
							testMat.bumpMap.mapList[i] = Normal_Bump()
							testMat.bumpMap.mapList[i].flipgreen = true
							testMat.bumpMap.mapList[i].normal_map = originalBump
							testMat.bumpMap.mapList[i].normal_map.alphasource = 2
							testMat.bumpMap.mapList[i].normal_map.monoOutput = 0
							testMat.bumpMap.mapList[i].normal_map.RGBOutput = 0
							testMat.bumpMap.mapList[i].normal_map.coordinates.mapChannel = 1
							
							i = i+1
						)
					)
				)
			)
		)
		
		-- Mental Ray
		if bMentalRayTest then (
			tmpMat = Arch___Design__mi()
			tmpMat.mapM0 = testMat.diffuseMap
			if arrValueComparison[1] then tmpMat.diff_rough = 0.5
			else tmpMat.diff_rough = arrCapturedValues[1]
				
			if bAlphaTest then (	-- for translucency	
				tmpMat.cutout_map = testMat.opacityMap
				
				-- reflection

				if arrValueComparison[2] then tmpMat.refl_weight = 0.9
				else tmpMat.refl_weight = arrCapturedValues[2]

				if arrValueComparison[3] then tmpMat.refl_gloss = 0.75
				else tmpMat.refl_gloss = arrCapturedValues[3]
				
				tmpMat.refl_interp  = true
				tmpMat.refl_hlonly = true
				if bSpecMap then (
					tmpMat.mapM2 = Two_Sided__base()
					tmpMat.mapM2.front = (color 128 128 128)
					tmpMat.mapM2.back = (color 0 0 0)					
					tmpMat.mapM2.front_shader = copy testMat.specularMap
				) else tmpMat.refl_color = (color 0 0 0)

				-- refraction
				if arrValueComparison[4] then tmpMat.refr_weight = 1.0
				else tmpMat.refr_weight = arrCapturedValues[4]

				tmpMat.mapM4 = testMat.diffuseMap
				
				if arrValueComparison[5] then tmpMat.refr_gloss = 0.5
				else tmpMat.refr_gloss = arrCapturedValues[5]
				
				tmpMat.refr_ior = 1.0 -- vaccuum
				
				if arrValueComparison[6] then tmpMat.refr_trans_on = true
				else tmpMat.refr_trans_on = arrCapturedValues[6]
				
				if arrValueComparison[7] then tmpMat.refr_transc = (color 170 200 110)
				else tmpMat.refr_transc = arrCapturedValues[7]
				
				tmpMat.mapM7 = testMat.diffuseMap			
				tmpMat.mapM8 = Two_Sided__base()
				tmpMat.mapM8.front = (color 128 128 128)
				tmpMat.mapM8.back = (color 255 255 255)	
				
				if bSpecMap And bTransTest then (
					tmpMat.mapM8.front_shader = copy testMat.specularMap
					tmpMat.mapM8.front_shader.RGBOutput = 1
					tmpMat.mapM8.front_shader.alphasource = 0
					tmpMat.mapM8.front_shader.monoOutput = 1
					if arrValueComparison[8] then tmpMat.mapM8.front_shader.output.Output_Amount = 1.5
					else tmpMat.mapM8.front_shader.output.Output_Amount = arrCapturedValues[8]
						
					tmpMat.mapM8.back_shader = copy tmpMat.mapM8.front_shader
					if arrValueComparison[9] then tmpMat.mapM8.back_shader.output.Output_Amount = 3.0
					else tmpMat.mapM8.back_shader.output.Output_Amount = arrCapturedValues[9]
				)
				
				-- BRDF
				if arrValueComparison[10] then tmpMat.refl_func_low = 0.1
				else tmpMat.refl_func_low = arrCapturedValues[10]

				if arrValueComparison[11] then tmpMat.refl_func_high = 0.9
				else tmpMat.refl_func_high = arrCapturedValues[11]
				
				-- advanced rendering options			
				tmpMat.refl_falloff_dist = 25.0
				tmpMat.opts_1sided = true -- thin-walled
				tmpMat.opts_do_refractive_caustics = true -- use "transparent shodows
				tmpMat.opts_propagate_alpha = true -- transparency propagates alpha channel
				
				-- fast glossy interpolation
				tmpMat.inter_density = 4
			) else (
				tmpMat.refl_weight = 0.1
				tmpMat.refl_gloss = 0.2
				tmpMat.mapM2 = testMat.diffuseMap
				tmpMat.refl_color = (color 0 0 0)
			)
			
			
			-- catch bump map if it's there
			if bBumpMap then (
				tmpMat.bump_map = testMat.bumpMap
				if arrValueComparison[12] then tmpMat.bump_map_amt = 0.5
				else tmpMat.bump_map_amt = arrCapturedValues[12]
				
				if (classof tmpMat.bump_map == CompositeTexturemap) then (
					numProperties = (getPropNames tmpMat.bumpMap).count
					if  numProperties >= 8 then (	-- catch the version of 3DS Max; this property changed in Max 2009
						local i = 1
						while i <= tmpMat.bump_map.mapList.count do (
							if (Normal_Bump != undefined AND classof tmpMat.bump_map.mapList[i] == Normal_Bump) then (						
								if STextras.cbGammaCorrect.checked then (
									originalNormal= testMat.bumpMap.mapList[i].normal_map
									tmpMat.bump_map.mapList[i].normal_map = Utility_Gamma___Gain__mi()
									tmpMat.bump_map.mapList[i].normal_map.gamma = fileInGamma
									tmpMat.bump_map.mapList[i].normal_map.reverse = false
									tmpMat.bump_map.mapList[i].normal_map.input_connected = true
									tmpMat.bump_map.mapList[i].normal_map.input_shader = originalNormal
								)
							)
							i = i+1
						)
					)
				) else if (Normal_Bump != undefined AND  classof tmpMat.bump_map == Normal_Bump) then (
					if STextras.cbGammaCorrect.checked then (
						originalNormal= tmpMat.bump_map.normal_map
						tmpMat.bump_map.normal_map = Utility_Gamma___Gain__mi()
						tmpMat.bump_map.normal_map.gamma = fileInGamma
						tmpMat.bump_map.normal_map.reverse = false
						tmpMat.bump_map.normal_map.input_connected = true
						tmpMat.bump_map.normal_map.input_shader = originalNormal
					)
				)
			)
			
				
			if (STextras.cbDisplacement.checked == true AND bBumpTest AND bAlphaTest == false) then (
				renderers.production.Smooth = false
				renderers.production.Max_Level = 4
				
				tmpMat.displacement_map = tmpMat.bump_map
				if (classof tmpMat.displacement_map == Normal_Bump) then (
						tmpMat.displacement_map = tmpMat.displacement_map.normal_map
						if STextras.cbGammaCorrect.checked then
							tmpMat.displacement_map = tmpMat.displacement_map.input_shader
						tmpMat.displacement_map.RGBOutput = 1
						tmpMat.displacement_map.alphasource = 0
						tmpMat.displacement_map.monoOutput = 1
				) else if (classof tmpMat.displacement_map == CompositeTexturemap) then (
					numProperties = (getPropNames tmpMat.displacement_map).count
					if  numProperties >= 8 then (	-- catch the version of 3DS Max; this property changed in Max 2009
						local i = 1
						while i <= tmpMat.displacement_map.mapList.count do (
							if (Normal_Bump != undefined AND classof tmpMat.displacement_map.mapList[i] == Normal_Bump) then (						
								tmpMat.displacement_map.mapList[i] = tmpMat.displacement_map.mapList[i].normal_map
								if STextras.cbGammaCorrect.checked then
									tmpMat.displacement_map.mapList[i] = tmpMat.displacement_map.mapList[i].input_shader
								tmpMat.displacement_map.mapList[i].RGBOutput = 1
								tmpMat.displacement_map.mapList[i].alphasource = 0
								tmpMat.displacement_map.mapList[i].monoOutput = 1
							)
							i = i+1
						)
					)
				)
				
				if STextras.spVolumeInfluence.value != 0 then (
					-- enable map support for volume displacement
					if (meshop.getMapSupport obj 99 == false) then
						meshop.setVertAlpha obj 99 #{1..obj.numverts}  1.0
					
					originalDispMap = tmpMat.displacement_map
					tmpMat.displacement_map = RGB_Multiply()
					tmpMat.displacement_map.map1 = originalDispMap
					tmpMat.displacement_map.map2 = Vertex_Color()
					tmpMat.displacement_map.map2.map = 99
					tmpMat.displacement_map.alphaFrom = 1
					tmpMat.displacement_map_amt = 0.5
				)
				
				if bImporting then (
					if bPromptWeld == undefined then (
						if queryBox "Displacement is intended to work with watertight (welded) geometry. Apply 'Vertex Weld' modifier now?" beep:false then
							bPromptWeld = true
						else bPromptWeld = false
					)
					
					if bPromptWeld == true then (
						addModifier obj(Vertex_Weld())
						obj.modifiers[#Vertex_Weld].threshold = 0.001
						convertToMesh obj 
					)
				)
			)
			
			if arrValueComparison[13] then tmpMat.displacement_map_on = true
			else tmpMat.displacement_map_on = arrCapturedValues[13]			
			
			if arrValueComparison[14] then tmpMat.displacement_map_amt = 0.15	
			else tmpMat.displacement_map_amt = arrCapturedValues[14]						
			
			--	if (STextras.cbSwapTwoSided.checked == true AND classof tmpMat == Arch___Design__mi) then (
			--		globalMaterial = tmpMat
			--		for m = 1 to 16 do (
			--			str =  "if (classof globalMaterial.mapM" + m as string + " == Two_Sided__base) then ( \n" + 
			--						"swap globalMaterial.mapM" + m as string + ".front_shader globalMaterial.mapM" + m as string + ".back_shader \n" +
			--						"swap globalMaterial.mapM" + m as string + ".front globalMaterial.mapM" + m as string + ".back \n" +
			--					")"
			--			execute str
			--		)
			--		globalMaterial = undefined
			-- )
			
			if (STextras.cbIncludeAO.checked == true AND bAlphaTest != true AND classof tmpMat.diff_color_map.map1 == CompositeTexturemap) then
				showTextureMap tmpMat tmpMat.diff_color_map.map1 on
			else tmpMat.showInViewport = true
			
			testMat = tmpMat
			bReplaceMaterial = true
		)
		
		-- replace material if necessary
		if bReplaceMaterial then (
			testMat.name = oldName
			if getNumSubMtls obj.material == 0 then
				obj.material = testMat
			else obj.material[id] = testMat
		)
		
		if (STextras.cbGammaCorrect.checked AND STextras.cbGammaCorrect.enabled AND IDisplayGamma.colorCorrectionMode != #gamma) then
			print "WARNING: Gamma correction has been applied to the material(s), but is not enabled under 'Rendering -> Gamma/LUT setup'"
		
		if (STextras.cbGammaCorrect.checked == false AND STextras.cbGammaCorrect.enabled AND IDisplayGamma.colorCorrectionMode == #gamma) then
			print "WARNING: Gamma correction has NOT been applied to the material(s). Normal maps will render incorrectly.'"
		
	) else print "unsupported material class"

)
				
fn ProcessMaterials obj = (
	undo on (
		if obj != undefined then postNodes = obj
		
		for n in postNodes do (
			mat = n.material
			if mat != undefined then (
				if classof mat == StandardMaterial AND mat.diffuseMap != undefined then (
					if (findItem postMats mat == 0) then
						DoProcess mat n 1
				) else if classof mat == MultiMaterial then (
				
					-- have to sort out which material ID's are used on this object
					usedIDs = #()
					if SuperClassOf n == GeometryClass AND classOf n == Editable_Mesh then (
						faceCount = n.numfaces				
						for f = 1 to faceCount do
							append usedIDs (getFaceMatID n f)
					
					) else if SuperClassOf n == GeometryClass AND classOf n == Editable_Poly then (
						faceCount = n.getNumFaces()
						for f = 1 to faceCount do
							append usedIDs (polyop.getFaceMatID n f)
					)
					
					for i = 1 to usedIDs.count do (
						k = i + 1
						while (k <= usedIDs.count) do (
							if usedIDs[k] == usedIDs[i] then 
								deleteItem usedIDs k
							else k = k + 1
						)
					)
					
					for i in usedIDs do
						DoProcess mat[i] n i
				)
				if (findItem postMats n.material == 0) then
					append postMats n.material
			)			
		)
		
		bPromptWeld = undefined
		
		for i = 1 to postMats.count do (
			k = i + 1
			while (k <= postNodes.count) do (
				if postMats[k] != undefined AND postMats[k].name == postMats[i].name then 
					deleteItem postMats k
				else k = k + 1
			)
		)
		
		
		if postMats.count > 1 then (
		--	sort postMats
			postMulti = multimaterial()
			postMulti.materialList = postMats
		) else 
			postMulti = postMats[1]
		
		if STextras.cbComposite.checked == true AND postMats.count > 1 then (		-- multimaterial
			
			for n in postNodes do (
				newID = findItem postMulti.materialList n.material
				if newID == 0 then newID = 1
				
				if SuperClassOf n == GeometryClass AND classOf n == Editable_Mesh then (
					faceCount = n.numfaces				
					for f = 1 to faceCount do
						setFaceMatID n f newID
				
				) else if SuperClassOf n == GeometryClass AND classOf n == Editable_Poly then (
					faceCount = n.getNumFaces()
					for f = 1 to faceCount do
						polyop.setFaceMatID n f newID
				)
				n.material = postMulti
			)
			
			m = s = 1
			used = false
			while used == false do (
				if s > scenematerials.count OR scenematerials[s] == undefined then (
					setMeditMaterial m postMulti
					MeditMaterials[m].name = "SpeedTree Material " + (materialCount as string)
					materialCount = materialCount + 1
					used = true
				) else (
					while s <= scenematerials.count AND m < 25 AND meditmaterials[m].name != postMulti.name do (
						if meditmaterials[m].name == scenematerials[s].name then (
							m = m + 1
							s = 1
						) else s = s + 1
					) 
					setMeditMaterial m postMulti	
					MeditMaterials[m].name = "SpeedTree Material " + (materialCount as string)
					materialCount = materialCount + 1					
					used = true
				)
			)
			
		) else (		-- separate materials
			m = 1
			for p in postMats do (
				used = false
				s = 1
				while used == false do (
					if s > scenematerials.count OR scenematerials[s] == undefined then (
						setMeditMaterial m p
						used = true
						m = m + 1
					) else (
						while s <= scenematerials.count AND m < 25 AND meditmaterials[m].name != p.name do (
							if scenematerials[s].name == meditmaterials[m].name then (
								m = m + 1
								s = 1
							) else s = s + 1
						) 
						setMeditMaterial m p
						used = true
					)
				)
			)
		)
		postMats = postMulti = #()
	)
)

fn FindImportedObjects = (
postNodes = for o in objects where o.parent == undefined collect o
	if preNodes.count != 0 do (
		for n = 1 to preNodes.count do (
			if findItem postNodes preNodes[n] != 0 then (
				i = findItem postNodes preNodes[n]
				deleteItem postNodes i
			)
		)
	)
	ProcessMaterials postNodes
)

fn PostImport = (
	FindImportedObjects()
	bImporting = false
)


-- Utility  --------------------------------------------------

utility STextras "SpeedTree Material Utilities 5.1" (

	group "FBX Material Processor" (	
		button btnProcessMaterial "Process Selected" width:142 height:20 offset:[-1,3]
		label lblLine1 "__General Settings__________" offset:[-8,6] align:#left	
		
		checkbox cbProcessImported "Process imported objects" checked:(iniProcessImported as BooleanClass) offset:[-3,0]			
		checkbox cbComposite "Create multimaterial" checked:(iniComposite as BooleanClass) offset:[-3,0]
		checkbox cbUncPaths "Create UNC filepaths" checked:(iniUncPaths as BooleanClass) offset:[-3,0]
		label lblLine2 "__Renderable Settings_______" offset:[-8,6] align:#left	
		checkbox cbIncludeTranslucency "Include translucency" checked:(iniIncludeTranslucency as BooleanClass) offset:[-3,0]
		checkbox cbIncludeAO "Include ambient occlusion" checked:(iniIncludeAO as BooleanClass) offset:[-3,0]
		label lblVertexColors "(renders vertex colors)" offset:[15,0] align:#left
		label lblLine3 "__mental ray Settings________" offset:[-8,6] align:#left	
		checkbox cbGammaCorrect "Fix normal map gamma" checked:true offset:[-3,0]		
		checkbox cbMentalRay "Use mental ray materials" checked:(iniMentalRay as BooleanClass) offset:[-3,0]
		--checkbox cbSwapTwoSided "Swap Two-Sided Base"  checked:(iniSwapTwoSided as BooleanClass) offset:[16,0] visible:false
		checkbox cbDisplacement "Include displacement" checked:(iniDisplacement as BooleanClass) offset:[16,0]
		spinner spVolumeInfluence "Volume scalar" range:[0,3,0.15] type:#float scale:0.01 fieldwidth: 35 offset:[0,5]
		button btnShowVolume "show" width:56 height:16 pos:[37,328]
		button btnHideVolume "hide" width:56 height:16 pos:[37,328] visible:false
		button btnClearVolume "clear" width:56 height:16 pos:[93,328]
		button btnCalculateVolume "calculate now" width:112 height:16 offset:[11,0]
	)
	
	on btnProcessMaterial pressed do (
		if (selection.count > 0) then (
			ProcessMaterials (selection as array)
		)
	)
	on cbProcessImported changed optionState do (
		setINISetting mx_ini "IDV" "ste_processimported" (optionState as string)
		iniProcessImported = optionState as string
		ProcessImported()
	)
	on cbComposite changed optionState do (
		setINISetting mx_ini "IDV" "ste_composite" (optionState as string)
	)
	on cbUncPaths changed optionState do (
		setINISetting mx_ini "IDV" "ste_uncpaths" (optionState as string)
	)
	on cbIncludeTranslucency changed optionState do (
		setINISetting mx_ini "IDV" "ste_IncludeTranslucency" (optionState as string)
	)
	on cbIncludeAO changed optionState do (
		setINISetting mx_ini "IDV" "ste_includeao" (optionState as string)
	)
	on cbMentalRay changed optionState do (
		setINISetting mx_ini "IDV" "ste_mentalray" (optionState as string)
		
		if optionState then (
		--	cbSwapTwoSided.enabled = true
			cbDisplacement.enabled = true
			if (selection.count > 0 OR cbDisplacement.checked) then
				spVolumeInfluence.enabled = true
			else spVolumeInfluence.enabled = false
		) else (
		--	cbSwapTwoSided.enabled =false
			cbDisplacement.enabled = false
			if (selection.count == 0) then
				spVolumeInfluence.enabled = false
		)
	)
	on cbSwapTwoSided changed optionState do (
		setINISetting mx_ini "IDV" "ste_swaptwosided" (optionState as string)
	)
	on cbDisplacement changed optionState do (
		setINISetting mx_ini "IDV" "ste_displacement" (optionState as string)
		
		if optionState then (
			spVolumeInfluence.enabled = true
		) else (
			if (selection.count == 0) then
				spVolumeInfluence.enabled = false
		)
	)
	
	on btnShowVolume pressed do (
		arrSelected = for s in Geometry where (s.isSelected == true) collect s
		for s in arrSelected do (
			s.showVertexColors = true
			s.vertexColorType = #map_channel
			s.vertexColorMapChannel = 99
		)
		btnShowVolume.visible = false
		btnHideVolume.visible = true
	)
	
	on btnHideVolume pressed do (
		arrSelected = for s in Geometry where (s.isSelected == true) collect s
		for s in arrSelected do 
			s.showVertexColors = false

		btnShowVolume.visible = true
		btnHideVolume.visible = false
	)
	
	on btnClearVolume pressed do (
		undo on (
			for s in arrSelected do (
				local bConvertBack = false
				if SuperClassOf s == GeometryClass AND classOf s.baseObject == Editable_Poly then (
					ConvertToMesh s
					bConvertBack = true
				)
				if SuperClassOf s == GeometryClass AND classOf s.baseObject == Editable_Mesh then (
					sVerts = #{1..s.numverts} 
					meshop.setVertAlpha s 99 sVerts 1.0
				)
				if bConvertBack then convertTo s Editable_Poly
				redrawViews()
			)
		)
	)
	
	on btnCalculateVolume pressed do (
		for s in arrSelected do (
			SetVolumeDisplacement s
			s.showVertexColors = true
			s.vertexColorType = #map_channel
			s.vertexColorMapChannel = 99
		)
		btnShowVolume.visible = false
		btnHideVolume.visible = true
	)
		
	group "Assign Material ID" (
		spinner id_spinner "ID" range:[1,12,1] fieldwidth:24 offset:[0,3] type:#integer align:#left
		button btnAssign "- Assign -"  width:84 height:20 offset:[26,-23]	
	)

	on btnAssign pressed do (
		undo on (
			for s in arrSelected do (
				if SuperClassOf s == GeometryClass AND classOf s.baseObject == Editable_Mesh then (
					faceCount = s.numfaces
					faceSel = #()						
					for f = 1 to faceCount do (
						setFaceMatID s f STextras.id_spinner.value
					)
				
				) else if SuperClassOf s == GeometryClass AND classOf s.baseObject == Editable_Poly then (
					faceCount = s.baseObject.getNumFaces()
					for f = 1 to faceCount do (
						polyop.setFaceMatID s.baseObject f STextras.id_spinner.value
					)
				)
			)
		)
	)

	
	on STextras open do (
		if (stm != undefined) AND stm.Open do CloseUtility stm	
		SteCallbacks()
		UpdateSTextras()
		ProcessImported()
	)
	
	on STextras close do (
		steRemoveScripts ()
		preNodes = postNodes = postMats = #()
	)
)

if (STextras != undefined) AND STextras.Open do CloseUtility STextras

-- Force the launcher if it's around
LauncherPath = iniUtilityPath + "SpeedTree Utility Launcher 5.1.ms"
if (doesFileExist LauncherPath == true) then fileIn LauncherPath

OpenUtility STextras

ProcessImported()
UpdateSTextras()