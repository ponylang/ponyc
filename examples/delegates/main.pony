"""
Demonstrate the basic use of delegates in pony
"""
use "time"

trait Wombat
  fun box battle_call(): String val =>
    "Huzzah!"

class SimpleWombat is Wombat

class KungFuWombat is Wombat
  fun box battle_call(): String val =>
    "Bonzai!"

trait Drone
  fun box battle_call(): String val =>
    "Beep Boop!"

class DroneWombat is (Drone & Wombat)
  fun box battle_call(): String val =>
    "Beep boop Huzzah!"

actor Main is Wombat
  let wombatness: Wombat delegate Wombat

  new create(env: Env) =>
    let x = Time.nanos() % 3

    wombatness =
      match x
      | 0 => SimpleWombat
      | 1 => DroneWombat
      | 2 => KungFuWombat
      else
        SimpleWombat
      end

    env.out.print("Welcome to Wombat Combat!")
    env.out.print("Battle cry: " + this.battle_call())
