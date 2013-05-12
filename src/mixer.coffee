stream = require 'stream'

class Mixer extends stream.Readable
  constructor: (@channels, @alignment) ->
    stream.Readable.call this
    @inputs = for i in [0...@channels]
      do (i) => (new stream.PassThrough).on 'readable', => @readInput(i)

    @mono = @inputs[0] if @channels == 1
    [@left, @right] = [@inputs[0], @inputs[1]] if @channels == 2

    @channelSamplesPerBuffer = 1024
    @channelBytesPerBuffer = @channelSamplesPerBuffer * @alignment
    @bufferBytes = @channelBytesPerBuffer
    
    @buffer = new Buffer @bufferBytes
    @channelSamplesWritten = (0 for i in [0...@channels])
    @channelBuffers = (new Buffer @channelBytesPerBuffer for i in [0...@channels])

  readInput: (c) ->
    channelBytesWritten = @channelSamplesWritten[c] * @alignment
    channelBytesNeeded = @channelBytesPerBuffer - channelBytesWritten
    chunk = @inputs[c].read channelBytesNeeded
    return false unless chunk?

    if chunk.length > channelBytesNeeded
      @inputs[c].unshift chunk.slice(channelBytesNeeded, chunk.length)
      chunk = chunk.slice 0, channelBytesNeeded

    leftovers = chunk.length % @alignment
    if leftovers > 0
      @inputs[c].unshift chunk.slice(chunk.length - leftovers, chunk.length)
      chunk = chunk.slice(0, chunk.length - leftovers)

    chunk.copy @channelBuffers[c], channelBytesWritten, 0, chunk.length
    @channelSamplesWritten[c] += parseInt(chunk.length / @alignment)

    @checkForWrite()

  checkForWrite: ->
    finished = (i for i in [0...@channels] when @channelSamplesWritten[i] == @channelSamplesPerBuffer)
    return false unless finished.length == @channels
    for i in [0...@channelSamplesPerBuffer]
      sum = 0
      for c in [0...@channels]
        sum += @channelBuffers[c].readFloatLE(i * @alignment)
      @buffer.writeFloatLE(sum / @channels, i * @alignment)
    @push @buffer
    @channelSamplesWritten = (0 for i in [0...@channels])

  _read: (size) ->
    return @push ''

module.exports = Mixer