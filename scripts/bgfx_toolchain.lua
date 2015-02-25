--
-- Copyright 2014-2015 Dario Manesku. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--

-- This file is adopted from: https://github.com/bkaradzic/bx/blob/master/premake/toolchain.lua
--
-- Copyright 2010-2013 Branimir Karadzic. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--

local naclToolchain = ""

function bgfx_toolchain(_buildDir, _projDir, _bxDir, _libDir)

	newoption {
		trigger = "compiler",
        description = "Choose compiler",
		allowed = {
			{ "android-arm", "Android - ARM" },
			{ "android-mips", "Android - MIPS" },
			{ "android-x86", "Android - x86" },
			{ "asmjs", "Emscripten/asm.js" },
			{ "linux-gcc", "Linux (GCC compiler)" },
			{ "linux-clang", "Linux (Clang compiler)" },
			{ "mingw", "MinGW" },
			{ "nacl", "Native Client" },
			{ "nacl-arm", "Native Client - ARM" },
			{ "pnacl", "Native Client - PNaCl" },
			{ "osx", "OSX" },
			{ "ios-arm", "iOS - ARM" },
			{ "ios-simulator", "iOS - Simulator" },
			{ "qnx-arm", "QNX/Blackberry - ARM" },
		}
	}

	-- Avoid error when invoking premake4 --help.
	if (_ACTION == nil) then return end

	location (_projDir .. _ACTION)

	if _ACTION == "clean" then
		os.rmdir(BUILD_DIR)
	end

	if _ACTION == "gmake" then

		if nil == _OPTIONS["compiler"] then
			print("Compiler must be specified!")
			os.exit(1)
		end

		flags {
			"ExtraWarnings",
		}

		if "android-arm" == _OPTIONS["compiler"] then

			if not os.getenv("ANDROID_NDK_ARM") or not os.getenv("ANDROID_NDK_ROOT") then
				print("Set ANDROID_NDK_ARM and ANDROID_NDK_ROOT envrionment variables.")
			end

			premake.gcc.cc = "$(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-gcc"
			premake.gcc.cxx = "$(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-g++"
			premake.gcc.ar = "$(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-ar"
			location (_projDir .. _ACTION .. "-android-arm")
		end

		if "android-mips" == _OPTIONS["compiler"] then

			if not os.getenv("ANDROID_NDK_MIPS") or not os.getenv("ANDROID_NDK_ROOT") then
				print("Set ANDROID_NDK_MIPS and ANDROID_NDK_ROOT envrionment variables.")
			end

			premake.gcc.cc = "$(ANDROID_NDK_MIPS)/bin/mipsel-linux-android-gcc"
			premake.gcc.cxx = "$(ANDROID_NDK_MIPS)/bin/mipsel-linux-android-g++"
			premake.gcc.ar = "$(ANDROID_NDK_MIPS)/bin/mipsel-linux-android-ar"
			location (_projDir .. _ACTION .. "-android-mips")
		end

		if "android-x86" == _OPTIONS["compiler"] then

			if not os.getenv("ANDROID_NDK_X86") or not os.getenv("ANDROID_NDK_ROOT") then
				print("Set ANDROID_NDK_X86 and ANDROID_NDK_ROOT envrionment variables.")
			end

			premake.gcc.cc = "$(ANDROID_NDK_X86)/bin/i686-linux-android-gcc"
			premake.gcc.cxx = "$(ANDROID_NDK_X86)/bin/i686-linux-android-g++"
			premake.gcc.ar = "$(ANDROID_NDK_X86)/bin/i686-linux-android-ar"
			location (_projDir .. _ACTION .. "-android-x86")
		end

		if "asmjs" == _OPTIONS["compiler"] then

			if not os.getenv("EMSCRIPTEN") then
				print("Set EMSCRIPTEN enviroment variables.")
			end

			premake.gcc.cc = "$(EMSCRIPTEN)/emcc"
			premake.gcc.cxx = "$(EMSCRIPTEN)/em++"
			premake.gcc.ar = "ar"
			location (_projDir .. _ACTION .. "-asmjs")
		end

		if "linux-gcc" == _OPTIONS["compiler"] then
			location (_projDir .. _ACTION .. "-linux")
		end

		if "linux-clang" == _OPTIONS["compiler"] then
			premake.gcc.cc = "clang"
			premake.gcc.cxx = "clang++"
			premake.gcc.ar = "ar"
			location (_projDir .. _ACTION .. "-linux-clang")
		end

		if "mingw" == _OPTIONS["compiler"] then
			premake.gcc.cc = "$(MINGW)/bin/x86_64-w64-mingw32-gcc"
			premake.gcc.cxx = "$(MINGW)/bin/x86_64-w64-mingw32-g++"
			premake.gcc.ar = "$(MINGW)/bin/ar"
			location (_projDir .. _ACTION .. "-mingw")
		end

		if "nacl" == _OPTIONS["compiler"] then

			if not os.getenv("NACL_SDK_ROOT") then
				print("Set NACL_SDK_ROOT enviroment variables.")
			end

			naclToolchain = "$(NACL_SDK_ROOT)/toolchain/win_x86_newlib/bin/x86_64-nacl-"
			if os.is("macosx") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/mac_x86_newlib/bin/x86_64-nacl-"
			elseif os.is("linux") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/linux_x86_newlib/bin/x86_64-nacl-"
			end

			premake.gcc.cc  = naclToolchain .. "gcc"
			premake.gcc.cxx = naclToolchain .. "g++"
			premake.gcc.ar  = naclToolchain .. "ar"
			location (_projDir .. _ACTION .. "-nacl")
		end

		if "nacl-arm" == _OPTIONS["compiler"] then

			if not os.getenv("NACL_SDK_ROOT") then
				print("Set NACL_SDK_ROOT enviroment variables.")
			end

			naclToolchain = "$(NACL_SDK_ROOT)/toolchain/win_arm_newlib/bin/arm-nacl-"
			if os.is("macosx") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/mac_arm_newlib/bin/arm-nacl-"
			elseif os.is("linux") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/linux_arm_newlib/bin/arm-nacl-"
			end

			premake.gcc.cc  = naclToolchain .. "gcc"
			premake.gcc.cxx = naclToolchain .. "g++"
			premake.gcc.ar  = naclToolchain .. "ar"
			location (_projDir .. _ACTION .. "-nacl-arm")
		end

		if "pnacl" == _OPTIONS["compiler"] then

			if not os.getenv("NACL_SDK_ROOT") then
				print("Set NACL_SDK_ROOT enviroment variables.")
			end

			naclToolchain = "$(NACL_SDK_ROOT)/toolchain/win_pnacl/bin/pnacl-"
			if os.is("macosx") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/mac_pnacl/bin/pnacl-"
			elseif os.is("linux") then
				naclToolchain = "$(NACL_SDK_ROOT)/toolchain/linux_pnacl/bin/pnacl-"
			end

			premake.gcc.cc  = naclToolchain .. "clang"
			premake.gcc.cxx = naclToolchain .. "clang++"
			premake.gcc.ar  = naclToolchain .. "ar"
			location (_projDir .. _ACTION .. "-pnacl")
		end

		if "osx" == _OPTIONS["compiler"] then
			location (_projDir .. _ACTION .. "-osx")
		end

		if "ios-arm" == _OPTIONS["compiler"] then
			premake.gcc.cc = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
			premake.gcc.cxx = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
			premake.gcc.ar = "ar"
			location (_projDir .. _ACTION .. "-ios-arm")
		end

		if "ios-simulator" == _OPTIONS["compiler"] then
			premake.gcc.cc = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
			premake.gcc.cxx = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
			premake.gcc.ar = "ar"
			location (_projDir .. _ACTION .. "-ios-simulator")
		end

		if "qnx-arm" == _OPTIONS["compiler"] then

			if not os.getenv("QNX_HOST") then
				print("Set QNX_HOST enviroment variables.")
			end

			premake.gcc.cc = "$(QNX_HOST)/usr/bin/arm-unknown-nto-qnx8.0.0eabi-gcc"
			premake.gcc.cxx = "$(QNX_HOST)/usr/bin/arm-unknown-nto-qnx8.0.0eabi-g++"
			premake.gcc.ar = "$(QNX_HOST)/usr/bin/arm-unknown-nto-qnx8.0.0eabi-ar"
			location (_projDir .. _ACTION .. "-qnx-arm")
		end
	end

	flags {
		"StaticRuntime",
		"NativeWChar",
		"NoPCH",
		"NoRTTI",
		"NoExceptions",
		"NoEditAndContinue",
		"Symbols",
	}

	defines {
		"__STDC_LIMIT_MACROS",
		"__STDC_FORMAT_MACROS",
		"__STDC_CONSTANT_MACROS",
	}

	configuration { "Debug" }
		targetsuffix "Debug"

	configuration { "Release" }
		flags {
			"OptimizeSpeed",
		    "NoPCH",
		}
		targetsuffix "Release"

	configuration { "vs*" }
		includedirs { _bxDir .. "include/compat/msvc" }
		defines {
			"WIN32",
			"_WIN32",
			"_HAS_EXCEPTIONS=0",
			"_HAS_ITERATOR_DEBUGGING=0",
			"_SCL_SECURE=0",
			"_SECURE_SCL=0",
			"_SCL_SECURE_NO_WARNINGS",
			"_CRT_SECURE_NO_WARNINGS",
			"_CRT_SECURE_NO_DEPRECATE",
		}
		linkoptions {
			"/ignore:4221", -- LNK4221: This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library
		}

	configuration { "vs*", "Debug" }
		buildoptions
		{
			"/Oy-"
		}

	configuration { "vs*", "Release" }
		flags
		{
			"NoFramePointer"
		}
		buildoptions
		{
			"/Ob2", -- The Inline Function Expansion.
			"/Ox",  -- Full optimization.
			"/Oi",  -- Enable intrinsics.
			"/Ot",  -- Favor fast code.
		}

	configuration { "vs2008" }
		includedirs { _bxDir .. "include/compat/msvc/pre1600" }

	configuration { "x32", "vs*" }
		flags
		{
			"EnableSSE2",
		}
		targetdir (_buildDir .. "win32_" .. _ACTION .. "/bin")
		objdir (_buildDir .. "win32_" .. _ACTION .. "/obj")
		libdirs {
			_libDir .. "lib/win32_" .. _ACTION,
			"$(DXSDK_DIR)/lib/x86",
		}

	configuration { "x64", "vs*" }
		defines { "_WIN64" }
		targetdir (_buildDir .. "win64_" .. _ACTION .. "/bin")
		objdir (_buildDir .. "win64_" .. _ACTION .. "/obj")
		libdirs {
			_libDir .. "lib/win64_" .. _ACTION,
			"$(DXSDK_DIR)/lib/x64",
		}

	configuration { "mingw" }
		defines { "WIN32" }
		includedirs { _bxDir .. "include/compat/mingw" }
		buildoptions {
			"-std=c++11",
			"-U__STRICT_ANSI__",
			"-Wunused-value",
			"-fdata-sections",
			"-ffunction-sections",
			"-msse2",
		}
		linkoptions {
			"-Wl,--gc-sections",
		}

	configuration { "x32", "mingw" }
		targetdir (_buildDir .. "win32_mingw" .. "/bin")
		objdir (_buildDir .. "win32_mingw" .. "/obj")
		libdirs {
			_libDir .. "lib/win32_mingw",
			"$(DXSDK_DIR)/lib/x86",
		}
		buildoptions { "-m32" }

	configuration { "x64", "mingw" }
		targetdir (_buildDir .. "win64_mingw" .. "/bin")
		objdir (_buildDir .. "win64_mingw" .. "/obj")
		libdirs {
			_libDir .. "lib/win64_mingw",
			"$(DXSDK_DIR)/lib/x64",
			"$(GLES_X64_DIR)",
		}
		buildoptions { "-m64" }

	configuration { "linux-gcc and not linux-clang" }
		buildoptions {
			"-mfpmath=sse", -- force SSE to get 32-bit and 64-bit builds deterministic.
		}

	configuration { "linux-clang" }
		buildoptions {
			"--analyze",
		}

	configuration { "linux-*" }
		buildoptions {
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-Wunused-value",
			"-msse2",
		}
		links {
			"rt",
		}
		linkoptions {
			"-Wl,--gc-sections",
		}

	configuration { "linux-gcc", "x32" }
		targetdir (_buildDir .. "linux32_gcc" .. "/bin")
		objdir (_buildDir .. "linux32_gcc" .. "/obj")
		libdirs { _libDir .. "lib/linux32_gcc" }
		buildoptions {
			"-m32",
		}

	configuration { "linux-gcc", "x64" }
		targetdir (_buildDir .. "linux64_gcc" .. "/bin")
		objdir (_buildDir .. "linux64_gcc" .. "/obj")
		libdirs { _libDir .. "lib/linux64_gcc" }
		buildoptions {
			"-m64",
		}

	configuration { "linux-clang", "x32" }
		targetdir (_buildDir .. "linux32_clang" .. "/bin")
		objdir (_buildDir .. "linux32_clang" .. "/obj")
		libdirs { _libDir .. "lib/linux32_clang" }
		buildoptions {
			"-m32",
		}

	configuration { "linux-clang", "x64" }
		targetdir (_buildDir .. "linux64_clang" .. "/bin")
		objdir (_buildDir .. "linux64_clang" .. "/obj")
		libdirs { _libDir .. "lib/linux64_clang" }
		buildoptions {
			"-m64",
		}

	configuration { "android-*" }
		flags {
			"NoImportLib",
		}
		includedirs {
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/include",
			"$(ANDROID_NDK_ROOT)/sources/android/native_app_glue",
		}
		linkoptions {
			"-nostdlib",
			"-static-libgcc",
		}
		links {
			"c",
			"dl",
			"m",
			"android",
			"log",
			"gnustl_static",
			"gcc",
		}
		buildoptions {
			"-fPIC",
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-Wno-psabi", -- note: the mangling of 'va_list' has changed in GCC 4.4.0
			"-no-canonical-prefixes",
			"-Wa,--noexecstack",
			"-fstack-protector",
			"-ffunction-sections",
		}
		linkoptions {
			"-no-canonical-prefixes",
			"-Wl,--no-undefined",
			"-Wl,-z,noexecstack",
			"-Wl,-z,relro",
			"-Wl,-z,now",
		}

	configuration { "android-arm" }
		targetdir (_buildDir .. "android-arm" .. "/bin")
		objdir (_buildDir .. "android-arm" .. "/obj")
		libdirs {
			_libDir .. "lib/android-arm",
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a",
		}
		includedirs {
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include",
		}
		buildoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-arm",
			"-mthumb",
			"-march=armv7-a",
			"-mfloat-abi=softfp",
			"-mfpu=neon",
		}
		linkoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-arm",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-arm/usr/lib/crtbegin_so.o",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-arm/usr/lib/crtend_so.o",
			"-march=armv7-a",
			"-Wl,--fix-cortex-a8",
		}

	configuration { "android-mips" }
		targetdir (_buildDir .. "android-mips" .. "/bin")
		objdir (_buildDir .. "android-mips" .. "/obj")
		libdirs {
			_libDir .. "lib/android-mips",
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/mips",
		}
		includedirs {
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/mips/include",
		}
		buildoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-mips",
		}
		linkoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-mips",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-mips/usr/lib/crtbegin_so.o",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-mips/usr/lib/crtend_so.o",
		}

	configuration { "android-x86" }
		targetdir (_buildDir .. "android-x86" .. "/bin")
		objdir (_buildDir .. "android-x86" .. "/obj")
		libdirs {
			_libDir .. "lib/android-x86",
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/x86",
		}
		includedirs {
			"$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/x86/include",
		}
		buildoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-x86",
			"-march=i686",
			"-mtune=atom",
			"-mstackrealign",
			"-msse3",
			"-mfpmath=sse",
		}
		linkoptions {
			"--sysroot=$(ANDROID_NDK_ROOT)/platforms/android-14/arch-x86",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-x86/usr/lib/crtbegin_so.o",
			"$(ANDROID_NDK_ROOT)/platforms/android-14/arch-x86/usr/lib/crtend_so.o",
		}

	configuration { "asmjs" }
		targetdir (_buildDir .. "asmjs" .. "/bin")
		objdir (_buildDir .. "asmjs" .. "/obj")
		libdirs { _libDir .. "lib/asmjs" }
		includedirs {
			"$(EMSCRIPTEN)/system/include",
			"$(EMSCRIPTEN)/system/include/libc",
		}
		buildoptions {
			"-Wno-unknown-warning-option", -- Linux Emscripten doesn't know about no-warn-absolute-paths...
			"-Wno-warn-absolute-paths",
		}

	configuration { "nacl or nacl-arm or pnacl" }
		includedirs {
			"$(NACL_SDK_ROOT)/include",
			_bxDir .. "include/compat/nacl",
		}

	configuration { "nacl" }
		buildoptions {
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-pthread",
			"-fno-stack-protector",
			"-fdiagnostics-show-option",
			"-Wunused-value",
			"-fdata-sections",
			"-ffunction-sections",
			"-mfpmath=sse", -- force SSE to get 32-bit and 64-bit builds deterministic.
			"-msse2",
		}
		linkoptions {
			"-Wl,--gc-sections",
		}

	configuration { "x32", "nacl" }
		targetdir (_buildDir .. "nacl-x86" .. "/bin")
		objdir (_buildDir .. "nacl-x86" .. "/obj")
		libdirs { _libDir .. "lib/nacl-x86" }
		linkoptions { "-melf32_nacl" }

	configuration { "x32", "nacl", "Debug" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_x86_32/Debug" }

	configuration { "x32", "nacl", "Release" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_x86_32/Release" }

	configuration { "x64", "nacl" }
		targetdir (_buildDir .. "nacl-x64" .. "/bin")
		objdir (_buildDir .. "nacl-x64" .. "/obj")
		libdirs { _libDir .. "lib/nacl-x64" }
		linkoptions { "-melf64_nacl" }

	configuration { "x64", "nacl", "Debug" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_x86_64/Debug" }

	configuration { "x64", "nacl", "Release" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_x86_64/Release" }

	configuration { "nacl-arm" }
		buildoptions {
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-fno-stack-protector",
			"-fdiagnostics-show-option",
			"-Wunused-value",
			"-Wno-psabi", -- note: the mangling of 'va_list' has changed in GCC 4.4.0
			"-fdata-sections",
			"-ffunction-sections",
		}
		targetdir (_buildDir .. "nacl-arm" .. "/bin")
		objdir (_buildDir .. "nacl-arm" .. "/obj")
		libdirs { _libDir .. "lib/nacl-arm" }

	configuration { "nacl-arm", "Debug" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_arm/Debug" }

	configuration { "nacl-arm", "Release" }
		libdirs { "$(NACL_SDK_ROOT)/lib/newlib_arm/Release" }

	configuration { "pnacl" }
		buildoptions {
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-fno-stack-protector",
			"-fdiagnostics-show-option",
			"-Wunused-value",
			"-fdata-sections",
			"-ffunction-sections",
		}
		targetdir (_buildDir .. "pnacl" .. "/bin")
		objdir (_buildDir .. "pnacl" .. "/obj")
		libdirs { _libDir .. "lib/pnacl" }

	configuration { "pnacl", "Debug" }
		libdirs { "$(NACL_SDK_ROOT)/lib/pnacl/Debug" }

	configuration { "pnacl", "Release" }
		libdirs { "$(NACL_SDK_ROOT)/lib/pnacl/Release" }

	configuration { "Xbox360" }
		targetdir (_buildDir .. "xbox360" .. "/bin")
		objdir (_buildDir .. "xbox360" .. "/obj")
		includedirs { _bxDir .. "include/compat/msvc" }
		libdirs { _libDir .. "lib/xbox360" }
		defines {
			"NOMINMAX",
			"_XBOX",
		}

	configuration { "osx", "x32" }
		targetdir (_buildDir .. "osx32_gcc" .. "/bin")
		objdir (_buildDir .. "osx32_gcc" .. "/obj")
		libdirs { _libDir .. "lib/osx32_gcc" }
		buildoptions {
			"-m32",
		}

	configuration { "osx", "x64" }
		targetdir (_buildDir .. "osx64_gcc" .. "/bin")
		objdir (_buildDir .. "osx64_gcc" .. "/obj")
		libdirs { _libDir .. "lib/osx64_gcc" }
		buildoptions {
			"-m64",
		}

	configuration { "osx" }
		buildoptions {
			"-U__STRICT_ANSI__",
			"-Wfatal-errors",
			"-Wunused-value",
			"-msse2",
		}
		includedirs { _bxDir .. "include/compat/osx" }

	configuration { "ios-*" }
		linkoptions {
			"-lc++",
		}
		buildoptions {
			"-miphoneos-version-min=7.0",
			"-U__STRICT_ANSI__",
			"-Wfatal-errors",
			"-Wunused-value",
		}
		includedirs { _bxDir .. "include/compat/ios" }

	configuration { "ios-arm" }
		targetdir (_buildDir .. "ios-arm" .. "/bin")
		objdir (_buildDir .. "ios-arm" .. "/obj")
		libdirs { _libDir .. "lib/ios-arm" }
		linkoptions {
			"-arch armv7",
			"--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS7.1.sdk",
			"-L/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS7.1.sdk/usr/lib/system",
			"-F/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS7.1.sdk/System/Library/Frameworks",
			"-F/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS7.1.sdk/System/Library/PrivateFrameworks",
		}
		buildoptions {
			"-arch armv7",
			"--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS7.1.sdk",
		}

	configuration { "ios-simulator" }
		targetdir (_buildDir .. "ios-simulator" .. "/bin")
		objdir (_buildDir .. "ios-simulator" .. "/obj")
		libdirs { _libDir .. "lib/ios-simulator" }
		linkoptions {
			"-arch i386",
			"--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.1.sdk",
			"-L/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.1.sdk/usr/lib/system",
			"-F/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.1.sdk/System/Library/Frameworks",
			"-F/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.1.sdk/System/Library/PrivateFrameworks",
		}
		buildoptions {
			"-arch i386",
			"--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.1.sdk",
		}

	configuration { "qnx-arm" }
		targetdir (_buildDir .. "qnx-arm" .. "/bin")
		objdir (_buildDir .. "qnx-arm" .. "/obj")
		libdirs { _libDir .. "lib/qnx-arm" }
--		includedirs { _bxDir .. "include/compat/qnx" }
		buildoptions {
			"-std=c++0x",
			"-U__STRICT_ANSI__",
			"-Wno-psabi", -- note: the mangling of 'va_list' has changed in GCC 4.4.0
		}

	configuration {} -- reset configuration
end

function strip()

	configuration { "android-arm", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@$(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-strip -s \"$(TARGET)\""
		}

	configuration { "android-mips", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@$(ANDROID_NDK_MIPS)/bin/mipsel-linux-android-strip -s \"$(TARGET)\""
		}

	configuration { "android-x86", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@$(ANDROID_NDK_X86)/bin/i686-linux-android-strip -s \"$(TARGET)\""
		}

	configuration { "linux-*", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@strip -s \"$(TARGET)\""
		}

	configuration { "mingw", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@$(MINGW)/bin/strip -s \"$(TARGET)\""
		}

	configuration { "pnacl" }
		postbuildcommands {
			"@echo Running pnacl-finalize.",
			"@" .. naclToolchain .. "finalize \"$(TARGET)\""
		}

	configuration { "*nacl*", "Release" }
		postbuildcommands {
			"@echo Stripping symbols.",
			"@" .. naclToolchain .. "strip -s \"$(TARGET)\""
		}

	configuration { "asmjs" }
		postbuildcommands {
			"@echo Running asmjs finalize.",
			"@$(EMSCRIPTEN)/emcc -O2 -s TOTAL_MEMORY=268435456 \"$(TARGET)\" -o \"$(TARGET)\".html"
			-- ALLOW_MEMORY_GROWTH
		}

	configuration {} -- reset configuration
end

-- vim: set sw=4 ts=4 expandtab:
