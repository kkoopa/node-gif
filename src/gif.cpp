#include <cstdlib>
#include <cstring>
#include "common.h"
#include "gif_encoder.h"
#include "gif.h"

using namespace v8;
using namespace node;

void
Gif::Initialize(Handle<Object> target)
{
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "encode", GifEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(t, "encodeSync", GifEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(t, "setTransparencyColor", SetTransparencyColor);
    target->Set(String::NewSymbol("Gif"), t->GetFunction());
}

Gif::Gif(int wwidth, int hheight, buffer_type bbuf_type) :
  width(wwidth), height(hheight), buf_type(bbuf_type) {}

Handle<Value>
Gif::GifEncodeSync()
{
    NanScope();

    Local<Value> buf_val = NanObjectWrapHandle(this)->GetHiddenValue(String::New("buffer"));

    char *buf_data = Buffer::Data(buf_val->ToObject());

    try {
        GifEncoder encoder((unsigned char*)buf_data, width, height, buf_type);
        if (transparency_color.color_present) {
            encoder.set_transparency_color(transparency_color);
        }
        encoder.encode();
        int gif_len = encoder.get_gif_len();
        Local<Object> retbuf = NanNewBufferHandle(gif_len);
        memcpy(Buffer::Data(retbuf), encoder.get_gif(), gif_len);
        return scope.Close(retbuf);
    }
    catch (const char *err) {
        return ThrowException(Exception::Error(String::New(err)));
    }
}

void
Gif::SetTransparencyColor(unsigned char r, unsigned char g, unsigned char b)
{
    transparency_color = Color(r, g, b, true);
}

NAN_METHOD(Gif::New)
{
    NanScope();

    if (args.Length() < 3)
        return NanThrowError("At least three arguments required - data buffer, width, height, [and input buffer type]");
    if (!Buffer::HasInstance(args[0]))
        return NanThrowTypeError("First argument must be Buffer.");
    if (!args[1]->IsInt32())
        return NanThrowTypeError("Second argument must be integer width.");
    if (!args[2]->IsInt32())
        return NanThrowTypeError("Third argument must be integer height.");

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 4) {
        if (!args[3]->IsString())
            return NanThrowTypeError("Fourth argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::AsciiValue bts(args[3]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            return NanThrowTypeError("Fourth argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        if (str_eq(*bts, "rgb"))
            buf_type = BUF_RGB;
        else if (str_eq(*bts, "bgr"))
            buf_type = BUF_BGR;
        else if (str_eq(*bts, "rgba"))
            buf_type = BUF_RGBA;
        else if (str_eq(*bts, "bgra"))
            buf_type = BUF_BGRA;
        else
            return NanThrowTypeError("Fourth argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }


    int w = args[1]->Int32Value();
    int h = args[2]->Int32Value();

    if (w < 0)
        return NanThrowRangeError("Width smaller than 0.");
    if (h < 0)
        return NanThrowRangeError("Height smaller than 0.");

    Gif *gif = new Gif(w, h, buf_type);
    gif->Wrap(args.This());

    // Save buffer.
    NanObjectWrapHandle(gif)->SetHiddenValue(String::New("buffer"), args[0]);

    NanReturnValue(args.This());
}

NAN_METHOD(Gif::GifEncodeSync)
{
    NanScope();

    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());
    NanReturnValue(gif->GifEncodeSync());
}

NAN_METHOD(Gif::SetTransparencyColor)
{
    NanScope();

    if (args.Length() != 3)
        return NanThrowError("Three arguments required - r, g, b");

    if (!args[0]->IsInt32())
        return NanThrowTypeError("First argument must be integer red.");
    if (!args[1]->IsInt32())
        return NanThrowTypeError("Second argument must be integer green.");
    if (!args[2]->IsInt32())
        return NanThrowTypeError("Third argument must be integer blue.");

    unsigned char r = args[0]->Int32Value();
    unsigned char g = args[1]->Int32Value();
    unsigned char b = args[2]->Int32Value();

    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());
    gif->SetTransparencyColor(r, g, b);

    NanReturnUndefined();
}

void Gif::GifEncodeWorker::Execute() {
    try {
        GifEncoder encoder((unsigned char *)buf_data, gif_obj->width, gif_obj->height, gif_obj->buf_type);
        if (gif_obj->transparency_color.color_present) {
            encoder.set_transparency_color(gif_obj->transparency_color);
        }
        encoder.encode();
        gif_len = encoder.get_gif_len();
        gif = (char *)malloc(sizeof(*gif)*gif_len);
        if (!gif) {
            errmsg = strdup("malloc in Gif::GifEncodeWorker::Execute() failed.");
        }
        else {
            memcpy(gif, encoder.get_gif(), gif_len);
        }
    }
    catch (const char *err) {
        errmsg = strdup(err);
    }
}

void Gif::GifEncodeWorker::HandleOKCallback() {
    NanScope();

    Local<Object> buf = NanNewBufferHandle(gif_len);
    memcpy(Buffer::Data(buf), gif, gif_len);
    Local<Value> argv[2] = {buf, Undefined()};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    free(gif);
    gif = NULL;

    gif_obj->Unref();
}

void Gif::GifEncodeWorker::HandleErrorCallback() {
    NanScope();
    Local<Value> argv[2] = {Undefined(), v8::Exception::Error(v8::String::New(errmsg))};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    if (gif) {
        free(gif);
        gif = NULL;
    }
}

NAN_METHOD(Gif::GifEncodeAsync)
{
    NanScope();

    if (args.Length() != 1)
        return NanThrowError("One argument required - callback function.");

    if (!args[0]->IsFunction())
        return NanThrowTypeError("First argument must be a function.");

    Local<Function> callback = Local<Function>::Cast(args[0]);
    Gif *gif = ObjectWrap::Unwrap<Gif>(args.This());

    // We need to pull out the buffer data before
    // we go to the thread pool.
    Local<Value> buf_val = NanObjectWrapHandle(gif)->GetHiddenValue(String::New("buffer"));

    NanAsyncQueueWorker(new Gif::GifEncodeWorker(new NanCallback(callback), gif, Buffer::Data(buf_val->ToObject())));

    gif->Ref();

    NanReturnUndefined();
}
