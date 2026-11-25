#include "../pch.h"
#include "TrainingTeamEditor.h"
#include "Localizer.h"
#include "ReleaseUpdate.h"

#include "../gui/imgui/imgui.h"
#include "umvc3menu.h"
#include "umvc3utils.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <shellapi.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using Localizer::UiText;

namespace
{
	constexpr int SlotsPerTeam = 3;
	constexpr int SlotCount = 2 * SlotsPerTeam;
	constexpr uintptr_t SelectionOffset = 0x40;
	constexpr uintptr_t ActionFlagsOffset = 0xc4;
	constexpr uintptr_t ActionStepOffset = 0xd8;
	constexpr uintptr_t NextActionStepOffset = 0xdc;
	constexpr uintptr_t RestartOffset = 0x118;
	constexpr uintptr_t CharacterReloadCountdownOffset = 0x1210;
	constexpr uintptr_t LoadedAssistOffset = 0x2dbc; // mAssistType on the loaded fighter.
	constexpr uintptr_t LoadedColorOffset = 0x2db8; // Costume and palette index copied from the selection on creation.
	constexpr const char* AssistTypes[] = { "TEAM_ALPHA", "TEAM_BETA", "TEAM_GAMMA" };
	// Native stage-selector IDs, in the order used by the game's stage resolver.
	constexpr std::array<const char*, TrainingShortcut::StageCount> Stages{
		"Bonne Wonderland", "Kattlelox Island", "Demon Village Redux", "Demon Village",
		"Chaos at Tricell", "Tricell Laboratory", "Days of Future Past", "Metro City",
		"City That Never Sleeps", "Daily Bugle", "Asgard: Sea of Space", "Asgard",
		"S.H.I.E.L.D. Air Show", "S.H.I.E.L.D. Helicarrier", "Shadowland", "Hand Hideout",
		"Danger Room", "Training Room"
	};
	constexpr int FightingStep = 5;
	constexpr int RestartStep = 12;
	constexpr uint32_t RestartTraining = 3;
	constexpr uint32_t ReloadPlayer1 = 0x1000;
	constexpr uint32_t ReloadPlayer2 = 0x2000;
	constexpr uint32_t ReloadTeams = ReloadPlayer1 | ReloadPlayer2;
	constexpr DWORD WatchIntervalMs = 500;
	constexpr ULONGLONG RestartTimeoutMs = 12000;
	constexpr ULONGLONG RestartSettleMs = 100;

	struct Fighter
	{
		const char* name;
		const char* gameClass;
		std::array<const char*, std::size(AssistTypes)> assists;
	};

	// Indexes match the base game's character IDs. Assist names are in character select order.
	// https://steamcommunity.com/app/357190/discussions/0/4299250686371974156/
	// Assist names follow the game's mnchs.arc and mnchs_msg.arc data.
	constexpr Fighter Roster[] = {
		{ "", "", {} },
		{ "Ryu", "uRyu", { "Shoryuken", "Hadoken", "Tatsumaki Senpukyaku" } },
		{ "Chun-Li", "uChunli", { "Kikoken", "Tenshokyaku", "Hyakuretsukyaku" } },
		{ "Akuma", "uGouki", { "Gohadoken", "Tatsumaki Zankukyaku", "Hyakki Gojin" } },
		{ "Chris", "uChris", { "Combination Punch", "Gun Fire", "Grenade Toss" } },
		{ "Wesker", "uWesker", { "Ghost Butterfly", "Samurai Edge (Lower Shot)", "Jaguar Dash" } },
		{ "Viewtiful Joe", "uVJoe", { "Voomerang", "Groovy Uppercut", "Shocking Pink" } },
		{ "Dante", "uDante", { "Jam Session", "Crystal", "Weasel Shot" } },
		{ "Trish", "uTrish", { "Trick \"Hopscotch\"", "Trick \"Peekaboo\"", "Low Voltage" } },
		{ "Frank West", "uFrank", { "Shopping Cart", "Tools of Survival", "Pick Me Up" } },
		{ "Spencer", "uSpencer", { "Wire Grapple (H. Shot)", "Wire Grapple (Slant Shot)", "Armor Piercer" } },
		{ "Arthur", "uArthur", { "Heavenly Slash", "Dagger Toss", "Fire Bottle Toss" } },
		{ "Amaterasu", "uAmaterasu", { "Solar Flare", "Cold Star", "Bloom" } },
		{ "Zero", "uZero", { "Ryuenjin", "Hadangeki", "Shippuga" } },
		{ "Tron Bonne", "uTron", { "Bonne Strike", "Gustaff Fire", "Bandit Boulder" } },
		{ "Morrigan", "uMorrigan", { "Shadow Blade", "Soul Fist", "Dark Harmonizer" } },
		{ "Hsien-Ko", "uLeilei", { "Senpu Bu", "Henkyo Ki", "Anki Hou" } },
		{ "Felicia", "uFelicia", { "Rolling Buckler / Rolling Slide", "Sand Splash", "Cat Spike" } },
		{ "C. Viper", "uCViper", { "Thunder Knuckle", "Seismic Hammer", "Burning Kick" } },
		{ "Haggar", "uHaggar", { "Double Lariat", "Violent Ax", "Steel Pipe" } },
		{ "Jill", "uJill", { "Cartwheel Kick", "Arrow Kick", "Somersault Kick" } },
		{ "Strider Hiryu", "uHiryu", { "Ame-no-Murakumo", "Gram", "Vajra" } },
		{ "Vergil", "uVergil", { "Judgment Cut", "Rising Sun", "Rapid Slash" } },
		{ "Phoenix Wright", "uNaruhodo", { "Paperwork (High)", "Press the Witness", "Get 'em, Missile!" } },
		{ "Firebrand", "uRedArremer", { "Hell Spitfire", "Demon Missile (Charge)", "Demon Missile (Swoop)" } },
		{ "Nemesis", "uNemesis", { "Clothesline Rocket", "Launcher Slam", "Rocket Launcher" } },
		{ "Spider-Man", "uSpiderMan", { "Web Ball", "Web Swing", "Spider Sting" } },
		{ "Captain America", "uCapAmerica", { "Shield Slash", "Stars & Stripes", "Charging Star" } },
		{ "Wolverine", "uWolverine", { "Tornado Claw", "Berserker Slash", "Berserker Barrage" } },
		{ "Magneto", "uMagneto", { "Electromagnetic Disruptor", "Hyper Gravitation", "Force Field" } },
		{ "Hulk", "uHulk", { "Gamma Wave", "Anti-Air Gamma Charge", "Gamma Charge" } },
		{ "She-Hulk", "uSheHulk", { "Torpedo", "Clothesline", "Somersault Kick+" } },
		{ "Taskmaster", "uTaskMaster", { "Aim Master (H. Shot)", "Aim Master (Parabolic Shot)", "Aim Master (Vertical Shot)" } },
		{ "Iron Man", "uIronMan", { "Unibeam", "Repulsor Blast", "Smart Bomb" } },
		{ "Thor", "uThor", { "Mighty Spark", "Mighty Smash", "Mighty Strike" } },
		{ "Doctor Doom", "uDrDoom", { "Plasma Beam", "Hidden Missiles", "Molecular Shield" } },
		{ "Phoenix", "uPhoenix", { "TK Shot", "TK Overdrive", "TK Trap" } },
		{ "Shuma-Gorath", "uShuma", { "Mystic Ray", "Mystic Stare", "Mystic Smash" } },
		{ "MODOK", "uModok", { "Barrier", "Balloon Bomb", "Psionic Blast" } },
		{ "Dormammu", "uDormammu", { "Dark Hole", "Purification", "Liberation" } },
		{ "Deadpool", "uDeadPool", { "Quick Work", "Katana-Rama!", "Trigger Happy" } },
		{ "Storm", "uStorm", { "Whirlwind", "Double Typhoon", "Lightning Attack" } },
		{ "Super-Skrull", "uSuperSkrull", { "Stone Smite", "Orbital Grudge", "Tenderizer" } },
		{ "Sentinel", "uSentinel", { "Sentinel Force (Charge)", "Sentinel Force (Bomb)", "Rocket Punch" } },
		{ "X-23", "uX23", { "Neck Slice", "Ankle Slice", "Crescent Scythe" } },
		{ "Nova", "uNova", { "Gravimetric Pulse", "Centurion Rush", "Nova Strike" } },
		{ "Rocket Raccoon", "uRRaccoon", { "Spitfire Twice", "Claymore", "Pendulum" } },
		{ "Ghost Rider", "uGhostRider", { "Chain of Rebuttal", "Heartless Spire", "Hellfire" } },
		{ "Iron Fist", "uIronFist", { "Dragon's Touch", "Crescent Heel", "Rising Fang" } },
		{ "Doctor Strange", "uDrStrange", { "Daggers of Denak", "Eye of Agamotto", "Bolts of Balthakk" } },
		{ "Hawkeye", "uHawkEye", { "Quick Shot (Greyhound)", "Trick Shot (Violet Fizz)", "Rag Time Shot (Kamikaze)" } }
	};

	constexpr auto FighterDisplayOrder = []
	{
		std::array<int, std::size(Roster) - 1> ids{};
		std::iota(ids.begin(), ids.end(), 1);
		std::sort(ids.begin(), ids.end(), [](int left, int right)
		{
			const std::string_view leftName = Roster[left].name;
			const std::string_view rightName = Roster[right].name;
			const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
			return std::lexicographical_compare(leftName.begin(), leftName.end(), rightName.begin(), rightName.end(),
				[lower](char a, char b) { return lower(a) < lower(b); });
		});
		return ids;
	}();

	constexpr uint32_t ClassId(const char* name)
	{
		uint32_t crc = 0xffffffff;
		for (; *name; ++name)
		{
			crc ^= static_cast<unsigned char>(*name);
			for (int bit = 0; bit < 8; ++bit)
				crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
		}
		return crc & 0x7fffffff;
	}

	static_assert(ClassId(Roster[1].gameClass) == 0x33f0cd9f);
	static_assert(ClassId(Roster[22].gameClass) == 0x2b5384f5);
	static_assert(ClassId(Roster[47].gameClass) == 0x298a709d);

	struct Selection
	{
		uint32_t side;
		int32_t id;
		uint32_t classId;
		int32_t color;
		uint32_t flags;
		int32_t assist;
		std::array<unsigned char, 0x40> optionsAfterAssist;

		bool operator==(const Selection&) const = default;
	};
	static_assert(sizeof(Selection) == 0x58);
	static_assert(offsetof(Selection, color) == 0x0c);
	static_assert(offsetof(Selection, assist) == 0x14);

	using Teams = std::array<Selection, SlotCount>;
	using Classes = std::array<uintptr_t, SlotCount>;

	struct LoadedFighter
	{
		uintptr_t gameClass;
		int32_t assist;
		int32_t color;

		bool operator==(const LoadedFighter&) const = default;
	};
	using LoadedTeams = std::array<LoadedFighter, SlotCount>;

	struct DraftSlot
	{
		int source;
		int id;
		int assist;
		int color = TrainingShortcut::AutomaticColor;
	};

	constexpr std::array QuickResetChoices{
		std::pair{ "QUICK_RESET_CENTER", TrainingTeamEditor::Center },
		std::pair{ "QUICK_RESET_LEFT", TrainingTeamEditor::LeftCorner },
		std::pair{ "QUICK_RESET_RIGHT", TrainingTeamEditor::RightCorner }
	};
	constexpr std::array QuickReactionLabels{
		"REC_REACTION_HIT", "REC_REACTION_BLOCK", "REC_REACTION_WAKEUP", "REC_REACTION_INCOMING"
	};
	constexpr std::array DummyReactionSettings{ &DummyOnHit, &DummyOnBlock, &DummyOnWakeup, &DummyOnIncoming };
	struct QuickExercise
	{
		int resetPosition = 0;
		std::array<DummyReaction, QuickReactionLabels.size()> reactions{};

		bool operator==(const QuickExercise&) const = default;
	};

	struct QuickPreset
	{
		std::string name;
		TrainingShortcut::TeamPicks picks{};
		int stage{};
		std::optional<QuickExercise> exercise;

		bool operator==(const QuickPreset&) const = default;
	};

	struct Match
	{
		uintptr_t settings{};
		uintptr_t action{};
		Teams teams{};
	};

	struct Trial
	{
		Match before;
		Teams proposed;
		LoadedTeams originalFighters;
		Classes expectedClasses;
		ULONGLONG started = 0;
		bool pending = false;
		bool uncertain = false;
		unsigned number = 0;
		uint32_t reloadFlags = 0;
		const char* successStatus = nullptr;
		std::optional<QuickExercise> exercise;
	};

	struct ActionState
	{
		uint32_t flags{};
		uint32_t restart{};
		int step = -1;
		int nextStep = -1;
	};

	struct PendingRestart
	{
		uintptr_t settings{};
		uintptr_t action{};
		ULONGLONG readyAt{};
		std::optional<TrainingTeamEditor::XPositions> positions;
		ULONGLONG started{};
		bool waitLogged = false;
		bool initialPlacement = false;
	};

	Match draftFrom;
	std::array<DraftSlot, SlotCount> draft{};
	std::array<DraftSlot, SlotCount> quickDraft{{
		{ 0, 1, 0 }, { 1, 2, 0 }, { 2, 3, 0 },
		{ 3, 28, 0 }, { 4, 27, 0 }, { 5, 26, 0 }
	}};
	int quickStage = 0; // Bonne Wonderland.
	bool quickDirectLaunch = true;
	const char* quickSetupStatus = "";
	constexpr size_t PresetNameCapacity = 64;
	constexpr int TeamPresetVersion = 1;
	constexpr int ExercisePresetVersion = 2;
	constexpr int ColorPresetVersion = 3;
	bool useQuickExercise = false;
	QuickExercise quickExercise;
	std::optional<QuickExercise> queuedExercise;
	uintptr_t quickExerciseAction = 0;
	TrainingTeamEditor::XPositions quickExerciseReset{};
	std::filesystem::path quickPresetFolder;
	std::vector<QuickPreset> quickPresets;
	std::optional<QuickPreset> selectedQuickPreset;
	char quickPresetName[PresetNameCapacity]{};
	const char* quickPresetStatus = "";
	Trial trial;
	PendingRestart pendingRestart;
	const char* status = "";
	constexpr wchar_t LogName[] = L"UMVC3TrainingToolsInt-Teams.log";
	constexpr wchar_t RecordingLogName[] = L"UMVC3TrainingToolsInt-Recording.log";
	constexpr std::uintmax_t LogRotateBytes = 4 * 1024 * 1024;
	std::filesystem::path logPath;
	std::filesystem::path recordingLogPath;
	SRWLOCK logLock = SRWLOCK_INIT;
	bool logAvailable = false;
	bool logInTemp = false;
	unsigned attemptCount = 0;
	std::atomic<unsigned> watchedAttempt{ 0 };
	std::atomic<ULONGLONG> lastEditorUpdate{ 0 };
	constexpr size_t RecordingLogCapacity = 128;
	using RecordingLogLine = std::array<char, 320>;
	std::array<RecordingLogLine, RecordingLogCapacity> recordingLog{};
	size_t recordingLogSize = 0;
	unsigned droppedRecordingLogs = 0;
	SRWLOCK recordingLogLock = SRWLOCK_INIT;

	void WriteLogTimestamp(FILE* file)
	{
		SYSTEMTIME time{};
		GetSystemTime(&time);
		std::fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC ",
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
	}

	bool Log(unsigned attempt, const char* format, ...)
	{
		AcquireSRWLockExclusive(&logLock);
		FILE* file = nullptr;
		const bool opened = !logPath.empty() && _wfopen_s(&file, logPath.c_str(), L"a") == 0;
		if (opened)
		{
			WriteLogTimestamp(file);
			std::fprintf(file, "pid=%lu attempt=%u ", GetCurrentProcessId(), attempt);
			va_list args;
			va_start(args, format);
			std::vfprintf(file, format, args);
			va_end(args);
			std::fputc('\n', file);
			std::fclose(file);
		}
		ReleaseSRWLockExclusive(&logLock);
		return opened;
	}

	void RotateLog(const std::filesystem::path& path)
	{
		std::error_code error;
		const auto size = std::filesystem::file_size(path, error);
		if (!error && size >= LogRotateBytes)
		{
			auto previous = path;
			previous.replace_extension(L".previous.log");
			if (!MoveFileExW(path.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING))
				Log(0, "diagnostics: rotation failed (Windows error %lu)", GetLastError());
		}
	}

	void OpenRecordingLog(const std::filesystem::path& directory)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error)
			return;

		const auto path = directory / RecordingLogName;
		RotateLog(path);
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"a") != 0)
			return;

		WriteLogTimestamp(file);
		std::fprintf(file, "Recording diagnostics started pid=%lu version=%s build=%s %s\n",
			GetCurrentProcessId(), ReleaseUpdate::VersionTag, __DATE__, __TIME__);
		std::fclose(file);
		recordingLogPath = path;
	}

	bool OpenLog(const std::filesystem::path& directory, const std::filesystem::path& previous)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error)
			return false;

		logPath = directory / LogName;
		DWORD moveError = ERROR_SUCCESS;
		if (std::filesystem::is_regular_file(previous, error) && !MoveFileW(previous.c_str(), logPath.c_str()))
		{
			moveError = GetLastError();
			if (moveError == ERROR_ALREADY_EXISTS || moveError == ERROR_FILE_EXISTS)
			{
				SYSTEMTIME time{};
				GetLocalTime(&time);
				wchar_t archiveName[96];
				swprintf_s(archiveName, L"UMVC3TrainingToolsInt-Teams-legacy-%04d%02d%02d-%02d%02d%02d-%lu.log",
					time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
				const auto archive = directory / archiveName;
				moveError = MoveFileW(previous.c_str(), archive.c_str()) ? ERROR_SUCCESS : GetLastError();
			}
		}

		RotateLog(logPath);
		const bool opened = Log(0, "diagnostics: started version=%s build=%s %s", ReleaseUpdate::VersionTag, __DATE__, __TIME__);
		if (opened && moveError != ERROR_SUCCESS && moveError != ERROR_FILE_NOT_FOUND)
			Log(0, "Could not move the previous log (Windows error %lu)", moveError);
		return opened;
	}

	template <typename T>
	bool Read(uintptr_t address, T& value)
	{
		SIZE_T bytes = 0;
		return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
			&value, sizeof(value), &bytes) && bytes == sizeof(value);
	}

	template <typename T>
	bool Write(uintptr_t address, const T& value)
	{
		SIZE_T bytes = 0;
		return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
			&value, sizeof(value), &bytes) && bytes == sizeof(value);
	}

	uintptr_t PointerAt(uintptr_t address)
	{
		uintptr_t value = 0;
		Read(address, value);
		return value;
	}

	bool SupportsTeamReload()
	{
		// The native restart dispatches both team reload flags through sCharacter.
		constexpr std::array<uint8_t, 13> player1Check{
			0xba, 0x00, 0x10, 0x00, 0x00, 0x48, 0x8b, 0xce, 0xe8, 0x33, 0x15, 0x00, 0x00
		};
		constexpr std::array<uint8_t, 13> player2Check{
			0xba, 0x00, 0x20, 0x00, 0x00, 0x48, 0x8b, 0xce, 0xe8, 0x0d, 0x15, 0x00, 0x00
		};
		std::array<uint8_t, 13> actual{};
		return Read(_addr(0x14000a300), actual) && actual == player1Check
			&& Read(_addr(0x14000a326), actual) && actual == player2Check;
	}

	bool ReadAction(uintptr_t action, ActionState& state)
	{
		return action && Read(action + ActionFlagsOffset, state.flags)
			&& Read(action + RestartOffset, state.restart)
			&& Read(action + ActionStepOffset, state.step)
			&& Read(action + NextActionStepOffset, state.nextStep);
	}

	bool IsFighting(const ActionState& state)
	{
		return state.step == FightingStep && state.nextStep == -1 && !state.restart;
	}

	void ClearReloadFlags(uintptr_t action, uint32_t flags)
	{
		InterlockedAnd(reinterpret_cast<volatile LONG*>(action + ActionFlagsOffset), static_cast<LONG>(~flags));
	}

	bool ReadMatch(Match& match)
	{
		match.settings = PointerAt(_addr(0x140d50e58));
		int mode = 0;
		if (!match.settings || !Read(match.settings + 0x34c, mode) || mode != 5)
			return false;

		match.action = PointerAt(_addr(0x140d47e68));
		return match.action && Read(match.settings + SelectionOffset, match.teams);
	}

	bool ReadLoadedTeams(LoadedTeams& teams)
	{
		const auto characters = PointerAt(_addr(0x140d44a70));
		if (!characters)
			return false;
		for (int side = 0; side < 2; ++side)
		{
			const auto chain = PointerAt(characters + (side == 0 ? 0x58 : 0x328));
			if (!chain)
				return false;
			const auto next = PointerAt(chain + 0x10);
			if (!next)
				return false;
			const auto first = PointerAt(chain + 0x8);
			const auto second = PointerAt(next + 0x8);
			const auto third = second ? PointerAt(second + 0x18) : 0;
			const uintptr_t fighters[] = { first, second, third };
			for (int position = 0; position < SlotsPerTeam; ++position)
			{
				const auto fighter = fighters[position];
				auto& loaded = teams[side * SlotsPerTeam + position];
				if (!Read(fighter, loaded.gameClass) || !loaded.gameClass
					|| !Read(fighter + LoadedAssistOffset, loaded.assist)
					|| !Read(fighter + LoadedColorOffset, loaded.color))
					return false;
			}
		}
		return true;
	}

	bool InRoster(int id)
	{
		return id > 0 && id < static_cast<int>(std::size(Roster));
	}

	bool HasAlternateCostume(int id)
	{
		static const auto available = []
		{
			std::array<bool, std::size(Roster)> costumes{};
			wchar_t executable[MAX_PATH]{};
			const auto length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
			if (!length || length >= MAX_PATH)
				return costumes;
			const auto directory = std::filesystem::path(executable).parent_path() / L"nativePCx64/chr/archive";
			for (int fighter = 1; fighter < static_cast<int>(costumes.size()); ++fighter)
			{
				wchar_t name[16];
				swprintf_s(name, L"%04d_%02d.arc", fighter, TrainingShortcut::AlternateColor);
				const auto attributes = GetFileAttributesW((directory / name).c_str());
				costumes[fighter] = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
			}
			return costumes;
		}();
		return InRoster(id) && available[id];
	}

	bool HasAvailableColors(const TrainingShortcut::TeamPicks& picks)
	{
		return std::all_of(picks.begin(), picks.end(), [](const auto& pick)
		{
			return pick.color != TrainingShortcut::AlternateColor || HasAlternateCostume(pick.id);
		});
	}

	bool ValidPresetColor(int color)
	{
		return color == TrainingShortcut::AlternateColor
			|| (color >= TrainingShortcut::AutomaticColor && color < TrainingShortcut::BaseColorCount);
	}

	bool ValidPicks(const TrainingShortcut::TeamPicks& picks)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto pick = picks[slot];
			if (!InRoster(pick.id) || pick.assist < 0 || pick.assist >= IM_ARRAYSIZE(AssistTypes)
				|| !ValidPresetColor(pick.color))
				return false;
			for (int previous = slot / SlotsPerTeam * SlotsPerTeam; previous < slot; ++previous)
			{
				if (picks[previous].id == pick.id)
					return false;
			}
		}
		return true;
	}

	TrainingShortcut::TeamPicks QuickPicks()
	{
		TrainingShortcut::TeamPicks picks{};
		for (int slot = 0; slot < SlotCount; ++slot)
			picks[slot] = { quickDraft[slot].id, quickDraft[slot].assist, quickDraft[slot].color };
		return picks;
	}

	bool ValidPresetName(std::string_view name)
	{
		return !name.empty() && name.size() < PresetNameCapacity && name.find("##") == std::string_view::npos
			&& std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; });
	}

	std::filesystem::path PresetPath(std::string_view name)
	{
		// Encode the UTF-8 name so it is safe to use as a Windows filename.
		constexpr wchar_t hex[] = L"0123456789abcdef";
		std::wstring filename;
		for (unsigned char c : name)
		{
			filename += hex[c >> 4];
			filename += hex[c & 0xf];
		}
		return quickPresetFolder / (filename + L".preset");
	}

	bool ValidQuickExercise(const QuickExercise& exercise)
	{
		return std::all_of(exercise.reactions.begin(), exercise.reactions.end(), [](const DummyReaction& reaction)
		{
			return !reaction.slot && reaction.delay >= 0 && reaction.delay <= MaxDummyReactionDelay
				&& (reaction.move == NoDummyMove || IsDummyMoveValid(reaction));
		});
	}

	bool ReadQuickExercise(std::istream& input, QuickExercise& exercise)
	{
		if (!(input >> exercise.resetPosition) || exercise.resetPosition < 0
			|| exercise.resetPosition >= static_cast<int>(QuickResetChoices.size()))
			return false;
		for (auto& reaction : exercise.reactions)
		{
			std::string notation;
			if (!(input >> reaction.move >> reaction.delay >> std::quoted(notation))
				|| notation.size() >= DummyInputCapacity || notation.find('\0') != std::string::npos
				|| (reaction.move != CustomDummyMove && !notation.empty()))
				return false;
			std::copy(notation.begin(), notation.end(), reaction.customInput.begin());
			reaction.customDraft = reaction.customInput;
		}
		return ValidQuickExercise(exercise);
	}

	bool ReadPreset(const std::filesystem::path& path, QuickPreset& preset)
	{
		std::ifstream input(path, std::ios::binary);
		int version = 0;
		const auto reject = [&](const char* reason)
		{
			Log(0, "presets: read failed version=%d stage=%d reason=%s", version, preset.stage, reason);
			return false;
		};
		if (!input.is_open())
			return reject("open");
		if (!(input >> version >> std::quoted(preset.name) >> preset.stage)
			|| (version != TeamPresetVersion && version != ExercisePresetVersion && version != ColorPresetVersion)
			|| !ValidPresetName(preset.name) || preset.stage < 0 || preset.stage >= TrainingShortcut::StageCount)
			return reject("header");
		for (auto& pick : preset.picks)
		{
			if (!(input >> pick.id >> pick.assist))
				return reject("fighter or assist");
			if (version == ColorPresetVersion && !(input >> pick.color))
				return reject("color");
		}
		int includeExercise = version == ExercisePresetVersion;
		if (version == ColorPresetVersion && (!(input >> includeExercise) || (includeExercise != 0 && includeExercise != 1)))
			return reject("exercise flag");
		if (includeExercise && !ReadQuickExercise(input, preset.exercise.emplace()))
			return reject("exercise settings");
		if (!(input >> std::ws).eof())
			return reject("trailing data");
		return ValidPicks(preset.picks) || reject("invalid team selection");
	}

	void WritePreset(std::ostream& output, const QuickPreset& preset)
	{
		output << ColorPresetVersion << ' ' << std::quoted(preset.name) << ' ' << preset.stage;
		for (const auto& pick : preset.picks)
			output << ' ' << pick.id << ' ' << pick.assist << ' ' << pick.color;
		output << ' ' << static_cast<int>(preset.exercise.has_value());
		if (preset.exercise)
		{
			output << ' ' << preset.exercise->resetPosition;
			for (const auto& reaction : preset.exercise->reactions)
				output << ' ' << reaction.move << ' ' << reaction.delay << ' '
					<< std::quoted(reaction.move == CustomDummyMove ? reaction.customInput.data() : "");
		}
		output << '\n';
	}

	void RefreshQuickPresets()
	{
		quickPresets.clear();
		wchar_t localAppData[MAX_PATH]{};
		const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
		if (!length || length >= MAX_PATH)
		{
			quickPresetFolder.clear();
			quickPresetStatus = "PRESET_DIR_MISSING";
			Log(0, "presets: local app data unavailable (Windows error %lu)",
				length ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
			return;
		}
		quickPresetFolder = std::filesystem::path(localAppData) / L"UMVC3TrainingToolsInt" / L"QuickTraining";
		std::error_code error;
		std::filesystem::create_directories(quickPresetFolder, error);
		if (error)
		{
			quickPresetStatus = "PRESET_DIR_OPEN";
			Log(0, "presets: create directory failed error=%d", error.value());
			return;
		}

		quickPresetStatus = "";
		const std::filesystem::directory_iterator end;
		unsigned rejected = 0;
		for (std::filesystem::directory_iterator entry(quickPresetFolder, error); !error && entry != end; entry.increment(error))
		{
			const auto& path = entry->path();
			if (path.extension() != L".preset")
				continue;
			QuickPreset preset;
			const bool read = ReadPreset(path, preset);
			if (read && path.filename() == PresetPath(preset.name).filename())
				quickPresets.push_back(std::move(preset));
			else
			{
				if (read)
					Log(0, "presets: file name does not match preset identifier");
				++rejected;
				quickPresetStatus = "PRESET_READ_FAILED";
			}
		}
		if (error)
			quickPresetStatus = "PRESET_LIST_FAILED";
		Log(0, "presets: list complete loaded=%zu rejected=%u error=%d", quickPresets.size(), rejected, error.value());
		std::sort(quickPresets.begin(), quickPresets.end(), [](const QuickPreset& left, const QuickPreset& right)
		{
			return left.name < right.name;
		});
		if (selectedQuickPreset && std::none_of(quickPresets.begin(), quickPresets.end(), [](const QuickPreset& preset)
			{ return preset.name == selectedQuickPreset->name; }))
			selectedQuickPreset.reset();
	}

	std::unique_ptr<void, decltype(&CloseHandle)> LockQuickPresets()
	{
		HANDLE file = INVALID_HANDLE_VALUE;
		if (!quickPresetFolder.empty())
		{
			const auto path = quickPresetFolder / L".edit.lock";
			file = CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, OPEN_ALWAYS,
				FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		}
		if (file == INVALID_HANDLE_VALUE)
		{
			Log(0, "presets: edit lock unavailable error=%lu", quickPresetFolder.empty() ? ERROR_PATH_NOT_FOUND : GetLastError());
			quickPresetStatus = "PRESET_EDIT_FAILED";
		}
		return { file == INVALID_HANDLE_VALUE ? nullptr : file, CloseHandle };
	}

	bool CheckSelectedQuickPreset(const std::filesystem::path& path)
	{
		QuickPreset current;
		if (ReadPreset(path, current) && current == *selectedQuickPreset)
			return true;
		RefreshQuickPresets();
		quickPresetStatus = "PRESET_CHANGED";
		Log(0, "presets: edit cancelled because the selected file changed");
		return false;
	}

	void LoadQuickPresetDraft(const QuickPreset& preset)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
			quickDraft[slot] = { slot, preset.picks[slot].id, preset.picks[slot].assist, preset.picks[slot].color };
		quickStage = preset.stage;
		useQuickExercise = preset.exercise.has_value();
		quickExercise = preset.exercise.value_or(QuickExercise{});
		quickSetupStatus = "";
	}

	void SaveQuickPreset(QuickPreset preset, bool update, bool fromMatch = false)
	{
		Log(0, "presets: %s requested source=%s stage=%d exercise=%d", update ? "update" : "save",
			fromMatch ? "training tab" : "F4", preset.stage, preset.exercise.has_value());
		std::string_view name = quickPresetName;
		const auto first = name.find_first_not_of(' ');
		if (first != std::string_view::npos)
			name = name.substr(first, name.find_last_not_of(' ') - first + 1);
		else
			name = {};
		if (!ValidPresetName(name))
		{
			quickPresetStatus = "PRESET_NAME_INVALID";
			Log(0, "presets: save rejected reason=invalid name");
			return;
		}
		preset.name = name;
		if (!ValidPicks(preset.picks))
		{
			quickPresetStatus = "QUICK_COMPLETE_TEAMS";
			Log(0, "presets: save rejected reason=invalid team selection");
			return;
		}
		if (preset.exercise)
		{
			if (!ValidQuickExercise(*preset.exercise))
			{
				quickPresetStatus = "QUICK_EXERCISE_INVALID";
				Log(0, "presets: save rejected reason=invalid exercise");
				return;
			}
			for (auto& reaction : preset.exercise->reactions)
			{
				const auto terminator = std::find(reaction.customInput.begin(), reaction.customInput.end(), '\0');
				std::fill(terminator, reaction.customInput.end(), '\0');
			}
		}

		const auto lock = LockQuickPresets();
		if (!lock || (update && !selectedQuickPreset))
			return;
		const auto previous = update ? PresetPath(selectedQuickPreset->name) : std::filesystem::path{};
		if (update && !CheckSelectedQuickPreset(previous))
			return;

		const auto path = PresetPath(preset.name);
		const bool rename = update && path != previous;
		const DWORD flags = MOVEFILE_WRITE_THROUGH | (update && !rename ? MOVEFILE_REPLACE_EXISTING : 0);
		wchar_t temporary[MAX_PATH]{};
		if (!GetTempFileNameW(quickPresetFolder.c_str(), L"QTP", 0, temporary))
		{
			Log(0, "presets: temporary file creation failed error=%lu", GetLastError());
			quickPresetStatus = "PRESET_SAVE_FAILED";
			return;
		}
		std::ofstream output(temporary, std::ios::binary);
		WritePreset(output, preset);
		output.close();
		if (output && MoveFileExW(temporary, path.c_str(), flags))
		{
			if (rename && !DeleteFileW(previous.c_str()))
			{
				const auto error = GetLastError();
				const bool restored = DeleteFileW(path.c_str()) != FALSE;
				Log(0, "presets: rename failed error=%lu restored=%d", error, restored);
				RefreshQuickPresets();
				quickPresetStatus = restored ? "PRESET_RENAME_RESTORED"
					: "PRESET_RENAME_DUPLICATE";
				return;
			}
			selectedQuickPreset = preset;
			if (fromMatch)
				LoadQuickPresetDraft(preset);
			std::snprintf(quickPresetName, sizeof(quickPresetName), "%s", preset.name.c_str());
			RefreshQuickPresets();
			if (!*quickPresetStatus)
				quickPresetStatus = update ? "PRESET_UPDATED" : "PRESET_SAVED";
			Log(0, "presets: %s complete version=%d renamed=%d", update ? "update" : "save", ColorPresetVersion, rename);
			return;
		}

		const auto error = output ? GetLastError() : ERROR_WRITE_FAULT;
		Log(0, "presets: save failed error=%lu write_complete=%d", error, static_cast<bool>(output));
		DeleteFileW(temporary);
		quickPresetStatus = error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
			? "PRESET_DUPLICATE" : "PRESET_SAVE_FAILED";
	}

	void DeleteQuickPreset()
	{
		const auto lock = LockQuickPresets();
		if (!lock || !selectedQuickPreset)
			return;
		const auto path = PresetPath(selectedQuickPreset->name);
		if (!CheckSelectedQuickPreset(path))
			return;
		if (!DeleteFileW(path.c_str()))
		{
			Log(0, "presets: delete failed error=%lu", GetLastError());
			quickPresetStatus = "PRESET_DELETE_FAILED";
			return;
		}
		selectedQuickPreset.reset();
		quickPresetName[0] = '\0';
		RefreshQuickPresets();
		if (!*quickPresetStatus)
			quickPresetStatus = "PRESET_DELETED";
		Log(0, "presets: delete complete");
	}

	bool DrawQuickPresetSelector(const char* source)
	{
		bool changed = false;
		ImGui::BeginDisabled(quickPresets.empty());
		const char* preview = selectedQuickPreset ? selectedQuickPreset->name.c_str() : UiText("PRESET_LOAD");
		if (ImGui::BeginCombo("##QuickPreset", preview))
		{
			for (const auto& preset : quickPresets)
			{
				const bool selected = selectedQuickPreset && preset.name == selectedQuickPreset->name;
				if (ImGui::Selectable(preset.name.c_str(), selected))
				{
					selectedQuickPreset = preset;
					std::snprintf(quickPresetName, sizeof(quickPresetName), "%s", preset.name.c_str());
					LoadQuickPresetDraft(preset);
					quickPresetStatus = "PRESET_LOADED";
					Log(0, "presets: selected source=%s stage=%d exercise=%d", source, preset.stage, preset.exercise.has_value());
					changed = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		return changed;
	}

	template <class SavePreset>
	void DrawQuickPresetActions(SavePreset savePreset, bool canSave = true)
	{
		ImGui::InputTextWithHint("##QuickPresetName", UiText("PRESET_NAME"), quickPresetName, sizeof(quickPresetName));
		ImGui::BeginDisabled(!canSave);
		if (ImGui::Button(selectedQuickPreset ? UiText("PRESET_SAVE_NEW") : UiText("PRESET_SAVE")))
			savePreset(false);
		if (selectedQuickPreset)
		{
			ImGui::SameLine();
			if (ImGui::Button(UiText("PRESET_UPDATE")))
				savePreset(true);
		}
		ImGui::EndDisabled();
		if (selectedQuickPreset)
		{
			ImGui::SameLine();
			if (ImGui::Button(UiText("PRESET_DELETE")))
				ImGui::OpenPopup(Localizer::UiLabel("PRESET_DELETE_TITLE").c_str());
		}
		if (ImGui::BeginPopupModal(Localizer::UiLabel("PRESET_DELETE_TITLE").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!selectedQuickPreset)
				ImGui::CloseCurrentPopup();
			else
			{
				ImGui::Text(UiText("PRESET_DELETE_QUESTION"), selectedQuickPreset->name.c_str());
				if (ImGui::Button(UiText("UI_CANCEL")) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)
					|| ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
					ImGui::CloseCurrentPopup();
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button(UiText("PRESET_DELETE_CONFIRM")))
				{
					DeleteQuickPreset();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}
		if (*quickPresetStatus)
			ImGui::TextWrapped("%s", UiText(quickPresetStatus));
	}

	bool ValidSelection(const Teams& teams)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto& fighter = teams[slot];
			if (fighter.side != static_cast<uint32_t>(slot / SlotsPerTeam)
				|| !InRoster(fighter.id) || fighter.classId != ClassId(Roster[fighter.id].gameClass))
				return false;
		}
		return true;
	}

	bool SameSelection(const Teams& left, const Teams& right)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			if (left[slot].side != right[slot].side || left[slot].id != right[slot].id
				|| left[slot].classId != right[slot].classId || left[slot].assist != right[slot].assist
				|| left[slot].color != right[slot].color)
				return false;
		}
		return true;
	}

	void LogTeams(unsigned attempt, const char* label, const Teams& teams)
	{
		Log(attempt, "%s (fighter/assist/color) P1=[%d/%d/%d,%d/%d/%d,%d/%d/%d] P2=[%d/%d/%d,%d/%d/%d,%d/%d/%d]", label,
			teams[0].id, teams[0].assist, teams[0].color, teams[1].id, teams[1].assist, teams[1].color,
			teams[2].id, teams[2].assist, teams[2].color, teams[3].id, teams[3].assist, teams[3].color,
			teams[4].id, teams[4].assist, teams[4].color, teams[5].id, teams[5].assist, teams[5].color);
	}

	void LogClasses(unsigned attempt, const char* label, const Classes& classes)
	{
		const auto base = static_cast<uintptr_t>(GetUMVC3EntryPoint());
		auto offset = [base](uintptr_t address) { return static_cast<unsigned long long>(address ? address - base : 0); };
		Log(attempt, "%s P1=[%llX,%llX,%llX] P2=[%llX,%llX,%llX]", label,
			offset(classes[0]), offset(classes[1]), offset(classes[2]), offset(classes[3]), offset(classes[4]), offset(classes[5]));
	}

	void LogLoadedTeams(unsigned attempt, const char* label, const LoadedTeams& teams)
	{
		Classes classes{};
		for (int slot = 0; slot < SlotCount; ++slot)
			classes[slot] = teams[slot].gameClass;
		LogClasses(attempt, label, classes);
		Log(attempt, "%s assists P1=[%d,%d,%d] P2=[%d,%d,%d]", label,
			teams[0].assist, teams[1].assist, teams[2].assist, teams[3].assist, teams[4].assist, teams[5].assist);
		Log(attempt, "%s colors P1=[%d,%d,%d] P2=[%d,%d,%d]", label,
			teams[0].color, teams[1].color, teams[2].color, teams[3].color, teams[4].color, teams[5].color);
	}

	DWORD WINAPI WatchRestart(void* argument)
	{
		// Sample independently so a blocked render loop cannot hide the restart state.
		const auto attempt = static_cast<unsigned>(reinterpret_cast<uintptr_t>(argument));
		const auto started = GetTickCount64();
		while (watchedAttempt.load() == attempt)
		{
			const auto lastUpdate = lastEditorUpdate.load();
			const auto now = GetTickCount64();
			const auto settings = PointerAt(_addr(0x140d50e58));
			const auto action = PointerAt(_addr(0x140d47e68));
			int mode = -1;
			Read(settings ? settings + 0x34c : 0, mode);
			ActionState actionState;
			const bool haveAction = ReadAction(action, actionState);
			const auto characters = PointerAt(_addr(0x140d44a70));
			int loadCountdown = -1;
			if (characters)
				Read(characters + CharacterReloadCountdownOffset, loadCountdown);
			Log(attempt, "watch +%llu ms mode=%d settings=%p action=%p restart=%d step=%d next=%d flags=0x%08X reload_countdown=%d editor_update_age_ms=%llu",
				now - started, mode, reinterpret_cast<void*>(settings), reinterpret_cast<void*>(action),
				haveAction ? static_cast<int>(actionState.restart) : -1, actionState.step,
				actionState.nextStep, actionState.flags, loadCountdown, now - lastUpdate);

			Teams selected{};
			if (mode == 5 && Read(settings + SelectionOffset, selected))
				LogTeams(attempt, "watch selection", selected);
			LoadedTeams loaded{};
			if (mode == 5 && ReadLoadedTeams(loaded))
				LogLoadedTeams(attempt, "watch loaded", loaded);
			else
				Log(attempt, "watch loaded classes unavailable");

			if (now - started >= RestartTimeoutMs + WatchIntervalMs)
				break;
			Sleep(WatchIntervalMs);
		}
		Log(attempt, "watch finished");
		return 0;
	}

	void DrawColorChoice(DraftSlot& fighter, bool allowAutomatic)
	{
		auto& color = fighter.color;
		const bool hasAlternate = HasAlternateCostume(fighter.id);
		const char* automaticLabel = UiText("TEAM_COLOR_AUTO");
		auto preview = color >= 0 && color < TrainingShortcut::BaseColorCount
			? std::to_string(color + 1) : std::string(allowAutomatic ? automaticLabel : UiText("TEAM_COLOR_CURRENT"));
		if (color == TrainingShortcut::AlternateColor)
			preview = UiText(hasAlternate ? "TEAM_COLOR_ALT" : "TEAM_COLOR_ALT_MISSING");
		if (ImGui::BeginCombo(UiText("TEAM_COLOR"), preview.c_str()))
		{
			if (allowAutomatic && ImGui::Selectable(automaticLabel, color == TrainingShortcut::AutomaticColor))
				color = TrainingShortcut::AutomaticColor;
			for (int index = 0; index < TrainingShortcut::BaseColorCount; ++index)
			{
				const bool selected = index == color;
				if (ImGui::Selectable(std::to_string(index + 1).c_str(), selected))
					color = index;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			if (hasAlternate)
			{
				const bool selected = color == TrainingShortcut::AlternateColor;
				if (ImGui::Selectable(UiText("TEAM_COLOR_ALT"), selected))
					color = TrainingShortcut::AlternateColor;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	void DrawResetChoice(int& position)
	{
		if (!ImGui::BeginCombo(UiText("QUICK_RESET_POSITION"), UiText(QuickResetChoices[position].first)))
			return;
		for (int index = 0; index < static_cast<int>(QuickResetChoices.size()); ++index)
		{
			const bool selected = index == position;
			if (ImGui::Selectable(UiText(QuickResetChoices[index].first), selected))
				position = index;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	void DrawFighterOptions(DraftSlot& fighter, bool quickSetup)
	{
		if (ImGui::BeginCombo(UiText("TEAM_FIGHTER"), Roster[fighter.id].name))
		{
			for (int id : FighterDisplayOrder)
			{
				const bool selected = id == fighter.id;
				if (ImGui::Selectable(Roster[id].name, selected) && !selected)
				{
					fighter.id = id;
					fighter.color = 0;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		const auto& names = Roster[fighter.id].assists;
		const bool knownAssist = fighter.assist >= 0 && fighter.assist < IM_ARRAYSIZE(AssistTypes);
		if (ImGui::BeginCombo(UiText("TEAM_ASSIST"), knownAssist ? names[fighter.assist] : UiText("TEAM_UNKNOWN_ASSIST")))
		{
			for (int type = 0; type < IM_ARRAYSIZE(AssistTypes); ++type)
			{
				char option[64];
				std::snprintf(option, sizeof(option), "%s - %s", UiText(AssistTypes[type]), names[type]);
				const bool selected = type == fighter.assist;
				if (ImGui::Selectable(option, selected))
					fighter.assist = type;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		DrawColorChoice(fighter, quickSetup);
	}

	void DrawTeamDraft(std::array<DraftSlot, SlotCount>& slots, bool quickSetup = false)
	{
		const float buttonGap = ImGui::GetStyle().ItemInnerSpacing.x;
		const float orderWidth = 2.0f * ImGui::GetFrameHeight() + buttonGap;
		for (int first = 0; first < SlotCount; first += SlotsPerTeam)
		{
			ImGui::SeparatorText(UiText(first == 0 ? "TEAM_PLAYER1" : "TEAM_PLAYER2"));
			ImGui::PushID(first);
			if (ImGui::BeginTable("##TeamList", 2))
			{
				ImGui::TableSetupColumn("##Fighter", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("##Order", ImGuiTableColumnFlags_WidthFixed, orderWidth);
				int moveFrom = first;
				int moveTo = first;
				for (int position = 0; position < SlotsPerTeam; ++position)
				{
					const int slot = first + position;
					auto& fighter = slots[slot];
					ImGui::PushID(fighter.source);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const bool expanded = ImGui::TreeNodeEx("##FighterDetails",
						ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding,
						"%s", Roster[fighter.id].name);
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(position == 0);
					if (ImGui::ArrowButton("Up", ImGuiDir_Up))
					{
						moveFrom = slot;
						moveTo = slot - 1;
					}
					ImGui::EndDisabled();
					ImGui::SameLine(0.0f, buttonGap);
					ImGui::BeginDisabled(position == SlotsPerTeam - 1);
					if (ImGui::ArrowButton("Down", ImGuiDir_Down))
					{
						moveFrom = slot;
						moveTo = slot + 1;
					}
					ImGui::EndDisabled();
					if (expanded)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Indent();
						DrawFighterOptions(fighter, quickSetup);
						ImGui::Unindent();
					}
					ImGui::PopID();
				}
				ImGui::EndTable();
				if (moveFrom != moveTo)
					std::swap(slots[moveFrom], slots[moveTo]);
			}
			ImGui::PopID();
		}
	}

	void ResetDraft(const Match& match)
	{
		draftFrom = match;
		for (int slot = 0; slot < SlotCount; ++slot)
			draft[slot] = { slot, match.teams[slot].id, match.teams[slot].assist, match.teams[slot].color };
	}

	std::array<DraftSlot, SlotCount> DraftFromPreset(const Match& match, const QuickPreset& preset)
	{
		std::array<DraftSlot, SlotCount> selection{};
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto& pick = preset.picks[slot];
			const int first = slot / SlotsPerTeam * SlotsPerTeam;
			const auto begin = match.teams.begin() + first;
			const auto end = begin + SlotsPerTeam;
			const auto existing = std::find_if(begin, end, [&](const Selection& fighter) { return fighter.id == pick.id; });
			const int source = existing == end ? slot : static_cast<int>(existing - match.teams.begin());
			const int color = pick.color != TrainingShortcut::AutomaticColor ? pick.color
				: (existing == end ? 0 : existing->color);
			selection[slot] = { source, pick.id, pick.assist, color };
		}
		return selection;
	}

	void ApplyExercise(uintptr_t action, const QuickExercise& exercise)
	{
		quickExerciseAction = action;
		quickExerciseReset = QuickResetChoices[exercise.resetPosition].second;
		{
			const std::lock_guard lock(RecordingMutex);
			for (size_t index = 0; index < DummyReactionSettings.size(); ++index)
			{
				auto& reaction = *DummyReactionSettings[index];
				reaction = exercise.reactions[index];
				reaction.customDraft = reaction.customInput;
			}
		}
		Log(0, "exercise: applied position=%d reactions (move/delay) hit=%d/%d block=%d/%d wakeup=%d/%d incoming=%d/%d",
			exercise.resetPosition, exercise.reactions[0].move, exercise.reactions[0].delay, exercise.reactions[1].move,
			exercise.reactions[1].delay, exercise.reactions[2].move, exercise.reactions[2].delay,
			exercise.reactions[3].move, exercise.reactions[3].delay);
	}

	void Apply(const Match& shown, const std::array<DraftSlot, SlotCount>& selection, const QuickPreset* preset = nullptr)
	{
		const unsigned attempt = ++attemptCount;
		Log(attempt, "apply requested source=%s preset_stage=%d exercise=%d", preset ? "preset tab" : "team editor",
			preset ? preset->stage : -1, preset && preset->exercise.has_value());
		if (recording || recordingP1 || recordingP2 || replaying || replayingP1 || replayingP2 || IsDummyControllerRecording())
		{
			status = "TEAM_STOP_RECORDING";
			Log(attempt, "blocked by active recording or playback");
			return;
		}

		Match current;
		LoadedTeams loaded{};
		ActionState action;
		if (!ReadMatch(current) || current.settings != shown.settings || current.action != shown.action
			|| !SameSelection(current.teams, shown.teams) || !ReadLoadedTeams(loaded)
			|| !ReadAction(current.action, action) || !IsFighting(action) || (action.flags & ReloadTeams))
		{
			status = "TEAM_WAIT";
			Log(attempt, "match preflight failed settings=%p action=%p restart=%u step=%d next=%d flags=0x%08X",
				reinterpret_cast<void*>(current.settings), reinterpret_cast<void*>(current.action),
				action.restart, action.step, action.nextStep, action.flags);
			return;
		}

		Teams proposed{};
		Classes expected{};
		uint32_t reloadFlags = 0;
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto edit = selection[slot];
			const int first = (slot / SlotsPerTeam) * SlotsPerTeam;
			for (int other = first; other < slot; ++other)
			{
				if (edit.id == selection[other].id)
				{
					status = "TEAM_DISTINCT";
					Log(attempt, "duplicate fighter id=%d side=%d", edit.id, slot / SlotsPerTeam + 1);
					return;
				}
			}
			proposed[slot] = current.teams[edit.source];
			proposed[slot].id = edit.id;
			proposed[slot].classId = ClassId(Roster[edit.id].gameClass);
			proposed[slot].assist = edit.assist;
			proposed[slot].color = edit.color;
			if (proposed[slot] != current.teams[slot])
				reloadFlags |= slot < SlotsPerTeam ? ReloadPlayer1 : ReloadPlayer2;
			for (int original = 0; original < SlotCount; ++original)
			{
				if (current.teams[original].id == edit.id)
				{
					expected[slot] = loaded[original].gameClass;
					break;
				}
			}
		}

		if (!reloadFlags)
		{
			Log(attempt, "team selection unchanged");
			status = "TEAM_NO_CHANGE";
			if (preset)
			{
				status = "PRESET_APPLIED";
				if (preset->exercise)
				{
					const auto& exercise = *preset->exercise;
					if (TrainingTeamEditor::RequestTrainingRestart(QuickResetChoices[exercise.resetPosition].second))
						ApplyExercise(current.action, exercise);
					else
						status = "TEAM_WAIT";
				}
			}
			return;
		}
		if (!SupportsTeamReload())
		{
			status = "TEAM_UNAVAILABLE";
			Log(attempt, "native reload signature mismatch");
			return;
		}

		LogTeams(attempt, "before", current.teams);
		LogTeams(attempt, "requested", proposed);
		LogLoadedTeams(attempt, "before loaded", loaded);
		LogClasses(attempt, "expected class RVAs (0=unknown)", expected);
		const auto selectionAddress = current.settings + SelectionOffset;
		const bool wroteSelection = Write(selectionAddress, proposed);
		const auto selectionError = wroteSelection ? 0 : GetLastError();
		uint32_t previousFlags = 0;
		if (wroteSelection)
			previousFlags = static_cast<uint32_t>(InterlockedOr(
				reinterpret_cast<volatile LONG*>(current.action + ActionFlagsOffset), static_cast<LONG>(reloadFlags)));
		const bool wroteRestart = wroteSelection && Write(current.action + RestartOffset, RestartTraining);
		const auto restartError = wroteRestart || !wroteSelection ? 0 : GetLastError();
		if (wroteSelection)
			Log(attempt, "requested native reload flags=0x%08X previous action flags=0x%08X", reloadFlags, previousFlags);
		Log(attempt, "write selection=%d error=%lu; write restart=%d error=%lu", wroteSelection, selectionError,
			wroteRestart, restartError);
		if (!wroteRestart)
		{
			if (wroteSelection)
				ClearReloadFlags(current.action, reloadFlags);
			const bool restored = Write(selectionAddress, current.teams);
			Log(attempt, "restore after failed write=%d", restored);
			status = restored ? "TEAM_RESTORE_OK"
				: "TEAM_RESTORE_FAILED";
			if (!restored)
			{
				trial.before = current;
				trial.uncertain = true;
			}
			return;
		}

		trial = { current, proposed, loaded, expected, GetTickCount64(), true, false, attempt, reloadFlags };
		if (preset)
		{
			trial.successStatus = "PRESET_APPLIED";
			trial.exercise = preset->exercise;
		}
		status = "TEAM_RELOAD_WAIT";
		if (logAvailable)
		{
			lastEditorUpdate.store(trial.started);
			watchedAttempt.store(attempt);
			const HANDLE watcher = CreateThread(nullptr, 0, WatchRestart, reinterpret_cast<void*>(static_cast<uintptr_t>(attempt)), 0, nullptr);
			if (watcher)
				CloseHandle(watcher);
			else
			{
				const auto error = GetLastError();
				watchedAttempt.store(0);
				Log(attempt, "could not start restart watcher error=%lu", error);
			}
		}
	}

	void BeginRestart(const Match& match, std::optional<TrainingTeamEditor::XPositions> positions, const char* source,
		bool initialPlacement = false)
	{
		pendingRestart = { match.settings, match.action, 0, positions, GetTickCount64(), false, initialPlacement };
		const auto target = positions.value_or(TrainingTeamEditor::XPositions{ p1Pos, p2Pos });
		Log(0, "restart: started source=%s target_p1=%.1f target_p2=%.1f", source, target.p1, target.p2);
	}

	void LogRestartWait(const char* reason)
	{
		if (!pendingRestart.waitLogged && GetTickCount64() - pendingRestart.started >= 2000)
		{
			pendingRestart.waitLogged = true;
			Log(0, "restart: still waiting reason=%s", reason);
		}
	}
}

void TrainingTeamEditor::OpenQuickSetup()
{
	quickSetupStatus = "";
	RefreshQuickPresets();
}

bool TrainingTeamEditor::DrawQuickSetup()
{
	ImGui::SeparatorText(UiText("QUICK_TITLE"));
	DrawQuickPresetSelector("F4");
	ImGui::Combo(UiText("QUICK_STAGE"), &quickStage, Stages.data(), static_cast<int>(Stages.size()));
	DrawTeamDraft(quickDraft, true);
	ImGui::Checkbox(UiText("QUICK_EXERCISE"), &useQuickExercise);
	if (useQuickExercise && ImGui::TreeNode(Localizer::UiLabel("QUICK_EXERCISE_SETTINGS").c_str()))
	{
		DrawResetChoice(quickExercise.resetPosition);
		for (size_t index = 0; index < QuickReactionLabels.size(); ++index)
			DrawDummyReaction(QuickReactionLabels[index], quickExercise.reactions[index], false);
		ImGui::TreePop();
	}
	DrawQuickPresetActions([](bool update)
	{
		SaveQuickPreset({ {}, QuickPicks(), quickStage, useQuickExercise ? std::optional{ quickExercise } : std::nullopt }, update);
	});
	if (TrainingShortcut::CanLaunchDirectly())
	{
		if (TrainingShortcut::CanLaunchViaNativeMenus())
			ImGui::Checkbox(UiText("QUICK_DIRECT_LAUNCH"), &quickDirectLaunch);
		else
			ImGui::TextWrapped("%s", UiText("QUICK_DIRECT_ONLY"));
	}
	if (*quickSetupStatus)
		ImGui::TextWrapped("%s", UiText(quickSetupStatus));

	if (ImGui::Button(UiText("QUICK_START")))
	{
		const auto picks = QuickPicks();
		if (!ValidPicks(picks))
		{
			TraceQuickShortcut("Start Training rejected: invalid fighter or assist selection");
			quickSetupStatus = "QUICK_COMPLETE_TEAMS";
		}
		else if (useQuickExercise && !ValidQuickExercise(quickExercise))
		{
			quickSetupStatus = "QUICK_EXERCISE_INVALID";
			Log(0, "quick training: launch rejected reason=invalid exercise");
		}
		else if (!HasAvailableColors(picks))
		{
			quickSetupStatus = "QUICK_ALT_UNAVAILABLE";
			Log(0, "quick training: launch rejected reason=alternate costume missing");
		}
		else if (TrainingShortcut::StartTraining(picks, quickStage, quickDirectLaunch))
		{
			queuedExercise = useQuickExercise ? std::optional{ quickExercise } : std::nullopt;
			return true;
		}
		else
			quickSetupStatus = "QUICK_RETURN_MAIN";
	}
	ImGui::SameLine();
	return ImGui::Button(UiText("UI_CANCEL"));
}

void TrainingTeamEditor::ApplyQuickExercise(uintptr_t action)
{
	const auto exercise = std::exchange(queuedExercise, std::nullopt);
	if (exercise)
	{
		ApplyExercise(action, *exercise);
		Match match;
		if (ReadMatch(match) && match.action == action)
			BeginRestart(match, quickExerciseReset, "quick exercise initial position", true);
		TraceQuickShortcut("exercise applied: reset position and dummy reactions ready");
	}
	else
		quickExerciseAction = 0;
}

std::optional<TrainingTeamEditor::XPositions> TrainingTeamEditor::QuickResetPositions()
{
	if (quickExerciseAction && quickExerciseAction == GetFightingAction())
		return quickExerciseReset;
	return std::nullopt;
}

bool TrainingTeamEditor::RequestTrainingRestart(std::optional<XPositions> positions)
{
	Match match{};
	ActionState action{};
	if (!ReadMatch(match) || !ReadAction(match.action, action) || !IsFighting(action) || (action.flags & ReloadTeams))
	{
		Log(0, "restart: rejected step=%d next=%d restart=%u flags=0x%08X", action.step, action.nextStep, action.restart, action.flags);
		return false;
	}
	if (!Write(match.action + RestartOffset, RestartTraining))
	{
		Log(0, "restart: request write failed error=%lu", GetLastError());
		return false;
	}

	if (!positions && quickExerciseAction == match.action)
		positions = quickExerciseReset;
	BeginRestart(match, positions, "tool request");
	return true;
}

void TrainingTeamEditor::UpdateTrainingRestart(bool watchNativeRestart)
{
	if (!watchNativeRestart && !pendingRestart.action && !quickExerciseAction)
		return;

	Match match{};
	ActionState action{};
	if (!ReadMatch(match) || !ReadAction(match.action, action))
	{
		if (pendingRestart.action)
			Log(0, "restart: cancelled because the match is unavailable");
		pendingRestart = {};
		quickExerciseAction = 0;
		return;
	}
	if (quickExerciseAction != match.action)
		quickExerciseAction = 0;
	if (pendingRestart.action && (pendingRestart.action != match.action || pendingRestart.settings != match.settings))
	{
		Log(0, "restart: cancelled because the match changed");
		pendingRestart = {};
	}
	if ((watchNativeRestart || quickExerciseAction) && action.restart && !pendingRestart.action)
	{
		BeginRestart(match, quickExerciseAction ? std::optional{ quickExerciseReset } : std::nullopt, "native game");
	}
	if (!pendingRestart.action)
		return;

	if (!IsFighting(action) || (action.flags & ReloadTeams))
	{
		pendingRestart.readyAt = 0;
		LogRestartWait("battle restart or team reload in progress");
		return;
	}

	const auto now = GetTickCount64();
	if (now < pendingRestart.readyAt)
		return;

	LoadedTeams loaded{};
	if (!ReadLoadedTeams(loaded) || !CheckTheMode())
	{
		LogRestartWait("teams not ready");
		return;
	}
	GetMainPointers();
	if (sAction != match.action || sBattleSetting != match.settings
		|| !Player1TeamTable || !Player2TeamTable || !PointerToGameSpeedA)
	{
		LogRestartWait("battle settings not ready");
		return;
	}
	GetPlayerData();
	if (!P1Character1Data || !P1Character2Data || !P1Character3Data
		|| !P2Character1Data || !P2Character2Data || !P2Character3Data)
	{
		LogRestartWait("fighter data not ready");
		return;
	}

	if (!pendingRestart.readyAt)
	{
		const auto positions = pendingRestart.positions.value_or(XPositions{ p1Pos, p2Pos });
		*(float*)(P1Character1Data + 0x50) = positions.p1;
		*(float*)(P1Character2Data + 0x50) = positions.p1;
		*(float*)(P1Character3Data + 0x50) = positions.p1;
		*(float*)(P2Character1Data + 0x50) = positions.p2;
		*(float*)(P2Character2Data + 0x50) = positions.p2;
		*(float*)(P2Character3Data + 0x50) = positions.p2;
		Log(0, "restart: positions applied p1=%.1f p2=%.1f initial_placement=%d", positions.p1, positions.p2,
			pendingRestart.initialPlacement);
		if (pendingRestart.initialPlacement)
		{
			pendingRestart = {};
			return;
		}
		// Only the remaining settings need the settle delay.
		pendingRestart.readyAt = now + RestartSettleMs;
		return;
	}

	const auto elapsed = now - pendingRestart.started;
	pendingRestart = {};
	RestartWithChanges();
	SetIndividualCharacterHealth();
	SetMeters();
	GetDebugData();
	KOToggles();
	DeathDelay();
	Log(0, "restart: complete elapsed_ms=%llu", elapsed);
}

void TrainingTeamEditor::TraceQuickShortcut(const char* format, ...)
{
	char message[320];
	va_list args;
	va_start(args, format);
	vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
	va_end(args);
	Log(0, "quick training: %s", message);
}

void TrainingTeamEditor::TraceQuickSelection(int p1Phase, int p2Phase, int mode)
{
	Log(0, "quick training: selector phases P1=%d P2=%d mode=%d", p1Phase, p2Phase, mode);
}

void TrainingTeamEditor::TraceWindowRenderer(const char* operation, HRESULT result)
{
	Log(0, "tool window: %s (HRESULT=0x%08lX)", operation, static_cast<unsigned long>(result));
}

void TrainingTeamEditor::TraceRecording(const char* format, ...)
{
	AcquireSRWLockExclusive(&recordingLogLock);
	if (recordingLogSize < recordingLog.size())
	{
		auto& line = recordingLog[recordingLogSize++];
		va_list args;
		va_start(args, format);
		vsnprintf_s(line.data(), line.size(), _TRUNCATE, format, args);
		va_end(args);
	}
	else
	{
		++droppedRecordingLogs;
	}
	ReleaseSRWLockExclusive(&recordingLogLock);
}

void TrainingTeamEditor::FlushRecordingLog()
{
	static std::array<RecordingLogLine, RecordingLogCapacity> lines;
	AcquireSRWLockExclusive(&recordingLogLock);
	const auto count = recordingLogSize;
	std::copy_n(recordingLog.begin(), count, lines.begin());
	const auto dropped = droppedRecordingLogs;
	recordingLogSize = 0;
	droppedRecordingLogs = 0;
	ReleaseSRWLockExclusive(&recordingLogLock);

	if ((!count && !dropped) || recordingLogPath.empty())
		return;

	FILE* file = nullptr;
	static bool openFailed = false;
	const auto error = _wfopen_s(&file, recordingLogPath.c_str(), L"a");
	if (error != 0)
	{
		if (!openFailed)
			Log(0, "recording: could not append diagnostics error=%d", error);
		openFailed = true;
		return;
	}
	if (std::exchange(openFailed, false))
		Log(0, "recording: diagnostics append recovered");

	for (size_t index = 0; index < count; ++index)
	{
		WriteLogTimestamp(file);
		std::fprintf(file, "pid=%lu %s\n", GetCurrentProcessId(), lines[index].data());
	}
	if (dropped)
	{
		WriteLogTimestamp(file);
		std::fprintf(file, "pid=%lu Dropped %u diagnostic messages\n", GetCurrentProcessId(), dropped);
	}
	std::fclose(file);
}

bool TrainingTeamEditor::PrepareQuickMatch(const TrainingShortcut::TeamPicks& picks)
{
	const auto settings = PointerAt(_addr(0x140d50e58));
	Teams current{};
	// The selector and the direct launch check their respective game states before staging teams.
	const bool selectionReady = settings && Read(settings + SelectionOffset, current);
	const bool validPicks = ValidPicks(picks);
	const bool availableColors = validPicks && HasAvailableColors(picks);
	if (!selectionReady || !validPicks || !availableColors)
	{
		Log(0, "quick training: selection preflight failed selection_ready=%d valid_teams=%d available_colors=%d",
			selectionReady, validPicks, availableColors);
		return false;
	}

	auto proposed = current;
	for (int slot = 0; slot < SlotCount; ++slot)
	{
		if (picks[slot].color != TrainingShortcut::AutomaticColor)
			proposed[slot].color = picks[slot].color;
		else if (proposed[slot].id != picks[slot].id)
			proposed[slot].color = 0;
		proposed[slot].side = slot / SlotsPerTeam;
		proposed[slot].id = picks[slot].id;
		proposed[slot].classId = ClassId(Roster[picks[slot].id].gameClass);
		proposed[slot].assist = picks[slot].assist;
	}
	if (!Write(settings + SelectionOffset, proposed))
	{
		const bool restored = Write(settings + SelectionOffset, current);
		Log(0, "quick training: could not stage selections, original selection restored=%d", restored);
		return false;
	}
	LogTeams(0, "quick training: teams staged", proposed);
	return true;
}

uintptr_t TrainingTeamEditor::GetPlayableTrainingAction()
{
	const auto action = GetFightingAction();
	LoadedTeams loaded{};
	return action && ReadLoadedTeams(loaded) ? action : 0;
}

uintptr_t TrainingTeamEditor::GetFightingAction()
{
	Match match{};
	ActionState action{};
	if (!ReadMatch(match) || !ReadAction(match.action, action) || !IsFighting(action))
		return 0;
	return match.action;
}

void TrainingTeamEditor::InitializeLog(HMODULE module)
{
	wchar_t modulePath[MAX_PATH]{};
	const auto length = GetModuleFileNameW(module, modulePath, MAX_PATH);
	if (length && length < MAX_PATH)
	{
		const auto root = std::filesystem::path(modulePath).parent_path();
		logAvailable = OpenLog(root / L"logs", root / LogName);
		OpenRecordingLog(root / L"logs");
	}
	if (!logAvailable || recordingLogPath.empty())
	{
		std::error_code error;
		const auto temp = std::filesystem::temp_directory_path(error);
		if (!error)
		{
			const auto directory = temp / L"UMVC3TrainingToolsInt" / L"logs";
			if (!logAvailable)
			{
				logAvailable = OpenLog(directory, temp / LogName);
				logInTemp = logAvailable;
			}
			if (recordingLogPath.empty())
				OpenRecordingLog(directory);
		}
	}
	wchar_t gamePath[MAX_PATH]{};
	WIN32_FILE_ATTRIBUTE_DATA gameFile{};
	const auto lengthGamePath = GetModuleFileNameW(nullptr, gamePath, MAX_PATH);
	const bool identified = lengthGamePath && lengthGamePath < MAX_PATH
		&& GetFileAttributesExW(gamePath, GetFileExInfoStandard, &gameFile);
	Log(0, "diagnostics: game_file_identified=%d game_exe_bytes=%llu teams_in_temp=%d recording_log=%d recording_same_folder=%d",
		identified, (static_cast<std::uint64_t>(gameFile.nFileSizeHigh) << 32) | gameFile.nFileSizeLow,
		logInTemp, !recordingLogPath.empty(), logAvailable && !recordingLogPath.empty()
			&& logPath.parent_path() == recordingLogPath.parent_path());
}

void TrainingTeamEditor::DrawLogMenu()
{
	const auto teamsFolder = logAvailable ? logPath.parent_path() : std::filesystem::path{};
	const auto recordingFolder = recordingLogPath.parent_path();
	const auto& mainFolder = teamsFolder.empty() ? recordingFolder : teamsFolder;
	const auto open = [](const std::filesystem::path& folder)
	{
		const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
		if (result <= 32)
		{
			Log(0, "diagnostics: could not open log folder shell_result=%Id", result);
			MessageBoxW(nullptr, Localizer::UiWideText("LOG_OPEN_FAILED").c_str(), L"UMVC3 Training Tools", MB_OK | MB_ICONERROR);
		}
	};
	if (ImGui::MenuItem(UiText("LOG_OPEN_FOLDER"), nullptr, false, !mainFolder.empty()))
		open(mainFolder);
	if (logAvailable && !recordingFolder.empty() && recordingFolder != teamsFolder
		&& ImGui::MenuItem(UiText("LOG_OPEN_RECORDING_FOLDER")))
		open(recordingFolder);
}

bool TrainingTeamEditor::Update()
{
	if (!trial.pending && !trial.uncertain)
		return true;
	const auto now = GetTickCount64();
	if (trial.pending)
		lastEditorUpdate.store(now);
	const bool timedOut = now - trial.started >= RestartTimeoutMs;

	Match match;
	if (!ReadMatch(match))
	{
		if (trial.pending && !timedOut)
			return false;
		watchedAttempt.store(0);
		Log(trial.number, "editor ended trial: Training match unavailable");
		trial = {};
		draftFrom = {};
		status = "TEAM_MATCH_ENDED";
		return true;
	}
	if (trial.uncertain && match.settings == trial.before.settings && match.action == trial.before.action)
		return false;
	if (trial.uncertain)
	{
		trial = {};
		draftFrom = {};
		status = "TEAM_NEW_MATCH";
		return true;
	}

	ActionState action;
	const bool finishedRestart = ReadAction(match.action, action) && IsFighting(action);
	const bool sameMatch = match.settings == trial.before.settings && match.action == trial.before.action;
	LoadedTeams loaded{};
	const bool haveLoaded = ((sameMatch && finishedRestart) || timedOut) && ReadLoadedTeams(loaded);
	if (sameMatch && finishedRestart && haveLoaded && loaded != trial.originalFighters
		&& SameSelection(match.teams, trial.proposed))
	{
		bool matches = true;
		bool known = true;
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto expected = trial.expectedClasses[slot];
			known &= expected != 0;
			matches &= expected ? loaded[slot].gameClass == expected
				: loaded[slot].gameClass != trial.originalFighters[slot].gameClass;
			matches &= loaded[slot].assist == trial.proposed[slot].assist;
			if (trial.before.teams[slot].color != trial.proposed[slot].color)
				matches &= loaded[slot].color == trial.proposed[slot].color;
		}
		if (matches)
		{
			if (action.flags & trial.reloadFlags)
				ClearReloadFlags(match.action, trial.reloadFlags);
			trial.pending = false;
			watchedAttempt.store(0);
			if (trial.exercise)
				ApplyExercise(match.action, *trial.exercise);
			BeginRestart(match, quickExerciseAction == match.action ? std::optional{ quickExerciseReset } : std::nullopt, "team reload");
			Log(trial.number, "editor completed trial: assists and color changes verified, all class IDs verifiable=%d", known);
			LogLoadedTeams(trial.number, "confirmed loaded", loaded);
			status = known ? (trial.successStatus ? trial.successStatus : "TEAM_VERIFIED") : "TEAM_VERIFY_FIGHTER";
			return true;
		}
	}

	if (!timedOut)
		return false;

	trial.pending = false;
	watchedAttempt.store(0);
	Log(trial.number, "editor timed out: fighting=%d step=%d next=%d restart=%u flags=0x%08X loaded_readable=%d loaded_unchanged=%d",
		finishedRestart, action.step, action.nextStep, action.restart, action.flags,
		haveLoaded, haveLoaded && loaded == trial.originalFighters);
	if (haveLoaded)
		LogLoadedTeams(trial.number, "loaded at timeout", loaded);
	if (sameMatch && finishedRestart && haveLoaded && loaded == trial.originalFighters)
	{
		if (action.flags & trial.reloadFlags)
			ClearReloadFlags(match.action, trial.reloadFlags);
		if (SameSelection(match.teams, trial.before.teams)
			|| (SameSelection(match.teams, trial.proposed) && Write(match.settings + SelectionOffset, trial.before.teams)))
		{
			draftFrom = {};
			Log(trial.number, "editor confirmed original selection restored");
			status = "TEAM_ORIGINAL_RESTORED";
			return true;
		}
	}

	trial.uncertain = true;
	Log(trial.number, "editor could not confirm teams; further edits disabled for this match");
	status = action.restart || action.step == RestartStep ? "TEAM_RELOAD_TIMEOUT"
		: "TEAM_NO_CONFIRMATION";
	return false;
}

void TrainingTeamEditor::Draw()
{
	if (!ImGui::BeginTabItem(Localizer::UiLabel("TEAM_TAB").c_str()))
		return;

	ImGui::TextWrapped("%s", UiText("TEAM_INFO"));
	if (logAvailable)
		ImGui::TextDisabled(UiText("TEAM_LOG"), logInTemp ? "%TEMP%\\UMVC3TrainingToolsInt\\logs\\UMVC3TrainingToolsInt-Teams.log"
			: "logs\\UMVC3TrainingToolsInt-Teams.log");
	else
		ImGui::TextDisabled("%s", UiText("TEAM_NO_LOG"));
	if (*status)
		ImGui::TextWrapped("%s", UiText(status));

	Match match;
	if (trial.uncertain || !ReadMatch(match) || !ValidSelection(match.teams))
	{
		if (!trial.uncertain)
			ImGui::TextWrapped("%s", UiText("TEAM_NEED_MATCH"));
		ImGui::EndTabItem();
		return;
	}
	if (!trial.pending && (match.settings != draftFrom.settings || match.action != draftFrom.action
		|| !SameSelection(match.teams, draftFrom.teams)))
		ResetDraft(match);

	ImGui::BeginDisabled(trial.pending);
	DrawTeamDraft(draft);

	if (ImGui::Button(UiText("TEAM_APPLY")))
		Apply(match, draft);
	ImGui::SameLine();
	if (ImGui::Button(UiText("TEAM_RESET_DRAFT")))
	{
		ResetDraft(match);
		status = "";
	}
	ImGui::EndDisabled();
	ImGui::EndTabItem();
}

void TrainingTeamEditor::DrawPresetTab()
{
	if (!ImGui::BeginTabItem(Localizer::UiLabel("PRESET_TAB").c_str()))
		return;

	Match match;
	if (!ReadMatch(match) || !ValidSelection(match.teams))
	{
		ImGui::TextWrapped("%s", UiText("TEAM_NEED_MATCH"));
		ImGui::EndTabItem();
		return;
	}

	static uintptr_t lastPresetMatch = 0;
	static bool captureExercise = false;
	static int capturePosition = 0;
	if (lastPresetMatch != match.action)
	{
		lastPresetMatch = match.action;
		RefreshQuickPresets();
		const auto positions = quickExerciseAction == match.action ? quickExerciseReset : XPositions{ p1Pos, p2Pos };
		const auto choice = std::find_if(QuickResetChoices.begin(), QuickResetChoices.end(), [&](const auto& entry)
		{
			return entry.second.p1 == positions.p1 && entry.second.p2 == positions.p2;
		});
		capturePosition = choice == QuickResetChoices.end() ? 0 : static_cast<int>(choice - QuickResetChoices.begin());
	}

	const int stage = TrainingShortcut::CurrentTrainingStage();
	ImGui::BeginDisabled(trial.pending);
	if (DrawQuickPresetSelector("training tab") && !trial.uncertain)
		status = "";
	if (selectedQuickPreset)
	{
		const auto& preset = *selectedQuickPreset;
		ImGui::Text(UiText("PRESET_SAVED_STAGE"), Stages[preset.stage]);
		const bool sameStage = stage == preset.stage;
		if (!sameStage)
			ImGui::TextWrapped("%s", UiText("PRESET_STAGE_FROM_MAIN"));
		ImGui::BeginDisabled(trial.uncertain);
		if (ImGui::Button(UiText(sameStage ? "PRESET_APPLY" : "PRESET_APPLY_CURRENT_STAGE")))
		{
			Log(0, "presets: apply requested saved_stage=%d current_stage=%d same_stage=%d", preset.stage, stage, sameStage);
			if (HasAvailableColors(preset.picks))
				Apply(match, DraftFromPreset(match, preset), &preset);
			else
			{
				status = "QUICK_ALT_UNAVAILABLE";
				Log(0, "presets: apply rejected reason=alternate costume missing");
			}
		}
		ImGui::EndDisabled();
	}
	if (*status)
		ImGui::TextWrapped("%s", UiText(status));

	ImGui::SeparatorText(UiText("PRESET_SAVE_CURRENT"));
	if (stage < 0)
		ImGui::TextWrapped("%s", UiText("PRESET_STAGE_UNKNOWN"));
	ImGui::Checkbox(UiText("QUICK_EXERCISE"), &captureExercise);
	if (captureExercise)
		DrawResetChoice(capturePosition);
	DrawQuickPresetActions([&](bool update)
	{
		QuickPreset snapshot{ {}, {}, stage };
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto& fighter = match.teams[slot];
			snapshot.picks[slot] = { fighter.id, fighter.assist,
				ValidPresetColor(fighter.color) ? fighter.color : TrainingShortcut::AutomaticColor };
		}
		if (captureExercise)
		{
			QuickExercise exercise;
			exercise.resetPosition = capturePosition;
			{
				const std::lock_guard lock(RecordingMutex);
				for (size_t index = 0; index < DummyReactionSettings.size(); ++index)
					exercise.reactions[index] = *DummyReactionSettings[index];
			}
			if (std::any_of(exercise.reactions.begin(), exercise.reactions.end(), [](const DummyReaction& reaction) { return reaction.slot != 0; }))
			{
				quickPresetStatus = "PRESET_RECORDING_UNSUPPORTED";
				Log(0, "presets: save rejected reason=exercise references a recording slot");
				return;
			}
			snapshot.exercise = exercise;
		}
		SaveQuickPreset(std::move(snapshot), update, true);
	}, stage >= 0 && !trial.pending && !trial.uncertain);
	ImGui::EndDisabled();
	ImGui::EndTabItem();
}
