stream = require 'stream'

class Unzipper extends stream.Writable
  constructor: (@channels, @alignment) ->
    stream.Writable.call this
    @outputs = (new stream.PassThrough for i in [0...@channels])
    @frameAlignment = @channels * @alignment
    @leftovers = new Buffer @frameAlignment
    @leftoversLength = 0
    @mono = @outputs[0] if @channels == 1
    [@left, @right] = [@outputs[0], @outputs[1]] if @channels == 2
      

  _write: (chunk, encoding, callback) ->
    totalLength = @leftoversLength + chunk.length
    newLeftoversLength = totalLength % @frameAlignment

    chunk.copy(@leftovers, @leftoversLength, 0, @frameAlignment - @leftoversLength)
    @outputs[i].write(@leftovers.slice(i * @alignment, i * @alignment + @alignment)) for i in [0...@channels]

    chunk.copy(@leftovers, 0, chunk.length - newLeftoversLength)
    chunk = chunk.slice(@frameAlignment - @leftoversLength, chunk.length - newLeftoversLength)
    @leftoversLength = newLeftoversLength
    
    for j in [0...chunk.length] by @frameAlignment
      @outputs[i].write(chunk.slice(i * @alignment + j, i * @alignment + @alignment + j)) for i in [0...@channels]

    callback()

module.exports = Unzipper