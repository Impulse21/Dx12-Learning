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
			-- "3rdParty/dxc_2020_10-22/inc",
		}

		links
		{
			"d3d12.lib",
			"dxgi.lib",
			"dxguid.lib",
			"GLFW",
			-- "3rdParty/dxc_2020_10-22/lib/dxcompiler.lib"
		}

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