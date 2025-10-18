#include "NoiseMeasureModule.h"
#include "configuration.h"

NoiseMeasureModule *noiseMeasureModule;

NoiseMeasureModule::NoiseMeasureModule()
    : SinglePortModule("Noise", meshtastic_PortNum_UNKNOWN_APP), concurrency::OSThread("Noise")
{
}

int32_t NoiseMeasureModule::runOnce()
{
    if (router->getInterface()->runcheck()) {
        return 60 * 1000;
    } else {
        return 2 * 1000;
    }
}
