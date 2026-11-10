package("nemo-speech")
    set_homepage("https://github.com/NVIDIA/NeMo-Speech.cpp")
    set_description("NVIDIA NeMo Speech native ASR runtime")
    set_license("Apache-2.0")

    if is_plat("linux") and is_arch("x86_64") then
        add_urls("https://github.com/NVIDIA/NeMo-Speech.cpp/releases/download/v$(version)/nemo-speech-$(version)-linux-x86_64-cpu.tar.gz")
        add_versions("0.3.0", "255f12b722ab15b2a38a51cf125d56e3fa97058b528f7e895b39d6a473deb06e")
    end

    add_links("nemo_speech_asr_c")

    on_install("linux|x86_64", function (package)
        os.cp("include", package:installdir())
        os.cp("lib/*.so*", package:installdir("lib"), {symlink = true})
        os.rm(package:installdir("lib/libstdc++.so*"))
        os.rm(package:installdir("lib/libgcc_s.so*"))
        os.rm(package:installdir("lib/libnemo_speech_nmt*.so*"))
        os.rm(package:installdir("lib/libnemo_speech_tts*.so*"))
        os.cp("share/licenses/nemo-speech", package:installdir("share/licenses"))
    end)
package_end()
