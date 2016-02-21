"""
Demonstrate the basic use of delegates in pony
"""

use "time"


trait Wombat
  fun box battle_call() : String val => 
    "Huzzah!"

class SimpleWombat is Wombat

class KungFuWombat is Wombat
  fun box battle_call() : String val => 
    "Bonzai!"

trait Drone
  fun box battle_call() : String val => 
    "Beep Boop!"

class DroneWombat is ( Drone & Wombat)
  fun box battle_call() : String val =>
    "Beep boop Huzzah!"

actor Main is Wombat
 let d : Wombat delegate Wombat = DroneWombat
 let k : Wombat delegate Wombat = KungFuWombat

  new create(env : Env) =>
    let x = Time.nanos() % 4

    let chosen_wombat = match x
    | 0 => SimpleWombat
    | 1 => k
    | 2 => d
    else
      this
    end
    env.out.print("Welcome to Wombat Combat!")
    env.out.print("Battle cry: " + chosen_wombat.battle_call())

  fun box battle_call() : String val =>
    "Bonzai! Beep boop! Huzzah!"
