#include "pch.h"
#if _WIN32
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL
#endif


// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// by export a NvOptimusEnablement variable here, we are focing nvidia GPU to be selected.
/*
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
*/

// Enable usage of more performant GPUs on laptops.
extern "C" {
	EXPORT_SYMBOL int NvOptimusEnablement = 1;
	EXPORT_SYMBOL int AmdPowerXpressRequestHighPerformance = 1;
}