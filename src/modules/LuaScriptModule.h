#pragma once
#ifdef ESP32C6
#define SOL_NO_THREAD_LOCAL 1
#include "ProtobufModule.h"
#include "SinglePortModule.h"
#include <FS.h>
#include <sol/sol.hpp>

extern sol::state lua;

enum FileServerState { FILESERVER_DISABLED, FILESERVER_INIT, FILESERVER_RUNNING, FILESERVER_DEINIT, FILESERVER_IDLE };

namespace FileServer
{
bool openFs();
uint32_t checkFileFlags(fs::FS &fs, String filename, uint32_t flags);
bool startFilemanager();
void endFilemanager();
} // namespace FileServer

class LuaScriptModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    LuaScriptModule();
    void startFileServer();
    void stopFileServer();
    bool isRunning();

  protected:
  private:
    FileServerState fileServerState = FILESERVER_DISABLED;
    virtual int32_t runOnce() override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    bool isReady = false;
};

extern LuaScriptModule *luaScriptModule;
#endif