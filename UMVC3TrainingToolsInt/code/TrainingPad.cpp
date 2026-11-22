#include "../pch.h"
#include "TrainingPad.h"
#include "../gui/imgui/imgui.h"

#include <Xinput.h>
#include <delayimp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

namespace
{
	using GetState = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
	constexpr DWORD LeftTrigger = 0x10000;
	constexpr DWORD RightTrigger = 0x20000;
	constexpr DWORD TriggerThreshold = 128;
	constexpr wchar_t Section[] = L"Controller";

	struct Button
	{
		DWORD mask;
		const char* label;
		ImGuiKey navigationKey;
	};

	constexpr Button Buttons[]{
		{ 0, "Disabled", ImGuiKey_None },
		{ XINPUT_GAMEPAD_BACK, "View / Select", ImGuiKey_GamepadBack },
		{ XINPUT_GAMEPAD_START, "Menu / Start", ImGuiKey_GamepadStart },
		{ XINPUT_GAMEPAD_LEFT_THUMB, "L3 (left stick click)", ImGuiKey_GamepadL3 },
		{ XINPUT_GAMEPAD_RIGHT_THUMB, "R3 (right stick click)", ImGuiKey_GamepadR3 },
		{ XINPUT_GAMEPAD_LEFT_SHOULDER, "LB / L1", ImGuiKey_GamepadL1 },
		{ XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB / R1", ImGuiKey_GamepadR1 },
		{ LeftTrigger, "LT / L2", ImGuiKey_GamepadL2 },
		{ RightTrigger, "RT / R2", ImGuiKey_GamepadR2 },
		{ XINPUT_GAMEPAD_A, "A / Cross", ImGuiKey_GamepadFaceDown },
		{ XINPUT_GAMEPAD_B, "B / Circle", ImGuiKey_GamepadFaceRight },
		{ XINPUT_GAMEPAD_X, "X / Square", ImGuiKey_GamepadFaceLeft },
		{ XINPUT_GAMEPAD_Y, "Y / Triangle", ImGuiKey_GamepadFaceUp },
		{ XINPUT_GAMEPAD_DPAD_UP, "D-pad up", ImGuiKey_GamepadDpadUp },
		{ XINPUT_GAMEPAD_DPAD_DOWN, "D-pad down", ImGuiKey_GamepadDpadDown },
		{ XINPUT_GAMEPAD_DPAD_LEFT, "D-pad left", ImGuiKey_GamepadDpadLeft },
		{ XINPUT_GAMEPAD_DPAD_RIGHT, "D-pad right", ImGuiKey_GamepadDpadRight }
	};

	struct Controller
	{
		XINPUT_GAMEPAD pad{};
		DWORD held{};
		DWORD pressed{};
		ULONGLONG retryAt{};
		bool connected{};
	};

	struct Binding
	{
		const wchar_t* key;
		const char* label;
		DWORD button;
	};

	struct GameInput
	{
		DWORD packetOffset{};
		bool masked{};
	};

	std::array<Controller, XUSER_MAX_COUNT> controllers{};
	std::array<GameInput, XUSER_MAX_COUNT> gameInput{};
	std::mutex gameInputMutex;
	std::atomic<HWND> menuWindow{ nullptr };
	std::atomic<GetState> gameGetState{ nullptr };
	void* volatile* gameStateImport = nullptr;
	bool gameStateHooked = false;
	DWORD activeController = XUSER_MAX_COUNT;
	DWORD suppressedButton = 0;
	HMODULE ownedXInput = nullptr;
	GetState getState = nullptr;
	std::filesystem::path configPath;
	std::array bindings{
		Binding{ L"QuickTraining", "Quick Training", XINPUT_GAMEPAD_BACK },
		Binding{ L"Tools", "Tool window", XINPUT_GAMEPAD_RIGHT_THUMB },
		Binding{ L"ResetTraining", "Reset Training", XINPUT_GAMEPAD_LEFT_THUMB }
	};
	const char* settingsStatus = "";

	bool IsNeutral(const XINPUT_GAMEPAD& pad)
	{
		return !pad.wButtons
			&& pad.bLeftTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD
			&& pad.bRightTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD
			&& std::abs(pad.sThumbLX) <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			&& std::abs(pad.sThumbLY) <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			&& std::abs(pad.sThumbRX) <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
			&& std::abs(pad.sThumbRY) <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
	}

	void FilterGameInput(GameInput& input, XINPUT_STATE& state, bool focused)
	{
		// Wait for release after closing the menu so its final input does not reach the game.
		const bool masked = focused || (input.masked && !IsNeutral(state.Gamepad));
		if (masked != input.masked)
			++input.packetOffset;
		input.masked = masked;
		state.dwPacketNumber += input.packetOffset;
		if (masked)
			state.Gamepad = {};
	}

	DWORD WINAPI GameGetState(DWORD index, XINPUT_STATE* state)
	{
		const DWORD result = gameGetState.load()(index, state);
		const auto window = menuWindow.load();
		if (window && result == ERROR_SUCCESS && index < gameInput.size())
		{
			const std::lock_guard lock(gameInputMutex);
			FilterGameInput(gameInput[index], *state, GetForegroundWindow() == window);
		}
		return result;
	}

	void* volatile* FindGameStateImport()
	{
		const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
		if (!directory.VirtualAddress)
			return nullptr;

		const auto imports = reinterpret_cast<const ImgDelayDescr*>(base + directory.VirtualAddress);
		for (size_t i = 0; i < directory.Size / sizeof(ImgDelayDescr) && imports[i].rvaDLLName; ++i)
		{
			const auto& entry = imports[i];
			if (!(entry.grAttrs & dlattrRva) || _stricmp(reinterpret_cast<const char*>(base + entry.rvaDLLName), "xinput1_3.dll") != 0)
				continue;

			const auto names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + entry.rvaINT);
			for (size_t index = 0; names[index].u1.Ordinal; ++index)
			{
				const auto ordinal = names[index].u1.Ordinal;
				// UMVC3 imports XInputGetState by ordinal 2.
				if (IMAGE_SNAP_BY_ORDINAL(ordinal) && IMAGE_ORDINAL(ordinal) == 2)
					return reinterpret_cast<void* volatile*>(reinterpret_cast<uintptr_t>(base) + entry.rvaIAT) + index;
			}
		}
		return nullptr;
	}

	bool ReplaceGameStateImport(void* expected, void* replacement)
	{
		auto* address = const_cast<void**>(gameStateImport);
		DWORD protection = 0;
		if (!VirtualProtect(address, sizeof(*address), PAGE_READWRITE, &protection))
			return false;
		const bool replaced = InterlockedCompareExchangePointer(gameStateImport, replacement, expected) == expected;
		DWORD unused = 0;
		VirtualProtect(address, sizeof(*address), protection, &unused);
		return replaced;
	}

	void HookGameState()
	{
		if (gameStateHooked || !gameStateImport)
			return;

		void* current = *gameStateImport;
		MEMORY_BASIC_INFORMATION memory{};
		if (!VirtualQuery(current, &memory, sizeof(memory)) || memory.AllocationBase == GetModuleHandleW(nullptr))
			return; // The import still points to the delay-load thunk in the game.

		gameGetState.store(reinterpret_cast<GetState>(current));
		gameStateHooked = ReplaceGameStateImport(current, reinterpret_cast<void*>(GameGetState));
	}

	const Button* FindButton(DWORD mask)
	{
		const auto button = std::find_if(std::begin(Buttons), std::end(Buttons),
			[mask](const Button& candidate) { return candidate.mask == mask; });
		return button == std::end(Buttons) ? nullptr : button;
	}

	bool SaveBinding(const Binding& binding, DWORD button)
	{
		if (configPath.empty())
			return false;

		const auto value = std::to_wstring(button);
		return WritePrivateProfileStringW(Section, binding.key, value.c_str(), configPath.c_str()) != 0;
	}

	void DrawBinding(Binding& binding)
	{
		if (!ImGui::BeginCombo(binding.label, FindButton(binding.button)->label))
			return;

		for (const auto& button : Buttons)
		{
			const bool selected = binding.button == button.mask;
			const bool duplicate = button.mask != 0 && std::any_of(bindings.begin(), bindings.end(),
				[&](const Binding& other) { return &binding != &other && other.button == button.mask; });
			ImGui::BeginDisabled(duplicate);
			if (ImGui::Selectable(button.label, selected) && !selected)
			{
				if (SaveBinding(binding, button.mask))
				{
					binding.button = button.mask;
					settingsStatus = "";
				}
				else
					settingsStatus = "Could not save controller bindings.";
			}
			ImGui::EndDisabled();
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void TrainingPad::Initialize(HMODULE mod, HWND window)
{
	menuWindow.store(window);
	gameStateImport = FindGameStateImport();
	HookGameState();

	wchar_t modulePath[MAX_PATH]{};
	const auto length = GetModuleFileNameW(mod, modulePath, MAX_PATH);
	if (length && length < MAX_PATH)
	{
		configPath = std::filesystem::path(modulePath).parent_path() / L"UMVC3TrainingToolsInt.ini";
		// Keep existing opening shortcuts when an older configuration already uses L3.
		for (auto binding = bindings.begin(); binding != bindings.end(); ++binding)
		{
			const auto saved = GetPrivateProfileIntW(Section, binding->key, binding->button, configPath.c_str());
			if (FindButton(saved))
				binding->button = saved;
			if (std::any_of(bindings.begin(), binding,
				[&](const Binding& previous) { return previous.button == binding->button; }))
			{
				binding->button = 0;
			}
		}
	}

	// Prefer the version already used by the game and by its controller integrations.
	if (const auto gameXInput = GetModuleHandleW(L"xinput1_3.dll"))
		getState = reinterpret_cast<GetState>(GetProcAddress(gameXInput, "XInputGetState"));
	if (!getState)
	{
		const wchar_t* versions[]{ L"xinput1_4.dll", L"xinput9_1_0.dll" };
		for (const auto version : versions)
		{
			ownedXInput = LoadLibraryExW(version, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (!ownedXInput)
				continue;
			getState = reinterpret_cast<GetState>(GetProcAddress(ownedXInput, "XInputGetState"));
			if (getState)
				break;
			FreeLibrary(ownedXInput);
			ownedXInput = nullptr;
		}
	}
}

void TrainingPad::Shutdown()
{
	menuWindow.store(nullptr);
	if (gameStateHooked)
		ReplaceGameStateImport(reinterpret_cast<void*>(GameGetState), reinterpret_cast<void*>(gameGetState.load()));
	if (ownedXInput)
		FreeLibrary(ownedXInput);
}

void TrainingPad::Poll()
{
	HookGameState();
	if (!getState)
		return;

	const auto now = GetTickCount64();
	for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index)
	{
		auto& controller = controllers[index];
		controller.pressed = 0;
		if (!controller.connected && now < controller.retryAt)
			continue;

		XINPUT_STATE state{};
		if (getState(index, &state) != ERROR_SUCCESS)
		{
			controller = {};
			controller.retryAt = now + 1000;
			continue;
		}

		controller.pad = state.Gamepad;
		DWORD held = controller.pad.wButtons;
		if (controller.pad.bLeftTrigger >= TriggerThreshold)
			held |= LeftTrigger;
		if (controller.pad.bRightTrigger >= TriggerThreshold)
			held |= RightTrigger;
		controller.pressed = controller.connected ? held & ~controller.held : 0;
		controller.held = held;
		controller.connected = true;
	}

	if (activeController == XUSER_MAX_COUNT || !controllers[activeController].connected)
	{
		const auto connected = std::find_if(controllers.begin(), controllers.end(),
			[](const Controller& controller) { return controller.connected; });
		activeController = static_cast<DWORD>(connected - controllers.begin());
		suppressedButton = 0;
	}
	if (activeController < XUSER_MAX_COUNT)
		suppressedButton &= controllers[activeController].held;
}

bool TrainingPad::Pressed(Shortcut shortcut)
{
	const DWORD mask = bindings[static_cast<size_t>(shortcut)].button;
	for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index)
	{
		if (controllers[index].pressed & mask)
		{
			activeController = index;
			suppressedButton = mask;
			return true;
		}
	}
	return false;
}

TrainingPad::HorizontalDirection TrainingPad::HeldHorizontalDirection()
{
	if (activeController >= XUSER_MAX_COUNT)
		return HorizontalDirection::Neutral;
	const auto& controller = controllers[activeController];
	const DWORD directions = controller.held & ~suppressedButton;
	const bool left = (directions & XINPUT_GAMEPAD_DPAD_LEFT)
		|| controller.pad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	const bool right = (directions & XINPUT_GAMEPAD_DPAD_RIGHT)
		|| controller.pad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	if (left == right)
		return HorizontalDirection::Neutral;
	return left ? HorizontalDirection::Left : HorizontalDirection::Right;
}

void TrainingPad::UpdateNavigation(bool focused)
{
	auto& io = ImGui::GetIO();
	const bool connected = focused && activeController < XUSER_MAX_COUNT;
	io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
	if (connected)
		io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

	const Controller idle{};
	const auto& controller = connected ? controllers[activeController] : idle;
	for (const auto& button : Buttons)
	{
		if (button.navigationKey != ImGuiKey_None)
			io.AddKeyEvent(button.navigationKey, (controller.held & ~suppressedButton & button.mask) != 0);
	}

	const auto axis = [&io](ImGuiKey key, int value, int deadZone)
	{
		const float strength = std::clamp(static_cast<float>(value - deadZone) / (32767 - deadZone), 0.0f, 1.0f);
		io.AddKeyAnalogEvent(key, strength > 0.1f, strength);
	};
	axis(ImGuiKey_GamepadLStickLeft, -controller.pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	axis(ImGuiKey_GamepadLStickRight, controller.pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	axis(ImGuiKey_GamepadLStickUp, controller.pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	axis(ImGuiKey_GamepadLStickDown, -controller.pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

void TrainingPad::DrawSettings()
{
	if (!ImGui::CollapsingHeader("Controller shortcuts"))
		return;

	if (activeController < XUSER_MAX_COUNT)
		ImGui::Text("Controller %lu connected", activeController + 1);
	else
		ImGui::TextDisabled("No XInput controller detected.");

	for (auto& binding : bindings)
		DrawBinding(binding);
	if (bindings[static_cast<size_t>(Shortcut::ResetTraining)].button)
		ImGui::TextWrapped("Hold left or right and press Reset Training to put P2 in that corner.");
	if (*settingsStatus)
		ImGui::TextWrapped("%s", settingsStatus);
}
