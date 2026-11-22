#pragma once

#include <Windows.h>

namespace ReleaseUpdate
{
	inline constexpr char VersionTag[] = "v0.9.8";
	void CheckAtStartup(HWND toolWindow);
}
