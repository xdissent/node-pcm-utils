#include <node.h>
#include "mixer.h"
#include "unzipper.h"
#include "zipper.h"

using namespace v8;
using namespace node;

namespace pcmutils {

void Init(Handle<Object> exports) {
  Mixer::Init(exports);
  Unzipper::Init(exports);
  Zipper::Init(exports);
}

}

NODE_MODULE(binding, pcmutils::Init)