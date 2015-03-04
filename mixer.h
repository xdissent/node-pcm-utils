#ifndef MIXER_H
#define MIXER_H

#include <cstdlib>
#include <node.h>
#include <node_buffer.h>
#include "macros.h"

#define MIX_BUFFER_SAMPLES 1024

using namespace v8;
using namespace node;

namespace pcmutils {

class Mixer;

class Mixer : public ObjectWrap {
public:
  static void Init(Handle<Object> exports);

protected:
  Mixer() : ObjectWrap(), channels(0), alignment(0), format(0), mixing(false) {
    channelBuffers.Clear();
    channelsReady.Clear();
    callback.Clear();
  }

  ~Mixer() {
    channels = 0;
    alignment = 0;
    format = 0;
    mixing = false;
    channelBuffers.Dispose();
    channelsReady.Dispose();
    callback.Dispose();
  }

  struct Baton {
    uv_work_t request;
    Mixer* mix;

    Baton(Mixer* mix_) : mix(mix_) {
      mix->Ref();
      request.data = this;
    }
    virtual ~Baton() {
      mix->Unref();
    }
  };

  struct WriteBaton : Baton {
    Persistent<Function> callback;
    int channel;

    WriteBaton(Mixer* mix_, Handle<Function> cb_, int channel_) : Baton(mix_), channel(channel_) {
      callback = Persistent<Function>::New(cb_);
    }
    virtual ~WriteBaton() {
      callback.Dispose();
    }
  };

  struct MixBaton : Baton {
    char** channelData;

    MixBaton(Mixer* mix_) : Baton(mix_), channelData(NULL) {
      channelData = (char**)malloc(mix->channels * sizeof(char*));
      for (int i = 0; i < mix->channels; i++) {
        channelData[i] = Buffer::Data(mix->channelBuffers->Get(i)->ToObject());
      }
    }
    virtual ~MixBaton() {
      free(channelData);
    }
  };

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Write(const Arguments& args);
  static Handle<Value> ChannelBuffersGetter(Local<String> str, const AccessorInfo& accessor);
  static Handle<Value> ChannelsReadyGetter(Local<String> str, const AccessorInfo& accessor);
  static Handle<Value> SamplesPerBufferGetter(Local<String> str, const AccessorInfo& accessor);
  static Handle<Value> MixingGetter(Local<String> str, const AccessorInfo& accessor);

  static void BeginWrite(Baton* baton);
  static void DoWrite(uv_work_t* req);
  static void AfterWrite(uv_work_t* req);

  static void BeginMix(Baton* baton);
  static void DoMix(uv_work_t* req);
  static void AfterMix(uv_work_t* req);

  Persistent<Array> channelBuffers;
  Persistent<Array> channelsReady;
  Persistent<Function> callback;
  int channels;
  int alignment;
  int format;
  bool mixing;
};

}

#endif
