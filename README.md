# ~~Unity~~ Unreal Real Time Strategy: Build Your Own RTS Game
> An Unreal RTS inspired by the [Unity course by Gamedev.tv](https://www.gamedev.tv/courses/unity-realtime-strategy?utm_source=products-banner). This project reimagines the experience in UE with a fundamentally different approach in many areas due to engine & language differences, personal learning goals, and the UE's ecosystem. Additionally, I have a stretch goal combining an RTS with an RPG


# Project Features
- RTS/Tycoon style player camera
  - Smoothly replicated even on bad server and client network emulation (Player's location is represented to other players).
  - C++ with blueprint exposed properties.
  - Collision with terrain.
  - Movement, zoom, rotation.
  - Debug visualizations.

# WIP  *Nav Mesh, Nav Agent, Player Input*

- [x] Quick nav prototype map
- [x] update ray cast to use max value
- [x] Player color
- [ ] Nav mesh (remember to bake or build paths)
  - [ ] Update Navigation auto = false.
- [ ] Look at Nav visualizers (avoidance)
- [ ] Stop distance is diameter of capsule
- [ ] adjust CMC so (turn, accel, (UseContRotYaw=f, RotRate=180, OrRotToMo=t)) looks good. 
- [ ] Left Click select: 2.4_4:30
- [ ] Selected unit has bright name, dim name.
- [ ] Decal at move location (look at games, templates)
- [ ] Decal only on floor. maybe water.
- [ ] right click to move 2.6
- [ ] Drag select 2.7
- [ ] shift key
- [ ] Common UI 
- [ ] Unit portraits *prototype*  
- [ ] Avoid water
- [ ] nav mesh modifiers/obstacle for resources and/or buildings to avoid re-baking the nav mesh. UE has NavModVol.AreaClass?
- [ ] No Unit Dancing. 2.13_0:55 has some solutions
  - [ ] around obstacles (they'll collapse in)
  - [ ] small spaces.
- [ ] flying unit. 
  - [ ] can go over obstacles that would stop a walking unit e.g. some buildings, some mountains, other units. Is still limited by map bounds, height, arbitrary things. 
  - [ ] Can't use his system, bc he is using flat ground. I have hills and mountains.
  - [ ] Ok if flying units stack, but prefer not. 
- [ ] The inside of a large box shouldn't have a navigable islands (RecastNavMesh.DrawOffset).

## Stretch
- [ ] TargetLocation_Decal shows direction
- [ ] Consider using interface over a component. I can still have default functions with no state.
- [ ] Units push each other out of the way
- [ ] faction system
- [ ] Actual real-time portraits
- [ ] Steam multiplayer to avoid so much temp code. like `Team =  PIE_ID % 2`
- [ ] Units can enter buildings. Players can see through buildings with their units in it
- [ ] Unit individual names like John or Sam
- [ ] Units move in formation
  - [ ] cluster fk
  - [ ] wedge
    - [ ] Ash and trash in the middle
  - [ ] modified wedge
  - [ ] file 
- [ ] Units seek cover
- [ ] Players can move units to face behind cover.
- [ ] helicopter can land.
- [ ] NavMesh in world partition level

## Notes: 
- review Navigation
  - [Intro to AI with BP](https://dev.epicgames.com/community/learning/courses/67R/unreal-engine-introduction-to-ai-with-blueprints/DYXe/navigation-theory)
- review Collision
  - [Collision Data in UE5: Practical Tips for Managing Collision Settings & Queries | Unreal Fest 2023](https://youtu.be/xIQI6nXFygA?si=xumINQMLFOvm06OF)
- Can HUD use input? test with print strings and have input in both PC and HUD. IDK if I want to do this, but see if it's an option.  
- Event Bus?
  - Not Delegates bc one has to know about the other. In EventBus both know about EventBus.
  - Gameplay Message Subsystem(GpMsgSs):
  - MVVM:
  - Use one for UI and the other for other things?
    - Singleton without state

### Usage and limitation: (compile times, coupling trade off, I would like to be able to move a class to another project, When do I want to bring other classes?)  
In general use direct cast unless: 
  - it's UI, I don't want UI code in my game code.
  - becomes to confusing to direct communication
  - I may want to move a class to another project
  - BP don't direct cast to other BPs due to asset bloat
  - Compile times
---
- **U&L of EventBus code 2.8, .9, .10**
  - Singleton without state
  - Does he use for gameplay? YES 2.8_7:03 | Does he use for UI?
- **U&L of Delegates**
  - Typically, when I use delegates, the PC would know about other actors. I can limit what the PC knows about.
- **U&L of Interfaces**
  - a
- **U&L of Components**
  - a
- **U&L of GpMsgSs**
  - a
- **U&L of MVVM**
  - a
- **U&L of WidgetController**
  - a
- **U&L of HUD as WidgetController**
  -   a
- **U&L of HUD as BPFL**
  - a
- **U&L of HUD as Subsystem**
  - a
- **U&L of Tags & Gameplay Tags**
  - a
- **U&L of Gameplay Task**
  - a
--- 
Selecting and deselecting units and actors in an RTS? What all is involved? 
- PC needs to know what units are selected, unit needs to know which PC selected, UI needs to know, other players need to know when units are selected and deselected. 
- Done with PC.mouseclick, UI click portrait, Is shift held in PC and UI? 
- Other players may select units in multiplayer, bc player's share units.
- Units can die
- Units can be people, vehicles, animals.
- All units and actors that the players can select have state, like name (John, Sam, Frisky's Pub, AK-47, table, pot) that needs to be displayed in the UI, However, this could be a different system.
- Players can only select units in the players' faction
- Players can drag select any pawn in faction
- Players can issue commands to any pawn in faction that's selected
- Players can select any actor in the game to display information in the hud about said item.
- I'll test this system in 2 vs 2 players, so co-op with pvp combined.
- There are a few different systems, but not completely unrelated and worth considering. 
  - Units can preform tasks on actors and pawns in game e.g. attack, lock pick. as well as tasks like move to location.
  - UI will populate with a list of actions filtered by what any selected unit can preform, and the actions that can be preformed on that actor.
  - There will be a list of alive units in the players' faction, probably in the GameState.
  - Units can join and leave the players' faction.
  - It can be co-op where players can be in the same faction, or pvp where players can be in different factions, or combinations
  - AI will use StateTree for behavior.