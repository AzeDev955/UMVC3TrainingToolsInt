#pragma once

#include <array>
#include <cstdint>

struct IDirect3DDevice9;

namespace TrainingShortcut
{
	inline constexpr int AutomaticColor = -1;
	inline constexpr int BaseColorCount = 6;
	inline constexpr int AlternateColor = 7;

	struct FighterPick
	{
		int32_t id;
		int32_t assist;
		int32_t color = AutomaticColor;

		bool operator==(const FighterPick&) const = default;
	};
	using TeamPicks = std::array<FighterPick, 6>;
	constexpr int StageCount = 18;

	void Initialize(IDirect3DDevice9* toolDevice);
	bool TakeSetupRequest(bool controllerPressed);
	bool CanLaunchViaNativeMenus();
	bool CanLaunchDirectly();
	int CurrentTrainingStage();
	bool StartTraining(const TeamPicks& picks, int stage, bool direct);
	void DispatchTraining();
}
