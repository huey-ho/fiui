#pragma once

#if defined(_WIN32)
#if defined(FIUI_BUILDING_DLL)
#define FIUI_API __declspec(dllexport)
#else
#define FIUI_API __declspec(dllimport)
#endif
#else
#define FIUI_API
#endif
