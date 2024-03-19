#include "include/logger.h"

#include <iostream>

Logger l{};

void logger_init(bool verbose) { l.verbose = verbose; }

Logger& log() { return l; }
