#ifndef FORMATTER_H
#define FORMATTER_H

#include <cstdlib>
#include <node.h>
#include <node_buffer.h>
#include "macros.h"

#define FMT_BUFFER_SAMPLES 1024

using namespace v8;
using namespace node;

namespace pcmutils {

class Formatter;

class Formatter : public ObjectWrap {
public:
  static void Init(Handle<Object> exports);

protected:
  Formatter() : ObjectWrap(), inFormat(0), outFormat(0), 
      inAlignment(0), outAlignment(0), formatting(false), buffer(NULL) {
  }

  ~Formatter() {
    inFormat = 0;
    outFormat = 0;
    inAlignment = 0;
    outAlignment = 0;
    formatting = false;
    if (buffer != NULL) free(buffer);
    buffer = NULL;
  }

  struct Baton {
    uv_work_t request;
    Formatter* fmt;

    Baton(Formatter* fmt_) : fmt(fmt_) {
      fmt->Ref();
      request.data = this;
    }
    virtual ~Baton() {
      fmt->Unref();
    }
  };

  struct FormatBaton : Baton {
    Persistent<Function> callback;
    Persistent<Object> chunk;
    size_t chunkLength;
    char* chunkData;
    int totalSamples;
    int formattedSamples;

    FormatBaton(Formatter* fmt_, Handle<Function> cb_, Handle<Object> chunk_) : Baton(fmt_), 
        chunkLength(0), chunkData(NULL), totalSamples(0), formattedSamples(0) {

      callback = Persistent<Function>::New(cb_);
      chunk = Persistent<Object>::New(chunk_);
      chunkData = Buffer::Data(chunk);
      chunkLength = Buffer::Length(chunk);
    }
    virtual ~FormatBaton() {
      callback.Dispose();
      chunk.Dispose();
    }
  };

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Format(const Arguments& args);

  static void BeginFormat(Baton* baton);
  static void DoFormat(uv_work_t* req);
  static void AfterFormat(uv_work_t* req);

  int inFormat;
  int outFormat;
  int inAlignment;
  int outAlignment;
  bool formatting;
  char* buffer;
};

}

#endif
