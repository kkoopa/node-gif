#include <cerrno>
#include <cstdlib>

#include "common.h"
#include "utils.h"
#include "gif_encoder.h"
#include "async_animated_gif.h"

using namespace v8;
using namespace node;

void
AsyncAnimatedGif::Initialize(Handle<Object> target)
{
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "push", Push);
    NODE_SET_PROTOTYPE_METHOD(t, "endPush", EndPush);
    NODE_SET_PROTOTYPE_METHOD(t, "encode", Encode);
    NODE_SET_PROTOTYPE_METHOD(t, "setOutputFile", SetOutputFile);
    NODE_SET_PROTOTYPE_METHOD(t, "setTmpDir", SetTmpDir);
    target->Set(String::NewSymbol("AsyncAnimatedGif"), t->GetFunction());
}

AsyncAnimatedGif::AsyncAnimatedGif(int wwidth, int hheight, buffer_type bbuf_type) :
    width(wwidth), height(hheight), buf_type(bbuf_type),
    transparency_color(0xFF, 0xFF, 0xFE),
    push_id(0), fragment_id(0) {}

void PushWorker::Execute() {
    char fragment_dir[512];
    snprintf(fragment_dir, 512, "%s/%d", tmp_dir, push_id);

    if (!is_dir(fragment_dir)) {
        if (mkdir(fragment_dir, 0775) == -1) {
            fprintf(stderr, "Could not mkdir(%s) in PushWorker::Execute().\n",
                fragment_dir);
            return;
        }
    }

    char filename[512];
    snprintf(filename, 512, "%s/%d/rect-%d-%d-%d-%d-%d.dat",
        tmp_dir, push_id, fragment_id,
        x, y, w, h);
    FILE *out = fopen(filename, "w+");
    if (!out) {
        fprintf(stderr, "Failed to open %s in PushWorker::Execute().\n",
            filename);
        return;
    }
    int written = fwrite(data, sizeof(unsigned char), data_size, out);
    if (written != data_size) {
        fprintf(stderr, "Failed to write all data to %s. Wrote only %d of %d.\n",
            filename, written, data_size);
    }

    fclose(out);
}

void PushWorker::HandleOKCallback() {
}

void PushWorker::HandleErrorCallback() {
}

Handle<Value>
AsyncAnimatedGif::Push(unsigned char *data_buf, int x, int y, int w, int h)
{
    NanScope();

    if (tmp_dir.empty())
        throw "Tmp dir is not set. Use .setTmpDir to set it before pushing.";

    if (output_file.empty())
        throw "Output file is not set. Use .setOutputFile to set it before pushing.";

    NanAsyncQueueWorker(new PushWorker(push_id, fragment_id++, tmp_dir.c_str(), data_buf, x, y, w, h));

    return scope.Close(Undefined());
}


void
AsyncAnimatedGif::EndPush()
{
    push_id++;
    fragment_id = 0;
}

NAN_METHOD(AsyncAnimatedGif::New)
{
    NanScope();

    if (args.Length() < 2)
        return NanThrowError("At least two arguments required - width, height, [and input buffer type]");
    if (!args[0]->IsInt32())
        return NanThrowTypeError("First argument must be integer width.");
    if (!args[1]->IsInt32())
        return NanThrowTypeError("Second argument must be integer height.");

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 3) {
        if (!args[2]->IsString())
            return NanThrowTypeError("Third argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::AsciiValue bts(args[2]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            return NanThrowTypeError("Third argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
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
            return NanThrowTypeError("Third argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }

    int w = args[0]->Int32Value();
    int h = args[1]->Int32Value();

    if (w < 0)
        return NanThrowRangeError("Width smaller than 0.");
    if (h < 0)
        return NanThrowRangeError("Height smaller than 0.");

    AsyncAnimatedGif *gif = new AsyncAnimatedGif(w, h, buf_type);
    gif->Wrap(args.This());
    NanReturnValue(args.This());
}

NAN_METHOD(AsyncAnimatedGif::Push)
{
    NanScope();

    if (!Buffer::HasInstance(args[0]))
        return NanThrowTypeError("First argument must be Buffer.");
    if (!args[1]->IsInt32())
        return NanThrowTypeError("Second argument must be integer x.");
    if (!args[2]->IsInt32())
        return NanThrowTypeError("Third argument must be integer y.");
    if (!args[3]->IsInt32())
        return NanThrowTypeError("Fourth argument must be integer w.");
    if (!args[4]->IsInt32())
        return NanThrowTypeError("Fifth argument must be integer h.");

    AsyncAnimatedGif *gif = ObjectWrap::Unwrap<AsyncAnimatedGif>(args.This());
    int x = args[1]->Int32Value();
    int y = args[2]->Int32Value();
    int w = args[3]->Int32Value();
    int h = args[4]->Int32Value();

    if (x < 0)
        return NanThrowRangeError("Coordinate x smaller than 0.");
    if (y < 0)
        return NanThrowRangeError("Coordinate y smaller than 0.");
    if (w < 0)
        return NanThrowRangeError("Width smaller than 0.");
    if (h < 0)
        return NanThrowRangeError("Height smaller than 0.");
    if (x >= gif->width)
        return NanThrowRangeError("Coordinate x exceeds AsyncAnimatedGif's dimensions.");
    if (y >= gif->height)
        return NanThrowRangeError("Coordinate y exceeds AsyncAnimatedGif's dimensions.");
    if (x+w > gif->width)
        return NanThrowRangeError("Pushed fragment exceeds AsyncAnimatedGif's width.");
    if (y+h > gif->height)
        return NanThrowRangeError("Pushed fragment exceeds AsyncAnimatedGif's height.");

    try {
        char *buf_data = Buffer::Data(args[0]->ToObject());
        gif->Push((unsigned char*)buf_data, x, y, w, h);
    }
    catch (const char *err) {
        return NanThrowError(err);
    }

    NanReturnUndefined();
}

NAN_METHOD(AsyncAnimatedGif::EndPush)
{
    NanScope();

    AsyncAnimatedGif *gif = ObjectWrap::Unwrap<AsyncAnimatedGif>(args.This());
    gif->EndPush();

    NanReturnUndefined();
}

int
fragment_sort(const void *aa, const void *bb)
{
    const char *a = *(const char **)aa;
    const char *b = *(const char **)bb;
    int na, nb;
    sscanf(a, "rect-%d", &na);
    sscanf(b, "rect-%d", &nb);
    return na > nb;
}

unsigned char *
AsyncAnimatedGif::init_frame(int width, int height, Color &transparency_color)
{
    unsigned char *frame = (unsigned char *)malloc(sizeof(*frame)*width*height*3);
    if (!frame) return NULL;

    unsigned char *framgep = frame;
    for (int i = 0; i < width*height; i++) {
        *framgep++ = transparency_color.r;
        *framgep++ = transparency_color.g;
        *framgep++ = transparency_color.b;
    }

    return frame;
}

void
AsyncAnimatedGif::push_fragment(unsigned char *frame, int width, int height,
    buffer_type buf_type, unsigned char *fragment, int x, int y, int w, int h)
{
    int start = y*width*3 + x*3;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *framep = &frame[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *framep++ = *fragment++;
                *framep++ = *fragment++;
                *framep++ = *fragment++;
            }
        }
        break;
    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *framep = &frame[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *framep++ = *(fragment + 2);
                *framep++ = *(fragment + 1);
                *framep++ = *fragment;
                framep += 3;
            }
        }
        break;
    case BUF_RGBA:
        for (int i = 0; i < h; i++) {
            unsigned char *framep = &frame[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *framep++ = *fragment++;
                *framep++ = *fragment++;
                *framep++ = *fragment++;
                fragment++;
            }
        }
        break;
    case BUF_BGRA:
        for (int i = 0; i < h; i++) {
            unsigned char *framep = &frame[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *framep++ = *(fragment + 2);
                *framep++ = *(fragment + 1);
                *framep++ = *fragment;
                framep += 4;
            }
        }
        break;
    }
}

Rect
AsyncAnimatedGif::rect_dims(const char *fragment_name)
{
    int moo, x, y, w, h;
    sscanf(fragment_name, "rect-%d-%d-%d-%d-%d", &moo, &x, &y, &w, &h);
    return Rect(x, y, w, h);
}

void AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute() {
    AnimatedGifEncoder encoder(gif_obj->width, gif_obj->height, BUF_RGB);
    encoder.set_output_file(gif_obj->output_file.c_str());
    encoder.set_transparency_color(gif_obj->transparency_color);

    for (size_t push_id = 0; push_id < gif_obj->push_id; push_id++) {
        char fragment_path[512];
        snprintf(fragment_path, 512, "%s/%lu", gif_obj->tmp_dir.c_str(), push_id);
        if (!is_dir(fragment_path)) {
            char error[600];
            snprintf(error, 600, "Error in AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute() %s is not a dir.",
                fragment_path);
            errmsg = strdup(error);
            return;
        }

        char **fragments = find_files(fragment_path);
        int nfragments = file_list_length(fragments);

        qsort(fragments, nfragments, sizeof(char *), fragment_sort);

        unsigned char *frame = init_frame(gif_obj->width, gif_obj->height, gif_obj->transparency_color);
        if (!frame) {
            free_file_list(fragments);
            errmsg = strdup("malloc failed in AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute().");
            return;
        }

        for (int i = 0; i < nfragments; i++) {
            snprintf(fragment_path, 512, "%s/%lu/%s",
                gif_obj->tmp_dir.c_str(), push_id, fragments[i]);
            FILE *in = fopen(fragment_path, "r");
            if (!in) {
                free_file_list(fragments);
                free(frame);
                char error[600];
                snprintf(error, 600, "Failed opening %s in AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute().",
                    fragment_path);
                errmsg = strdup(error);
                return;
            }
            int size = file_size(fragment_path);
            unsigned char *data = (unsigned char *)malloc(sizeof(*data)*size);
            if (!data) {
                free_file_list(fragments);
                free(frame);
                fclose(in);
                errmsg = strdup("malloc failed in AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute().");
                return;
            }
            int read = fread(data, sizeof *data, size, in);
            if (read != size) {
                free_file_list(fragments);
                free(frame);
                fclose(in);
                free(data);
                char error[600];
                snprintf(error, 600, "Error - should have read %d but read only %d from %s in AsyncAnimatedGif::AnimatedGifEncodeWorker::Execute()", size, read, fragment_path);
                errmsg = strdup(error);
                return;
            }
            Rect dims = rect_dims(fragments[i]);
            push_fragment(frame, gif_obj->width, gif_obj->height, gif_obj->buf_type,
                data, dims.x, dims.y, dims.w, dims.h);
            fclose(in);
            free(data);
        }
        encoder.new_frame(frame);
        free_file_list(fragments);
        free(frame);
    }
    encoder.finish();

    return;
}

void AsyncAnimatedGif::AnimatedGifEncodeWorker::HandleOKCallback() {
    NanScope();
    Local<Value> argv[2] = {True(), Undefined()};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    gif_obj->Unref();
}

void AsyncAnimatedGif::AnimatedGifEncodeWorker::HandleErrorCallback() {
    NanScope();
    Local<Value> argv[2] = {False(), Exception::Error(String::New(errmsg))};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    gif_obj->Unref();
}

NAN_METHOD(AsyncAnimatedGif::Encode)
{
    NanScope();

    if (args.Length() != 1)
        return NanThrowError("One argument required - callback function.");

    if (!args[0]->IsFunction())
        return NanThrowTypeError("First argument must be a function.");

    Local<Function> callback = Local<Function>::Cast(args[0]);
    AsyncAnimatedGif *gif = ObjectWrap::Unwrap<AsyncAnimatedGif>(args.This());

    NanAsyncQueueWorker(new AsyncAnimatedGif::AnimatedGifEncodeWorker(new NanCallback(callback), gif));

    gif->Ref();

    NanReturnUndefined();
}

NAN_METHOD(AsyncAnimatedGif::SetOutputFile)
{
    NanScope();

    if (args.Length() != 1)
        return NanThrowError("One argument required - path to output file.");

    if (!args[0]->IsString())
        return NanThrowTypeError("First argument must be string.");

    String::AsciiValue file_name(args[0]->ToString());

    AsyncAnimatedGif *gif = ObjectWrap::Unwrap<AsyncAnimatedGif>(args.This());
    gif->output_file = *file_name;

    NanReturnUndefined();
}

NAN_METHOD(AsyncAnimatedGif::SetTmpDir)
{
    NanScope();

    if (args.Length() != 1)
        return NanThrowError("One argument required - path to tmp dir.");

    if (!args[0]->IsString())
        return NanThrowTypeError("First argument must be string.");

    String::AsciiValue tmp_dir(args[0].As<String>());

    AsyncAnimatedGif *gif = ObjectWrap::Unwrap<AsyncAnimatedGif>(args.This());
    gif->tmp_dir = *tmp_dir;

    NanReturnUndefined();
}
