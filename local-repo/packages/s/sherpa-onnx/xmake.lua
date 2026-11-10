package("sherpa-onnx")
    set_homepage("https://github.com/k2-fsa/sherpa-onnx")
    set_description("Speech recognition with next-gen Kaldi and onnxruntime (prebuilt shared C API)")
    set_license("Apache-2.0")

    if is_plat("linux") and is_arch("x86_64") then
        add_urls("https://github.com/k2-fsa/sherpa-onnx/releases/download/v$(version)/sherpa-onnx-v$(version)-linux-x64-shared.tar.bz2")
        add_versions("2.0.1", "2e823376987c3b6960d50726b6560155e2ad92da0b8b3886325773225ec6bdec")
    end

    add_links("sherpa-onnx-c-api", "onnxruntime")

    on_install("linux|x86_64", function (package)
        os.cp("include/*", package:installdir("include"))
        os.cp("lib/*.so*", package:installdir("lib"))
    end)

package_end()
