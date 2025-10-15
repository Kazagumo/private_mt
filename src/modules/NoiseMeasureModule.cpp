#include "NoiseMeasureModule.h"
#include "configuration.h"

NoiseMeasureModule *noiseMeasureModule;

NoiseMeasureModule::NoiseMeasureModule()
    : SinglePortModule("Noise", meshtastic_PortNum_UNKNOWN_APP), concurrency::OSThread("Noise")
{
}

int32_t NoiseMeasureModule::runOnce()
{
    router->getInterface()->runcheck();
    return 2000;
}
