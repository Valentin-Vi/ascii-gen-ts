#include <napi.h>
#include <cstring>
#include <vector>

extern "C" {
#include "converter.h"
}

// convertFrameNative(pixels: Buffer, fw: number, fh: number,
//                    cols: number, rows: number, ramp: string) -> Array<{char,r,g,b}>
Napi::Value ConvertFrameNative(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (info.Length() < 6) {
        Napi::TypeError::New(env, "Expected 6 arguments: pixels, fw, fh, cols, rows, ramp")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsBuffer()) {
        Napi::TypeError::New(env, "pixels must be a Buffer").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<uint8_t> pixelsBuf = info[0].As<Napi::Buffer<uint8_t>>();
    int fw   = info[1].As<Napi::Number>().Int32Value();
    int fh   = info[2].As<Napi::Number>().Int32Value();
    int cols = info[3].As<Napi::Number>().Int32Value();
    int rows = info[4].As<Napi::Number>().Int32Value();
    std::string ramp = info[5].As<Napi::String>().Utf8Value();

    if (ramp.size() < 2 || ramp.size() > 1024) {
        Napi::RangeError::New(env, "ramp length must be between 2 and 1024")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    std::vector<Cell> out(cols * rows);

    int rc = convert_frame(
        pixelsBuf.Data(),
        fw, fh,
        cols, rows,
        ramp.c_str(), (int)ramp.size(),
        out.data()
    );

    if (rc < 0) {
        Napi::Error::New(env, std::string("convert_frame failed with code ") + std::to_string(rc))
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Array result = Napi::Array::New(env, out.size());
    for (size_t i = 0; i < out.size(); i++) {
        Napi::Object cell = Napi::Object::New(env);
        char ch[2] = { out[i].c, '\0' };
        cell.Set("char", Napi::String::New(env, ch));
        cell.Set("r", Napi::Number::New(env, out[i].r));
        cell.Set("g", Napi::Number::New(env, out[i].g));
        cell.Set("b", Napi::Number::New(env, out[i].b));
        result.Set((uint32_t)i, cell);
    }

    return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("convertFrameNative",
        Napi::Function::New(env, ConvertFrameNative));
    return exports;
}

NODE_API_MODULE(iag_native, Init)
