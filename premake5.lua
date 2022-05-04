name = "parsec-tray"

workspace (name)
    configurations {"Debug", "Release"}
    platforms {"x64"}

project (name)
    kind "WindowedApp"
    toolset "v143"
    language "C++"
    cppdialect "C++20"
    characterset "MBCS"
    files { "src/**.*" }
    flags { 
      "MultiProcessorCompile", 
      "NoMinimalRebuild", 
      -- "OmitDefaultLibrary",
    }
    removefiles { "temp/**", }
    includedirs { 
      "./src",
      "./parsec-sdk/sdk",
      "./libmatoya/src",
      "./libmatoya/deps"
    }
    defines { 
        "_WIN32",
        "WIN32_LEAN_AND_MEAN",
        "_CRT_SECURE_NO_WARNINGS",
        "_WINSOCK_DEPRECATED_NO_WARNINGS",
    }
    links {
       "libmatoya"
    }
    pchsource "src/pch.cc"
    pchheader "pch.h"
    forceincludes { "pch.h" }
    filter {"platforms:x64"}
        system "Windows"
        architecture "x64"
    filter {"configurations:Debug"}
        defines { "_DEBUG" }
        optimize "Debug"
        symbols "On"
    filter {"configurations:Release"}
        defines { "NDEBUG" }
        optimize "Speed"
    dependson "libmatoya"

project "libmatoya"
  kind "StaticLib"
  basedir ("libmatoya")
  toolset "v143"
  language "C"
  characterset "MBCS"
  files { 
    "libmatoya/src/*.c",
    "libmatoya/src/*.h",
    "libmatoya/src/gfx/**.*",
    "libmatoya/src/hid/**.*",
    "libmatoya/src/hid/**.*",
    "libmatoya/src/windows/**.*" 
  }
  flags { 
    "MultiProcessorCompile", 
    "NoMinimalRebuild", 
    -- "OmitDefaultLibrary",
  }
  includedirs { 
    "./libmatoya",
    "./libmatoya/deps",
    "./libmatoya/src",
    "./libmatoya/src/windows",
  }
    defines { 
        "_WIN32",
        "UNICODE",
        "WIN32_LEAN_AND_MEAN",
        "_CRT_SECURE_NO_WARNINGS",
    }
    links {
       "kernel32.lib",
       "gdi32.lib",
       "winmm.lib",
       "imm32.lib",
       "shell32.lib",
       "advapi32.lib",
       "ole32.lib",
       "oleaut32.lib",
       "opengl32.lib",
       "user32.lib",
       "uuid.lib",
       "version.lib",
       "setupapi.lib",
       "hid.lib",
       "dxgi.lib",
       "ws2_32.lib",
       "bcrypt.lib",
       "windowscodecs.lib",
       "dxguid.lib",
       "xinput9_1_0.lib",
       "d3d9.lib",
       "shlwapi.lib",
       "d3d11.lib",
       "userenv.lib",
    }
    filter {"platforms:x64"}
        system "Windows"
        architecture "x64"
    filter {"configurations:Debug"}
        defines { "_DEBUG" }
        optimize "Debug"
        symbols "On"
    filter {"configurations:Release"}
        defines { "NDEBUG" }
        optimize "Speed"
