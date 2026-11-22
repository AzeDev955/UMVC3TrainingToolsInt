#include "../pch.h"
#include "TrainingPad.h"
#include "../gui/imgui/imgui.h"

#include <Xinput.h>
#include <algorithm>
#include <array>
#include <filesystem>

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

	std::array<Controller, XUSER_MAX_COUNT> controllers{};
	DWORD activeController = XUSER_MAX_COUNT;
	DWORD suppressedButton = 0;
	HMODULE ownedXInput = nullptr;
	GetState getState = nullptr;
	std::filesystem::path configPath;
	DWORD quickButton = XINPUT_GAMEPAD_BACK;
	DWORD toolsButton = XINPUT_GAMEPAD_RIGHT_THUMB;
	const char* settingsStatus = "";

	const Button* FindButton(DWORD mask)
	{
		const auto button = std::find_if(std::begin(Buttons), std::end(Buttons),
			[mask](const Button& candidate) { return candidate.mask == mask; });
		return button == std::end(Buttons) ? nullptr : button;
	}

	bool SaveBindings(DWORD quick, DWORD tools)
	{
		if (configPath.empty())
			return false;

		wchar_t values[80]{};
		const int length = swprintf_s(values, L"QuickTraining=%lu", quick);
		swprintf_s(values + length + 1, std::size(values) - length - 1, L"Tools=%lu", tools);
		return WritePrivateProfileSectionW(Section, values, configPath.c_str()) != 0;
	}

	void DrawBinding(const char* label, DWORD& binding, DWORD other, bool isQuick)
	{
		if (!ImGui::BeginCombo(label, FindButton(binding)->label))
			return;

		for (const auto& button : Buttons)
		{
			const bool selected = binding == button.mask;
			const bool duplicate = button.mask != 0 && button.mask == other;
			ImGui::BeginDisabled(duplicate);
			if (ImGui::Selectable(button.label, selected) && !selected)
			{
				const DWORD quick = isQuick ? button.mask : quickButton;
				const DWORD tools = isQuick ? toolsButton : button.mask;
				if (SaveBindings(quick, tools))
				{
					binding = button.mask;
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

void TrainingPad::Initialize(HMODULE mod)
{
	wchar_t modulePath[MAX_PATH]{};
	const auto length = GetModuleFileNameW(mod, modulePath, MAX_PATH);
	if (length && length < MAX_PATH)
	{
		configPath = std::filesystem::path(modulePath).parent_path() / L"UMVC3TrainingToolsInt.ini";
		const auto quick = GetPrivateProfileIntW(Section, L"QuickTraining", quickButton, configPath.c_str());
		const auto tools = GetPrivateProfileIntW(Section, L"Tools", toolsButton, configPath.c_str());
		if (FindButton(quick) && FindButton(tools) && (quick == 0 || quick != tools))
		{
			quickButton = quick;
			toolsButton = tools;
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
	if (ownedXInput)
		FreeLibrary(ownedXInput);
}

void TrainingPad::Poll()
{
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
	const DWORD mask = shortcut == Shortcut::QuickTraining ? quickButton : toolsButton;
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

	DrawBinding("Quick Training", quickButton, toolsButton, true);
	DrawBinding("Tool window", toolsButton, quickButton, false);
	if (*settingsStatus)
		ImGui::TextWrapped("%s", settingsStatus);
}
