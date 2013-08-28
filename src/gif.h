#ifndef NODE_GIF_H
#define NODE_GIF_H

#include <node.h>
#include <node_buffer.h>

#include "common.h"
#include "gif_encoder.h"

class Gif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;
    Color transparency_color;

public:
    static void Initialize(v8::Handle<v8::Object> target);
    Gif(int wwidth, int hheight, buffer_type bbuf_type);
    v8::Handle<v8::Value> GifEncodeSync();
    void SetTransparencyColor(unsigned char r, unsigned char g, unsigned char b);

    class GifEncodeWorker : public GifEncoder::EncodeWorker {
    public:
        GifEncodeWorker(NanCallback *callback, Gif *gif, char *buf_data) : GifEncoder::EncodeWorker(callback, buf_data), gif_obj(gif) {
        };

        void Execute();
        void HandleOKCallback();
        void HandleErrorCallback();

    private:
        Gif *gif_obj;
    };

    static NAN_METHOD(New);
    static NAN_METHOD(GifEncodeSync);
    static NAN_METHOD(GifEncodeAsync);
    static NAN_METHOD(SetTransparencyColor);
};

#endif

