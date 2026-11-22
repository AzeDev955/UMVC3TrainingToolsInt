#pragma once

#include <filesystem>
#include <string>

namespace Localizer
{
	void Initialize(const std::filesystem::path& folder);
	bool IsSpanish();
	bool SetSpanish(bool spanish);
	const char* UiText(const char* key);
	std::string UiLabel(const char* key);
	std::wstring UiWideText(const char* key);
}
