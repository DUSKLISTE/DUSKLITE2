/**
 * m_Do_main.cpp
 * Main Initialization
 * PC Port Version - based on Aurora integration from Vorversion
 */

#include "m_Do/m_Do_main.h"
#include <dolphin/vi.h>
#include <cstring>
#include "DynamicLink.h"
#include "JSystem/JAudio2/JASAudioThread.h"
#include "JSystem/JAudio2/JAUSectionHeap.h"
#include "JSystem/JAudio2/JAUSoundTable.h"
#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JHostIO/JORServer.h"
#include "JSystem/JKernel/JKRAram.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JUtility/JUTConsole.h"
#include "JSystem/JUtility/JUTException.h"
#include "JSystem/JUtility/JUTProcBar.h"
#include "JSystem/JUtility/JUTReport.h"
#include "SSystem/SComponent/c_counter.h"
#include "SSystem/SComponent/c_API_graphic.h"
#include "Z2AudioLib/Z2WolfHowlMgr.h"
#include "c/c_dylink.h"
#include "d/d_com_inf_game.h"
#include "d/d_debug_pad.h"
#include "d/d_s_logo.h"
#include "d/d_s_menu.h"
#include "d/d_s_play.h"
#include "dusk/time.h"
#include "f_ap/f_ap_game.h"
#include "f_op/f_op_msg.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext2.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_machine.h"
#include "m_Do/m_Do_printf.h"
#include "m_Do/m_Do_ext2.h"
#include "SSystem/SComponent/c_counter.h"
#include <cstring>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include "SSystem/SComponent/c_API.h"
#include "dusk/app_info.hpp"
#include "dusk/crash_reporting.h"
#include "dusk/data.hpp"
#include "dusk/dusk.h"
#include "dusk/frame_interpolation.h"
#include "dusk/game_clock.h"
#include "dusk/gyro.h"
#include "dusk/imgui/ImGuiConsole.hpp"
#include "dusk/imgui/ImGuiEngine.hpp"
#include "dusk/iso_validate.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/ui/menu_bar.hpp"
#include "dusk/ui/overlay.hpp"
#include "dusk/ui/prelaunch.hpp"
#include "dusk/ui/preset.hpp"
#include "dusk/ui/ui.hpp"
#include "version.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <aurora/dvd.h>
#include <dolphin/dvd.h>

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_misc.h"
#include "cxxopts.hpp"
#include "d/actor/d_a_movie_player.h"
#include "dusk/audio/DuskAudioSystem.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/config.hpp"
#include "dusk/speedrun.h"
#include "dusk/settings.h"
#include "dusk/io.hpp"
#include "dusk/version.hpp"
#include "dusk/discord_presence.hpp"
#include "tracy/Tracy.hpp"
#include "f_pc/f_pc_draw.h"
#include "tracy/Tracy.hpp"
#include <RmlUi/Core.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if DUSK_ENABLE_SENTRY_NATIVE
#include "dusk/ui/reporting.hpp"
#endif

#if defined(TARGET_ANDROID)
#include <sys/system_properties.h>
#endif

// --- GLOBALS ---
s8 mDoMain::developmentMode = -1;
OSTime mDoMain::sPowerOnTime;
OSTime mDoMain::sHungUpTime;
u32 mDoMain::memMargin = 0xFFFFFFFF;
char mDoMain::COPYDATE_STRING[18] = "??/??/?? ??:??:??";
#if TARGET_PC
const int audioHeapSize = 0x14D800 * 2;
#else
const int audioHeapSize = 0x14D800;
#endif

// =========================================================================
// LOAD_COPYDATE - PC Version
// =========================================================================
#define COPYDATE_PATH "/str/Final/Release/COPYDATE"

#if TARGET_PC
bool dusk::IsRunning = true;
bool dusk::IsShuttingDown = false;
bool dusk::IsGameLaunched = false;
bool dusk::RestartRequested = false;
std::filesystem::path dusk::ConfigPath;
std::filesystem::path dusk::CachePath;
AuroraBackend dusk::RequestedGraphicsBackend = BACKEND_AUTO;
AuroraBackend dusk::ResolvedGraphicsBackend = BACKEND_AUTO;
dusk::GraphicsBackendSource dusk::GraphicsBackendSelectionSource = dusk::GraphicsBackendSource::PlatformDefault;
bool dusk::GraphicsBackendAutoRequested = true;
#endif

void dusk::RequestRestart() noexcept {
    RestartRequested = SupportsProcessRestart;
    IsRunning = false;
}

s32 LOAD_COPYDATE(void*) {
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    DVDFileInfo fi;
    if (DVDOpen(COPYDATE_PATH, &fi)) {
        u32 readLen = (fi.length < sizeof(buffer) - 1) ? fi.length : sizeof(buffer) - 1;
        // DVDReadPrio requires 32-byte aligned buffer and length rounded up to 32
        u32 alignedLen = (readLen + 31) & ~31;
        alignas(32) char readBuf[64];
        DVDReadPrio(&fi, readBuf, alignedLen, 0, 2);
        DVDClose(&fi);

        memcpy(buffer, readBuf, readLen);
        buffer[readLen] = '\0';
    } else {
        strcpy(buffer, "PC PORT BUILD");
        DuskLog.warn("COPYDATE file not found at {}", COPYDATE_PATH);
    }

    memcpy(mDoMain::COPYDATE_STRING, buffer, sizeof(mDoMain::COPYDATE_STRING) - 1);
    mDoMain::COPYDATE_STRING[sizeof(mDoMain::COPYDATE_STRING) - 1] = '\0';

    DuskLog.info("COPYDATE=[{}]", mDoMain::COPYDATE_STRING);
    return 1;
}

AuroraInfo auroraInfo;
AuroraStats dusk::lastFrameAuroraStats;
float dusk::frameUsagePct = 0.0f;

bool launchUILoop() {
    while (dusk::IsRunning && !dusk::IsGameLaunched) {
        const AuroraEvent* event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            switch (event->type) {
            case AURORA_SDL_EVENT:
                dusk::ui::handle_event(event->sdl);
                dusk::g_imguiConsole.HandleSDLEvent(event->sdl);
                break;
            case AURORA_DISPLAY_SCALE_CHANGED:
                dusk::ImGuiEngine_Initialize(event->windowSize.scale);
                break;
            case AURORA_EXIT:
                return false;
            }

            event++;
        }

        if (!aurora_begin_frame()) {
            DuskLog.debug("aurora_begin_frame returned false, skipping draw this frame");
            continue;
        }

        dusk::ui::update();

        dusk::g_imguiConsole.PreDraw();
        dusk::g_imguiConsole.PostDraw();

        aurora_end_frame();
    }

    return dusk::IsRunning;
}

void main01(void) {
    OS_REPORT("\x1b[m");

    // 1. Setup
    mDoMch_Create();
    mDoGph_Create();
    mDoCPd_c::create();

    // Console Setup
    JUTConsole* console = JFWSystem::getSystemConsole();
    if (console) {
        console->setOutput(mDoMain::developmentMode ? JUTConsole::OUTPUT_OSR_AND_CONSOLE :
                                                      JUTConsole::OUTPUT_NONE);
        console->setPosition(32, 42);
    }

    // Loader Init
    mDoDvdThd_callback_c::create((mDoDvdThd_callback_func)LOAD_COPYDATE, NULL);

    OSReport("Calling fapGm_Create()...\n");
    fapGm_Create();

    OSReport("Calling fopAcM_initManager()...\n");
    fopAcM_initManager();

    OSReport("Calling cDyl_InitAsync()...\n");
    cDyl_InitAsync();

    g_mDoAud_audioHeap = JKRCreateSolidHeap(audioHeapSize, JKRGetCurrentHeap(), false);
    JKRHEAP_NAME(g_mDoAud_audioHeap, "g_mDoAud_audioHeap");

    if (DUSK_AUDIO_DISABLED) {
        // Pretend the audio engine initialized already. This is a lie, but needed to boot.
        mDoAud_zelAudio_c::onInitFlag();
    }

    OSReport("Entering Main Loop (main01)...\n");

    dusk::game_clock::ensure_initialized();

    do {
        // 1. Update Window Events
        const AuroraEvent* event = aurora_update();
        while (true) {
            switch (event->type) {
            case AURORA_NONE:
                goto eventsDone;
            case AURORA_PAUSED:
                dusk::audio::SetPaused(true);
                break;
            case AURORA_UNPAUSED:
                dusk::audio::SetPaused(false);
                dusk::game_clock::reset_frame_timer();
                break;
            case AURORA_SDL_EVENT:
                dusk::ui::handle_event(event->sdl);
                dusk::g_imguiConsole.HandleSDLEvent(event->sdl);
                break;
            case AURORA_DISPLAY_SCALE_CHANGED:
                dusk::ImGuiEngine_Initialize(event->windowSize.scale);
                break;
            case AURORA_EXIT:
                goto exit;
            }

            event++;
        }

        eventsDone:;

        if (!aurora_begin_frame()) {
            DuskLog.debug("aurora_begin_frame returned false, skipping draw this frame");
            continue;
        }

        VIWaitForRetrace();

        dusk::lastFrameAuroraStats = *aurora_get_stats();
        mDoGph_gInf_c::updateRenderSize();

        dusk::ui::update();

        const auto pacing = dusk::game_clock::advance_main_loop();
        if (pacing.is_interpolating) {
            if (pacing.sim_ticks_to_run > 0) {
                dusk::frame_interp::begin_frame(dusk::getSettings().game.enableFrameInterpolation, true, 0.0f);
                dusk::frame_interp::set_ui_tick_pending(true);

                for (int sim_tick = 0; sim_tick < pacing.sim_ticks_to_run; ++sim_tick) {
                    dusk::frame_interp::begin_sim_tick();
                    mDoCPd_c::read();
                    dusk::gyro::read(pacing.sim_pace);
                    fapGm_Execute();
                    mDoAud_Execute();
                    dusk::game_clock::commit_sim_tick();
                }
            }

            dusk::frame_interp::begin_frame(dusk::getSettings().game.enableFrameInterpolation, false,
                                            dusk::game_clock::sample_interpolation_step());
            dusk::frame_interp::interpolate();
            dusk::frame_interp::begin_presentation_camera();
            // run draw functions for anything specially marked to handle interp
            fpcM_DrawIterater((fpcM_DrawIteraterFunc)fpcM_Draw);
            cAPIGph_Painter();
            dusk::frame_interp::end_presentation_camera();
            dusk::frame_interp::set_ui_tick_pending(false);
        } else {
            dusk::frame_interp::begin_frame(dusk::FrameInterpMode::Off, true, 0.0f);
            dusk::frame_interp::set_ui_tick_pending(true);

            // Game Inputs
            mDoCPd_c::read();
            dusk::gyro::read(pacing.presentation_dt_seconds);

            // EXECUTE GAME LOGIC & RENDER
            // This calls mDoGph_Painter -> JFWDisplay -> GX Functions
            fapGm_Execute();

            mDoAud_Execute();
        }

        static Limiter main_loop_limiter;
        static double last_fps_setting = 0.0;
        static Limiter::duration_t target_ns = 0;

        if (dusk::getSettings().game.enableFrameInterpolation.getValue() == dusk::FrameInterpMode::Capped && !dusk::getTransientSettings().skipFrameRateLimit) {
            double current_fps = dusk::getSettings().video.maxFrameRate.getValue();
            if (current_fps != last_fps_setting) {
                last_fps_setting = current_fps;
                target_ns = static_cast<Limiter::duration_t>(1'000'000'000.0 / current_fps);
            }

            Limiter::duration_t sleepTime = main_loop_limiter.Sleep(target_ns);
            dusk::frameUsagePct = 100.0f * (1.0f - static_cast<float>(sleepTime) / static_cast<float>(target_ns));
        } else {
            main_loop_limiter.Reset();
        }

        aurora_end_frame();


        FrameMark;

#ifdef DUSK_DISCORD
        dusk::discord::run_callbacks();
        dusk::discord::update_presence();
#endif
    } while (dusk::IsRunning);

    exit:;
    dusk::ui::shutdown();
}

static bool IsBackendAvailable(AuroraBackend backend) {
    if (backend == BACKEND_AUTO) {
        return true;
    }

    size_t availableBackendCount = 0;
    const AuroraBackend* availableBackends = aurora_get_available_backends(&availableBackendCount);
    for (size_t i = 0; i < availableBackendCount; ++i) {
        if (availableBackends[i] == backend) {
            return true;
        }
    }

    return false;
}

#if defined(TARGET_ANDROID)
static constexpr std::string_view kAndroidBackendHelp = "auto, gles3, opengles3, opengl, gl3, vulkan, null";
static constexpr std::string_view kAndroidDefaultBackendId = "gles3";

static bool IsAndroidBackendAllowed(AuroraBackend backend) {
    return backend == BACKEND_OPENGLES || backend == BACKEND_VULKAN || backend == BACKEND_NULL;
}

static AuroraBackend ResolveAndroidBackendAlias(AuroraBackend backend) {
    if (backend == BACKEND_OPENGL) {
        return BACKEND_OPENGLES;
    }
    return backend;
}

static std::string_view GraphicsBackendSourceName(dusk::GraphicsBackendSource source) {
    switch (source) {
    case dusk::GraphicsBackendSource::CommandLine:
        return "adb/cli";
    case dusk::GraphicsBackendSource::ConfigFile:
        return "config.json";
    case dusk::GraphicsBackendSource::PlatformDefault:
    default:
        return "platform-default";
    }
}
#endif

static AuroraBackend ResolveDesiredBackend(const cxxopts::ParseResult& parsedArgOptions) {
    AuroraBackend desiredBackend = BACKEND_AUTO;
    const bool hasBackendArg = parsedArgOptions.count("backend") != 0;
    std::string requestedBackend;
#if defined(TARGET_ANDROID)
    auto backendSource = dusk::GraphicsBackendSource::PlatformDefault;
#endif

    if (hasBackendArg) {
        requestedBackend = parsedArgOptions["backend"].as<std::string>();
        if (!dusk::try_parse_backend(requestedBackend, desiredBackend)) {
            fmt::print(stderr, "Unknown backend: {}\n", requestedBackend);
            exit(1);
        }
#if defined(TARGET_ANDROID)
        backendSource = dusk::GraphicsBackendSource::CommandLine;
#endif
    } else {
#if defined(TARGET_ANDROID)
        const auto& backendSetting = dusk::getSettings().backend.graphicsBackend;
        requestedBackend = backendSetting.getValue();
        if (backendSetting.getLayer() != dusk::config::ConfigVarLayer::Default &&
            requestedBackend != backendSetting.getDefaultValue())
        {
            backendSource = dusk::GraphicsBackendSource::ConfigFile;
        } else {
            backendSource = dusk::GraphicsBackendSource::PlatformDefault;
        }
#else
        requestedBackend = static_cast<const std::string&>(dusk::getSettings().backend.graphicsBackend);
#endif
        if (!dusk::try_parse_backend(requestedBackend, desiredBackend)) {
            DuskLog.warn("Unknown configured backend '{}', falling back to Auto", requestedBackend);
            desiredBackend = BACKEND_AUTO;
            requestedBackend = "auto";
        }
    }

#if defined(TARGET_ANDROID)
    desiredBackend = ResolveAndroidBackendAlias(desiredBackend);
    dusk::RequestedGraphicsBackend = desiredBackend;
    dusk::GraphicsBackendAutoRequested = desiredBackend == BACKEND_AUTO;
    dusk::GraphicsBackendSelectionSource = backendSource;

    DuskLog.info("[Graphics] Platform: Android");
    DuskLog.info("[Graphics] Vulkan default: disabled");
    DuskLog.info("[Graphics] Default Android graphics backend: OpenGL ES 3");
    DuskLog.info("[Graphics] Backend selection source: {}", GraphicsBackendSourceName(backendSource));
    DuskLog.info("[Graphics] Requested backend: {}", requestedBackend.empty() ? "auto" : requestedBackend);

    if (desiredBackend == BACKEND_AUTO) {
        DuskLog.info("[Graphics] Auto backend policy (Android): prefer OpenGL ES 3 for compatibility.");
        desiredBackend = BACKEND_OPENGLES;
    }

    if (!IsAndroidBackendAllowed(desiredBackend)) {
        const auto message =
            fmt::format("Backend '{}' is not available on Android. Use: {}.", requestedBackend, kAndroidBackendHelp);
        DuskLog.error("[Graphics] {}", message);
        if (hasBackendArg) {
            fmt::print(stderr, "{}\n", message);
            exit(1);
        }
        DuskLog.warn("[Graphics] Falling back to OpenGL ES 3");
        requestedBackend = std::string(kAndroidDefaultBackendId);
        desiredBackend = BACKEND_OPENGLES;
    }

    if (desiredBackend == BACKEND_VULKAN) {
        DuskLog.info("[Graphics] Vulkan is disabled by default on Android, but was explicitly requested.");
        DuskLog.info("[Graphics] Trying Vulkan...");
    } else if (desiredBackend == BACKEND_OPENGLES) {
        DuskLog.info("[Graphics] Resolved backend: gles3");
        DuskLog.info("[Graphics] Trying OpenGL ES 3...");
        DuskLog.info("[Graphics] Adreno compatibility mode: {}",
                     dusk::getSettings().backend.enableAdrenoCompatibilityMode ? "enabled" : "disabled");
    } else {
        DuskLog.info("[Graphics] Resolved backend: {}", dusk::backend_id(desiredBackend));
    }
    dusk::ResolvedGraphicsBackend = desiredBackend;
#endif

    if (!IsBackendAvailable(desiredBackend)) {
#if defined(TARGET_ANDROID)
        DuskLog.error("[Graphics] Requested backend '{}' is unavailable", dusk::backend_name(desiredBackend));
        if (desiredBackend == BACKEND_OPENGLES) {
            DuskLog.error("[Graphics] OpenGL ES 3 initialization failed.");
            DuskLog.error("[Graphics] Reason: Aurora/Dawn did not report an available OpenGL ES backend.");
        } else if (desiredBackend == BACKEND_VULKAN) {
            DuskLog.error("[Graphics] Vulkan was requested manually but is unavailable on this device/driver.");
            DuskLog.error("[Graphics] Use --backend gles3 for compatibility.");
        }
        desiredBackend = BACKEND_NULL;
        dusk::ResolvedGraphicsBackend = desiredBackend;
#else
        DuskLog.warn("Requested backend '{}' is unavailable, falling back to Auto",
                     dusk::backend_name(desiredBackend));
        desiredBackend = BACKEND_AUTO;
#endif
    }

    return desiredBackend;
}

#if defined(TARGET_ANDROID)
static std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

static bool ContainsAscii(std::string_view value, std::string_view needle) {
    return value.find(needle) != std::string_view::npos;
}

static std::string AndroidSystemProperty(const char* name) {
    char value[PROP_VALUE_MAX] = {};
    const int len = __system_property_get(name, value);
    if (len <= 0) {
        return {};
    }
    return std::string(value, static_cast<size_t>(len));
}

struct AndroidCompatibilityProfile {
    bool snapdragon685 = false;
    bool adreno610 = false;
    bool lowEndGpu = false;
    std::string boardPlatform;
    std::string hardware;
    std::string productBoard;
    std::string socModel;
};

static bool ContainsAnyAscii(std::string_view value,
                             std::initializer_list<std::string_view> markers) {
    for (const auto marker : markers) {
        if (ContainsAscii(value, marker)) {
            return true;
        }
    }
    return false;
}

static std::string_view AndroidPerformanceProfileName(AndroidPerformanceProfile profile) {
    switch (profile) {
    case AndroidPerformanceProfile::Quality:
        return "Qualidade";
    case AndroidPerformanceProfile::Balanced:
        return "Balanceado";
    case AndroidPerformanceProfile::Performance:
        return "Desempenho";
    case AndroidPerformanceProfile::LowEndGpu:
        return "GPU fraca";
    case AndroidPerformanceProfile::Auto:
    default:
        return "Auto";
    }
}

static AndroidCompatibilityProfile DetectAndroidCompatibilityProfile() {
    const auto boardPlatform = ToLowerAscii(AndroidSystemProperty("ro.board.platform"));
    const auto hardware = ToLowerAscii(AndroidSystemProperty("ro.hardware"));
    const auto productBoard = ToLowerAscii(AndroidSystemProperty("ro.product.board"));
    const auto socModel = ToLowerAscii(AndroidSystemProperty("ro.soc.model"));

    DuskLog.info("[Graphics] Target compatibility profile: Snapdragon 685 / Adreno 610");
    DuskLog.info("[Graphics] Android SoC properties: ro.board.platform='{}', ro.hardware='{}', ro.product.board='{}', ro.soc.model='{}'",
                 boardPlatform, hardware, productBoard, socModel);

    const auto hasLowEndMarker = [&](std::string_view value) {
        return ContainsAnyAscii(value,
                                {"snapdragon 685", "snapdragon 680", "snapdragon 665",
                                 "snapdragon 660", "sm6225", "sm6115", "sdm660",
                                 "bengal", "adreno 610", "adreno 512", "mali-g52",
                                 "mali-g57", "g52", "g57"});
    };

    const bool snapdragon685 = ContainsAnyAscii(boardPlatform, {"snapdragon 685", "sm6225", "sdm685", "bengal"}) ||
                               ContainsAnyAscii(hardware, {"snapdragon 685", "sm6225", "sdm685", "bengal"}) ||
                               ContainsAnyAscii(productBoard, {"snapdragon 685", "sm6225", "sdm685", "bengal"}) ||
                               ContainsAnyAscii(socModel, {"snapdragon 685", "sm6225", "sdm685", "bengal"});
    const bool adreno610 = snapdragon685 ||
                           ContainsAnyAscii(boardPlatform, {"adreno 610"}) ||
                           ContainsAnyAscii(hardware, {"adreno 610"}) ||
                           ContainsAnyAscii(productBoard, {"adreno 610"}) ||
                           ContainsAnyAscii(socModel, {"adreno 610"});
    const bool lowEndGpu = hasLowEndMarker(boardPlatform) ||
                           hasLowEndMarker(hardware) ||
                           hasLowEndMarker(productBoard) ||
                           hasLowEndMarker(socModel);

    DuskLog.info("[Graphics] Snapdragon 685 detected: {}", snapdragon685 ? "true" : "false");
    DuskLog.info("[Graphics] Adreno 610 detected: {}", adreno610 ? "true" : "false");
    DuskLog.info("[Graphics] Low-end GPU profile candidate: {}", lowEndGpu ? "true" : "false");

    return {
        .snapdragon685 = snapdragon685,
        .adreno610 = adreno610,
        .lowEndGpu = lowEndGpu,
        .boardPlatform = boardPlatform,
        .hardware = hardware,
        .productBoard = productBoard,
        .socModel = socModel,
    };
}

static void ApplyAndroidCompatibilityProfile() {
    const auto detected = DetectAndroidCompatibilityProfile();
    auto& settings = dusk::getSettings();

    AndroidPerformanceProfile requestedProfile = settings.backend.performanceProfile.getValue();
    AndroidPerformanceProfile effectiveProfile = requestedProfile;
    if (requestedProfile == AndroidPerformanceProfile::Auto) {
        effectiveProfile = detected.lowEndGpu ? AndroidPerformanceProfile::LowEndGpu
                                              : AndroidPerformanceProfile::Balanced;
    }

    DuskLog.info("[Graphics] Performance profile (configured): {}",
                 AndroidPerformanceProfileName(requestedProfile));
    DuskLog.info("[Graphics] Performance profile (effective): {}",
                 AndroidPerformanceProfileName(effectiveProfile));

    bool changed = false;
    const auto setBool = [&](auto& var, bool value) {
        if (var.getValue() != value) {
            var.setValue(value);
            changed = true;
        }
    };
    const auto setInt = [&](auto& var, int value) {
        if (var.getValue() != value) {
            var.setValue(value);
            changed = true;
        }
    };
    const auto setEnum = [&](auto& var, auto value) {
        if (var.getValue() != value) {
            var.setValue(value);
            changed = true;
        }
    };

    switch (effectiveProfile) {
    case AndroidPerformanceProfile::Quality:
        setBool(settings.backend.enableAdrenoCompatibilityMode, false);
        setBool(settings.backend.disableMSAAOnAndroid, false);
        setBool(settings.backend.preferConservativeFramebufferFormats, false);
        setInt(settings.game.internalResolutionScale, 2);
        setEnum(settings.game.enableFrameInterpolation, dusk::FrameInterpMode::Capped);
        setInt(settings.game.shadowResolutionMultiplier, 2);
        setEnum(settings.game.bloomMode, dusk::BloomMode::Dusk);
        setBool(settings.game.enableDepthOfField, true);
        setBool(settings.game.enableMapBackground, true);
        break;
    case AndroidPerformanceProfile::Balanced:
        setBool(settings.backend.enableAdrenoCompatibilityMode, true);
        setBool(settings.backend.disableMSAAOnAndroid, true);
        setBool(settings.backend.preferConservativeFramebufferFormats, true);
        setInt(settings.game.internalResolutionScale, 1);
        setEnum(settings.game.enableFrameInterpolation, dusk::FrameInterpMode::Capped);
        setInt(settings.game.shadowResolutionMultiplier, 1);
        setEnum(settings.game.bloomMode, dusk::BloomMode::Classic);
        setBool(settings.game.enableDepthOfField, false);
        setBool(settings.game.enableMapBackground, true);
        break;
    case AndroidPerformanceProfile::Performance:
        setBool(settings.backend.enableAdrenoCompatibilityMode, true);
        setBool(settings.backend.disableMSAAOnAndroid, true);
        setBool(settings.backend.preferConservativeFramebufferFormats, true);
        setInt(settings.game.internalResolutionScale, 1);
        setEnum(settings.game.enableFrameInterpolation, dusk::FrameInterpMode::Off);
        setInt(settings.game.shadowResolutionMultiplier, 1);
        setEnum(settings.game.bloomMode, dusk::BloomMode::Classic);
        setBool(settings.game.enableDepthOfField, false);
        setBool(settings.game.enableMapBackground, false);
        break;
    case AndroidPerformanceProfile::LowEndGpu:
        DuskLog.info("[Graphics] Applying low-end GPU profile.");
        setBool(settings.backend.enableAdrenoCompatibilityMode, true);
        setBool(settings.backend.disableMSAAOnAndroid, true);
        setBool(settings.backend.preferConservativeFramebufferFormats, true);
        setInt(settings.game.internalResolutionScale, 1);
        setEnum(settings.game.enableFrameInterpolation, dusk::FrameInterpMode::Off);
        setInt(settings.game.shadowResolutionMultiplier, 1);
        setEnum(settings.game.bloomMode, dusk::BloomMode::Classic);
        setBool(settings.game.enableDepthOfField, false);
        setBool(settings.game.enableMapBackground, false);
        break;
    case AndroidPerformanceProfile::Auto:
    default:
        break;
    }

    DuskLog.info("[Graphics] Internal resolution scale: {}x",
                 settings.game.internalResolutionScale.getValue());
    DuskLog.info("[Graphics] MSAA: {}", settings.backend.disableMSAAOnAndroid ? "disabled" : "enabled");
    DuskLog.info("[Graphics] Frame interpolation: {}",
                 settings.game.enableFrameInterpolation.getValue() == dusk::FrameInterpMode::Off
                     ? "disabled"
                     : "enabled");
    DuskLog.info("[Graphics] Backend: OpenGL ES 3");
    if (changed) {
        dusk::config::Save();
    }
}
#endif

static void aurora_imgui_init_callback(const AuroraWindowSize* size) {
    dusk::ImGuiEngine_Initialize(size->scale);
    dusk::ImGuiEngine_AddTextures();
}

static void ApplyCVarOverrides(const cxxopts::OptionValue& option) {
    if (option.count() == 0) {
        return;
    }

    const auto& cVars = option.as<std::vector<std::string>>();
    for (const auto& cvarArg : cVars) {
        const auto sep = cvarArg.find('=');
        if (sep == std::string::npos) {
            DuskLog.fatal("--cvar argument has no '=': '{}'", cvarArg);
            continue;
        }

        const auto name = std::string_view(cvarArg).substr(0, sep);
        const auto value = std::string_view(cvarArg).substr(sep + 1);

        const auto cVar = dusk::config::GetConfigVar(name);
        if (!cVar) {
            DuskLog.fatal("Unknown --cvar name: '{}'", name);
        }

        try {
            cVar->getImpl()->loadFromArg(*cVar, value);
        } catch (const std::exception& e) {
            DuskLog.fatal("Unable to parse: '{}': {}", value, e.what());
        }
    }
}

static constexpr PADDefaultMapping defaultPadMapping = {
    .buttons = {
        {SDL_GAMEPAD_BUTTON_SOUTH, PAD_BUTTON_A},
        {SDL_GAMEPAD_BUTTON_EAST, PAD_BUTTON_B},
        {SDL_GAMEPAD_BUTTON_WEST, PAD_BUTTON_X},
        {SDL_GAMEPAD_BUTTON_NORTH, PAD_BUTTON_Y},
        {SDL_GAMEPAD_BUTTON_START, PAD_BUTTON_START},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_TRIGGER_Z},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_L},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_R},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_BUTTON_UP},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_BUTTON_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_BUTTON_LEFT},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_BUTTON_RIGHT},
    },
    .axes = {
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_POS},
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_NEG},
        // SDL's gamepad y-axis is inverted from GC's
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_POS},
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_NEG},
        // see above
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_L},
        {{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_R},
    },
};

static bool mainCalled = false;

static u8 selectedLanguage;

u8 OSGetLanguage() {
    return selectedLanguage;
}

static void LanguageInit() {
    // Keep language at 0 (English) if not on a PAL disc.
    // Doubt this matters, but avoid funky shit.
    if (!dusk::version::isRegionPal()) {
        return;
    }

    // Cache this to avoid funky shenanigans.
    selectedLanguage = static_cast<u8>(dusk::getSettings().game.language.getValue());
}

static std::string asset_path(const char* assetName) {
    const char* basePath = SDL_GetBasePath();
    if (basePath != nullptr && basePath[0] != '\0') {
        return std::string(basePath) + "res/" + assetName;
    }
    return std::string("res/") + assetName;
}

static void log_build_info() {
    DuskLog.info("Build: {} (rev {}, built {}, type {})", DUSK_WC_DESCRIBE, DUSK_WC_REVISION, DUSK_WC_DATE, DUSK_BUILD_TYPE);
    DuskLog.info("Platform: {}", DUSK_PLATFORM_NAME);
}

// =========================================================================
// PC ENTRY POINT
// =========================================================================
int game_main(int argc, char* argv[]) {
    // On iOS, when connected to an external monitor, SDLUIKitSceneDelegate scene:willConnectToSession:
    // can call our main function again. Explicitly guard against this reinitialization.
    if (mainCalled) {
        return 0;
    }
    mainCalled = true;

    dusk::registerSettings();
    dusk::config::FinishRegistration();

    cxxopts::ParseResult parsed_arg_options;

    try {
        cxxopts::Options arg_options("Dusklight", "PC Port of a classic adventure game");

        arg_options.add_options()
            ("l,log-level", "Log level from " + std::to_string(AuroraLogLevel::LOG_DEBUG) + " to " + std::to_string(AuroraLogLevel::LOG_FATAL), cxxopts::value<uint8_t>()->default_value("0"))
            ("h,help", "Print usage")
            ("console", "Show the Windows console window for logs", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
            ("dvd", "Path to DVD image file", cxxopts::value<std::string>())
            ("backend", "Graphics API backend to use (auto, d3d12, d3d11, metal, vulkan, opengl, opengles, gles3, gl3, null)", cxxopts::value<std::string>())
            ("cvar", "Override configuration variables without modifying config", cxxopts::value<std::vector<std::string>>());

        arg_options.parse_positional({"dvd"});
        arg_options.positional_help("<dvd-image>");
        arg_options.allow_unrecognised_options();

        parsed_arg_options = arg_options.parse(argc, argv);

        if (parsed_arg_options.count("help"))
        {
            printf("%s", (arg_options.help() + "\n").c_str());
            exit(0);
        }
    }
    catch (const cxxopts::exceptions::exception& e) {
        fprintf(stderr, "Argument Error: %s\n", e.what());
        exit(1);
    }

    const auto startupLogLevel =
        static_cast<AuroraLogLevel>(parsed_arg_options["log-level"].as<uint8_t>());
    const auto dataPaths = dusk::data::initialize_data();
    dusk::ConfigPath = dataPaths.userPath;
    dusk::CachePath = dataPaths.cachePath;
    dusk::InitializeFileLogging(dusk::CachePath, startupLogLevel);

    log_build_info();

    dusk::config::LoadFromUserPreferences();
#if defined(TARGET_ANDROID)
    ApplyAndroidCompatibilityProfile();
#endif
    if (dusk::getSettings().game.speedrunMode) {
        dusk::resetForSpeedrunMode();
    }
    ApplyCVarOverrides(parsed_arg_options["cvar"]);
    dusk::crash_reporting::initialize();
    // TODO: How to handle this?
    // PADSetDefaultMapping(&defaultPadMapping, PAD_TYPE_STANDARD);

    {
        // Load mappings from https://github.com/mdqinc/SDL_GameControllerDB
        const auto mappingsPath = asset_path("gamecontrollerdb.txt");
        if (SDL_AddGamepadMappingsFromFile(mappingsPath.c_str()) < 0) {
            DuskLog.warn("Failed to load gamecontrollerdb.txt: {}", SDL_GetError());
        }
    }

    // Set SDL metadata for audio mixers and macOS "About" menu
    SDL_SetAppMetadata("Dusklight", DUSK_VERSION_STRING, "dev.twilitrealm.dusk");

    {
        const auto userPathString = dusk::ConfigPath.u8string();
        const auto cachePathString = dusk::CachePath.u8string();
        AuroraConfig config{};
        config.appName = dusk::AppName;
        config.userPath = reinterpret_cast<const char*>(userPathString.c_str());
        config.cachePath = reinterpret_cast<const char*>(cachePathString.c_str());
        config.vsync = dusk::getSettings().video.enableVsync;
        config.startFullscreen = dusk::getSettings().video.enableFullscreen;
        config.windowPosX = -1;
        config.windowPosY = -1;
        config.windowWidth = defaultWindowWidth * 2;
        config.windowHeight = defaultWindowHeight * 2;
        config.desiredBackend = ResolveDesiredBackend(parsed_arg_options);
        config.logCallback = &aurora_log_callback;
        config.logLevel = startupLogLevel;
        config.mem1Size = 256 * 1024 * 1024;
        config.mem2Size = 24 * 1024 * 1024;
        config.allowJoystickBackgroundEvents = dusk::getSettings().game.allowBackgroundInput;
        config.pauseOnFocusLost = dusk::getSettings().game.pauseOnFocusLost;
        config.imGuiInitCallback = &aurora_imgui_init_callback;
        config.allowTextureReplacements = dusk::getSettings().game.enableTextureReplacements;
        config.allowTextureDumps = false;
#if defined(TARGET_ANDROID)
        if (dusk::getSettings().backend.enableAdrenoCompatibilityMode ||
            dusk::getSettings().backend.disableMSAAOnAndroid)
        {
            config.msaa = 1;
            config.maxTextureAnisotropy = 1;
        }
        if (dusk::getSettings().backend.preferConservativeFramebufferFormats) {
            DuskLog.info("[Graphics] Conservative framebuffer formats: preferred");
        }
#endif
        auroraInfo = aurora_initialize(argc, argv, &config);
    }

#if defined(TARGET_ANDROID)
    const AuroraBackend activeBackend = aurora_get_backend();
    dusk::ResolvedGraphicsBackend = activeBackend;
    DuskLog.info("[Graphics] Active backend: {}", dusk::backend_id(activeBackend));
    DuskLog.info("[Graphics] Window size: {}x{}", auroraInfo.windowSize.width, auroraInfo.windowSize.height);
    DuskLog.info("[Graphics] Drawable size: {}x{}", auroraInfo.windowSize.native_fb_width,
                 auroraInfo.windowSize.native_fb_height);
    if (activeBackend != auroraInfo.backend) {
        DuskLog.warn("[Graphics] Backend mismatch detected (AuroraInfo={}, aurora_get_backend={})",
                     dusk::backend_id(auroraInfo.backend), dusk::backend_id(activeBackend));
    }

    if (activeBackend == BACKEND_OPENGLES) {
        DuskLog.info("[Graphics] OpenGL ES 3 initialized successfully.");
    } else if (activeBackend == BACKEND_VULKAN) {
        DuskLog.info("[Graphics] Vulkan initialized because it was explicitly requested.");
    } else if (activeBackend == BACKEND_NULL) {
        if (dusk::RequestedGraphicsBackend == BACKEND_VULKAN) {
            DuskLog.error("[Graphics] Vulkan initialization failed on this device.");
            DuskLog.error("[Graphics] Reason: Aurora initialized the null graphics backend.");
            DuskLog.error("[Graphics] Recommendation: use OpenGL ES 3 for compatibility.");
        } else if (dusk::RequestedGraphicsBackend == BACKEND_AUTO) {
            DuskLog.error("[Graphics] Auto backend initialization failed.");
            DuskLog.error("[Graphics] Tested backends: OpenGL ES 3 and Vulkan.");
            DuskLog.error("[Graphics] Reason: Aurora initialized the null graphics backend.");
        } else {
            DuskLog.error("[Graphics] OpenGL ES 3 initialization failed.");
            DuskLog.error("[Graphics] Reason: Aurora initialized the null graphics backend.");
        }
    }
#endif

#ifdef DUSK_DISCORD
    if (dusk::getSettings().game.enableDiscordPresence) {
        dusk::discord::initialize();
    }
#endif

    VISetWindowTitle(
        fmt::format("Dusklight {} [{}]", DUSK_WC_DESCRIBE, dusk::backend_name(aurora_get_backend()))
        .c_str());

    if (dusk::getSettings().video.lockAspectRatio) {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
    } else {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
    }
    VISetFrameBufferScale(dusk::getSettings().game.internalResolutionScale.getValue());
    switch (dusk::getSettings().game.resampler.getValue()) {
    case dusk::Resampler::Area:
        aurora_set_resampler(SAMPLER_AREA);
        break;
    case dusk::Resampler::Bilinear:
    default:
        aurora_set_resampler(SAMPLER_BILINEAR);
        break;
    }

    dusk::audio::SetMasterVolume(dusk::audio::MasterVolumeToLinear(dusk::getSettings().audio.masterVolume / 100.0f));
    dusk::audio::SetEnableReverb(dusk::getSettings().audio.enableReverb);
    dusk::audio::EnableHrtf = dusk::getSettings().audio.enableHrtf;

    // Run ImGui UI loop if Aurora couldn't initialize a backend
    if (aurora_get_backend() == BACKEND_NULL) {
        launchUILoop();
        dusk::crash_reporting::shutdown();
        dusk::ShutdownFileLogging();
        fflush(stdout);
        fflush(stderr);
#ifdef DUSK_DISCORD
        dusk::discord::shutdown();
#endif
        dusk::ui::shutdown();
        aurora_shutdown();
        return 0;
    }

    dusk::ui::initialize();
    dusk::ui::push_document(std::make_unique<dusk::ui::Overlay>(), true, true);
    dusk::ui::push_document(std::make_unique<dusk::ui::MenuBar>(), false);

    // Invalidate a bad saved isoPath so that Dusklight can't get blocked from starting up.
    // This is only a metadata check; full hash verification is handled by the prelaunch UI.
    bool forcePreLaunchUI = false;
    bool saveConfigBeforePrelaunch = false;

    const std::string p = dusk::getSettings().backend.isoPath;
    dusk::iso::DiscInfo discInfo{};
    if (!p.empty() &&
        dusk::iso::inspect(p.c_str(), discInfo) != dusk::iso::ValidationError::Success)
    {
        DuskLog.warn("Saved DVD image path failed validation, clearing configured path: {}", p);
        dusk::getSettings().backend.isoPath.setValue("");
        dusk::getSettings().backend.isoVerification.setValue(dusk::DiscVerificationState::Unknown);
        forcePreLaunchUI = true;
        saveConfigBeforePrelaunch = true;
    }

    std::string dvd_path;
    bool dvd_opened = false;
    if (parsed_arg_options.count("dvd")) {
        dvd_path = parsed_arg_options["dvd"].as<std::string>();
        if (dusk::iso::inspect(dvd_path.c_str(), discInfo) == dusk::iso::ValidationError::Success) {
            DuskLog.info("Loading DVD image from command line: {}", dvd_path);
            dvd_opened = aurora_dvd_open(dvd_path.c_str());
            if (!dvd_opened) {
                DuskLog.warn("Failed to open DVD image from command line: {}, opening prelaunch UI", dvd_path);
                forcePreLaunchUI = true;
            } else {
                dusk::getSettings().backend.isoPath.setValue(dvd_path);
                dusk::getSettings().backend.isoVerification.setValue(
                    dusk::DiscVerificationState::Unknown);
                dusk::config::Save();
                dusk::IsGameLaunched = true;
            }
        } else {
            DuskLog.warn("DVD image from command line failed validation: {}, opening prelaunch UI", dvd_path);
            forcePreLaunchUI = true;
        }
    }

    dusk::iso::log_verification_state(
        dusk::getSettings().backend.isoPath.getValue(),
        dusk::getSettings().backend.isoVerification.getValue());

    if (!dvd_opened) {
        if (dusk::getSettings().backend.isoPath.getValue().empty()) {
            forcePreLaunchUI = true;
        }
        if (forcePreLaunchUI && dusk::getSettings().backend.skipPreLaunchUI.getValue()) {
            DuskLog.warn("Prelaunch UI was disabled with no usable DVD image, enabling prelaunch UI");
            dusk::getSettings().backend.skipPreLaunchUI.setValue(false);
            saveConfigBeforePrelaunch = true;
        }
        if (saveConfigBeforePrelaunch) {
            dusk::config::Save();
        }

        if (!dusk::getSettings().backend.skipPreLaunchUI) {
            dusk::ui::push_document(std::make_unique<dusk::ui::Prelaunch>(), true);

            // pre game launch ui main loop
            if (!launchUILoop()) {
                dusk::crash_reporting::shutdown();
                dusk::ShutdownFileLogging();
                fflush(stdout);
                fflush(stderr);
#ifdef DUSK_DISCORD
                dusk::discord::shutdown();
#endif
                dusk::ui::shutdown();
                aurora_shutdown();
                return 0;
            }
        }

        dvd_path = dusk::getSettings().backend.isoPath;

        if (dvd_path.empty()) {
            DuskLog.fatal("No DVD image specified, unable to boot!");
        }
        if (!dusk::IsGameLaunched &&
            dusk::iso::inspect(dvd_path.c_str(), discInfo) != dusk::iso::ValidationError::Success)
        {
            DuskLog.fatal("DVD image failed validation: {}", dvd_path);
        }
        DuskLog.info("Loading DVD image: {}", dvd_path);
        if (!aurora_dvd_open(dvd_path.c_str())) {
            DuskLog.fatal("Failed to open DVD image: {}", dvd_path);
        }

        dusk::IsGameLaunched = true;
    }

#if DUSK_ENABLE_SENTRY_NATIVE
    if (dusk::crash_reporting::get_consent() == dusk::crash_reporting::Consent::Unknown) {
        dusk::ui::push_document(std::make_unique<dusk::ui::CrashReportWindow>());
    }
#endif

    if (!dusk::getSettings().backend.wasPresetChosen) {
        dusk::ui::push_document(std::make_unique<dusk::ui::PresetWindow>());
    }

    dusk::version::init();
    LanguageInit();

    OSInit();

    mDoMain::sPowerOnTime = OSGetTime();

    // Reset Data
    static mDoRstData sResetData = {0};
    mDoRst::setResetData(&sResetData);
    mDoRst::offReset();
    mDoRst::setLogoScnFlag(0);

    // Global Context Init
    dComIfG_ct();

    // Development Mode
    // mDoMain::developmentMode = 1;  // Force Dev Mode for Debugging
    mDoDvdThd::SyncWidthSound = false;

    OSReport("Starting main01 (Game Loop)...\n");


    main01();

    dusk::MoviePlayerShutdown();

    dusk::crash_reporting::shutdown();
    dusk::ShutdownFileLogging();
    fflush(stdout);
    fflush(stderr);

    mDoMch_Destroy();

    // Notifies all CVs and causes threads to exit
    OSResetSystem(OS_RESET_SHUTDOWN, 0, 0);

#ifdef DUSK_DISCORD
    dusk::discord::shutdown();
#endif
    dusk::ui::shutdown();
    aurora_shutdown();

    return 0;
}


bool JKRHeap::dump_sort() {
    return true;
}

#ifdef __MWERKS__
template <typename T>
JHIComPortManager<T>* JHIComPortManager<T>::instance = nullptr;

template <>
JHIComPortManager<JHICmnMem>* JHIComPortManager<JHICmnMem>::instance = nullptr;

template<>
Z2WolfHowlMgr* JASGlobalInstance<Z2WolfHowlMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2EnvSeMgr* JASGlobalInstance<Z2EnvSeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2FxLineMgr* JASGlobalInstance<Z2FxLineMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2Audience* JASGlobalInstance<Z2Audience>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundObjMgr* JASGlobalInstance<Z2SoundObjMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundInfo* JASGlobalInstance<Z2SoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundInfo* JASGlobalInstance<JAUSoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundNameTable* JASGlobalInstance<JAUSoundNameTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundTable* JASGlobalInstance<JAUSoundTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISoundInfo* JASGlobalInstance<JAISoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundMgr* JASGlobalInstance<Z2SoundMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAIStreamMgr* JASGlobalInstance<JAIStreamMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISeqMgr* JASGlobalInstance<JAISeqMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISeMgr* JASGlobalInstance<JAISeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SpeechMgr2* JASGlobalInstance<Z2SpeechMgr2>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundStarter* JASGlobalInstance<Z2SoundStarter>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISoundStarter* JASGlobalInstance<JAISoundStarter>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2StatusMgr* JASGlobalInstance<Z2StatusMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SceneMgr* JASGlobalInstance<Z2SceneMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SeqMgr* JASGlobalInstance<Z2SeqMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SeMgr* JASGlobalInstance<Z2SeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JASAudioThread* JASGlobalInstance<JASAudioThread>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JASDefaultBankTable* JASGlobalInstance<JASDefaultBankTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;
#endif // __MWERKS__
