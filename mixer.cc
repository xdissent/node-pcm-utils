#include "mixer.h"

using namespace pcmutils;

void Mixer::Init(Handle<Object> exports) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(String::NewSymbol("Mixer"));

  NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);

  NODE_SET_GETTER(tpl, "channelBuffers", ChannelBuffersGetter);
  NODE_SET_GETTER(tpl, "channelsReady", ChannelsReadyGetter);
  NODE_SET_GETTER(tpl, "samplesPerBuffer", SamplesPerBufferGetter);
  NODE_SET_GETTER(tpl, "mixing", MixingGetter);

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("Mixer"), constructor);
}

Handle<Value> Mixer::New(const Arguments& args) {
  HandleScope scope;

  if (!args.IsConstructCall())
    return ThrowException(Exception::TypeError(String::New("Use the new operator")));

  REQUIRE_ARGUMENTS(4);
  REQUIRE_ARGUMENT_FUNCTION(3, callback);

  Mixer* mix = new Mixer();
  mix->Wrap(args.This());

  mix->channels = args[0]->Int32Value();
  mix->alignment = args[1]->Int32Value();
  mix->format = args[2]->Int32Value();

  if (mix->format % 2 > 0)
    return ThrowException(Exception::TypeError(String::New("Big-Endian formats currently unsupported by Mixer")));

  mix->callback = Persistent<Function>::New(callback);
  mix->mixing = false;

  mix->channelBuffers = Persistent<Array>::New(Array::New(mix->channels));

  mix->channelsReady = Persistent<Array>::New(Array::New(mix->channels));
  for (int i = 0; i < mix->channels; i++) {
    mix->channelsReady->Set(i, Boolean::New(false));
  }

  return args.This();
}

Handle<Value> Mixer::ChannelBuffersGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Mixer* mix = ObjectWrap::Unwrap<Mixer>(accessor.This());
  return mix->channelBuffers;
}

Handle<Value> Mixer::ChannelsReadyGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Mixer* mix = ObjectWrap::Unwrap<Mixer>(accessor.This());
  return mix->channelsReady;
}

Handle<Value> Mixer::SamplesPerBufferGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  return scope.Close(Integer::New(MIX_BUFFER_SAMPLES));
}

Handle<Value> Mixer::MixingGetter(Local<String> str, const AccessorInfo& accessor) {
  HandleScope scope;
  Mixer* mix = ObjectWrap::Unwrap<Mixer>(accessor.This());
  return scope.Close(Boolean::New(mix->mixing));
}

Handle<Value> Mixer::Write(const Arguments& args) {
  HandleScope scope;

  REQUIRE_ARGUMENTS(2);
  OPTIONAL_ARGUMENT_FUNCTION(2, callback);

  Mixer* mix = ObjectWrap::Unwrap<Mixer>(args.Holder());

  COND_ERR_CALL(mix->mixing, callback, "Still mixing");
  int channel = args[0]->Int32Value();
  COND_ERR_CALL(mix->channelsReady->Get(channel)->BooleanValue(), callback, "Already Ready");

  mix->channelBuffers->Set(channel, args[1]->ToObject());
  mix->channelsReady->Set(channel, Boolean::New(true));

  WriteBaton* baton = new WriteBaton(mix, callback, channel); 
  BeginWrite(baton);

  return scope.Close(args.Holder());
}

void Mixer::BeginWrite(Baton* baton) {
  uv_queue_work(uv_default_loop(), &baton->request, DoWrite, (uv_after_work_cb)AfterWrite);
}

void Mixer::DoWrite(uv_work_t* req) {
  // Just a hop thru the evq
}

void Mixer::AfterWrite(uv_work_t* req) {
  HandleScope scope;

  WriteBaton* baton = static_cast<WriteBaton*>(req->data);
  Mixer* mix = baton->mix;

  if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
    Local<Value> argv[0] = { };
    TRY_CATCH_CALL(mix->handle_, baton->callback, 0, argv);
  }

  if (!mix->mixing) {
    bool ready = true;
    for (int i = 0; i < mix->channels; i++) {
      if (!mix->channelsReady->Get(i)->BooleanValue()) ready = false;
    }

    if (ready) {
      MixBaton* mixBaton = new MixBaton(mix);
      mix->mixing = true;
      BeginMix(mixBaton);
    }
  }

  delete baton;
}

void Mixer::BeginMix(Baton* baton) {
  uv_queue_work(uv_default_loop(), &baton->request, DoMix, (uv_after_work_cb)AfterMix);
}

void Mixer::DoMix(uv_work_t* req) {
  MixBaton* baton = static_cast<MixBaton*>(req->data);
  Mixer* mix = baton->mix;

  // Save back into first channel buffer
  if (mix->format == 0) {
    float* out = static_cast<float*>(static_cast<void*>(baton->channelData[0]));
    float sum;
    for (int i = 0; i < MIX_BUFFER_SAMPLES; i++) {
      sum = 0;
      for (int c = 0; c < mix->channels; c++) {
        float* in = static_cast<float*>(static_cast<void*>(baton->channelData[c]));
        sum += in[i] / mix->channels;
      }
      out[i] = sum;
    }
  } else if (mix->format == 2) {
    int16_t* out = static_cast<int16_t*>(static_cast<void*>(baton->channelData[0]));
    int16_t sum;
    for (int i = 0; i < MIX_BUFFER_SAMPLES; i++) {
      sum = 0;
      for (int c = 0; c < mix->channels; c++) {
        int16_t* in = static_cast<int16_t*>(static_cast<void*>(baton->channelData[c]));
        sum += in[i] / mix->channels;
      }
      out[i] = sum;
    }
  } else if (mix->format == 4) {
    uint16_t* out = static_cast<uint16_t*>(static_cast<void*>(baton->channelData[0]));
    uint16_t sum;
    for (int i = 0; i < MIX_BUFFER_SAMPLES; i++) {
      sum = 0;
      for (int c = 0; c < mix->channels; c++) {
        uint16_t* in = static_cast<uint16_t*>(static_cast<void*>(baton->channelData[c]));
        sum += (in[i] - 32768) / mix->channels;
      }
      out[i] = sum + 32768;
    }
  } else {
    fprintf(stderr, "Unsupported format\n");
  }
}

void Mixer::AfterMix(uv_work_t* req) {
  HandleScope scope;

  MixBaton* baton = static_cast<MixBaton*>(req->data);
  Mixer* mix = baton->mix;

  size_t blen = Buffer::Length(mix->channelBuffers->Get(0)->ToObject());
  Buffer* buffer = Buffer::New(Buffer::Data(mix->channelBuffers->Get(0)->ToObject()), blen);

  for (int i = 0; i < mix->channels; i++) {
    mix->channelsReady->Set(i, Boolean::New(false));
  }

  for (int i = 0; i < mix->channels; i++) {
    mix->channelBuffers->Set(i, Local<Value>::New(Null()));
  }

  mix->mixing = false;
  delete baton;

  if (!mix->callback.IsEmpty() && mix->callback->IsFunction()) {
    Local<Value> argv[2] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    TRY_CATCH_CALL(mix->handle_, mix->callback, 2, argv);
  }
}