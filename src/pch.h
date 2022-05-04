#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

// Windows API
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

// WinRT for HTTPS GET/POST
#pragma comment(lib, "windowsapp")
#include "winrt/Windows.Web.Http.Filters.h"
#include "winrt/Windows.Web.Http.Headers.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <string_view>

// Parsec
#include "parsec-dso.h"
#include "json.hpp"
#include "matoya.h"
#include "mtymap.h"

