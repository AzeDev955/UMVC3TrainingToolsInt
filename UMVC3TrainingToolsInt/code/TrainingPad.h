#pragma once

#include <Windows.h>

namespace TrainingPad
{
	enum class Shortcut { QuickTraining, Tools, ResetTraining };
	enum class Direction { Neutral, Left, Right, Down, Up };

	void Initialize(HMODULE mod, HWND menuWindow);
	void Shutdown();
	void Poll();
	bool Pressed(Shortcut shortcut);
	Direction HeldDirection();
	void UpdateNavigation(bool focused);
	void DrawSettings();
}
