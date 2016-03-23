"""
Prior to fix for issue 623, this would result in a segfault
when SpeechOut was gc'd during tracing of SpeechIn
"""
use "debug"

actor Main
  new create(env: Env) =>
    let talk = SpeechOut
    let config = Config.create(talk)

    SpeechIn.create(config)

actor SpeechOut
  new create() =>
    None

actor SpeechIn
  let _talk: SpeechOut tag

    new create(config: Config) =>
    _talk = config.talk
    _loadRules()

  fun ref _loadRules() =>
    var i: U8 = 0


    while i < 240 do
      i = i + 1
      Debug.out(i.string())
    end

class val Config
  let talk: SpeechOut tag

  new val create(talk': SpeechOut tag) =>
    talk = talk'
