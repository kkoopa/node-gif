#ifndef ASYNC_ANIMATED_GIF_H
#define ASYNC_ANIMATED_GIF_H

#include <string>

#include <node.h>
#include <node_buffer.h>

#include "gif_encoder.h"
#include "common.h"

class PushWorker : public NanAsyncWorker {
public:
    PushWorker(unsigned int push_id, unsigned int fragment_id, const char *tmp_dir, unsigned char *data_buf, int x, int y, int w, int h) : NanAsyncWorker(NULL), push_id(push_id), fragment_id(fragment_id), tmp_dir(tmp_dir), data_size(w * h * 3), x(x), y(y), w(w), h(h) {
        data = new unsigned char[sizeof(*data) * w * h * 3];
        if (!data) {
            throw "malloc in AsyncAnimatedGif::Push failed.";
        }

        memcpy(data, data_buf, w * h * 3);
    };

    ~PushWorker() {
        delete[] data;
    }

    void Execute();
    void HandleOKCallback();
    void HandleErrorCallback();

protected:
    unsigned int push_id;
    unsigned int fragment_id;
    const char *tmp_dir;
    unsigned char *data;
    int data_size;
    int x, y, w, h;
};

class AsyncAnimatedGif;

class AsyncAnimatedGif : public node::ObjectWrap {
    int width, height;
    buffer_type buf_type;

    Color transparency_color;

    unsigned int push_id, fragment_id;
    std::string tmp_dir, output_file;

    static unsigned char *init_frame(int width, int height, Color &transparency_color);
    static void push_fragment(unsigned char *frame, int width, int height, buffer_type buf_type,
        unsigned char *fragment, int x, int y, int w, int h);
    static Rect rect_dims(const char *fragment_name);

public:
    static void Initialize(v8::Handle<v8::Object> target);

    AsyncAnimatedGif(int wwidth, int hheight, buffer_type bbuf_type);
    v8::Handle<v8::Value> Push(unsigned char *data_buf, int x, int y, int w, int h);
    void EndPush();

    class AnimatedGifEncodeWorker : public AnimatedGifEncoder::EncodeWorker {
    public:
        AnimatedGifEncodeWorker(NanCallback *callback, AsyncAnimatedGif *gif) : AnimatedGifEncoder::EncodeWorker(callback), gif_obj(gif) {
        };

        void Execute();
        void HandleOKCallback();
        void HandleErrorCallback();

    private:
        AsyncAnimatedGif *gif_obj;
    };

    static NAN_METHOD(New);
    static NAN_METHOD(Push);
    static NAN_METHOD(Encode);
    static NAN_METHOD(EndPush);
    static NAN_METHOD(SetOutputFile);
    static NAN_METHOD(SetTmpDir);
};

#endif
