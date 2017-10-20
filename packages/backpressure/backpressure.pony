use @pony_apply_backpressure[None]()
use @pony_release_backpressure[None]()

type BackpressureAuth is (AmbientAuth | ApplyReleaseBackpressureAuth)

primitive Backpressure
  fun apply(auth: BackpressureAuth) =>
    @pony_apply_backpressure()

  fun release(auth: BackpressureAuth) =>
    @pony_release_backpressure()
