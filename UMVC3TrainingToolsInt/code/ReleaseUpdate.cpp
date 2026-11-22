#include "ReleaseUpdate.h"

#include <Windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <array>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowsapp.lib")

namespace
{
	constexpr wchar_t ReleaseApi[] = L"/repos/AzeDev955/UMVC3TrainingToolsInt/releases/latest";
	constexpr wchar_t DownloadRoot[] = L"https://github.com/AzeDev955/UMVC3TrainingToolsInt/releases/download/";
	constexpr wchar_t InstallerName[] = L"UMVC3TrainingToolsInt-Setup.exe";
	constexpr size_t MaxResponseBytes = 1024 * 1024;
	using Version = std::array<unsigned, 3>;
	using HttpHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;

	struct Update
	{
		std::wstring tag;
		std::wstring downloadUrl;
	};

	struct RuntimeApartment
	{
		RuntimeApartment() { winrt::init_apartment(winrt::apartment_type::single_threaded); }
		~RuntimeApartment() { winrt::uninit_apartment(); }
	};

	std::optional<Version> ParseVersion(std::string_view tag)
	{
		if (!tag.starts_with('v'))
			return std::nullopt;
		tag.remove_prefix(1);
		Version version{};
		const char* cursor = tag.data();
		const char* end = cursor + tag.size();
		for (auto& part : version)
		{
			const auto [next, error] = std::from_chars(cursor, end, part);
			if (error != std::errc{})
				return std::nullopt;
			if (&part == &version.back())
				return next == end ? std::optional{ version } : std::nullopt;
			if (next == end || *next != '.')
				return std::nullopt;
			cursor = next + 1;
		}
		return std::nullopt;
	}

	std::optional<Update> FindUpdate(std::string_view json)
	{
		using namespace winrt::Windows::Data::Json;
		JsonObject release{ nullptr };
		if (!JsonObject::TryParse(winrt::to_hstring(json), release)
			|| release.GetNamedBoolean(L"draft", true) || release.GetNamedBoolean(L"prerelease", true))
			return std::nullopt;

		const auto tag = release.GetNamedString(L"tag_name", L"");
		const auto installed = ParseVersion(ReleaseUpdate::VersionTag);
		const auto latest = ParseVersion(winrt::to_string(tag));
		if (!latest || !installed || *latest <= *installed)
			return std::nullopt;

		const auto url = std::wstring(DownloadRoot) + tag.c_str() + L"/" + InstallerName;
		const auto assets = release.GetNamedArray(L"assets", nullptr);
		if (!assets)
			return std::nullopt;
		for (const auto& value : assets)
		{
			if (value.ValueType() != JsonValueType::Object)
				continue;
			const auto asset = value.GetObject();
			if (asset.GetNamedString(L"name", L"") == InstallerName
				&& asset.GetNamedString(L"state", L"") == L"uploaded"
				&& asset.GetNamedString(L"browser_download_url", L"") == std::wstring_view(url))
				return Update{ tag.c_str(), url };
		}
		return std::nullopt;
	}

	std::string FetchLatestRelease()
	{
		const HttpHandle session(WinHttpOpen(L"UMVC3TrainingToolsInt", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), WinHttpCloseHandle);
		if (!session || !WinHttpSetTimeouts(session.get(), 5000, 5000, 5000, 5000))
			return {};
		const HttpHandle connection(WinHttpConnect(session.get(), L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0),
			WinHttpCloseHandle);
		if (!connection)
			return {};
		const HttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", ReleaseApi, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE), WinHttpCloseHandle);
		DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
		if (!request || !WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy))
			|| !WinHttpSendRequest(request.get(), L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1),
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr))
			return {};

		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != HTTP_STATUS_OK)
			return {};

		std::string json;
		char buffer[8192];
		DWORD read = 0;
		while (WinHttpReadData(request.get(), buffer, sizeof(buffer), &read))
		{
			if (!read)
				return json;
			if (json.size() + read > MaxResponseBytes)
				break;
			json.append(buffer, read);
		}
		return {};
	}

	DWORD WINAPI CheckRelease(void*)
	{
		try
		{
			const auto json = FetchLatestRelease();
			if (json.empty())
				return 0;
			const RuntimeApartment apartment;
			const auto update = FindUpdate(json);
			if (!update)
				return 0;

			const bool spanish = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH;
			const auto message = spanish
				? L"Est\u00e1 disponible UMVC3 Training Tools " + update->tag + L".\n\n\u00bfDescargar el instalador oficial? Cierra MVC3 antes de instalarlo."
				: L"UMVC3 Training Tools " + update->tag + L" is available.\n\nDownload the official installer? Close MVC3 before installing it.";
			if (MessageBoxW(nullptr, message.c_str(), L"UMVC3 Training Tools", MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) == IDYES
				&& reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", update->downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
			{
				MessageBoxW(nullptr, spanish ? L"No se pudo abrir la descarga en el navegador."
					: L"Could not open the download in your browser.", L"UMVC3 Training Tools", MB_OK | MB_ICONWARNING);
			}
		}
		catch (...)
		{
			// Update checks are optional and must not interfere with the game.
		}
		return 0;
	}
}

void ReleaseUpdate::CheckAtStartup()
{
	if (const HANDLE worker = CreateThread(nullptr, 0, CheckRelease, nullptr, 0, nullptr))
		CloseHandle(worker);
}
