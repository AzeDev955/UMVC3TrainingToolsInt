#pragma once

#include <Windows.h>

namespace ReleaseUpdate
{
	inline constexpr char VersionTag[] = "v1.0.0";
	void CheckAtStartup(HWND toolWindow);
}
