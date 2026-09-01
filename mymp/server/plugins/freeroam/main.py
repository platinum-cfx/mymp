"""Freeroam plugin — the MyMP equivalent of alt:V's MIT 'freeroam' example.

Commands:
    /weapon <name>   give yourself a weapon (sent to the GTA client)
    /heal            restore health + armour
    /armour <0-100>  set armour
    /hp <0-100>      set health
"""

WEAPONS = {
    "pistol": "WEAPON_PISTOL", "combatpistol": "WEAPON_COMBATPISTOL",
    "smg": "WEAPON_SMG", "carbine": "WEAPON_CARBINERIFLE",
    "shotgun": "WEAPON_PUMPSHOTGUN", "sniper": "WEAPON_SNIPERRIFLE",
    "rpg": "WEAPON_RPG", "knife": "WEAPON_KNIFE",
    "bat": "WEAPON_BAT", "stickynade": "WEAPON_STICKYBOMB",
}


def setup(ctx, world, log):
    def on_command(player, cmd, args):
        if cmd == "weapon":
            if not world.has_ace(player, "command.weapon"):
                world.send(player, {"t": "sys", "msg": "You need the command.weapon ace."})
                return True
            name = (args[0] if args else "").strip().lower()
            if name not in WEAPONS:
                world.send(player, {"t": "sys", "msg": "Weapons: " + ", ".join(sorted(WEAPONS))})
                return True
            world.send_event(player, "giveWeapon", {"weapon": WEAPONS[name]})
            world.send(player, {"t": "sys", "msg": f"Gave you {name} (GTA client)."})
            return True
        if cmd == "heal":
            player.ent.hp, player.ent.ar = 100, 100
            world.broadcast({"t": "sys", "msg": f"{player.name} healed."})
            return True
        if cmd == "armour":
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /armour <0-100>"})
                return True
            try:
                player.ent.ar = max(0, min(100, int(args[0])))
            except ValueError:
                world.send(player, {"t": "sys", "msg": "Usage: /armour <0-100>"})
            return True
        if cmd == "hp":
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /hp <0-100>"})
                return True
            try:
                player.ent.hp = max(0, min(100, int(args[0])))
            except ValueError:
                world.send(player, {"t": "sys", "msg": "Usage: /hp <0-100>"})
            return True
        return False

    ctx.register("command", on_command)
