"""Smallest useful program. Run: python examples/quickstart.py"""
import samskriti as sk

with sk.World(seed=1) as world:
    npc = world.spawn()
    player = world.spawn()
    npc.move_to(0, 0, 0)
    player.move_to(1, 0, 0)

    # The player helps the NPC, repeatedly. Experiences are absorbed on step().
    for _ in range(10):
        npc.experience("helped", 0.7, source=player)
        world.step()

    print(npc.state)                     # dominant feeling, age, impressions
    print(npc.behavior_toward(player))   # what it wants to do about the player
    print(npc.voice_toward(player))      # tone/formality/colour for dialogue or an LLM
