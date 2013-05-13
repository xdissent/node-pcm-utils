binding = require '../build/Release/binding'
stream = require 'stream'
pcm = require './constants'

class Unzipper extends stream.Writable
  constructor: (@channels=2, @format=pcm.FMT_F32LE) ->
    stream.Writable.call this
    @alignment = pcm.ALIGNMENTS[@format]
    @unzipper = new binding.Unzipper @channels, @alignment
    @outputs = (new stream.PassThrough for i in [0...@channels])
    @mono = @outputs[0] if @channels == 1
    [@left, @right] = [@outputs[0], @outputs[1]] if @channels == 2

  _write: (chunk, encoding, callback) ->
    @unzipper.unzip chunk, (err, chunks, done) =>
      throw err if err?
      @outputs[i].write chunk for chunk, i in chunks
      callback() if done

module.exports = Unzipper