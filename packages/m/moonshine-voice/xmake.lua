package("moonshine-voice")
    set_homepage("https://github.com/moonshine-ai/moonshine")
    set_description("Moonshine Voice native CPU runtime (from the official Linux wheel)")
    set_license("MIT")

    if is_plat("linux") and is_arch("x86_64") then
        add_urls("https://files.pythonhosted.org/packages/54/47/20993e0676642cf05273b6b2fed13076a5b4028156c938dd86136bb71797/moonshine_voice-$(version)-py3-none-manylinux_2_34_x86_64.whl",
                 {filename = "moonshine-voice.zip"})
        add_versions("0.1.6", "04e438f393af3f963ac7988204f379d04a9c8949801595cab4bd68a7ceedf6c7")
    end

    add_links("moonshine")

    on_install("linux|x86_64", function (package)
        os.cp("moonshine_voice/libmoonshine.so", package:installdir("lib"))
        -- Preserve the wheel's relative rpath and unique ONNX Runtime SONAME.
        os.cp("moonshine_voice.libs", package:installdir())
        os.cp("moonshine_voice-*.dist-info/licenses/*", package:installdir("share/licenses/moonshine-voice"))
    end)
package_end()
