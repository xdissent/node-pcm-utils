binding = require '../build/Release/binding'
stream = require 'stream'
pcm = require './constants'

class Zipper extends stream.Readable
  constructor: (@channels=2, @format=pcm.FMT_F32LE) ->
    stream.Readable.call this
    @alignment = pcm.ALIGNMENTS[@format]
    @zipper = new binding.Zipper @channels, @alignment, (err, chunk) =>
      throw err if err?
      @push chunk
    @bufferSize = @zipper.samplesPerBuffer * @alignment
    @inputs = for i in [0...@channels]
      do (i) => (new stream.PassThrough).on 'readable', => @readInput(i)
    @mono = @inputs[0] if @channels == 1
    [@left, @right] = [@inputs[0], @inputs[1]] if @channels == 2

  readInput: (channel) ->
    return false if @zipper.zipping || @zipper.channelsReady[channel]
    chunk = @inputs[channel].read @bufferSize
    return false unless chunk?
    if chunk.length > @bufferSize
      @inputs[channel].unshift chunk.slice(@bufferSize, chunk.length)
      chunk = chunk.slice 0, @bufferSize
    @zipper.write channel, chunk

  _read: (size) ->
    @readInput(i) for i in [0...@channels]
    @push ''

module.exports = Zipper