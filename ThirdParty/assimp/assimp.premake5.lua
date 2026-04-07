function linkAssimp()
	externalincludedirs {
		"%{wks.location}/ThirdParty/assimp/include"
	}


	linkDynamic("%{wks.location}/ThirdParty/assimp", { "assimp" }, { "assimp.6", "assimp.6.0.5" })
end
