#pragma once

#ifdef _DEBUG
#include <iostream>
#define ALEPH3_LOG(msg) do { std::cout << "[ALEPH3] " << msg << std::endl; } while(0)
#else
#define ALEPH3_LOG(msg) do {} while(0)
#endif