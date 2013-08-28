#ifndef ANIMATED_GIF_H
#define ANIMATED_GIF_H

#include <node.h>
#include <node_buffer.h>

#include "gif_encoder.h"
#include "common.h"

class AnimatedGif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;

    AnimatedGifEncoder gif_encoder;
    Color transparency_color;
    unsigned char *data;

public:
    NanCallback *ondata;

    static void Initialize(v8::Handle<v8::Object> target);

    AnimatedGif(int wwidth, int hheight, buffer_type bbuf_type);
    v8::Handle<v8::Value> Push(unsigned char *data_buf, int x, int y, int w, int h);
    void EndPush();

    ~AnimatedGif() {
        if (ondata) {
            delete ondata;
        }
    }

    static NAN_METHOD(New);
    static NAN_METHOD(Push);
    static NAN_METHOD(EndPush);
    static NAN_METHOD(End);
    static NAN_METHOD(GetGif);
    static NAN_METHOD(SetOutputFile);
    static NAN_METHOD(SetOutputCallback);
};

#endif
