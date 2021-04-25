#include "version.h"

float minGreetingDistance;
float minGreetingDistanceSquared;  //^2 for GetSquaredDistance
float allowedAngle;
bool enableDebug;


class ToYourFace
{
public:
	static void Patch()
	{
		auto& trampoline = SKSE::GetTrampoline();

		REL::Relocation<std::uintptr_t> DoGreeting{ REL::ID(38601) };
		_GetSquaredDistance = trampoline.write_call<5>(DoGreeting.address() + 0x1C2, enableDebug ? GetSquaredDistance_Debug : GetSquaredDistance);
	}

private:
	static float GetSquaredDistance_Debug(RE::Actor* a_subject, RE::Actor* a_target, bool a_ignoreDisable, bool a_ignoreDiffCell)
	{
		auto distance = _GetSquaredDistance(a_subject, a_target, a_ignoreDisable, a_ignoreDiffCell);
		if (distance < minGreetingDistanceSquared) {
			if (a_subject && a_target) {
				float headingAngle = a_target->GetHeadingAngle(a_subject->GetPosition(), true);
				if (headingAngle < allowedAngle) {
					Tint(a_target, green);
				} else {
					Tint(a_target, blue);
					return FLT_MAX;
				}
			}
		} else {
			Tint(a_target, red);
		}
		return distance;
	}


	static float GetSquaredDistance(RE::Actor* a_subject, RE::Actor* a_target, bool a_ignoreDisable, bool a_ignoreDiffCell)
	{
		auto distance = _GetSquaredDistance(a_subject, a_target, a_ignoreDisable, a_ignoreDiffCell);
		if (distance < minGreetingDistanceSquared) {
			if (a_subject && a_target) {
				float headingAngle = a_target->GetHeadingAngle(a_subject->GetPosition(), true);
				if (headingAngle >= allowedAngle) {
					return FLT_MAX;
				}
			}
		}
		return distance;
	}	
	static inline REL::Relocation<decltype(GetSquaredDistance)> _GetSquaredDistance;


	static void Tint(RE::TESObjectREFR* a_ref, const RE::NiColorA& a_color)
	{
		auto task = SKSE::GetTaskInterface();
		task->AddTask([a_ref, a_color]() {
			if (auto root = a_ref->Get3D(); root) {
				root->TintScenegraph(a_color);
			}
		});
	}

	static inline RE::NiColorA red{ 1.0f, 0.0f, 0.0f, 1.0f };
	static inline RE::NiColorA blue{ 0.0f, 0.0f, 1.0f, 1.0f };
	static inline RE::NiColorA green{ 0.0f, 1.0f, 0.0f, 1.0f };
};


void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
		auto const gameSettingCollection = RE::GameSettingCollection::GetSingleton();
		auto const gameSetting = gameSettingCollection ? gameSettingCollection->GetSetting("fAIMinGreetingDistance") : nullptr;

		if (gameSetting) {
			gameSetting->data.f = minGreetingDistance;
		}

		minGreetingDistanceSquared = minGreetingDistance * minGreetingDistance;

		ToYourFace::Patch();
	}
}


void LoadSettings()
{
	constexpr auto path = L"Data/SKSE/Plugins/po3_ToYourFace.ini";

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiKey();

	ini.LoadFile(path);

	allowedAngle = static_cast<float>(ini.GetDoubleValue("Settings", "Max Heading Angle", 30.0));
	ini.SetDoubleValue("Settings", "Max Heading Angle", static_cast<double>(allowedAngle), ";Only NPCs that are within this angle in front of the player can make comments.\n;An angle of 180 will allow all NPCs.\n", true);

	minGreetingDistance = static_cast<float>(ini.GetDoubleValue("Settings", "Min Greeting Distance", 150.0));
	ini.SetDoubleValue("Settings", "Min Greeting Distance", static_cast<double>(minGreetingDistance), ";Only NPCs that are within this distance of the player can make comments.\n", true);

	enableDebug = ini.GetBoolValue("Settings", "Enable Debug Shaders", false);
	ini.SetBoolValue("Settings", "Enable Debug Shaders", enableDebug, ";Applies shaders on NPCs.\n;Red: too far away. \n;Blue: within greeting distance but not facing player. \n;Green: within greeting distance and face to face.\n", true);

	ini.SaveFile(path);
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	try {
		auto path = logger::log_directory().value() / "po3_ToYourFace.log";
		auto log = spdlog::basic_logger_mt("global log", path.string(), true);
		log->flush_on(spdlog::level::info);

#ifndef NDEBUG
		log->set_level(spdlog::level::debug);
		log->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
		log->set_level(spdlog::level::info);

#endif
		spdlog::set_default_logger(log);
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");

		logger::info("To Your Face Redux {}", SOS_VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "To Your Face Redux";
		a_info->version = SOS_VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			logger::critical("Loaded in editor, marking as incompatible");
			return false;
		}

		const auto ver = a_skse->RuntimeVersion();
		if (ver < SKSE::RUNTIME_1_5_39) {
			logger::critical("Unsupported runtime version {}", ver.string());
			return false;
		}
	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	try {
		logger::info("To Your Face Redux loaded");

		SKSE::Init(a_skse);
		SKSE::AllocTrampoline(1 << 4);

		LoadSettings();

		const auto messaging = SKSE::GetMessagingInterface();
		if (!messaging->RegisterListener("SKSE", OnInit)) {
			return false;
		}

	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}
