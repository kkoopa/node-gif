#include "common.h"
#include "gif_encoder.h"
#include "dynamic_gif_stack.h"

using namespace v8;
using namespace node;

std::pair<Point, Point>
DynamicGifStack::optimal_dimension()
{
    Point top(-1, -1), bottom(-1, -1);
    for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
        GifUpdate *gif = *it;
        if (top.x == -1 || gif->x < top.x)
            top.x = gif->x;
        if (top.y == -1 || gif->y < top.y)
            top.y = gif->y;
        if (bottom.x == -1 || gif->x + gif->w > bottom.x)
            bottom.x = gif->x + gif->w;
        if (bottom.y == -1 || gif->y + gif->h > bottom.y)
            bottom.y = gif->y + gif->h;
    }

    return std::make_pair(top, bottom);
}

void
DynamicGifStack::construct_gif_data(unsigned char *data, Point &top)
{
    switch (buf_type) {
    case BUF_RGB:
    case BUF_RGBA:
        for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
            GifUpdate *gif = *it;
            int start = (gif->y - top.y)*width*3 + (gif->x - top.x)*3;
            unsigned char *gifdatap = gif->data;
            for (int i = 0; i < gif->h; i++) {
                unsigned char *datap = &data[start + i*width*3];
                for (int j = 0; j < gif->w; j++) {
                    *datap++ = *gifdatap++;
                    *datap++ = *gifdatap++;
                    *datap++ = *gifdatap++;
                    if (buf_type == BUF_RGBA) gifdatap++;
                }
            }
        }
        break;

    case BUF_BGR:
    case BUF_BGRA:
        for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it) {
            GifUpdate *gif = *it;
            int start = (gif->y - top.y)*width*3 + (gif->x - top.x)*3;
            unsigned char *gifdatap = gif->data;
            for (int i = 0; i < gif->h; i++) {
                unsigned char *datap = &data[start + i*width*3];
                for (int j = 0; j < gif->w; j++) {
                    *datap++ = *(gifdatap + 2);
                    *datap++ = *(gifdatap + 1);
                    *datap++ = *gifdatap;
                    gifdatap += (buf_type == BUF_BGRA) ? 4 : 3;
                }
            }
        }
        break;

    default:
        throw "Unexpected buf_type in DynamicGifStack::GifEncode";
    }
}

void
DynamicGifStack::Initialize(Handle<Object> target)
{
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "push", Push);
    NODE_SET_PROTOTYPE_METHOD(t, "encode", GifEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(t, "encodeSync", GifEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(t, "dimensions", Dimensions);
    target->Set(String::NewSymbol("DynamicGifStack"), t->GetFunction());
}

DynamicGifStack::DynamicGifStack(buffer_type bbuf_type) :
    buf_type(bbuf_type), transparency_color(0xFF, 0xFF, 0xFE) {}

DynamicGifStack::~DynamicGifStack()
{
    for (GifUpdates::iterator it = gif_stack.begin(); it != gif_stack.end(); ++it)
        delete *it;
}

Handle<Value>
DynamicGifStack::Push(unsigned char *buf_data, size_t buf_len, int x, int y, int w, int h)
{
    NanScope();

    try {
        GifUpdate *gif_update = new GifUpdate(buf_data, buf_len, x, y, w, h);
        gif_stack.push_back(gif_update);
        return scope.Close(Undefined());
    }
    catch (const char *e) {
        return scope.Close(ThrowException(Exception::Error(String::New(e))));
    }
}

Handle<Value>
DynamicGifStack::GifEncodeSync()
{
    NanScope();

    std::pair<Point, Point> optimal = optimal_dimension();
    Point top = optimal.first, bot = optimal.second;

    offset = top;
    width = bot.x - top.x;
    height = bot.y - top.y;

    unsigned char *data = (unsigned char*)malloc(sizeof(*data)*width*height*3);
    if (!data) return scope.Close(ThrowException(Exception::Error(String::New("malloc failed in DynamicGifStack::GifEncode"))));

    unsigned char *datap = data;
    for (int i = 0; i < width*height*3; i+=3) {
        *datap++ = transparency_color.r;
        *datap++ = transparency_color.g;
        *datap++ = transparency_color.b;
    }

    construct_gif_data(data, top);

    try {
        GifEncoder encoder(data, width, height, BUF_RGB);
        encoder.set_transparency_color(transparency_color);
        encoder.encode();
        free(data);
        int gif_len = encoder.get_gif_len();
        Local<Object> retbuf = NanNewBufferHandle(gif_len);
        memcpy(Buffer::Data(retbuf), encoder.get_gif(), gif_len);
        return scope.Close(retbuf);
    }
    catch (const char *err) {
        return scope.Close(ThrowException(Exception::Error(String::New(err))));
    }
}

Handle<Value>
DynamicGifStack::Dimensions()
{
    NanScope();

    Local<Object> dim = Object::New();
    dim->Set(String::NewSymbol("x"), Integer::New(offset.x));
    dim->Set(String::NewSymbol("y"), Integer::New(offset.y));
    dim->Set(String::NewSymbol("width"), Integer::New(width));
    dim->Set(String::NewSymbol("height"), Integer::New(height));

    return scope.Close(dim);
}

NAN_METHOD(DynamicGifStack::New)
{
    NanScope();

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 1) {
        if (!args[0]->IsString())
            return NanThrowTypeError("First argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");

        String::AsciiValue bts(args[0]->ToString());
        if (!(str_eq(*bts, "rgb") || str_eq(*bts, "bgr") ||
            str_eq(*bts, "rgba") || str_eq(*bts, "bgra")))
        {
            return NanThrowTypeError("First argument must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
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
            return NanThrowTypeError("First argument wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
    }

    DynamicGifStack *gif_stack = new DynamicGifStack(buf_type);
    gif_stack->Wrap(args.This());
    NanReturnValue(args.This());
}

NAN_METHOD(DynamicGifStack::Push)
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

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());

    Local<Object> buf_obj = args[0]->ToObject();
    char *buf_data = Buffer::Data(buf_obj);
    size_t buf_len = Buffer::Length(buf_obj);

    NanReturnValue(gif_stack->Push((unsigned char*)buf_data, buf_len, x, y, w, h));
}

NAN_METHOD(DynamicGifStack::Dimensions)
{
    NanScope();

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());
    NanReturnValue(gif_stack->Dimensions());
}

NAN_METHOD(DynamicGifStack::GifEncodeSync)
{
    NanScope();

    DynamicGifStack *gif_stack = ObjectWrap::Unwrap<DynamicGifStack>(args.This());
    NanReturnValue(gif_stack->GifEncodeSync());
}

void DynamicGifStack::DynamicGifEncodeWorker::Execute() {
    std::pair<Point, Point> optimal = gif_obj->optimal_dimension();
    Point top = optimal.first, bot = optimal.second;

    gif_obj->offset = top;
    gif_obj->width = bot.x - top.x;
    gif_obj->height = bot.y - top.y;

    unsigned char *data = (unsigned char*)malloc(sizeof(*data)*gif_obj->width*gif_obj->height*3);
    if (!data) {
        errmsg = strdup("malloc failed in DynamicGifStack::DynamicGifEncodeWorker::Execute().");
        return;
    }

    unsigned char *datap = data;
    for (int i = 0; i < gif_obj->width*gif_obj->height; i++) {
        *datap++ = gif_obj->transparency_color.r;
        *datap++ = gif_obj->transparency_color.g;
        *datap++ = gif_obj->transparency_color.b;
    }

    gif_obj->construct_gif_data(data, top);

    try {
        GifEncoder encoder(data, gif_obj->width, gif_obj->height, BUF_RGB);
        encoder.set_transparency_color(gif_obj->transparency_color);
        encoder.encode();
        free(data);
        gif_len = encoder.get_gif_len();
        gif = (char *)malloc(sizeof(*gif)*gif_len);
        if (!gif) {
            errmsg = strdup("malloc in DynamicGifStack::DynamicGifEncodeWorker::Execute() failed.");
            return;
        }
        else {
            memcpy(gif, encoder.get_gif(), gif_len);
        }
    }
    catch (const char *err) {
        free(data);
        errmsg = strdup(err);
    }
}

void DynamicGifStack::DynamicGifEncodeWorker::HandleOKCallback() {
    NanScope();
    Local<Object> buf = NanNewBufferHandle(gif_len);
    memcpy(Buffer::Data(buf), gif, gif_len);
    Local<Value> argv[3] = {buf, gif_obj->Dimensions(), Undefined()};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(3, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    free(gif);
    gif = NULL;

    gif_obj->Unref();
}

void DynamicGifStack::DynamicGifEncodeWorker::HandleErrorCallback() {
    NanScope();
    Local<Value> argv[3] = {Undefined(), Undefined(), Exception::Error(String::New(errmsg))};

    TryCatch try_catch; // don't quite see the necessity of this

    callback->Call(3, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    if (gif) {
        free(gif);
        gif = NULL;
    }

    gif_obj->Unref();
}

NAN_METHOD(DynamicGifStack::GifEncodeAsync)
{
    NanScope();

    if (args.Length() != 1)
        return NanThrowError("One argument required - callback function.");

    if (!args[0]->IsFunction())
        return NanThrowTypeError("First argument must be a function.");

    Local<Function> callback = Local<Function>::Cast(args[0]);
    DynamicGifStack *gif = ObjectWrap::Unwrap<DynamicGifStack>(args.This());

    NanAsyncQueueWorker(new DynamicGifStack::DynamicGifEncodeWorker(new NanCallback(callback), gif));

    gif->Ref();

    NanReturnUndefined();
}

