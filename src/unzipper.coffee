binding = require '../build/Release/binding'
stream = require 'stream'

class Unzipper extends stream.Writable
  constructor: (@channels, @alignment) ->
    stream.Writable.call this
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