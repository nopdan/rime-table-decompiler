add_rules("mode.debug", "mode.release")

if is_plat("wasm") then
    add_requires("boost[signals2] >=1.82.0", {system = false})
else
    add_requires("boost[signals2,interprocess,iostreams,container] >=1.82.0", {system = false})
end
add_requires("marisa")
if not is_plat("wasm") then
    add_requires("glog")
end
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
if is_plat("wasm") then
    add_requires("emscripten")
    set_toolchains("emcc@emscripten")
end

target("rime_table_decompiler", function()
    set_languages("cxx17")
    set_kind("binary")
    add_files("src/rime_table_decompiler.cc")
    add_files("src/rime/**.cc")
    add_includedirs("src")
    add_includedirs("include")

    if not is_plat("wasm") then
        add_defines("RIME_ENABLE_LOGGING")
    end
    add_packages("boost")
    add_packages("marisa")
    if not is_plat("wasm") then
        add_packages("glog")
    end
    set_targetdir("build/bin/")
end)

target("rime_table_v3_decompiler", function()
    set_languages("cxx17")
    set_kind("binary")
    add_files("src/rime_table_decompiler.cc")
    add_files("v3/rime/**.cc")
    add_includedirs("v3")
    add_includedirs("include")

    if not is_plat("wasm") then
        add_defines("RIME_ENABLE_LOGGING")
    end
    add_defines("TABLE_V3")
    add_packages("boost")
    add_packages("marisa")
    if not is_plat("wasm") then
        add_packages("glog")
    end
    set_targetdir("build/bin/")
end)

target("rime_table_decompiler_wasm", function()
    set_languages("cxx17")
    set_kind("binary")
    set_extension(".wasm")
    add_files("src/rime_table_decompiler.cc")
    add_files("src/rime/**.cc")
    add_includedirs("src")
    add_includedirs("include")

    add_defines("RIME_WASM")
    add_packages("boost")
    add_packages("marisa")

    if is_plat("wasm") then
        add_ldflags("-sALLOW_MEMORY_GROWTH=1", {force = true})
        add_ldflags("-sSTANDALONE_WASM=1", {force = true})
        add_ldflags("-sEXPORTED_FUNCTIONS=['_decompile','_decompile_free','_malloc','_free']", {force = true})
        add_ldflags("-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']", {force = true})
        add_ldflags("-Wl,--no-entry", {force = true})
        add_ldflags("-Wl,--export=__wasm_call_ctors", {force = true})
    end
    set_targetdir("build/bin/")
end)

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--
