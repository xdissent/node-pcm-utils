node-pcm-utils
==============

PCM audio utilities for Node.js

**WARNING** This is a javascript-only implementation currently, which will crush your CPU and probably ruin your event loop. It's a proof of concept to be ported to a proper C++ addon with evented operations.


Installation
------------

Install via git:

```sh
$ npm install git+https://github.com/xdissent/node-pcm-utils.git
```


Usage
-----

```js
var pcm = require('pcm-utils');

var channels = 2,  // 2 channels (left/right)
  alignment = 4;   // 4 bytes per sample (32 bit float)

var unzipper = new pcm.Unzipper(channels, alignment),
  zipper = new pcm.Zipper(channels, alignment),
  mixer = new pcm.Mixer(channels, alignment)

// Read interleaved PCM data from stdin
process.stdin.pipe(unzipper);

// Unzip then zip it right back up to stdout
unzipper.left.pipe(zipper.left);
unzipper.right.pipe(zipper.right);
zipper.pipe(process.stdout);

// Mix left and right channels and pipe mono to stderr
unzipper.left.pipe(mixer.left);
unzipper.right.pipe(mixer.right);
mixer.pipe(process.stderr);
```
