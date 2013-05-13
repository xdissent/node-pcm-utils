binding = require '../build/Release/binding'
stream = require 'stream'
pcm = require './constants'

class Formatter extends stream.Transform
  constructor: (@inFormat, @outFormat=pcm.FMT_F32LE) ->
    stream.Transform.call this
    @formatter = new binding.Formatter @inFormat, @outFormat

  _transform: (chunk, encoding, callback) ->
    throw "Alignment fail!" unless chunk.length % pcm.ALIGNMENTS[@inFormat] == 0
    @formatter.format chunk, (err, formatted, done) =>
      throw err if err?
      @push formatted
      callback() if done

  # TODO: implement
  # _flush: (callback) ->

module.exports = Formatter