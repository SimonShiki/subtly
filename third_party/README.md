Native packages include the following upstream notices and the project's MIT
license. Users download speech-model weights separately.

- `moonshine-LICENSE`: [Moonshine Voice 0.1.6](https://pypi.org/project/moonshine-voice/0.1.6/).
- `sherpa-onnx-LICENSE`: [sherpa-onnx 2.0.1](https://github.com/k2-fsa/sherpa-onnx/blob/v2.0.1/LICENSE).
- `onnxruntime-LICENSE`: [ONNX Runtime MIT license](https://github.com/microsoft/onnxruntime/blob/v1.28.2/LICENSE).
- `onnxruntime-1.28.2-ThirdPartyNotices.txt`: [notices for sherpa-onnx's bundled ONNX Runtime](https://github.com/microsoft/onnxruntime/blob/v1.28.2/ThirdPartyNotices.txt).
- `onnxruntime-1.23.2-ThirdPartyNotices.txt`: [notices for Moonshine's bundled ONNX Runtime](https://github.com/microsoft/onnxruntime/blob/v1.23.2/ThirdPartyNotices.txt).

The ONNX Runtime versions listed here come from the pinned prebuilt libraries.
The packaging stage also copies notices installed by the dependency recipes,
including the NeMo-Speech.cpp notices when that optional backend is enabled.
