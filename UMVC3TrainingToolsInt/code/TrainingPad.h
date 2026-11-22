#pragma once

#include <Windows.h>

namespace TrainingPad
{
	enum class Shortcut { QuickTraining, Tools, ResetTraining };
	enum class HorizontalDirection { Neutral, Left, Right };

	void Initialize(HMODULE mod, HWND menuWindow);
	void Shutdown();
	void Poll();
	bool Pressed(Shortcut shortcut);
	HorizontalDirection HeldHorizontalDirection();
	void UpdateNavigation(bool focused);
	void DrawSettings();
}
