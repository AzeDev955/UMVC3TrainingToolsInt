#include "ReleaseUpdate.h"
#include "Localizer.h"

#include <bcrypt.h>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <array>
#include <charconv>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "windowsapp.lib")

namespace
{
	constexpr wchar_t ReleaseApi[] = L"/repos/AzeDev955/UMVC3TrainingToolsInt/releases/latest";
	constexpr wchar_t DownloadRoot[] = L"https://github.com/AzeDev955/UMVC3TrainingToolsInt/releases/download/";
	constexpr wchar_t DownloadPath[] = L"/AzeDev955/UMVC3TrainingToolsInt/releases/download/";
	constexpr wchar_t InstallerName[] = L"UMVC3TrainingToolsInt-Setup.exe";
	constexpr size_t MaxResponseBytes = 1024 * 1024;
	constexpr size_t MaxInstallerBytes = 64 * 1024 * 1024;
	using Version = std::array<unsigned, 4>;
	using Sha256 = std::array<unsigned char, 32>;
	using HttpHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;

	struct Update
	{
		std::wstring tag;
		Sha256 hash;
		size_t size;
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
		for (size_t index = 0; index < version.size(); ++index)
		{
			const auto [next, error] = std::from_chars(cursor, end, version[index]);
			if (error != std::errc{})
				return std::nullopt;
			if (next == end)
				return index >= 2 ? std::optional{ version } : std::nullopt;
			if (*next != '.')
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
			if (asset.GetNamedString(L"name", L"") != InstallerName
				|| asset.GetNamedString(L"state", L"") != L"uploaded"
				|| asset.GetNamedString(L"browser_download_url", L"") != std::wstring_view(url))
				continue;

			const auto digest = winrt::to_string(asset.GetNamedString(L"digest", L""));
			const double size = asset.GetNamedNumber(L"size", 0);
			Sha256 hash{};
			constexpr std::string_view prefix = "sha256:";
			if (!digest.starts_with(prefix) || digest.size() != prefix.size() + 2 * hash.size()
				|| !(size >= 1 && size <= MaxInstallerBytes) || size != static_cast<size_t>(size))
				return std::nullopt;
			for (size_t index = 0; index < hash.size(); ++index)
			{
				const char* byte = digest.data() + prefix.size() + 2 * index;
				const auto [end, error] = std::from_chars(byte, byte + 2, hash[index], 16);
				if (error != std::errc{} || end != byte + 2)
					return std::nullopt;
			}
			return Update{ tag.c_str(), hash, static_cast<size_t>(size) };
		}
		return std::nullopt;
	}

	std::string Fetch(const wchar_t* host, const wchar_t* path, const wchar_t* accept, size_t limit, DWORD redirectPolicy)
	{
		const HttpHandle session(WinHttpOpen(L"UMVC3TrainingToolsInt", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), WinHttpCloseHandle);
		if (!session || !WinHttpSetTimeouts(session.get(), 5000, 5000, 5000, 5000))
			return {};
		const HttpHandle connection(WinHttpConnect(session.get(), host, INTERNET_DEFAULT_HTTPS_PORT, 0),
			WinHttpCloseHandle);
		if (!connection)
			return {};
		const HttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE), WinHttpCloseHandle);
		if (!request || !WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy))
			|| !WinHttpSendRequest(request.get(), accept, static_cast<DWORD>(-1),
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr))
			return {};

		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != HTTP_STATUS_OK)
			return {};

		std::string response;
		char buffer[8192];
		DWORD read = 0;
		while (WinHttpReadData(request.get(), buffer, sizeof(buffer), &read))
		{
			if (!read)
				return response;
			if (response.size() + read > limit)
				break;
			response.append(buffer, read);
		}
		return {};
	}

	bool VerifyInstaller(std::string& content, const Update& update)
	{
		Sha256 hash{};
		return content.size() == update.size
			&& BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(content.data()),
				static_cast<ULONG>(content.size()), hash.data(), static_cast<ULONG>(hash.size())) >= 0
			&& hash == update.hash;
	}

	bool LaunchVerifiedInstaller(std::string& content, const Update& update, HWND toolWindow)
	{
		if (!VerifyInstaller(content, update))
			return false;

		FILETIME creation, exit, kernel, user;
		if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
			return false;
		const auto started = (static_cast<ULONGLONG>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
		wchar_t temp[MAX_PATH]{};
		const DWORD length = GetTempPathW(MAX_PATH, temp);
		if (!length || length >= MAX_PATH)
			return false;
		const auto folder = std::filesystem::path(temp) / L"UMVC3TrainingToolsInt-Updates";
		std::filesystem::create_directories(folder);
		// Reuse the same cache entry across retries for this version.
		const auto path = folder / (L"UMVC3TrainingToolsInt-Setup-" + update.tag + L".exe");
		const auto staged = folder / (L"installer-" + std::to_wstring(GetCurrentProcessId())
			+ L"-" + std::to_wstring(GetTickCount64()) + L".tmp");
		const HANDLE destination = CreateFileW(staged.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (destination == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const bool wrote = WriteFile(destination, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
		CloseHandle(destination);
		if (!wrote || written != content.size() || !MoveFileExW(staged.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
		{
			DeleteFileW(staged.c_str());
			return false;
		}
		auto commandLine = L"\"" + path.wstring() + L"\" --apply-update " + std::to_wstring(GetCurrentProcessId())
			+ L" " + std::to_wstring(started) + L" " + std::to_wstring(reinterpret_cast<uintptr_t>(toolWindow));
		// Hold a read handle through process creation to prevent changes to the verified executable.
		const HANDLE opened = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (opened == INVALID_HANDLE_VALUE)
			return false;
		const std::unique_ptr<void, decltype(&CloseHandle)> file(opened, CloseHandle);
		LARGE_INTEGER size{};
		DWORD read = 0;
		if (!GetFileSizeEx(file.get(), &size) || size.QuadPart != static_cast<LONGLONG>(content.size())
			|| !ReadFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &read, nullptr)
			|| read != content.size() || !VerifyInstaller(content, update))
			return false;

		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!CreateProcessW(path.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup, &process))
			return false;
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return true;
	}

	bool StartInstaller(const Update& update, HWND toolWindow)
	{
		const auto route = std::wstring(DownloadPath) + update.tag + L"/" + InstallerName;
		auto content = Fetch(L"github.com", route.c_str(), L"Accept: application/octet-stream\r\n", update.size,
			WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP);
		return LaunchVerifiedInstaller(content, update, toolWindow);
	}

	DWORD WINAPI CheckRelease(void* toolWindow)
	{
		try
		{
			const auto json = Fetch(L"api.github.com", ReleaseApi, L"Accept: application/vnd.github+json\r\n",
				MaxResponseBytes, WINHTTP_OPTION_REDIRECT_POLICY_NEVER);
			if (json.empty())
				return 0;
			const RuntimeApartment apartment;
			const auto update = FindUpdate(json);
			if (!update)
				return 0;

			const bool spanish = Localizer::IsSpanish();
			const auto message = spanish
				? L"Est\u00e1 disponible UMVC3 Training Tools " + update->tag + L".\n\n\u00bfActualizar ahora? MVC3 se cerrar\u00e1 y volver\u00e1 a abrirse tras la instalaci\u00f3n."
				: L"UMVC3 Training Tools " + update->tag + L" is available.\n\nUpdate now? MVC3 will close and reopen after installation.";
			if (MessageBoxW(nullptr, message.c_str(), L"UMVC3 Training Tools", MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) == IDYES)
			{
				bool started = false;
				try { started = StartInstaller(*update, static_cast<HWND>(toolWindow)); }
				catch (const std::exception&) { }
				if (!started)
					MessageBoxW(nullptr, spanish ? L"No se pudo descargar, verificar o iniciar la actualizaci\u00f3n."
						: L"The update could not be downloaded, verified or started.", L"UMVC3 Training Tools", MB_OK | MB_ICONWARNING);
			}
		}
		catch (...)
		{
			// Update checks are optional and must not interfere with the game.
		}
		return 0;
	}
}

void ReleaseUpdate::CheckAtStartup(HWND toolWindow)
{
	if (const HANDLE worker = CreateThread(nullptr, 0, CheckRelease, toolWindow, 0, nullptr))
		CloseHandle(worker);
}
