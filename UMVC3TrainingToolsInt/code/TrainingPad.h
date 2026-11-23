#pragma once

#include <Windows.h>
#include <Xinput.h>
#include <array>
#include <cstdint>

namespace TrainingPad
{
	enum class Shortcut { QuickTraining, Tools, ResetTraining };
	enum class Direction { Neutral, Left, Right, Down, Up };
	struct InputCounts
	{
		uint32_t polls = 0;
		uint32_t active = 0;
		uint32_t suppressed = 0;
	};

	void Initialize(HMODULE mod, HWND menuWindow);
	void Shutdown();
	void Poll();
	bool Pressed(Shortcut shortcut);
	Direction HeldDirection();
	void UpdateNavigation(bool focused);
	void DrawSettings();
	std::array<InputCounts, XUSER_MAX_COUNT> ReadInputCounts();
}
