--- UNIVERSAL
set_arch("x86_64")
set_plat("linux")

--- RULES/POLICIES
add_rules("mode.debug", "mode.release"); set_defaultmode("debug")
add_rules("plugin.compile_commands.autoupdate")
-- set_policy("build.progress_style", "multirow")
-- add_rules("c++.unity_build")


--- TOOLCHAIN
toolchain("dotty.gnu")
    set_kind("standalone"); set_toolset("cxx", "g++")
    set_toolset("as",    "as"); set_toolset("ar",    "ar")
    set_toolset("ld",    "g++"); set_toolset("sh",    "g++")
    set_toolset("ex",    "g++"); set_toolset("strip", "strip")
toolchain_end()


toolchain("dotty.llvm")
    set_kind("standalone"); set_toolset("cxx", "clang++");
    set_toolset("as",    "clang"); set_toolset("ar",    "llvm-ar")
    set_toolset("ld",    "clang++"); set_toolset("sh",    "clang++")
    set_toolset("ex",    "clang++"); set_toolset("strip", "llvm-strip")
toolchain_end()

set_toolchains("dotty.gnu")


--- HOOK
target("script")
    set_kind("phony")
    set_policy("build.fence", true)
    on_build(function(target)
        os.exec("git submodule update --init")
    end)
target_end()

--- SCRIPT
    if is_mode("debug") then
        set_optimize("none")
        -- set_symbols("debug")
        set_symbols("none")
    elseif is_mode("release") then
        add_defines("DEBUG_ON=0")
        set_optimize("fastest")
        set_symbols("none")
        set_strip("all")
    end
--- SCRIPT-END



--- DEPENDENCIES
add_requires("cli11", {system = true})

--- GLOBAL
set_languages("c++23")
add_includedirs("include/", "core/include/", "vendor/", "deps/")
set_pcxxheader("include/common.hpp")

--- TESTS
target("input") add_files("tests/input.cpp")
target("exec") add_files("tests/exec.cpp")

--- TARGETS
target("core")
    set_kind("static")
    add_files("core/src/*.cpp")
    add_deps("script")
    add_packages("bat")

target("dotty")
    set_kind("binary")
    add_files("src/*.cpp")
    add_deps("core")
    add_packages("cli11")
