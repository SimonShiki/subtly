set_project("subtly")
set_version("0.1.0")
set_xmakever("3.0.0")

if not get_config("qt") and os.isdir("/usr/lib/qt6") then
    set_config("qt", "/usr/lib/qt6")
end

add_rules("mode.debug", "mode.release")
set_languages("c++23")
set_warnings("all")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})

add_repositories("local-repo local-repo", {rootdir = os.scriptdir()})
add_requires("sherpa-onnx 2.0.1", {configs = {shared = true}})
option("moonshine_sdk")
    set_showmenu(true)
    set_description("Use an installed Moonshine SDK root containing lib/libmoonshine.so; skip downloading")
option_end()
if not get_config("moonshine_sdk") then
    add_requires("moonshine-voice 0.1.6")
end
option("nemotron")
    set_default(false)
    set_showmenu(true)
    set_description("Enable NeMo-Speech.cpp CPU ASR (Linux x86_64)")
option_end()
if has_config("nemotron") then
    add_requires("nemo-speech 0.3.0")
    add_defines("SUBTLY_NEMOTRON")
end
add_requires("pkgconfig::libpipewire-0.3", {alias = "pipewire", system = true})

target("subtly")
    add_rules("qt.quickapp")
    add_frameworks("QtCore", "QtGui", "QtNetwork", "QtQuick", "QtQml", "QtQuickControls2", "QtWidgets", "QtSvg")
    add_files("src/**.cpp")
    add_files("src/**.h")
    add_files("qml/qml.qrc")
    add_includedirs("src")
    add_packages("sherpa-onnx", "pipewire")
    if has_config("nemotron") then add_packages("nemo-speech") end
    if get_config("moonshine_sdk") then
        add_linkdirs(path.join(get_config("moonshine_sdk"), "lib"))
        add_rpathdirs(path.join(get_config("moonshine_sdk"), "lib"))
        add_links("moonshine")
    else
        add_packages("moonshine-voice")
    end
    add_syslinks("LayerShellQtInterface") -- from layer-shell-qt (KDE), headers in /usr/include/LayerShellQt
    -- Locate the bundled libsherpa-onnx-c-api.so and libonnxruntime.so at runtime.
    on_load(function (target)
        local pkg = target:pkg("sherpa-onnx")
        if pkg and pkg:installdir() then
            target:add("rpathdirs", path.join(pkg:installdir(), "lib"))
        end
        local nemo = target:pkg("nemo-speech")
        if nemo and nemo:installdir() then
            target:add("rpathdirs", path.join(nemo:installdir(), "lib"))
        end
        local moonshine = target:pkg("moonshine-voice")
        if moonshine and moonshine:installdir() then
            target:add("rpathdirs", path.join(moonshine:installdir(), "lib"))
        end
    end)

-- Offline engine check: xmake run subtly-bench <model-dir> <file.wav> [threads]
target("subtly-bench")
    set_kind("binary")
    set_default(false)
    add_files("tools/bench.cpp", "src/asr/AsrEngine.cpp", "src/asr/SherpaStreamingEngine.cpp", "src/asr/NemotronStreamingEngine.cpp", "src/asr/MoonshineStreamingEngine.cpp")
    add_includedirs("src")
    add_packages("sherpa-onnx")
    if has_config("nemotron") then add_packages("nemo-speech") end
    if get_config("moonshine_sdk") then
        add_linkdirs(path.join(get_config("moonshine_sdk"), "lib"))
        add_rpathdirs(path.join(get_config("moonshine_sdk"), "lib"))
        add_links("moonshine")
    else
        add_packages("moonshine-voice")
    end
    on_load(function (target)
        local pkg = target:pkg("sherpa-onnx")
        if pkg and pkg:installdir() then
            target:add("rpathdirs", path.join(pkg:installdir(), "lib"))
        end
        local nemo = target:pkg("nemo-speech")
        if nemo and nemo:installdir() then
            target:add("rpathdirs", path.join(nemo:installdir(), "lib"))
        end
        local moonshine = target:pkg("moonshine-voice")
        if moonshine and moonshine:installdir() then
            target:add("rpathdirs", path.join(moonshine:installdir(), "lib"))
        end
    end)
