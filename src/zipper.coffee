stream = require 'stream'

class Zipper extends stream.Readable
  constructor: (@channels, @alignment) ->
    stream.Readable.call this
    @frameAlignment = @channels * @alignment
    @inputs = for i in [0...@channels]
      do (i) => (new stream.PassThrough).on 'readable', => @readInput(i)

    @mono = @inputs[0] if @channels == 1
    [@left, @right] = [@inputs[0], @inputs[1]] if @channels == 2

    @channelSamplesPerBuffer = 1024
    @channelBytesPerBuffer = @channelSamplesPerBuffer * @alignment
    @bufferBytes = @channelBytesPerBuffer * @channels
    
    @buffer = new Buffer @bufferBytes
    @channelSamplesWritten = (0 for i in [0...@channels])

  readInput: (c) ->
    channelBytesNeeded = @channelBytesPerBuffer - (@channelSamplesWritten[c] * @alignment)
    chunk = @inputs[c].read channelBytesNeeded
    return false unless chunk?

    if chunk.length > channelBytesNeeded
      @inputs[c].unshift chunk.slice(channelBytesNeeded, chunk.length)
      chunk = chunk.slice 0, channelBytesNeeded

    leftovers = chunk.length % @alignment
    if leftovers > 0
      @inputs[c].unshift chunk.slice(chunk.length - leftovers, chunk.length)
      chunk = chunk.slice(0, chunk.length - leftovers)

    for i in [0...chunk.length] by @alignment
      chunk.copy @buffer, (@channelSamplesWritten[c] * @frameAlignment) + (c * @alignment), i, i + @alignment
      @channelSamplesWritten[c]++

    @checkForWrite()

  checkForWrite: ->
    finished = (i for i in [0...@channels] when @channelSamplesWritten[i] == @channelSamplesPerBuffer)
    return false unless finished.length == @channels
    @push @buffer
    @channelSamplesWritten = (0 for i in [0...@channels])

  _read: (size) ->
    return @push ''

module.exports = Zipper