#pragma once
#include "ProtobufModule.h"
#include "SinglePortModule.h"

class NoiseMeasureModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    NoiseMeasureModule();

  protected:
  private:
    virtual int32_t runOnce() override;
};

extern NoiseMeasureModule *noiseMeasureModule;