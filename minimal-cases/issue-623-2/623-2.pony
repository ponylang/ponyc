"""
Prior to the resolution of issue 623, this would cause the gc to crash while
tracing SpeechOut from SpeechIn
"""
actor Main
  new create(env: Env) =>
    let talk = SpeechOut
    let config = Config.create(talk)

    SpeechIn.create(talk, config)

actor SpeechOut
  new create() => None

actor SpeechIn
  let talk: SpeechOut tag

  new create(talk': SpeechOut, config: Config) =>
    talk = config.talk
    recover String(16000) end // trigger gc, happens around 14336

class val Config
  let talk: SpeechOut tag

  new val create(talk': SpeechOut tag) =>
    talk = talk'
