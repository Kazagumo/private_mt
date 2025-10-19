#include "configuration.h"
#ifdef ESP32C6
#include "LuaScriptModule.h"
#include "graphics/Screen.h"
#include "mesh/NodeDB.h"
#include "modules/CannedMessageModule.h"
#include "serialization/MeshPacketSerializer.h"
#include <ESPFMfGK.h>
#include <FSCommon.h>
#include <NonBlockingRtttl.h>
#include <sol/sol.hpp>
#ifdef CONFIG_PM_ENABLE
#include "esp_pm.h"
esp_pm_lock_handle_t cpufreqlock;
esp_pm_lock_handle_t apbfreqlock;
#endif

inline void handle_panic(sol::optional<std::string> maybe_msg)
{
    log_e("Lua is in a panic state and will now abort() the application");
    if (maybe_msg) {
        const std::string &msg = maybe_msg.value();
        log_e("\terror message: %s", msg.c_str());
    }
    // When this function exits, Lua will exhibit default behavior and abort()
}

sol::state lua(sol::c_call<decltype(&handle_panic), &handle_panic>);

LuaScriptModule *luaScriptModule;

const word filemanagerport = 8080;
ESPFMfGK *filemgr = nullptr;

namespace FileServer
{

bool openFs()
{
    if (!filemgr->AddFS(FSCom, "LittleFS", false)) {
        LOG_INFO("Adding FS failed.");
        return false;
    }
    return true;
}

uint32_t checkFileFlags(fs::FS &fs, String filename, uint32_t flags)
{
    // Show file/path in Lists
    // filenames start without "/", pathnames start with "/"
    if (flags & (ESPFMfGK::flagCheckIsFilename | ESPFMfGK::flagCheckIsPathname)) {
        /*
        Serial.print("flagCheckIsFilename || flagCheckIsPathname check: ");
        Serial.println(filename);
        */
        if (flags | ESPFMfGK::flagCheckIsFilename) {
            if (filename.startsWith(".")) {
                // Serial.println(filename + " flagIsNotVisible");
                return ESPFMfGK::flagIsNotVisible;
            }
        }
        /*
           this will catch a pathname like /.test, but *not* /foo/.test
           so you might use .indexOf()
        */
        if (flags | ESPFMfGK::flagCheckIsPathname) {
            if (filename.startsWith("/.")) {
                // Serial.println(filename + " flagIsNotVisible");
                return ESPFMfGK::flagIsNotVisible;
            }
        }
    }

    // Checks if target file name is valid for action. This will simply allow everything by returning the queried flag
    if (flags & ESPFMfGK::flagIsValidAction) {
        return flags & (~ESPFMfGK::flagIsValidAction);
    }

    // Checks if target file name is valid for action.
    if (flags & ESPFMfGK::flagIsValidTargetFilename) {
        return flags & (~ESPFMfGK::flagIsValidTargetFilename);
    }

    // Default actions
    uint32_t defaultflags = ESPFMfGK::flagCanDelete | ESPFMfGK::flagCanRename | ESPFMfGK::flagCanGZip | // ^t
                            ESPFMfGK::flagCanDownload | ESPFMfGK::flagCanUpload;                        // ^t

    // editable files.
    const String extedit[] PROGMEM = {".lua", ".txt", ".msg", ".json"};

    filename.toLowerCase();
    // I simply assume, that editable files are also allowed to be previewd
    for (int i = 0; i < sizeof(extedit) / sizeof(extedit[0]); i++) {
        if (filename.endsWith(String(extedit[i]))) {
            defaultflags |= ESPFMfGK::flagCanEdit | ESPFMfGK::flagAllowPreview;
            break;
        }
    }

    return defaultflags;
}

bool startFilemanager()
{
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "filemanagerlock", &cpufreqlock);
    esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "filemanagerlock", &apbfreqlock);
    esp_pm_lock_acquire(cpufreqlock);
    esp_pm_lock_acquire(apbfreqlock);
#endif
    filemgr = new ESPFMfGK(filemanagerport);
    if (filemgr == nullptr) {
        LOG_INFO("alloc fault");
        return false;
    }
    if (!openFs()) {
        LOG_INFO("fs fault");
        return false;
    }
    // See above.
    filemgr->checkFileFlags = checkFileFlags;

    filemgr->WebPageTitle = "FileManager";
    filemgr->BackgroundColor = "white";
    filemgr->textareaCharset = "accept-charset=\"utf-8\"";

    // If you want authentication
    // filemgr->HttpUsername = "my";
    // filemgr->HttpPassword = "secret";

    // display the file date? change here. does not work well if you never set configTzTime()
    // filemgr->FileDateDisplay = ESPFMfGK::fddNone;

    if (filemgr->begin()) {
        return true;
    } else {
        return false;
    }
}

void endFilemanager()
{
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_release(cpufreqlock);
    esp_pm_lock_release(apbfreqlock);
    esp_pm_lock_delete(cpufreqlock);
    esp_pm_lock_delete(apbfreqlock);
#endif
    filemgr->end();
    delete filemgr;
    filemgr = nullptr;
}

} // namespace FileServer

LuaScriptModule::LuaScriptModule()
    : SinglePortModule("lua", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("LuaScript")
{
    if (config.network.wifi_enabled)
        fileServerState = FILESERVER_IDLE;
    if (FSCom.exists("/lua/main.lua")) {
        ;
        LOG_INFO("lua script detected.executing");
        lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::package, sol::lib::os, sol::lib::io,
                           sol::lib::math);
        setFunctions();
        if (!FSCom.exists("/exit_without_clean")) {
            FSCom.mkdir("/exit_without_clean");
            auto result = lua.safe_script_file("/littlefs/lua/main.lua");
            // fuck,we need to add the full path to the file.
            if (unlikely(!result.valid())) {
                sol::error err = result;
                LOG_ERROR(err.what());
            }
            isReady = true;
        }
        FSCom.rmdir("/exit_without_clean");
    }
}

void LuaScriptModule::setFunctions()
{
    lua.set_function("millis", millis);
    lua.set_function("print", [](const char *info) { LOG_INFO("[LuaEngine] %s", info); });
    lua.set_function("sendText", [](NodeNum dest, ChannelIndex channel, const char *message, bool wantReplies) {
        cannedMessageModule->sendTextSlient(dest, channel, message, wantReplies);
    });
    lua.set_function("getNodeId", []() { return nodeDB->getNodeNum(); });
    lua.set_function("getlongname", [](uint32_t node) {
        return nodeDB->getMeshNode(node) == nullptr ? "?" : nodeDB->getMeshNode(node)->user.long_name;
    });
    lua.set_function("getshortname", [](uint32_t node) {
        return nodeDB->getMeshNode(node) == nullptr ? "?" : nodeDB->getMeshNode(node)->user.short_name;
    });
    lua.set_function("showsimplebanner", [](const char *content, uint32_t duration) {
        screen->setOn(true);
        screen->showSimpleBanner(content, duration);
    });
}

void LuaScriptModule::startFileServer()
{
    if (fileServerState == FILESERVER_IDLE) {
        fileServerState = FILESERVER_INIT;
    }
}

void LuaScriptModule::stopFileServer()
{
    if (fileServerState == FILESERVER_RUNNING) {
        fileServerState = FILESERVER_DEINIT;
    }
}

bool LuaScriptModule::isRunning()
{
    return (fileServerState == FILESERVER_RUNNING) ? true : false;
}

ProcessMessage LuaScriptModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (isReady && mp.from != nodeDB->getNodeNum() && mp.from != 0x0) {
        lua["mp"] = MeshPacketSerializer::JsonSerialize(&mp, true).c_str();
        lua.safe_script("onReceive(mp)");
        LOG_INFO("[LuaEngine] memory usage %d", lua.memory_used());
        lua.safe_script("mp = nil");
        lua.collect_garbage();
    }
    return ProcessMessage::CONTINUE;
}

int32_t LuaScriptModule::runOnce()
{
    // handle fileserver
    // LOG_INFO("lua runonce %s:",runOncefunction.c_str());
    uint32_t delay = 0;
    if (isReady) {
        delay = lua["runOnce"]();
        // LOG_INFO("lua delay: %d",delay);
    }
    switch (fileServerState) {
    case FILESERVER_DISABLED:
        break;
    case FILESERVER_IDLE:
        break;
    case FILESERVER_INIT:
        FileServer::startFilemanager();
        fileServerState = FILESERVER_RUNNING;
        break;
    case FILESERVER_RUNNING:
        filemgr->handleClient();
        break;
    case FILESERVER_DEINIT:
        FileServer::endFilemanager();
        fileServerState = FILESERVER_IDLE;
        break;
    }
    if (delay > 0 && (fileServerState == FILESERVER_IDLE || fileServerState == FILESERVER_DISABLED)) {
        return delay;
    }
    return 30;
}
#endif
