#include "../pch.h"
#include "Localizer.h"
#include <atomic>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace
{
	constexpr wchar_t Section[] = L"Interface";
	constexpr wchar_t LanguageKey[] = L"Language";
	std::filesystem::path configPath;
	std::atomic<bool> spanish{ true };

	struct Translation
	{
		std::string english;
		std::string spanish;
	};

	struct TextHash
	{
		using is_transparent = void;
		size_t operator()(std::string_view text) const { return std::hash<std::string_view>{}(text); }
	};

	std::unordered_map<std::string, Translation, TextHash, std::equal_to<>> translations;

	std::vector<std::string> ParseRow(const std::string& line)
	{
		std::vector<std::string> fields(1);
		bool quoted = false;
		for (size_t index = 0; index < line.size(); ++index)
		{
			const char character = line[index];
			if (character == '"')
			{
				if (quoted && index + 1 < line.size() && line[index + 1] == '"')
				{
					fields.back() += '"';
					++index;
				}
				else
					quoted = !quoted;
			}
			else if (character == ',' && !quoted)
				fields.emplace_back();
			else if (character == '\\' && index + 1 < line.size() && line[index + 1] == 'n')
			{
				fields.back() += '\n';
				++index;
			}
			else
				fields.back() += character;
		}
		return fields;
	}
}

void Localizer::Initialize(const std::filesystem::path& folder)
{
	configPath = folder / L"UMVC3TrainingToolsInt.ini";
	wchar_t localAppData[MAX_PATH]{};
	const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
	if (length && length < MAX_PATH)
	{
		const auto preferences = std::filesystem::path(localAppData) / L"UMVC3TrainingToolsInt";
		std::error_code error;
		std::filesystem::create_directories(preferences, error);
		if (!error)
			configPath = preferences / L"Interface.ini";
	}

	wchar_t language[16]{};
	GetPrivateProfileStringW(Section, LanguageKey, L"es", language, static_cast<DWORD>(std::size(language)), configPath.c_str());
	spanish.store(lstrcmpiW(language, L"en") != 0, std::memory_order_relaxed);

	std::ifstream catalog(folder / L"Resources" / L"3TTAppText.csv");
	std::string line;
	translations.clear();
	std::getline(catalog, line);
	while (std::getline(catalog, line))
	{
		auto fields = ParseRow(line);
		if (fields.size() >= 2)
			translations.insert_or_assign(std::move(fields[0]), Translation{ std::move(fields[1]),
				fields.size() >= 3 ? std::move(fields[2]) : std::string{} });
	}
}

bool Localizer::IsSpanish()
{
	return spanish.load(std::memory_order_relaxed);
}

bool Localizer::SetSpanish(bool selected)
{
	spanish.store(selected, std::memory_order_relaxed);
	return WritePrivateProfileStringW(Section, LanguageKey, selected ? L"es" : L"en", configPath.c_str()) != 0;
}

const char* Localizer::UiText(const char* key)
{
	const auto entry = translations.find(key);
	if (entry == translations.end())
		return key;
	const auto& text = entry->second;
	return IsSpanish() && !text.spanish.empty() ? text.spanish.c_str() : text.english.c_str();
}

std::string Localizer::UiLabel(const char* key)
{
	return std::string(UiText(key)) + "###" + key;
}

std::wstring Localizer::UiWideText(const char* key)
{
	const auto* text = UiText(key);
	const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
	if (size <= 0)
		return {};
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
	result.pop_back();
	return result;
}
