# rbmkcpp

An interactive RBMK simulator

## Implemented features :

* Stack & control rod layout
* Control rod movement
* Arbitrary control rod groups
* Basic fission
* Basic neutron flux monitoring


## User Manual (WIP)

```
  make
  ./main
```
  
Run in a sufficiently large terminal, if default settings don't work decrease the font size to get enough room, e.g. `urxvt -fn "6x12" -e ./main`.

You're presented with multiple panels summarizing the state of the simulated power plant, you can enter commands (bottom right) to change that state.

### Commands

* `select x y` - Select a single control rod
* `select all` - Select all control rods
* `select group g` - Select all control rods in a group
* `pull` - Withdraw selected rods all the way out
* `pull x` - Withdraw selected rods by a given number of centimeters
* `insert` - Insert selected rods all the way in
* `insert x` - Insert selected rods by a given number of centimeters
* `stop` - Arrest selected rods in place
* `scram` - Reactor shutdown mode
* `scram reset` - Exit reactor shutdown mode

