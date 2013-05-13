binding = require '../build/Release/binding'
stream = require 'stream'
pcm = require './constants'

class Mixer extends stream.Readable
  constructor: (@channels=2, @format=pcm.FMT_F32LE) ->
    stream.Readable.call this
    @alignment = pcm.ALIGNMENTS[@format]
    @mixer = new binding.Mixer @channels, @alignment, @format, (err, chunk) =>
      throw err if err?
      @push chunk
    @bufferSize = @mixer.samplesPerBuffer * @alignment
    @inputs = for i in [0...@channels]
      do (i) => (new stream.PassThrough).on 'readable', => @readInput(i)
    @mono = @inputs[0] if @channels == 1
    [@left, @right] = [@inputs[0], @inputs[1]] if @channels == 2

  readInput: (channel) ->
    return false if @mixer.mixing || @mixer.channelsReady[channel]
    chunk = @inputs[channel].read @bufferSize
    return false unless chunk?
    if chunk.length > @bufferSize
      @inputs[channel].unshift chunk.slice(@bufferSize, chunk.length)
      chunk = chunk.slice 0, @bufferSize
    @mixer.write channel, chunk

  _read: (size) ->
    @readInput(i) for i in [0...@channels]
    @push ''

module.exports = Mixer