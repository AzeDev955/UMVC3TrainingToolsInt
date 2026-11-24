#include "../pch.h"
#include "TrainingShortcut.h"

#include "TrainingTeamEditor.h"
#include "Localizer.h"
#include "umvc3utils.h"
#include "../minhook/include/MinHook.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <d3d9.h>
#include <mutex>
#include <vector>

namespace
{
	constexpr uintptr_t ManagerAddress = 0x140d510a0;
	constexpr uintptr_t MainMenuTable = 0x140b73510;
	constexpr uintptr_t OfflineMenuTable = 0x140b732f0;
	constexpr uintptr_t MenuUpdateSlot = 0x40;
	constexpr uintptr_t MenuSetStateSlot = 0x110;
	constexpr uintptr_t MainMenuSetState = 0x1403e8420;
	constexpr uintptr_t OfflineMenuSetState = 0x1403e6530;
	constexpr uintptr_t CharacterSelectTable = 0x140b3e6a0;
	constexpr uintptr_t CharacterSelectUpdate = 0x140368300;
	constexpr uintptr_t CharacterSelectPhases = 0x2b4;
	constexpr uintptr_t CharacterFinishSelection = 0x140368b00;
	constexpr uintptr_t UiCharacterSelectPhases = 0x58d8;
	constexpr uintptr_t StageOption = 0xf8;
	constexpr uintptr_t StageOptionTable = 0x140b40160;
	constexpr uintptr_t StageConfirmAddress = 0x140376560;
	constexpr uintptr_t StageOptionState = 0x09;
	constexpr uintptr_t StageOptionConfirmDelay = 0x1f0;
	constexpr uintptr_t StageOptionCursor = 0x1f4;
	constexpr uintptr_t UiSelectedStage = 0x5934;
	constexpr uintptr_t BattleSettingsAddress = 0x140d50e58;
	constexpr uintptr_t BattleModeOffset = 0x34c;
	constexpr uintptr_t BattleStageOffset = 0x358;
	constexpr uintptr_t StageSelectionIds = 0x140b400c0;
	constexpr uintptr_t ResolveStageAddress = 0x1402bc5d0;
	constexpr uintptr_t SetBattleModeAddress = 0x14024e050;
	constexpr uintptr_t SetBattleStageAddress = 0x1401cf670;
	constexpr uintptr_t SetBattleTimeAddress = 0x140146530;
	constexpr uintptr_t SetUiStageAddress = 0x14024e140;
	constexpr uintptr_t SceneManagerAddress = 0x140e17a80;
	constexpr uintptr_t SceneManagerTable = 0x140b090c0;
	constexpr uintptr_t QueueSceneAddress = 0x140545350;
	constexpr uintptr_t QueueSceneSlot = 0x50;
	constexpr uintptr_t QueuedSceneOffset = 0x7040;
	struct StageScene
	{
		int archive;
		uintptr_t type;
	};
	// Native stage scene descriptors. Each is a child of aAction, which in turn is a child of aGame.
	constexpr StageScene StageScenes[]{
		{ 0, 0x140d50790 }, { 2, 0x140d507c8 }, { 3, 0x140d50810 }, { 4, 0x140d50848 },
		{ 5, 0x140d50888 }, { 6, 0x140d508d0 }, { 7, 0x140d50910 }, { 9, 0x140d50950 },
		{ 10, 0x140d50990 }, { 11, 0x140d509d8 }, { 100, 0x140d50a10 }, { 102, 0x140d50a58 },
		{ 103, 0x140d50a98 }, { 104, 0x140d50ae0 }, { 105, 0x140d50b28 }, { 106, 0x140d50b60 },
		{ 107, 0x140d50ba8 }, { 109, 0x140d50be0 }, { 110, 0x140d50c28 }
	};
	constexpr unsigned PresentSlot = 17;
	constexpr unsigned EndSceneSlot = 42;
	constexpr unsigned SwapChainPresentSlot = 3;
	constexpr int MainMenuPhase = 1;
	constexpr int OfflineMenuPhase = 2;
	constexpr int MenuReady = 7;
	constexpr int MenuConfirmSelection = 8;
	constexpr int OfflineEntry = 0;
	constexpr int TrainingEntry = 2;
	constexpr int OfflineCommand = 1;
	constexpr int TrainingCommand = 8;
	constexpr int TrainingMode = 5;
	constexpr int UnlimitedBattleTime = -1;
	constexpr int AwaitPlayer = 0;
	constexpr int SelectFighter = 2;
	constexpr int LoadSelectedFighters = 11;
	constexpr uint8_t StageOptionsReady = 4;
	constexpr uint8_t StageConfirmed = 12;
	constexpr int TextureWidth = 256;
	constexpr int TextureHeight = 64;
	constexpr ULONGLONG TransitionTimeoutMs = 30000;
	constexpr ULONGLONG LoadingFrameTimeoutMs = 150;

	using MenuUpdate = void (*)(void*);
	using MenuSetState = void (*)(void*, int);
	using SelectUpdate = bool (*)(void*);
	using FinishSelection = void (*)(void*, int);
	using StageConfirm = void (*)(void*);
	using EndScene = HRESULT (WINAPI*)(IDirect3DDevice9*);
	using Present = HRESULT (WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
	using SwapChainPresent = HRESULT (WINAPI*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
	using SetGameValue = void (*)(uintptr_t, int);
	using ResolveStage = int (*)(uintptr_t, int);
	using QueueScene = void (*)(uintptr_t, uintptr_t);

	MenuUpdate originalMainUpdate = nullptr;
	MenuUpdate originalOfflineUpdate = nullptr;
	SelectUpdate originalSelectUpdate = nullptr;
	EndScene originalEndScene = nullptr;
	Present originalPresent = nullptr;
	SwapChainPresent originalSwapChainPresent = nullptr;
	HWND toolWindow = nullptr;
	IDirect3DDevice9* textureDevice = nullptr;
	bool textureSpanish = false;
	bool textureLoading = false;
	IDirect3DTexture9* overlayTexture = nullptr;
	UINT overlayTextureWidth = TextureWidth;
	IDirect3DSurface9* loadingSurface = nullptr;
	IDirect3DDevice9* loadingDevice = nullptr;
	D3DSURFACE_DESC loadingSurfaceDesc{};
	bool loadingSurfaceSpanish = false;
	thread_local bool presentingFromDevice = false;
	bool mouseWasDown = false;
	bool keyWasDown = false;
	bool firstGameEndScene = true;
	bool launchQueued = false; // Tool thread only.
	bool directLaunchSupported = false;
	bool nativeMenuLaunchSupported = false;
	bool firstSelectUpdate = true; // Game thread only.

	enum class Request
	{
		None,
		OpenOffline,
		OpenTraining,
		StageTeams,
		ConfirmStage,
		OpenDirect,
		WaitForBattle
	};

	std::atomic<Request> request{ Request::None };
	std::atomic<ULONGLONG> requestDeadline{ 0 };
	std::atomic<ULONGLONG> loadingFrameDeadline{ 0 };
	std::atomic<bool> loadingFramePresented{ false };
	std::atomic<bool> setupRequested{ false };
	std::atomic<bool> shortcutAvailable{ false };
	std::atomic<HWND> gameWindow{ nullptr };
	std::mutex picksMutex;
	TrainingShortcut::TeamPicks queuedPicks{};
	int queuedStage = 0;
	bool queuedDirect = false; // Tool thread only.
	std::atomic<int> directBattleStage{ -1 };
	std::array<int, 2> lastSelectPhases{ -1, -1 };
	int lastStageState = -1;
	std::array<int, 3> lastLaunchProgress{ -1, -1, -1 };

	struct Menu
	{
		uintptr_t address{};
		uintptr_t table{};
		int phase{};
		int state{};
	};

	bool IsDirectLoading(Request step)
	{
		return step == Request::OpenDirect || step == Request::WaitForBattle;
	}

	uintptr_t GameAddress(uintptr_t address)
	{
		return static_cast<uintptr_t>(_addr(address));
	}

	template <typename T>
	bool Read(uintptr_t address, T& value)
	{
		SIZE_T bytes = 0;
		return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
			&value, sizeof(value), &bytes) && bytes == sizeof(value);
	}

	uintptr_t PointerAt(uintptr_t address)
	{
		uintptr_t pointer = 0;
		Read(address, pointer);
		return pointer;
	}

	template <size_t Size>
	bool MatchesGameCode(uintptr_t address, const std::array<uint8_t, Size>& signature)
	{
		std::array<uint8_t, Size> bytes{};
		return Read(GameAddress(address), bytes) && bytes == signature;
	}

	uintptr_t GameUi()
	{
		const auto manager = PointerAt(GameAddress(ManagerAddress));
		return manager ? PointerAt(manager + 0x80) : 0;
	}

	Menu ReadMenu()
	{
		const auto gameUi = GameUi();
		Menu menu;
		int stackSize = 0;
		if (!gameUi || !Read(gameUi + 0x5790, menu.phase) || !Read(gameUi + 0x5b18, stackSize)
			|| stackSize < 1 || stackSize > 8)
			return {};

		const auto stack = PointerAt(gameUi + 0x5b28);
		menu.address = stack ? PointerAt(stack + (stackSize - 1) * sizeof(uintptr_t)) : 0;
		menu.table = PointerAt(menu.address);
		if (!menu.address || !Read(menu.address + 0x7c, menu.state))
			return {};
		return menu;
	}

	bool IsReadyMenu(const Menu& menu, int phase, uintptr_t table)
	{
		return menu.phase == phase && menu.table == GameAddress(table) && menu.state == MenuReady;
	}

	void TraceHook(const char* event, MH_STATUS status)
	{
		char message[128];
		std::snprintf(message, sizeof(message), "%s: %s", event, MH_StatusToString(status));
		TrainingTeamEditor::TraceQuickShortcut(message);
	}

	bool Pending(Request step)
	{
		if (request.load() != step)
			return false;
		if (GetTickCount64() <= requestDeadline.load())
			return true;
		if (request.compare_exchange_strong(step, Request::None))
		{
			const char* waitingFor = "character selection";
			if (step == Request::OpenOffline)
				waitingFor = "main menu";
			else if (step == Request::OpenTraining)
				waitingFor = "Offline menu";
			else if (step == Request::ConfirmStage)
				waitingFor = "stage selection";
			else if (step == Request::OpenDirect)
				waitingFor = "main menu for direct launch";
			else if (step == Request::WaitForBattle)
				waitingFor = "direct battle scene";
			char message[96];
			std::snprintf(message, sizeof(message), "Start Training timed out waiting for %s", waitingFor);
			TrainingTeamEditor::TraceQuickShortcut(message);
		}
		return false;
	}

	bool RequestSetup(const char* trigger)
	{
		if (!shortcutAvailable.load())
		{
			TrainingTeamEditor::TraceQuickShortcut("setup request ignored: native hooks unavailable");
			return false;
		}
		const auto queued = request.load();
		if (queued != Request::None && Pending(queued))
		{
			TrainingTeamEditor::TraceQuickShortcut("setup request ignored: Training launch already pending");
			return false;
		}
		const auto menu = ReadMenu();
		if (!IsReadyMenu(menu, MainMenuPhase, MainMenuTable))
		{
			char message[128];
			std::snprintf(message, sizeof(message), "setup request ignored: main menu not ready (phase=%d state=%d)",
				menu.phase, menu.state);
			TrainingTeamEditor::TraceQuickShortcut(message);
			return false;
		}
		TrainingTeamEditor::TraceQuickShortcut(trigger);
		return true;
	}

	bool ConfirmEntry(const Menu& menu, int index, int command)
	{
		int count = 0;
		int actualCommand = 0;
		const auto entries = PointerAt(menu.address + 0x98);
		if (!entries || !Read(menu.address + 0xa0, count) || index >= count
			|| !Read(entries + index * 0x18, actualCommand) || actualCommand != command)
			return false;

		const auto setState = PointerAt(menu.table + MenuSetStateSlot);
		const auto expected = menu.phase == OfflineMenuPhase ? OfflineMenuSetState : MainMenuSetState;
		if (setState != GameAddress(expected))
			return false;

		// State 8 runs the menu's preparation callback before dispatching the
		// entry. Offline also uses this state to schedule its native fade.
		*reinterpret_cast<int*>(menu.address + 0x78) = index;
		reinterpret_cast<MenuSetState>(setState)(reinterpret_cast<void*>(menu.address), MenuConfirmSelection);
		return true;
	}

	bool QueueDirectTraining()
	{
		const auto sceneManager = PointerAt(GameAddress(SceneManagerAddress));
		const auto gameUi = GameUi();
		const auto settings = PointerAt(GameAddress(BattleSettingsAddress));
		uintptr_t pendingScene = 0;
		int stage = 0;
		TrainingShortcut::TeamPicks picks{};
		{
			const std::lock_guard lock(picksMutex);
			picks = queuedPicks;
			stage = queuedStage;
		}
		int nativeStage = -1;
		// The game's stage table starts with Random. The preset stores the index after it.
		if (!gameUi || !settings || PointerAt(sceneManager) != GameAddress(SceneManagerTable)
			|| !Read(sceneManager + QueuedSceneOffset, pendingScene) || pendingScene
			|| !Read(GameAddress(StageSelectionIds) + (stage + 1) * sizeof(int), nativeStage)
			|| nativeStage != stage)
			return false;

		const int archiveStage = reinterpret_cast<ResolveStage>(GameAddress(ResolveStageAddress))(gameUi, nativeStage);
		const auto scene = std::find_if(std::begin(StageScenes), std::end(StageScenes), [archiveStage](const StageScene& entry)
		{
			return entry.archive == archiveStage;
		});
		if (scene == std::end(StageScenes) || !TrainingTeamEditor::PrepareQuickMatch(picks))
			return false;

		// Apply Training's rules before the stage constructs the battle.
		reinterpret_cast<SetGameValue>(GameAddress(SetBattleModeAddress))(settings, TrainingMode);
		reinterpret_cast<SetGameValue>(GameAddress(SetBattleTimeAddress))(settings, UnlimitedBattleTime);
		reinterpret_cast<SetGameValue>(GameAddress(SetBattleStageAddress))(settings, archiveStage);
		reinterpret_cast<SetGameValue>(GameAddress(SetUiStageAddress))(gameUi, archiveStage);
		directBattleStage.store(archiveStage);
		request.store(Request::WaitForBattle);
		reinterpret_cast<QueueScene>(GameAddress(QueueSceneAddress))(sceneManager, GameAddress(scene->type));
		char message[128];
		std::snprintf(message, sizeof(message), "direct Training scene queued (selector ID=%d archive ID=%03d)",
			stage, archiveStage);
		TrainingTeamEditor::TraceQuickShortcut(message);
		return true;
	}

	void MainUpdate(void* object)
	{
		originalMainUpdate(object);
		const auto step = request.load();
		if ((step != Request::OpenOffline && step != Request::OpenDirect) || !Pending(step))
			return;
		const auto menu = ReadMenu();
		if (menu.address == reinterpret_cast<uintptr_t>(object)
			&& IsReadyMenu(menu, MainMenuPhase, MainMenuTable))
		{
			if (step == Request::OpenDirect)
			{
				if (!loadingFramePresented.load() && GetTickCount64() < loadingFrameDeadline.load())
					return;
				if (QueueDirectTraining())
					return;
				if (!nativeMenuLaunchSupported)
				{
					TrainingTeamEditor::TraceQuickShortcut("direct launch preflight failed; native menus are unavailable");
					request.store(Request::None);
					return;
				}
				TrainingTeamEditor::TraceQuickShortcut("direct launch preflight failed; using native menus");
			}
			TrainingTeamEditor::TraceQuickShortcut("confirming Offline in the main menu");
			const bool confirmed = ConfirmEntry(menu, OfflineEntry, OfflineCommand);
			TrainingTeamEditor::TraceQuickShortcut(confirmed ? "Offline native transition requested" : "Offline confirmation rejected");
			request.store(confirmed ? Request::OpenTraining : Request::None);
		}
	}

	void OfflineUpdate(void* object)
	{
		originalOfflineUpdate(object);
		if (!Pending(Request::OpenTraining))
			return;
		const auto menu = ReadMenu();
		if (menu.address == reinterpret_cast<uintptr_t>(object)
			&& IsReadyMenu(menu, OfflineMenuPhase, OfflineMenuTable))
		{
			lastSelectPhases = { -1, -1 };
			lastStageState = -1;
			firstSelectUpdate = true;
			TrainingTeamEditor::TraceQuickShortcut("confirming Training in the Offline menu");
			const bool confirmed = ConfirmEntry(menu, TrainingEntry, TrainingCommand);
			TrainingTeamEditor::TraceQuickShortcut(confirmed ? "Training native transition requested" : "Training confirmation rejected");
			request.store(confirmed ? Request::StageTeams : Request::None);
		}
	}

	void ConfirmQuickStage(uintptr_t scene, uintptr_t gameUi)
	{
		const auto option = PointerAt(scene + StageOption);
		uint8_t state = 0;
		if (!gameUi || PointerAt(option) != GameAddress(StageOptionTable)
			|| !Read(option + StageOptionState, state))
			return;

		if (state != lastStageState)
		{
			lastStageState = state;
			char message[96];
			std::snprintf(message, sizeof(message), "native stage selector state=%u", static_cast<unsigned>(state));
			TrainingTeamEditor::TraceQuickShortcut(message);
		}
		if (state == StageConfirmed)
		{
			TrainingTeamEditor::TraceQuickShortcut("native stage selector was already confirmed");
			request.store(Request::None);
			return;
		}
		int confirmDelay = 0;
		if (state != StageOptionsReady || !Read(option + StageOptionConfirmDelay, confirmDelay) || confirmDelay >= 0)
			return;

		int stage = 0;
		{
			const std::lock_guard lock(picksMutex);
			stage = queuedStage;
		}
		// The native cursor starts with Random, followed by the selectable stages.
		auto* cursor = reinterpret_cast<int*>(option + StageOptionCursor);
		const int previousCursor = *cursor;
		*cursor = stage + 1;
		char message[128];
		std::snprintf(message, sizeof(message), "confirming stage (selector ID=%d)", stage);
		TrainingTeamEditor::TraceQuickShortcut(message);
		const auto confirm = reinterpret_cast<StageConfirm>(GameAddress(StageConfirmAddress));
		confirm(reinterpret_cast<void*>(option));
		int resolved = -1;
		if (Read(option + StageOptionState, state) && state == StageConfirmed)
		{
			Read(gameUi + UiSelectedStage, resolved);
			std::snprintf(message, sizeof(message), "stage confirmed (selector ID=%d archive ID=%03d)", stage, resolved);
			TrainingTeamEditor::TraceQuickShortcut(message);
		}
		else
		{
			*cursor = previousCursor;
			TrainingTeamEditor::TraceQuickShortcut("stage confirmation rejected");
		}
		request.store(Request::None);
	}

	bool CharacterSelectUpdateHook(void* object)
	{
		const auto scene = reinterpret_cast<uintptr_t>(object);
		const auto step = request.load();
		if ((step != Request::StageTeams && step != Request::ConfirmStage)
			|| !Pending(step))
			return originalSelectUpdate(object);

		const bool expectedSelector = PointerAt(scene) == GameAddress(CharacterSelectTable);
		if (firstSelectUpdate)
		{
			firstSelectUpdate = false;
			TrainingTeamEditor::TraceQuickShortcut(expectedSelector
				? "native character selector started" : "native character selector update has an unexpected object");
		}
		if (!expectedSelector)
			return originalSelectUpdate(object);

		const auto gameUi = GameUi();
		const auto settings = PointerAt(GameAddress(BattleSettingsAddress));
		int mode = -1;
		Read(settings ? settings + BattleModeOffset : 0, mode);
		std::array<int, 2> phases{};
		const bool havePhases = Read(scene + CharacterSelectPhases, phases);
		if (havePhases && phases != lastSelectPhases)
		{
			TrainingTeamEditor::TraceQuickSelection(phases[0], phases[1], mode);
			lastSelectPhases = phases;
		}
		if (step == Request::ConfirmStage)
		{
			if (mode == TrainingMode)
				ConfirmQuickStage(scene, gameUi);
			return originalSelectUpdate(object);
		}

		// With one controller the dummy remains inactive while P1 selects.
		const bool ready = havePhases && phases[0] == SelectFighter
			&& (phases[1] == AwaitPlayer || phases[1] == SelectFighter);
		std::array<int, 2> uiPhases{};
		if (gameUi && mode == TrainingMode && ready
			&& Read(gameUi + UiCharacterSelectPhases, uiPhases) && uiPhases == phases)
		{
			TrainingShortcut::TeamPicks picks{};
			{
				const std::lock_guard lock(picksMutex);
				picks = queuedPicks;
			}
			if (TrainingTeamEditor::PrepareQuickMatch(picks))
			{
				const auto finish = reinterpret_cast<FinishSelection>(GameAddress(CharacterFinishSelection));
				finish(object, 0);
				finish(object, 1);
				constexpr std::array loading{ LoadSelectedFighters, LoadSelectedFighters };
				*reinterpret_cast<std::array<int, 2>*>(gameUi + UiCharacterSelectPhases) = loading;
				request.store(Request::ConfirmStage);
				TrainingTeamEditor::TraceQuickShortcut("both teams confirmed; waiting for native stage selection");
			}
			else
			{
				request.store(Request::None);
			}
		}
		return originalSelectUpdate(object);
	}

	bool ReplaceMenuUpdate(uintptr_t table, MenuUpdate expected, MenuUpdate replacement)
	{
		auto* slot = reinterpret_cast<void* volatile*>(GameAddress(table) + MenuUpdateSlot);
		auto* writable = const_cast<void**>(slot);
		DWORD protection = 0;
		if (!VirtualProtect(writable, sizeof(void*), PAGE_READWRITE, &protection))
			return false;
		const auto old = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(replacement),
			reinterpret_cast<void*>(expected));
		DWORD ignored = 0;
		VirtualProtect(writable, sizeof(void*), protection, &ignored);
		return old == reinterpret_cast<void*>(expected);
	}

	void ReleaseTexture()
	{
		if (overlayTexture)
			overlayTexture->Release();
		overlayTexture = nullptr;
		textureDevice = nullptr;
	}

	std::vector<uint32_t> CreateOverlayPixels(UINT& width, UINT height, const wchar_t* label, bool fitLabel)
	{
		HDC dc = CreateCompatibleDC(nullptr);
		if (!dc)
			return {};

		HFONT font = CreateFontW(-MulDiv(26, static_cast<int>(height), TextureHeight), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
		constexpr UINT textFlags = DT_SINGLELINE | DT_NOPREFIX;
		if (fitLabel)
		{
			RECT textBounds{};
			DrawTextW(dc, label, -1, &textBounds, textFlags | DT_CALCRECT);
			const int horizontalPadding = MulDiv(20, static_cast<int>(height), TextureHeight);
			width = (std::max)(width, static_cast<UINT>(textBounds.right - textBounds.left + 2 * horizontalPadding));
		}

		BITMAPINFO info{};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = static_cast<LONG>(width);
		info.bmiHeader.biHeight = -static_cast<LONG>(height);
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		void* bitmapPixels = nullptr;
		HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bitmapPixels, nullptr, 0);
		if (!bitmap)
		{
			DeleteDC(dc);
			if (font)
				DeleteObject(font);
			return {};
		}

		HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
		RECT bounds{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		SetDCBrushColor(dc, RGB(28, 43, 64));
		FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		SetDCBrushColor(dc, RGB(110, 184, 236));
		FrameRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		SetTextColor(dc, RGB(240, 247, 255));
		SetBkMode(dc, TRANSPARENT);
		DrawTextW(dc, label, -1, &bounds, textFlags | DT_CENTER | DT_VCENTER);
		GdiFlush();
		const auto* source = static_cast<const uint32_t*>(bitmapPixels);
		std::vector<uint32_t> pixels(source, source + static_cast<size_t>(width) * height);
		for (auto& pixel : pixels)
			pixel |= 0xff000000u;

		if (oldFont)
			SelectObject(dc, oldFont);
		if (font)
			DeleteObject(font);
		SelectObject(dc, oldBitmap);
		DeleteObject(bitmap);
		DeleteDC(dc);
		return pixels;
	}

	void CopyOverlayPixels(const std::vector<uint32_t>& pixels, UINT width, UINT height, const D3DLOCKED_RECT& mapped)
	{
		for (UINT row = 0; row < height; ++row)
		{
			auto* destination = static_cast<char*>(mapped.pBits) + row * mapped.Pitch;
			std::memcpy(destination, pixels.data() + row * width, width * sizeof(uint32_t));
		}
	}

	bool CreateOverlayTexture(IDirect3DDevice9* device, bool spanish, bool loading)
	{
		const auto label = loading ? Localizer::UiWideText("QUICK_LOADING")
			: std::wstring(spanish ? L"Entrenar  [F4]" : L"Training  [F4]");
		UINT width = TextureWidth;
		const auto pixels = CreateOverlayPixels(width, TextureHeight, label.c_str(), loading);
		if (pixels.empty())
			return false;

		IDirect3DTexture9* texture = nullptr;
		if (SUCCEEDED(device->CreateTexture(width, TextureHeight, 1, 0, D3DFMT_A8R8G8B8,
			D3DPOOL_MANAGED, &texture, nullptr)))
		{
			D3DLOCKED_RECT mapped{};
			if (SUCCEEDED(texture->LockRect(0, &mapped, nullptr, 0)))
			{
				CopyOverlayPixels(pixels, width, TextureHeight, mapped);
				texture->UnlockRect(0);
				overlayTexture = texture;
				overlayTextureWidth = width;
				textureDevice = device;
				textureSpanish = spanish;
				textureLoading = loading;
			}
			else
				texture->Release();
		}

		return overlayTexture != nullptr;
	}

	void ReleaseLoadingSurface()
	{
		if (loadingSurface)
			loadingSurface->Release();
		loadingSurface = nullptr;
		loadingDevice = nullptr;
	}

	bool DrawLoadingBackBuffer(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer, const RECT* source)
	{
		D3DSURFACE_DESC target{};
		if (FAILED(backBuffer->GetDesc(&target)) || target.MultiSampleType != D3DMULTISAMPLE_NONE
			|| (target.Format != D3DFMT_X8R8G8B8 && target.Format != D3DFMT_A8R8G8B8))
			return false;

		const RECT bounds{ 0, 0, static_cast<LONG>(target.Width), static_cast<LONG>(target.Height) };
		RECT visible = bounds;
		if (source && !IntersectRect(&visible, &bounds, source))
			return false;
		const LONG availableWidth = visible.right - visible.left;
		const LONG availableHeight = visible.bottom - visible.top;
		const float scale = (std::max)(0.60f, availableHeight / 720.0f);
		const UINT height = static_cast<UINT>(56.0f * scale);
		if (height > static_cast<UINT>(availableHeight))
			return false;

		const bool spanish = Localizer::IsSpanish();
		if (loadingDevice != device || loadingSurfaceDesc.Height != height
			|| loadingSurfaceDesc.Format != target.Format || loadingSurfaceSpanish != spanish)
		{
			ReleaseLoadingSurface();
			const auto label = Localizer::UiWideText("QUICK_LOADING");
			UINT width = MulDiv(TextureWidth, height, TextureHeight);
			const auto pixels = CreateOverlayPixels(width, height, label.c_str(), true);
			if (pixels.empty() || FAILED(device->CreateOffscreenPlainSurface(width, height, target.Format,
				D3DPOOL_SYSTEMMEM, &loadingSurface, nullptr)))
				return false;
			D3DLOCKED_RECT mapped{};
			if (FAILED(loadingSurface->LockRect(&mapped, nullptr, 0)))
			{
				ReleaseLoadingSurface();
				return false;
			}
			CopyOverlayPixels(pixels, width, height, mapped);
			loadingSurface->UnlockRect();
			loadingDevice = device;
			loadingSurfaceDesc.Width = width;
			loadingSurfaceDesc.Height = height;
			loadingSurfaceDesc.Format = target.Format;
			loadingSurfaceSpanish = spanish;
		}

		const LONG width = static_cast<LONG>(loadingSurfaceDesc.Width);
		if (width > availableWidth)
			return false;
		const POINT center{ visible.left + (availableWidth - width) / 2,
			visible.top + (availableHeight - static_cast<LONG>(height)) / 2 };
		return SUCCEEDED(device->UpdateSurface(loadingSurface, nullptr, backBuffer, &center));
	}

	bool IsGameDevice(IDirect3DDevice9* device)
	{
		D3DDEVICE_CREATION_PARAMETERS parameters{};
		return SUCCEEDED(device->GetCreationParameters(&parameters))
			&& parameters.hFocusWindow && parameters.hFocusWindow != toolWindow;
	}

	void ReportLoadingFrame(HRESULT result, bool drawn)
	{
		if (result == D3D_OK && drawn && !loadingFramePresented.exchange(true))
			TrainingTeamEditor::TraceQuickShortcut("direct Training loading frame presented");
	}

	HRESULT WINAPI PresentGame(IDirect3DDevice9* device, const RECT* source, const RECT* destination,
		HWND window, const RGNDATA* dirty)
	{
		bool drawn = false;
		IDirect3DSurface9* backBuffer = nullptr;
		if (IsDirectLoading(request.load()) && IsGameDevice(device)
			&& SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
		{
			drawn = DrawLoadingBackBuffer(device, backBuffer, source);
			backBuffer->Release();
		}
		presentingFromDevice = drawn;
		const auto result = originalPresent(device, source, destination, window, dirty);
		presentingFromDevice = false;
		ReportLoadingFrame(result, drawn);
		return result;
	}

	HRESULT WINAPI PresentGameSwapChain(IDirect3DSwapChain9* swapChain, const RECT* source, const RECT* destination,
		HWND window, const RGNDATA* dirty, DWORD flags)
	{
		bool drawn = false;
		IDirect3DDevice9* device = nullptr;
		if (!presentingFromDevice && IsDirectLoading(request.load()) && SUCCEEDED(swapChain->GetDevice(&device)))
		{
			IDirect3DSurface9* backBuffer = nullptr;
			if (IsGameDevice(device) && SUCCEEDED(swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
			{
				drawn = DrawLoadingBackBuffer(device, backBuffer, source);
				backBuffer->Release();
			}
			device->Release();
		}
		const auto result = originalSwapChainPresent(swapChain, source, destination, window, dirty, flags);
		ReportLoadingFrame(result, drawn);
		return result;
	}

	struct Vertex
	{
		float x, y, z, rhw;
		D3DCOLOR color;
		float u, v;
	};

	void DrawOverlay(IDirect3DDevice9* device, HWND window, const D3DVIEWPORT9& viewport, bool loading)
	{
		const bool spanish = Localizer::IsSpanish();
		if (textureDevice != device || textureSpanish != spanish || textureLoading != loading)
		{
			ReleaseTexture();
			if (!CreateOverlayTexture(device, spanish, loading))
				return;
		}

		const float scale = (std::max)(0.60f, viewport.Height / 720.0f);
		float height = (loading ? 56.0f : 46.0f) * scale;
		float width = 188.0f * scale;
		if (loading)
		{
			const float aspectRatio = static_cast<float>(overlayTextureWidth) / TextureHeight;
			height = (std::min)({ height, static_cast<float>(viewport.Height), viewport.Width / aspectRatio });
			width = height * aspectRatio;
		}
		const float left = viewport.X + (loading ? (viewport.Width - width) * 0.5f : viewport.Width - width - 28.0f * scale);
		const float top = viewport.Y + (loading ? (viewport.Height - height) * 0.5f : viewport.Height - height - 62.0f * scale);
		const float right = left + width;
		const float bottom = top + height;
		bool hovered = false;
		if (!loading)
		{
			RECT client{};
			POINT cursor{};
			if (GetForegroundWindow() == window && GetClientRect(window, &client) && client.right > 0 && client.bottom > 0
				&& GetCursorPos(&cursor) && ScreenToClient(window, &cursor))
			{
				const float x = viewport.X + static_cast<float>(cursor.x) * viewport.Width / client.right;
				const float y = viewport.Y + static_cast<float>(cursor.y) * viewport.Height / client.bottom;
				hovered = x >= left && x < right && y >= top && y < bottom;
			}

			const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			if (hovered && mouseDown && !mouseWasDown && RequestSetup("menu button requested the setup"))
				setupRequested.store(true);
			mouseWasDown = mouseDown;
		}

		const D3DCOLOR color = loading || hovered ? 0xffffffff : 0xffdce9f5;
		const Vertex vertices[]{
			{ left, top, 0, 1, color, 0, 0 }, { right, top, 0, 1, color, 1, 0 },
			{ left, bottom, 0, 1, color, 0, 1 }, { right, bottom, 0, 1, color, 1, 1 }
		};
		IDirect3DStateBlock9* saved = nullptr;
		if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved)))
			return;
		if (SUCCEEDED(saved->Capture()))
		{
			device->SetVertexShader(nullptr);
			device->SetPixelShader(nullptr);
			device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
			device->SetRenderState(D3DRS_ZENABLE, FALSE);
			device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
				| D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
			device->SetTexture(0, overlayTexture);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
			device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
			device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
			saved->Apply();
		}
		saved->Release();
	}

	HRESULT WINAPI DrawGameMenu(IDirect3DDevice9* device)
	{
		D3DDEVICE_CREATION_PARAMETERS parameters{};
		if (SUCCEEDED(device->GetCreationParameters(&parameters)) && parameters.hFocusWindow
			&& parameters.hFocusWindow != toolWindow)
		{
			gameWindow.store(parameters.hFocusWindow);
			if (firstGameEndScene)
			{
				TrainingTeamEditor::TraceQuickShortcut("game EndScene observed");
				firstGameEndScene = false;
			}
			const bool loading = IsDirectLoading(request.load());
			if (!loading && loadingDevice == device)
				ReleaseLoadingSurface();
			if (loading ? !loadingFramePresented.load() : IsReadyMenu(ReadMenu(), MainMenuPhase, MainMenuTable))
			{
				D3DVIEWPORT9 viewport{};
				if (SUCCEEDED(device->GetViewport(&viewport)))
					DrawOverlay(device, parameters.hFocusWindow, viewport, loading);
			}
			else if (textureDevice == device)
				ReleaseTexture();
		}
		return originalEndScene(device);
	}
}

bool TrainingShortcut::TakeSetupRequest(bool controllerPressed)
{
	const auto step = request.load();
	if (step == Request::WaitForBattle && Pending(step))
	{
		const auto settings = PointerAt(GameAddress(BattleSettingsAddress));
		int stage = -1;
		if (settings && Read(settings + BattleStageOffset, stage) && stage == directBattleStage.load()
			&& TrainingTeamEditor::IsTrainingPlayable())
		{
			Request expected = Request::WaitForBattle;
			if (request.compare_exchange_strong(expected, Request::None))
				TrainingTeamEditor::TraceQuickShortcut("direct Training match playable with expected stage");
		}
	}
	if (step != Request::None && Pending(step) && step == Request::StageTeams)
	{
		const auto menu = ReadMenu();
		const auto settings = PointerAt(GameAddress(BattleSettingsAddress));
		int mode = -1;
		Read(settings ? settings + BattleModeOffset : 0, mode);
		const std::array progress{ menu.phase, menu.state, mode };
		if (progress != lastLaunchProgress)
		{
			lastLaunchProgress = progress;
			char message[104];
			std::snprintf(message, sizeof(message), "waiting for character selector (menu phase=%d state=%d mode=%d)",
				menu.phase, menu.state, mode);
			TrainingTeamEditor::TraceQuickShortcut(message);
		}
	}

	const bool fromButton = setupRequested.exchange(false);
	const bool keyDown = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
	const bool pressed = keyDown && !keyWasDown && (GetAsyncKeyState(VK_MENU) & 0x8000) == 0;
	keyWasDown = keyDown;
	if (pressed || controllerPressed)
	{
		const auto foreground = GetForegroundWindow();
		DWORD pid = 0;
		if (foreground && GetWindowThreadProcessId(foreground, &pid) && pid == GetCurrentProcessId())
		{
			TrainingTeamEditor::TraceQuickShortcut(controllerPressed ? "controller shortcut pressed in game" : "F4 pressed in game");
			if (foreground != toolWindow)
				gameWindow.store(foreground);
			if (!fromButton && RequestSetup(controllerPressed ? "controller requested the setup" : "F4 requested the setup"))
				return true;
		}
	}
	return fromButton;
}

bool TrainingShortcut::CanLaunchViaNativeMenus()
{
	return nativeMenuLaunchSupported;
}

bool TrainingShortcut::StartTraining(const TeamPicks& picks, int stage, bool direct)
{
	TrainingTeamEditor::TraceQuickShortcut("Start Training pressed");
	if (stage < 0 || stage >= StageCount || launchQueued || !shortcutAvailable.load() || request.load() != Request::None
		|| !IsReadyMenu(ReadMenu(), MainMenuPhase, MainMenuTable))
	{
		TrainingTeamEditor::TraceQuickShortcut("Start Training rejected: main menu or selection unavailable");
		return false;
	}
	{
		const std::lock_guard lock(picksMutex);
		queuedPicks = picks;
		queuedStage = stage;
	}
	queuedDirect = directLaunchSupported && (direct || !nativeMenuLaunchSupported);
	if (direct && !queuedDirect)
		TrainingTeamEditor::TraceQuickShortcut("direct launch unavailable for this executable; using native menus");
	launchQueued = true;
	TrainingTeamEditor::TraceQuickShortcut("Start Training queued; waiting for the tool window to hide");
	return true;
}

void TrainingShortcut::DispatchTraining()
{
	if (!launchQueued)
		return;
	launchQueued = false;
	if (!IsReadyMenu(ReadMenu(), MainMenuPhase, MainMenuTable))
	{
		TrainingTeamEditor::TraceQuickShortcut("Start Training cancelled: main menu changed before launch");
		return;
	}
	TrainingTeamEditor::TraceQuickShortcut("tool window hidden; returning focus to the game");
	if (const auto window = gameWindow.load())
		SetForegroundWindow(window);
	lastLaunchProgress = { -1, -1, -1 };
	const auto now = GetTickCount64();
	requestDeadline.store(now + TransitionTimeoutMs);
	loadingFramePresented.store(false);
	loadingFrameDeadline.store(now + LoadingFrameTimeoutMs);
	TrainingTeamEditor::TraceQuickShortcut("tool renderer idle; waiting for main menu update");
	request.store(queuedDirect ? Request::OpenDirect : Request::OpenOffline);
}

void TrainingShortcut::Initialize(IDirect3DDevice9* toolDevice)
{
	originalMainUpdate = reinterpret_cast<MenuUpdate>(GameAddress(0x1403e74f0));
	originalOfflineUpdate = reinterpret_cast<MenuUpdate>(GameAddress(0x1403e6b80));
	constexpr std::array<uint8_t, 8> selectSignature{ 0x40, 0x56, 0x57, 0x48, 0x83, 0xec, 0x58, 0x48 };
	constexpr std::array<uint8_t, 8> finishSignature{ 0x48, 0x63, 0xc2, 0xc7, 0x84, 0x81, 0xb4, 0x02 };
	constexpr std::array<uint8_t, 8> stageSignature{ 0x40, 0x57, 0x48, 0x83, 0xec, 0x20, 0x80, 0x79 };
	D3DDEVICE_CREATION_PARAMETERS parameters{};
	if (toolDevice && SUCCEEDED(toolDevice->GetCreationParameters(&parameters)))
		toolWindow = parameters.hFocusWindow;

	const bool nativeMenuSignaturesMatch =
		PointerAt(GameAddress(MainMenuTable) + MenuSetStateSlot) == GameAddress(MainMenuSetState)
		&& PointerAt(GameAddress(OfflineMenuTable) + MenuSetStateSlot) == GameAddress(OfflineMenuSetState)
		&& MatchesGameCode(CharacterSelectUpdate, selectSignature)
		&& MatchesGameCode(CharacterFinishSelection, finishSignature)
		&& MatchesGameCode(StageConfirmAddress, stageSignature);

	directLaunchSupported = PointerAt(GameAddress(SceneManagerTable) + QueueSceneSlot) == GameAddress(QueueSceneAddress)
		&& MatchesGameCode(QueueSceneAddress, std::array<uint8_t, 8>{ 0x48, 0x89, 0x91, 0x40, 0x70, 0x00, 0x00, 0xc3 })
		&& MatchesGameCode(SetBattleModeAddress, std::array<uint8_t, 8>{ 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83 })
		&& MatchesGameCode(SetBattleStageAddress, std::array<uint8_t, 7>{ 0x89, 0x91, 0x58, 0x03, 0x00, 0x00, 0xc3 })
		&& MatchesGameCode(SetBattleTimeAddress, std::array<uint8_t, 11>{ 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x8b })
		&& MatchesGameCode(SetBattleTimeAddress + 0x17, std::array<uint8_t, 6>{ 0x89, 0x9f, 0x5c, 0x03, 0x00, 0x00 })
		&& MatchesGameCode(SetUiStageAddress, std::array<uint8_t, 7>{ 0x89, 0x91, 0x34, 0x59, 0x00, 0x00, 0xc3 })
		&& MatchesGameCode(ResolveStageAddress, std::array<uint8_t, 8>{ 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48 });

	if (!directLaunchSupported && !nativeMenuSignaturesMatch)
	{
		TrainingTeamEditor::TraceQuickShortcut("Training launch unavailable: native menu and direct launch signatures do not match");
		return;
	}
	const auto initialized = MH_Initialize();
	if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
	{
		TraceHook("MinHook initialization failed", initialized);
		return;
	}
	if (!ReplaceMenuUpdate(MainMenuTable, originalMainUpdate, MainUpdate))
	{
		TrainingTeamEditor::TraceQuickShortcut("main menu hook unavailable");
		return;
	}

	void* original = nullptr;
	if (nativeMenuSignaturesMatch)
	{
		auto* selectTarget = reinterpret_cast<void*>(GameAddress(CharacterSelectUpdate));
		const auto createSelect = MH_CreateHook(selectTarget, reinterpret_cast<void*>(CharacterSelectUpdateHook), &original);
		if (createSelect == MH_OK)
		{
			originalSelectUpdate = reinterpret_cast<SelectUpdate>(original);
			const bool offlineHooked = ReplaceMenuUpdate(OfflineMenuTable, originalOfflineUpdate, OfflineUpdate);
			const auto enableSelect = offlineHooked ? MH_EnableHook(selectTarget) : MH_ERROR_DISABLED;
			nativeMenuLaunchSupported = offlineHooked && enableSelect == MH_OK;
			if (!nativeMenuLaunchSupported)
			{
				if (offlineHooked)
					ReplaceMenuUpdate(OfflineMenuTable, OfflineUpdate, originalOfflineUpdate);
				MH_RemoveHook(selectTarget);
				TraceHook("native menu launch unavailable", enableSelect);
			}
		}
		else
			TraceHook("could not hook character selection", createSelect);
	}
	else
		TrainingTeamEditor::TraceQuickShortcut("native menu signature mismatch; leaving character selection untouched");

	if (!nativeMenuLaunchSupported && !directLaunchSupported)
	{
		ReplaceMenuUpdate(MainMenuTable, MainUpdate, originalMainUpdate);
		return;
	}
	if (!nativeMenuLaunchSupported)
		TrainingTeamEditor::TraceQuickShortcut("F4 available with direct Training launch only");
	shortcutAvailable.store(true);
	TrainingTeamEditor::TraceQuickShortcut("native hooks ready; polling F4 independently of the overlay");

	if (!toolDevice || !toolWindow)
	{
		TrainingTeamEditor::TraceQuickShortcut("menu overlay unavailable: no tool Direct3D window");
		return;
	}
	void* endSceneTarget = (*reinterpret_cast<void***>(toolDevice))[EndSceneSlot];
	const auto createOverlay = MH_CreateHook(endSceneTarget, reinterpret_cast<void*>(DrawGameMenu), &original);
	if (createOverlay == MH_OK)
	{
		originalEndScene = reinterpret_cast<EndScene>(original);
		const auto enableOverlay = MH_EnableHook(endSceneTarget);
		TraceHook("Direct3D overlay hook", enableOverlay);
		if (enableOverlay != MH_OK)
			MH_RemoveHook(endSceneTarget);
	}
	else
		TraceHook("could not hook Direct3D overlay", createOverlay);

	void* presentTarget = (*reinterpret_cast<void***>(toolDevice))[PresentSlot];
	const auto createPresent = MH_CreateHook(presentTarget, reinterpret_cast<void*>(PresentGame), &original);
	if (createPresent == MH_OK)
	{
		originalPresent = reinterpret_cast<Present>(original);
		const auto enablePresent = MH_EnableHook(presentTarget);
		TraceHook("Direct3D loading Present hook", enablePresent);
		if (enablePresent != MH_OK)
			MH_RemoveHook(presentTarget);
	}
	else
		TraceHook("could not hook Direct3D loading Present", createPresent);

	IDirect3DSwapChain9* swapChain = nullptr;
	if (SUCCEEDED(toolDevice->GetSwapChain(0, &swapChain)))
	{
		void* swapChainTarget = (*reinterpret_cast<void***>(swapChain))[SwapChainPresentSlot];
		const auto createSwapChain = MH_CreateHook(swapChainTarget, reinterpret_cast<void*>(PresentGameSwapChain), &original);
		if (createSwapChain == MH_OK)
		{
			originalSwapChainPresent = reinterpret_cast<SwapChainPresent>(original);
			const auto enableSwapChain = MH_EnableHook(swapChainTarget);
			TraceHook("Direct3D loading swap-chain hook", enableSwapChain);
			if (enableSwapChain != MH_OK)
				MH_RemoveHook(swapChainTarget);
		}
		else
			TraceHook("could not hook Direct3D loading swap-chain", createSwapChain);
		swapChain->Release();
	}
}
