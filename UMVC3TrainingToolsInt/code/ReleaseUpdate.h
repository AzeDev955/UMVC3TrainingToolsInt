#pragma once

#include <Windows.h>

namespace ReleaseUpdate
{
	inline constexpr char VersionTag[] = "v0.9.4";
	void CheckAtStartup(HWND toolWindow);
}
