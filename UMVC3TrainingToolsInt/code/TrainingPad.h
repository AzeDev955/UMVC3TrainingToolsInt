#pragma once

#include <Windows.h>

namespace TrainingPad
{
	enum class Shortcut { QuickTraining, Tools, ResetTraining };

	void Initialize(HMODULE mod);
	void Shutdown();
	void Poll();
	bool Pressed(Shortcut shortcut);
	void UpdateNavigation(bool focused);
	void DrawSettings();
}
