#ifndef UNZIPPER_H
#define UNZIPPER_H

#include <cstdlib>
#include <cstring>
#include <node.h>
#include <node_buffer.h>
#include "macros.h"

#define UNZ_BUFFER_FRAMES 1024

using namespace v8;
using namespace node;

namespace pcmutils {

class Unzipper;

class Unzipper : public ObjectWrap {
public:
  static void Init(Handle<Object> exports);

protected:
  Unzipper() : ObjectWrap(), channels(0), alignment(0), frameAlignment(0), unzipping(false) {
    channelBuffers.Clear();
  }

  ~Unzipper() {
    channels = 0;
    alignment = 0;
    frameAlignment = 0;
    unzipping = false;
    channelBuffers.Dispose();
  }

  struct Baton {
    uv_work_t request;
    Unzipper* unz;

    Baton(Unzipper* unz_) : unz(unz_) {
      unz->Ref();
      request.data = this;
    }
    virtual ~Baton() {
      unz->Unref();
    }
  };

  struct UnzipBaton : Baton {
    Persistent<Function> callback;
    Persistent<Object> chunk;
    size_t chunkLength;
    char* chunkData;
    char** channelData;
    int totalFrames;
    int unzippedFrames;

    UnzipBaton(Unzipper* unz_, Handle<Function> cb_, Handle<Object> chunk_) : Baton(unz_), 
        chunkLength(0), chunkData(NULL), channelData(NULL), totalFrames(0), unzippedFrames(0) {

      callback = Persistent<Function>::New(cb_);

      chunk = Persistent<Object>::New(chunk_);
      chunkData = Buffer::Data(chunk);
      chunkLength = Buffer::Length(chunk);

      channelData = (char**)malloc(unz->channels * sizeof(char*));
      for (int i = 0; i < unz->channels; i++) {
        channelData[i] = Buffer::Data(unz->channelBuffers->Get(i)->ToObject());
      }

      totalFrames = chunkLength / unz->frameAlignment;

      // Todo - check for and handle alignment issues
      // int leftoverBytes = chunkLength % unz->frameAlignment;
      // if (leftoverBytes != 0) fprintf(stderr, "LEFTOVRES: %d\n", leftoverBytes);
    }
    virtual ~UnzipBaton() {
      callback.Dispose();
      chunk.Dispose();
      free(channelData);
    }
  };

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Unzip(const Arguments& args);

  static void BeginUnzip(Baton* baton);
  static void DoUnzip(uv_work_t* req);
  static void AfterUnzip(uv_work_t* req);

  Persistent<Array> channelBuffers;
  int channels;
  int alignment;
  int frameAlignment;
  bool unzipping;
};

}

#endif
