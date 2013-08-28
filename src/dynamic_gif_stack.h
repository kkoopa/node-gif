#ifndef DYNAMIC_gif_STACK_H
#define DYNAMIC_gif_STACK_H

#include <node.h>
#include <node_buffer.h>

#include <utility>
#include <vector>

#include <cstdlib>

#include "common.h"

struct GifUpdate {
    int len, x, y, w, h;
    unsigned char *data;

    GifUpdate(unsigned char *ddata, int llen, int xx, int yy, int ww, int hh) :
        len(llen), x(xx), y(yy), w(ww), h(hh)
    {
        data = (unsigned char *)malloc(sizeof(*data)*len);
        if (!data) throw "malloc failed in DynamicGifStack::GifUpdate";
        memcpy(data, ddata, len);
    }

    ~GifUpdate() {
        free(data);
    }
};

class DynamicGifStack : public node::ObjectWrap {
    typedef std::vector<GifUpdate *> GifUpdates;
    GifUpdates gif_stack;

    Point offset;
    int width, height;
    buffer_type buf_type;
    Color transparency_color;

    std::pair<Point, Point> optimal_dimension();

    void construct_gif_data(unsigned char *data, Point &top);

public:
    static void Initialize(v8::Handle<v8::Object> target);
    DynamicGifStack(buffer_type bbuf_type);
    ~DynamicGifStack();

    class DynamicGifEncodeWorker : public GifEncoder::EncodeWorker {
    public:
        DynamicGifEncodeWorker(NanCallback *callback, DynamicGifStack *gif) : GifEncoder::EncodeWorker(callback), gif_obj(gif) {
        };

        void Execute();
        void HandleOKCallback();
        void HandleErrorCallback();

    private:
        DynamicGifStack *gif_obj;
    };


    v8::Handle<v8::Value> Push(unsigned char *buf_data, size_t buf_len, int x, int y, int w, int h);
    v8::Handle<v8::Value> Dimensions();
    v8::Handle<v8::Value> GifEncodeSync();

    static NAN_METHOD(New);
    static NAN_METHOD(Push);
    static NAN_METHOD(Dimensions);
    static NAN_METHOD(GifEncodeSync);
    static NAN_METHOD(GifEncodeAsync);
};

#endif

