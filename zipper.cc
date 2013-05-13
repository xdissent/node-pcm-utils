#include "zipper.h"

using namespace pcmutils;

void Zipper::Init(Handle<Object> exports) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(String::NewSymbol("Zipper"));

  NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);

  NODE_SET_GETTER(tpl, "channelBuffers", ChannelBuffersGetter);
  NODE_SET_GETTER(tpl, "channelsReady", ChannelsReadyGetter);
  NODE_SET_GETTER(tpl, "samplesPerBuffer", SamplesPerBufferGetter);
  NODE_SET_GETTER(tpl, "zipping", ZippingGetter);

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("Zipper"), constructor);
}

Handle<Value> Zipper::New(const Arguments& args) {
  HandleScope scope;

  if (!args.IsConstructCall())
    return ThrowException(Exception::TypeError(String::New("Use the new operator")));

  REQUIRE_ARGUMENTS(3);
  REQUIRE_ARGUMENT_FUNCTION(2, callback);

  Zipper* zip = new Zipper();
  zip->Wrap(args.This());

  zip->channels = args[0]->Int32Value();
  zip->alignment = args[1]->Int32Value();
  zip->frameAlignment = zip->alignment * zip->channels;
  zip->callback = Persistent<Function>::New(callback);
  zip->zipping = false;

  zip->channelBuffers = Persistent<Array>::New(Array::New(zip->channels));

  zip->channelsReady = Persistent<Array>::New(Array::New(zip->channels));
  for (int i = 0; i < zip->channels; i++) {
    zip->channelsReady->Set(i, Boolean::New(false));
  }

  zip->buffer = (char*)malloc(ZIP_BUFFER_SAMPLES * zip->frameAlignment);

  return args.This();
}

Handle<Value> Zipper::ChannelBuffersGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Zipper* zip = ObjectWrap::Unwrap<Zipper>(accessor.This());
  return zip->channelBuffers;
}

Handle<Value> Zipper::ChannelsReadyGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Zipper* zip = ObjectWrap::Unwrap<Zipper>(accessor.This());
  return zip->channelsReady;
}

Handle<Value> Zipper::SamplesPerBufferGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  return scope.Close(Integer::New(ZIP_BUFFER_SAMPLES));
}

Handle<Value> Zipper::ZippingGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Zipper* zip = ObjectWrap::Unwrap<Zipper>(accessor.This());
  return scope.Close(Boolean::New(zip->zipping));
}

Handle<Value> Zipper::Write(const Arguments& args) {
  HandleScope scope;

  REQUIRE_ARGUMENTS(2);
  OPTIONAL_ARGUMENT_FUNCTION(2, callback);

  Zipper* zip = ObjectWrap::Unwrap<Zipper>(args.Holder());

  COND_ERR_CALL(zip->zipping, callback, "Still zipping");
  int channel = args[0]->Int32Value();
  COND_ERR_CALL(zip->channelsReady->Get(channel)->BooleanValue(), callback, "Already Ready");

  zip->channelBuffers->Set(channel, args[1]->ToObject());
  zip->channelsReady->Set(channel, Boolean::New(true));

  WriteBaton* baton = new WriteBaton(zip, callback, channel); 
  BeginWrite(baton);

  return scope.Close(args.Holder());
}

void Zipper::BeginWrite(Baton* baton) {
  uv_queue_work(uv_default_loop(), &baton->request, DoWrite, (uv_after_work_cb)AfterWrite);
}

void Zipper::DoWrite(uv_work_t* req) {
  // Just a hop thru the evq
}

void Zipper::AfterWrite(uv_work_t* req) {
  HandleScope scope;

  WriteBaton* baton = static_cast<WriteBaton*>(req->data);
  Zipper* zip = baton->zip;

  if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
    Local<Value> argv[0] = { };
    TRY_CATCH_CALL(zip->handle_, baton->callback, 0, argv);
  }

  if (!zip->zipping) {
    bool ready = true;
    for (int i = 0; i < zip->channels; i++) {
      if (!zip->channelsReady->Get(i)->BooleanValue()) ready = false;
    }

    if (ready) {
      ZipBaton* zipBaton = new ZipBaton(zip);
      zip->zipping = true;
      BeginZip(zipBaton);
    }
  }

  delete baton;
}

void Zipper::BeginZip(Baton* baton) {
  uv_queue_work(uv_default_loop(), &baton->request, DoZip, (uv_after_work_cb)AfterZip);
}

void Zipper::DoZip(uv_work_t* req) {
  ZipBaton* baton = static_cast<ZipBaton*>(req->data);
  Zipper* zip = baton->zip;
  
  for (int sample = 0; sample < ZIP_BUFFER_SAMPLES; sample++) {
    for (int channel = 0; channel < zip->channels; channel++) {
      memcpy(
        zip->buffer + (sample * zip->frameAlignment) + (channel * zip->alignment),
        baton->channelData[channel] + (sample * zip->alignment),
        zip->alignment
      );
    }
  }
}

void Zipper::AfterZip(uv_work_t* req) {
  HandleScope scope;

  ZipBaton* baton = static_cast<ZipBaton*>(req->data);
  Zipper* zip = baton->zip;

  size_t blen = ZIP_BUFFER_SAMPLES * zip->frameAlignment;
  Buffer* buffer = Buffer::New(zip->buffer, blen);

  for (int i = 0; i < zip->channels; i++) {
    zip->channelsReady->Set(i, Boolean::New(false));
  }

  for (int i = 0; i < zip->channels; i++) {
    zip->channelBuffers->Set(i, Local<Value>::New(Null()));
  }

  zip->zipping = false;
  delete baton;

  if (!zip->callback.IsEmpty() && zip->callback->IsFunction()) {
    Local<Value> argv[2] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    TRY_CATCH_CALL(zip->handle_, zip->callback, 2, argv);
  }
}