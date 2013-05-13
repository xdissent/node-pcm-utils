#include "unzipper.h"

using namespace pcmutils;

void Unzipper::Init(Handle<Object> exports) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(String::NewSymbol("Unzipper"));

  NODE_SET_PROTOTYPE_METHOD(tpl, "unzip", Unzip);

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("Unzipper"), constructor);
}

Handle<Value> Unzipper::New(const Arguments& args) {
  HandleScope scope;

  if (!args.IsConstructCall())
    return ThrowException(Exception::TypeError(String::New("Use the new operator")));

  REQUIRE_ARGUMENTS(2);

  Unzipper* unz = new Unzipper();
  unz->Wrap(args.This());

  unz->channels = args[0]->Int32Value();
  unz->alignment = args[1]->Int32Value();
  unz->frameAlignment = unz->channels * unz->alignment;
  unz->unzipping = false;

  unz->channelBuffers = Persistent<Array>::New(Array::New(unz->channels));
  for (int i = 0; i < unz->channels; i++) {
    size_t blen = unz->alignment * UNZ_BUFFER_FRAMES;
    Buffer* b = Buffer::New(blen);
    unz->channelBuffers->Set(i, b->handle_);
  }

  return args.This();
}

Handle<Value> Unzipper::Unzip(const Arguments& args) {
  HandleScope scope;

  REQUIRE_ARGUMENTS(2);
  REQUIRE_ARGUMENT_FUNCTION(1, callback);

  Unzipper* unz = ObjectWrap::Unwrap<Unzipper>(args.Holder());

  COND_ERR_CALL(unz->unzipping, callback, "Still unzipping");

  UnzipBaton* baton = new UnzipBaton(unz, callback, args[0]->ToObject()); 
  unz->unzipping = true;
  BeginUnzip(baton);

  return scope.Close(args.Holder());
}

void Unzipper::BeginUnzip(Baton* baton) {
  uv_queue_work(uv_default_loop(), &baton->request, DoUnzip, (uv_after_work_cb)AfterUnzip);
}

void Unzipper::DoUnzip(uv_work_t* req) {
  UnzipBaton* baton = static_cast<UnzipBaton*>(req->data);
  Unzipper* unz = baton->unz;

  int sample = 0;
  int limitFrames = baton->unzippedFrames + UNZ_BUFFER_FRAMES;
  for (; baton->unzippedFrames < limitFrames && baton->unzippedFrames < baton->totalFrames; baton->unzippedFrames++) {
    for (int channel = 0; channel < unz->channels; channel++) {
      memcpy(
        baton->channelData[channel] + (sample * unz->alignment),
        baton->chunkData + (baton->unzippedFrames * unz->frameAlignment) + (channel * unz->alignment),
        unz->alignment
      );
    }
    sample++;
  }
}

void Unzipper::AfterUnzip(uv_work_t* req) {
  HandleScope scope;

  UnzipBaton* baton = static_cast<UnzipBaton*>(req->data);
  Unzipper* unz = baton->unz;

  // Copy buffers because we may clobber them soon.
  Local<Array> channelBuffersCopy = Local<Array>::New(Array::New(unz->channels));
  for (int i = 0; i < unz->channels; i++) {
    // Yes, copy is ok
    size_t blen = unz->alignment * UNZ_BUFFER_FRAMES;
    Buffer* b = Buffer::New(Buffer::Data(unz->channelBuffers->Get(i)->ToObject()), blen);
    channelBuffersCopy->Set(i, b->handle_);
  }

  if (baton->unzippedFrames < baton->totalFrames) {
    Local<Value> argv[3] = { Local<Value>::New(Null()), Local<Value>::New(channelBuffersCopy), Local<Value>::New(Boolean::New(false)) };
    TRY_CATCH_CALL(unz->handle_, baton->callback, 3, argv);
    BeginUnzip(baton);
    return;
  }

  unz->unzipping = false;
  Local<Value> argv[3] = { Local<Value>::New(Null()), Local<Value>::New(channelBuffersCopy), Local<Value>::New(Boolean::New(true)) };
  TRY_CATCH_CALL(unz->handle_, baton->callback, 3, argv);
  delete baton;
}