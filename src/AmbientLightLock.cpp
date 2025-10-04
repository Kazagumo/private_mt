#include "AmbientLightLock.h"
#include "NodeDB.h"

bool isBusy = false;
int current = moduleConfig.ambient_lighting.current;