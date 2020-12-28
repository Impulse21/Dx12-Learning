workspace "Dx12Learning"
	location "Generated"
	architecture "x64"
	startproject "TriangleTest"

	configurations
	{
		"Debug",
		"Release"
	}
	
	flags
	{
		"MultiProcessorCompile"
	}

	defines { }

	filter "system:windows"
		systemversion "latest"

	-- We now only set settings for the Debug configuration
	filter { "configurations:Debug" }
		defines { "DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines { "RELEASE" }
		runtime "Release"
		optimize "On"

	-- Reset the filter for other settings
	filter { }

	-- Here we use some "tokens" (the things between %{ ... }). They will be replaced by Premake
	-- automatically when configuring the projects.
	-- * %{prj.name} will be replaced by "ExampleLib" / "App" / "UnitTests"
	--  * %{cfg.longname} will be replaced by "Debug" or "Release" depending on the configuration
	-- The path is relative to *this* folder
	targetdir ("Build/Bin/%{prj.name}/%{cfg.longname}")
	objdir ("Build/Obj/%{prj.name}/%{cfg.longname}")
	outputdir = "Build/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "_ThridParty"
	include "ThridParty/glfw"
	include "ThridParty/imgui"

	externalproject "DirectXTex_Desktop_2019"
		location "ThridParty/DirectXTex/DirectXTex"
		uuid "57940020-8E99-AEB6-271F-61E0F7F6B73B"
		kind "StaticLib"
		language "C++"

group "Core"
	project "Core"
		kind "StaticLib"
		language "C++"
		cppdialect "C++17"

		pchheader "pch.h"
		pchsource "Core/pch.cpp"

		-- nuget { "directxtk12_desktop_2017:2020.8.15.1" }

		files
		{
			"Core/**.h",
			"Core/**.inl",
			"Core/**.cpp",
		}

		includedirs
		{
			"Core",
			"ThridParty/spdlog/include",
			
			"ThridParty/glfw/include",
			"ThridParty/imgui",
			"ThridParty/DirectXTex"
		}

		links
		{
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
			"GLFW",
			"DirectXTex",
			"Imgui"
			-- "3rdParty/dxc_2020_10-22/lib/dxcompiler.lib"
		}

group "GraphicTechniques"
	project "Lighting"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"

		files
		{
			"GraphicTechniques/Lighting/**.h",
			"GraphicTechniques/Lighting/**.cpp",
			"GraphicTechniques/Shaders/**.hlsl",
			"GraphicTechniques/Shaders/**.hlsli",
		}

		includedirs
		{
			"GraphicTechniques/Lighting",
			"Core",

			"ThridParty/spdlog/include",
			"ThridParty/imgui",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"Imgui",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}

		filter { "files:**VS.hlsl" }
			shadertype "Vertex"
			shadermodel "6.0"
		filter { }

		filter { "files:**PS.hlsl" }
			shadertype "Pixel"
			shadermodel "6.0"
		filter { }
		
group "UnitTests"
	project "TriangleTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/TriangleTest/**.h",
			"UnitTests/TriangleTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/TriangleTest",
			"Core",
			
			"ThridParty/spdlog/include",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}

	project "ConstantBufferTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/ConstantBufferTest/**.h",
			"UnitTests/ConstantBufferTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/ConstantBufferTest",
			"Core",
			
			"ThridParty/spdlog/include",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}

	project "StructuredBufferTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/StructuredBufferTest/**.h",
			"UnitTests/StructuredBufferTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/StructuredBufferTest",
			"Core",
			
			"ThridParty/spdlog/include",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}


	project "TexturedTriangleTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/TexturedTriangleTest/**.h",
			"UnitTests/TexturedTriangleTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/TexturedTriangleTest",
			"Core",
			
			"ThridParty/spdlog/include",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}

	project "ImguiTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/ImguiTest/**.h",
			"UnitTests/ImguiTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/ImguiTest",
			"Core",
			
			"ThridParty/spdlog/include",
			"ThridParty/imgui",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"Imgui",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}

		
	project "ModelTest"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		
		files
		{
			"UnitTests/ModelTest/**.h",
			"UnitTests/ModelTest/**.cpp",
		}

		includedirs
		{
			"UnitTests/ModelTest",
			"Core",

			"ThridParty/spdlog/include",
			"ThridParty/imgui",
		}

		links 
		{ 
			"Core",
			"GLFW",
			"Imgui",
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
		}
	
	-- TODO
	-- Structured Buffer FromComput Shader
	-- Depth Test
	-- Model Loading
	-- SkyBox?
	-- Root Constants
	-- IMGUI
	-- Dynamic Upload Buffer
	-- Volatile Constant Buffer
	-- BEZIER Plane

	-- Techniques
	-- BRDF
	-- Shadow Map
	-- Deffered Shading
	-- Forward Plus Rendering
	-- OMNI Direction Shader map
	-- IBL
	-- Multiplue point shader maps