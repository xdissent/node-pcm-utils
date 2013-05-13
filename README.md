node-pcm-utils
==============

[![NPM version](https://badge.fury.io/js/pcm-utils.png)](http://badge.fury.io/js/pcm-utils)

PCM audio utilities for Node.js


Features
--------

* **Interleaving/deinterleaving** - Unzip interleaved PCM data into separate channel streams and vice-versa.

* **Mixing** - Mix 2 or more PCM channels into one.

* **Format conversion** - Transform a stream from one PCM format to another (ie. float to int).

* **Evented** - Doesn't block the main loop, thanks to [`uv_queue_work`](http://nikhilm.github.io/uvbook/threads.html#libuv-work-queue).

* **Streams2 compatible** - Everything's just a pipeable [stream](http://nodejs.org/api/stream.html).

Note: For sample rate conversion, check out [resampler](https://npmjs.org/package/resampler). For playback, try [speaker](https://npmjs.org/package/speaker) or [alsa](https://npmjs.org/package/alsa) (which also records).


Installation
------------

Install with npm:

```sh
$ npm install pcm-utils
```

or via git:

```sh
$ npm install git+https://github.com/xdissent/node-pcm-utils.git
```


Usage
-----

```js
var pcmUtils = require('pcm-utils'),

  // The following variables represent the defaults for all constructors.
  channels = 2,                // 2 channels (left/right)
  format = pcmUtils.FMT_F32LE, // 32 bit little-endian float

  // Available formats: No big-endian support yet!
  //
  // pcmUtils.FMT_F32LE - 32 bit little-endian float
  // pcmUtils.FMT_F32BE - 32 bit big-endian float **Not currently supported**
  // pcmUtils.FMT_S16LE - signed 16 bit little-endian integer
  // pcmUtils.FMT_S16BE - signed 16 bit big-endian integer **Not currently supported**
  // pcmUtils.FMT_U16LE - unsigned 16 bit little-endian integer
  // pcmUtils.FMT_U16BE - unsigned 16 bit big-endian integer **Not currently supported**

  // Unzipper deinterleaves PCM data into multiple single-channel streams.
  unzipper = new pcmUtils.Unzipper(channels, format),

  // Zipper interleaves multiple single-channel PCM streams into one.
  zipper = new pcmUtils.Zipper(channels, format),
  
  // Mixer mixes multiple channels into a single channel stream.
  mixer = new pcmUtils.Mixer(channels, format),
  
  // Formatter transforms single-channel PCM data from one format to another,
  // 32 bit little-endian float to signed 16 bit little-endian integer in this case.
  formatter = new pcmUtils.Formatter(format, pcmUtils.FMT_S16LE);

// Read interleaved PCM data from stdin
process.stdin.pipe(unzipper);

// Unzip (de-interleave) it then zip it right back up to stdout
unzipper.left.pipe(zipper.left);    // or `unzipper.outputs[0].pipe(zipper.inputs[0]);`
unzipper.right.pipe(zipper.right);  // or `unzipper.outputs[1].pipe(zipper.inputs[1]);`
zipper.pipe(process.stdout);

// Mix left and right channels and pipe mono to stderr
unzipper.left.pipe(mixer.left);     // or `unzipper.outputs[0].pipe(mixer.inputs[0]);`
unzipper.right.pipe(mixer.right);   // or `unzipper.outputs[1].pipe(mixer.inputs[1]);`
mixer.pipe(process.stderr);

// Convert the mono mixer output into signed 16 bit little-endian and write to file.
var fs = require('fs'),
  outFileStream = fs.createWriteStream('/tmp/s16le.pcm');
formatter.pipe(outFileStream);
mixer.pipe(formatter);
```
