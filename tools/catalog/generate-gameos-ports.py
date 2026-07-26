#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APPS = ROOT / "apps"
DOCS = ROOT / "docs" / "apps"
MANUAL_PORT_SLUGS = {"haunted_radio_operator"}


def action(label, result, hp, focus, kit, standing, wealth, xp, item):
    return {
        "label": label,
        "result": result,
        "hp": hp,
        "focus": focus,
        "kit": kit,
        "standing": standing,
        "wealth": wealth,
        "xp": xp,
        "item": item,
    }


GAMES = [
    {
        "slug": "cryptid_park_ranger",
        "title": "Cryptid Ranger",
        "role": "Role: junior ranger, field folklorist, and night patrol lead.",
        "premise": "Patrol a strange park, log sightings, manage gear, and keep visitors calm.",
        "accent": "0x07FF",
        "acts": ["Misty Trailhead", "Old Fire Tower", "Moonlit Sinkhole", "The Quiet Preserve"],
        "quests": [
            "Find what keeps circling the trail cameras before the weekend hikers arrive.",
            "Reach the tower, compare claw marks, and decide whether the warning signs are enough.",
            "Map the sinkhole tunnels while the radio chatter turns into copied voices.",
            "Prove the preserve can stay open without feeding the thing under the pines.",
        ],
        "lore": [
            "Rangers call the first prints deer-lies: hoof shapes that end in too many toes.",
            "The fire tower logbook mentions green lamps moving between trees in 1979.",
            "Old limestone caves run beneath the picnic loop and make every sound arrive twice.",
            "The preserve survives by bargains: honest reports, closed paths, and steady nerves.",
        ],
        "stats": ["Nerve", "Focus", "Gear", "Trust", "Budget"],
        "items": ["Track Cast", "Tower Key", "Bait Tin", "Quiet Map"],
        "actions": [
            action("Patrol", "Fresh prints cross your beam. You mark distance, depth, and direction.", -5, -4, -3, 6, 0, 18, 1),
            action("Interview", "A shaken camper repeats one detail: the antlers were listening.", 0, -3, 0, 8, 1, 16, 0),
            action("Camp", "You fix straps, brew coffee, and stop the team from inventing rumors.", 9, 8, 5, 2, -1, 12, 4),
            action("Report", "Your clean field note earns a permit extension and a late-night warning.", 0, 2, -2, 10, 2, 15, 8),
        ],
    },
    {
        "slug": "cyber_ranger",
        "title": "Cyber Ranger",
        "role": "Role: mesh warden, incident scribe, and field network medic.",
        "premise": "Guard a small mesh outpost from probes, outages, and bad field intel.",
        "accent": "0x07E0",
        "acts": ["Perimeter Ping", "Patch Night", "Relay Ghost", "Green Channel"],
        "quests": [
            "Sweep the outpost for unknown nodes before the supply convoy trusts the mesh.",
            "Keep services alive through a dirty firmware storm and a tired volunteer crew.",
            "Trace the relay that answers in your own packet timing.",
            "Publish a hardened route table and keep the valley talking.",
        ],
        "lore": [
            "The outpost mesh began as school roof antennas and borrowed batteries.",
            "Every patched node gets a green string tied to its mast for luck and inventory.",
            "Old relay boxes sometimes repeat messages from outages that happened years ago.",
            "The ranger oath is simple: no panic, no mystery port left open, no crew left offline.",
        ],
        "stats": ["Signal", "Focus", "Power", "Cred", "Parts"],
        "items": ["Clean Map", "Patch Kit", "Spare Cell", "Relay Token"],
        "actions": [
            action("Scan", "The scan finds a soft relay pretending to be a weather station.", -2, -5, -2, 5, 1, 18, 1),
            action("Patch", "You close the noisy service and write the fix where the next shift will see it.", 0, -6, -4, 7, -1, 20, 2),
            action("Train", "The crew drills handoffs until the channel feels boring again.", 4, 6, -2, 6, 0, 14, 4),
            action("Trace", "Three hops later, the ghost route points at a forgotten hilltop cache.", -5, -3, -3, 9, 3, 19, 8),
        ],
    },
    {
        "slug": "cyberdeck_hacker_rpg",
        "title": "Cyberdeck RPG",
        "role": "Role: careful deckrunner, favor broker, and heat manager.",
        "premise": "Run careful ops from a pocket deck, balancing heat, access, and reputation.",
        "accent": "0xFD20",
        "acts": ["Coffee Shop Recon", "Backdoor Hymnal", "Black Ice Wake", "Dead Drop Dawn"],
        "quests": [
            "Build a target map without letting the cafe camera learn your face.",
            "Open the hymn-coded backdoor and decide which favor to burn.",
            "Cross black ice with the client screaming for proof.",
            "Deliver the archive, wipe the route, and choose who gets paid.",
        ],
        "lore": [
            "Good deckrunners keep three lies ready: job, name, and reason for leaving.",
            "The Hymnal is an old access pattern hidden in corporate training audio.",
            "Black ice does not chase. It waits until you need to hurry.",
            "Every dead drop has a witness. The trick is making the witness owe you.",
        ],
        "stats": ["Cover", "Focus", "Tools", "Rep", "Cred"],
        "items": ["Clean Proxy", "Root Note", "Ice Charm", "Drop Key"],
        "actions": [
            action("Recon", "You profile guards, cameras, and coffee refills until the route appears.", -2, -5, -1, 5, 0, 18, 1),
            action("Exploit", "The exploit lands hard, opening access while the heat clock wakes up.", -7, -7, -4, 9, 4, 23, 2),
            action("Hide", "You salt logs, ditch a burner, and let the trail cool.", 5, 7, -3, 2, -1, 13, 4),
            action("Deliver", "The client accepts the packet, but the bonus arrives with a new problem.", -1, -2, -1, 8, 6, 17, 8),
        ],
    },
    {
        "slug": "dungeon_courier",
        "title": "Dungeon Courier",
        "role": "Role: oathbound courier, trap reader, and reluctant hero.",
        "premise": "Deliver sealed parcels through trap rooms, guild halls, and monster tolls.",
        "accent": "0xFBE0",
        "acts": ["Copper Gate", "Mimic Hall", "Underking Toll", "Last Door"],
        "quests": [
            "Carry a sealed writ past the Copper Gate before the wax sigil sweats loose.",
            "Survive the hall where furniture asks questions and teeth answer first.",
            "Cross the Underking's toll bridge without spending the kingdom's secret.",
            "Reach the Last Door and decide whether a courier's oath covers prophecy.",
        ],
        "lore": [
            "Courier guild law says a package is not late until the bearer is dead twice.",
            "Mimics learned etiquette from nobles and hunger from everyone else.",
            "The Underking taxes footsteps, lies, and names spoken with fear.",
            "The Last Door opens inward because heroes are expected to push too hard.",
        ],
        "stats": ["Grit", "Wits", "Rations", "Renown", "Coin"],
        "items": ["Wax Writ", "Silver Chalk", "Toll Token", "Door Name"],
        "actions": [
            action("Route", "You spot murder holes above the pretty tiles and take the ugly stairs.", -3, -5, -2, 5, 0, 18, 1),
            action("Barter", "A bored ogre accepts soup, gossip, and one copper less than tradition.", -1, -4, -6, 7, -2, 16, 4),
            action("Rest", "You camp behind a saint statue and wake before the spiders vote.", 10, 8, -3, 1, 0, 12, 2),
            action("Sprint", "You run the blade corridor on instinct and arrive breathless but early.", -9, -6, -2, 9, 4, 22, 8),
        ],
    },
    {
        "slug": "guildmaster_pocket",
        "title": "Guildmaster",
        "role": "Role: guildmaster, contract judge, and keeper of the job board.",
        "premise": "Assign jobs, keep supplies moving, and make your tiny guild matter.",
        "accent": "0xAFE5",
        "acts": ["Empty Hall", "First Contracts", "Council Trouble", "Guild Banner"],
        "quests": [
            "Turn one rented room and a crooked board into a guild worth joining.",
            "Match risky contracts with rookies before debt collectors smell weakness.",
            "Win council recognition without selling the guild's spine.",
            "Raise the banner and survive the jobs that fame brings.",
        ],
        "lore": [
            "The guild hall used to be a bakery. The oven is now the evidence locker.",
            "Rookies believe contracts. Veterans read who signed them.",
            "The council hates adventurers until a basement starts whispering.",
            "A guild banner is cloth, debt, and everyone who slept under it.",
        ],
        "stats": ["Morale", "Focus", "Stores", "Fame", "Coin"],
        "items": ["Job Seal", "Roster Book", "Council Pin", "Banner Cord"],
        "actions": [
            action("Recruit", "A shield-singer joins after you promise honest pay and dishonest odds.", 0, -4, -2, 8, -3, 18, 2),
            action("Quest", "The crew returns muddy, grinning, and carrying proof of useful danger.", -5, -5, -5, 10, 6, 22, 1),
            action("Supply", "You stock torches, poultices, and enough rope to sound professional.", 4, 3, 11, 1, -5, 13, 4),
            action("Negotiate", "The council grants a license after you name three disasters you prevented.", -1, -3, 0, 9, 3, 17, 8),
        ],
    },
    {
        "slug": "haunted_radio_operator",
        "title": "Haunted Radio",
        "role": "Role: midnight operator, signal medium, and logbook witness.",
        "premise": "Tune ghost traffic, triage strange messages, and survive the night shift.",
        "accent": "0xC81F",
        "acts": ["Static Hour", "Numbers Rain", "Voice Beneath", "Sign-Off"],
        "quests": [
            "Tune the band until the station stops broadcasting your own breathing.",
            "Decode the number rain before it names someone still alive.",
            "Answer the voice beneath the carrier without inviting it into the room.",
            "Sign off cleanly and leave the next operator a survivable log.",
        ],
        "lore": [
            "The station sits where three county lines disagree and compasses get polite.",
            "Number rain is never random. It is only waiting for a scared listener.",
            "The voice beneath the carrier borrows words from lost emergency calls.",
            "A proper sign-off thanks the living first and the dead only if necessary.",
        ],
        "stats": ["Calm", "Focus", "Ground", "Reach", "Tape"],
        "items": ["Clean Clip", "Code Wheel", "Salt Wire", "Sign-Off"],
        "actions": [
            action("Tune", "The static parts into a road name nobody has used since the flood.", -4, -5, -1, 5, 1, 18, 1),
            action("Record", "You catch a clean clip before the tape grows warm in your hand.", -2, -3, -3, 7, 2, 17, 2),
            action("Ground", "Copper, salt, and steady breathing pull the room back into shape.", 10, 8, -4, 1, -1, 12, 4),
            action("Reply", "Something answers in Morse, using the desk lamp as punctuation.", -8, -6, -2, 10, 3, 22, 8),
        ],
    },
    {
        "slug": "monster_ranch_trail",
        "title": "Monster Ranch",
        "role": "Role: trail boss, monster keeper, and storm reader.",
        "premise": "Guide odd little beasts across ranch land without losing feed or trust.",
        "accent": "0x87F0",
        "acts": ["Barn Gate", "Glass Creek", "Thunder Flats", "Home Pasture"],
        "quests": [
            "Move the herd out before the smallest monsters learn how gates work.",
            "Cross Glass Creek while the water reflects animals you do not own yet.",
            "Keep the herd together through thunder that speaks in old brands.",
            "Reach home pasture with more trust than bite marks.",
        ],
        "lore": [
            "Monster ranchers name every beast twice: once for manners and once for magic.",
            "Glass Creek shows hungry futures unless someone sings over the crossing.",
            "Thunder Flats were branded by sky giants who lost their cattle.",
            "A home pasture is any fence the herd chooses not to test.",
        ],
        "stats": ["Bond", "Focus", "Feed", "Trust", "Scrip"],
        "items": ["Blue Halter", "Creek Song", "Storm Bell", "Home Brand"],
        "actions": [
            action("Feed", "Warm mash settles the nippers before they chew the moon again.", 5, 2, -8, 6, -1, 13, 1),
            action("Scout", "You find a dry ridge and only one suspiciously friendly burrow.", -3, -5, -2, 5, 0, 18, 2),
            action("Groom", "Mud, burrs, and bad temper come off in careful handfuls.", 7, 6, -2, 7, 0, 15, 4),
            action("Herd", "The herd surges as one bright, weird river of horns and paws.", -7, -5, -4, 9, 4, 22, 8),
        ],
    },
    {
        "slug": "pocket_detective_agency",
        "title": "Pocket Detective",
        "role": "Role: street detective, note keeper, and tiny office legend.",
        "premise": "Work tiny cases with clues, witnesses, stakeouts, and careful notes.",
        "accent": "0xFFFF",
        "acts": ["Cold Desk", "Alley Ledger", "Neon Witness", "Case Closed"],
        "quests": [
            "Turn a cheap office and a stranger's envelope into your first real case.",
            "Follow the ledger through alleys that know more than they should.",
            "Protect the witness long enough for the lie to contradict itself.",
            "Close the case without letting the city file off the sharp parts.",
        ],
        "lore": [
            "The agency sign is painted on cardboard, but the coffee is serious.",
            "Every alley ledger has two totals: money owed and fear collected.",
            "Neon witnesses remember color better than faces.",
            "A closed case still knocks when the wrong person gets comfortable.",
        ],
        "stats": ["Grit", "Focus", "Leads", "Cred", "Cash"],
        "items": ["Photo Clue", "Ledger Page", "Witness Pin", "Case Seal"],
        "actions": [
            action("Search", "A matchbook under the radiator names a club that burned down twice.", -2, -5, -2, 5, 0, 18, 1),
            action("Question", "The witness edits a lie into something useful.", 0, -4, -1, 8, 1, 17, 2),
            action("Stakeout", "Rain, bad coffee, and patience catch the handoff at 2:13.", -6, -6, -3, 9, 2, 22, 4),
            action("Close", "You pin the story to the desk before it wriggles into politics.", -1, -3, -2, 10, 5, 19, 8),
        ],
    },
    {
        "slug": "pocket_kingdom_manager",
        "title": "Pocket Kingdom",
        "role": "Role: pocket sovereign, harvest planner, and reluctant diplomat.",
        "premise": "Balance workers, food, trade, and morale in a palm-sized kingdom.",
        "accent": "0xFFE0",
        "acts": ["First Crown", "Granary Season", "Border Lanterns", "Small Golden Age"],
        "quests": [
            "Keep the new crown from sliding off while the village watches.",
            "Fill the granary before weather, rats, or cousins become policy problems.",
            "Settle the border lantern dispute without marching anyone into mud.",
            "Build a golden age small enough to defend and kind enough to remember.",
        ],
        "lore": [
            "The crown is copper because gold made the first king unbearable.",
            "Granary mice are counted as citizens during hard winters.",
            "Border lanterns mark roads, graves, and who apologized last.",
            "A small golden age is measured in full bowls and quiet nights.",
        ],
        "stats": ["Morale", "Focus", "Food", "Unity", "Coin"],
        "items": ["Copper Crown", "Granary Key", "Lantern Writ", "Harvest Bell"],
        "actions": [
            action("Build", "The masons raise a watchroom that doubles as a rain shelter.", -2, -4, -5, 7, -5, 18, 1),
            action("Farm", "The fields answer with enough grain to quiet the council.", 3, -3, 12, 3, 0, 14, 2),
            action("Trade", "The caravan overcharges until your scribe remembers old favors.", 0, -4, -3, 6, 7, 17, 4),
            action("Decree", "Your decree is short, fair, and surprisingly hard to misquote.", -1, -3, 0, 10, 1, 20, 8),
        ],
    },
    {
        "slug": "signal_rat_cyberdeck_rpg",
        "title": "Signal Rat",
        "role": "Role: tunnel decker, packet thief, and bad-node survivor.",
        "premise": "Crawl signal tunnels, avoid bad nodes, and recover packets for hire.",
        "accent": "0x7DFF",
        "acts": ["Drain Node", "Blue Cable", "Rat King Cache", "Clean Exfil"],
        "quests": [
            "Enter the drain node and find the packet trail before the rain rises.",
            "Follow the blue cable through loops built to waste frightened minutes.",
            "Steal from the Rat King cache without waking every watcher on the mesh.",
            "Exfil clean and decide which employer deserves the dangerous truth.",
        ],
        "lore": [
            "Signal rats know every tunnel has two maps: water and data.",
            "Blue cable means official work, old money, or a trap with nice labels.",
            "The Rat King cache is many stolen packets tied by one ugly secret.",
            "Clean exfil is a myth, but clean enough still pays.",
        ],
        "stats": ["Cover", "Focus", "Charge", "Rep", "Cred"],
        "items": ["Wet Map", "Blue Tap", "Cache Fang", "Exit Ghost"],
        "actions": [
            action("Sniff", "You catch a live route pulsing under the maintenance grate.", -2, -5, -2, 5, 1, 18, 1),
            action("Crawl", "The tunnel narrows, but the packet trail stays bright.", -7, -4, -3, 7, 2, 20, 2),
            action("Cache", "A hidden stash gives you charge, tape, and a name to avoid.", 4, 5, 8, 2, 1, 14, 4),
            action("Exfil", "You ghost out before the lockout, boots full of water and proof.", -5, -5, -2, 10, 6, 22, 8),
        ],
    },
    {
        "slug": "star_trader_pocket_frontier",
        "title": "Star Trader",
        "role": "Role: frontier captain, market reader, and hull patcher.",
        "premise": "Haul cargo, read prices, dodge risk, and keep the little ship flying.",
        "accent": "0x7BEF",
        "acts": ["Dock Seven", "Salted Lane", "Pirate Moon", "Frontier Ledger"],
        "quests": [
            "Buy cheap cargo before Dock Seven fees eat the launch window.",
            "Cross the salted lane where sensors lie and engines cough dust.",
            "Slip Pirate Moon with your hold intact and your transponder boring.",
            "Balance the frontier ledger and become welcome at the next port.",
        ],
        "lore": [
            "Dock Seven sells fuel, rumors, and noodles in that order.",
            "The salted lane was mined, cleared, cursed, and then mined again.",
            "Pirate Moon broadcasts recipes when it wants ships to relax.",
            "A frontier ledger records profit, favors, and who got home.",
        ],
        "stats": ["Hull", "Focus", "Fuel", "Name", "Cred"],
        "items": ["Cargo Seal", "Lane Chart", "Moon Code", "Trade Writ"],
        "actions": [
            action("Buy", "You buy underpriced filters from a broker too sleepy to lie well.", 0, -3, -2, 2, -5, 14, 1),
            action("Haul", "The lane bucks hard, but the cargo net holds.", -6, -5, -7, 6, 4, 20, 2),
            action("Repair", "A patch plate and three ugly welds keep vacuum outside.", 10, 5, -4, 1, -3, 13, 4),
            action("Sell", "The sale clears debt and earns a berth nobody spits near.", -1, -3, -2, 10, 8, 21, 8),
        ],
    },
    {
        "slug": "star_trail_rancher",
        "title": "Star Trail",
        "role": "Role: convoy rancher, starherd guide, and campfire mechanic.",
        "premise": "Drive a cosmic ranch convoy through weather, markets, and odd signals.",
        "accent": "0xB7E0",
        "acts": ["Launch Pasture", "Comet Ford", "Market Nebula", "Long Camp"],
        "quests": [
            "Launch the convoy before the youngest starcalves chase the beacon.",
            "Ford the comet stream while ice sings against the hulls.",
            "Sell goods in the nebula market without trading away your route.",
            "Make long camp where the herd can finally glow without fear.",
        ],
        "lore": [
            "Starcalves follow songs, engine hum, and anyone carrying sweet mineral salt.",
            "Comet fords change course when pilots brag.",
            "Nebula markets price goods by rarity, smell, and drama.",
            "Long camp is where every convoy story becomes a map for someone smaller.",
        ],
        "stats": ["Herd", "Focus", "Stores", "Trust", "Scrip"],
        "items": ["Salt Bell", "Comet Rope", "Market Pin", "Camp Star"],
        "actions": [
            action("Navigate", "You bend the convoy around a gravity bruise before it becomes news.", -3, -5, -3, 5, 1, 18, 1),
            action("Tend", "Starcalves settle as you clean frost from their glowing hides.", 8, 6, -3, 7, 0, 15, 2),
            action("Trade", "The market takes wool, gives parts, and pretends that was fair.", 0, -3, -4, 5, 6, 17, 4),
            action("Camp", "Repairs hold under violet sky while the herd dreams in sparks.", 9, 7, -5, 3, -1, 13, 8),
        ],
    },
    {
        "slug": "tiny_wasteland",
        "title": "Tiny Wasteland",
        "role": "Role: crew lead, water counter, and scrap-route prophet.",
        "premise": "Scavenge, shelter, and keep a small crew alive across harsh ground.",
        "accent": "0xFD20",
        "acts": ["Dust Mile", "Glass Town", "Red Storm", "Green Rumor"],
        "quests": [
            "Cross the dust mile with enough water to still make choices.",
            "Scavenge Glass Town before sunset turns the windows into knives.",
            "Shelter through the red storm and keep the crew from splitting.",
            "Find the green rumor and decide whether hope is a place or a trap.",
        ],
        "lore": [
            "Dust mile markers are old solar posts with names scratched over names.",
            "Glass Town shines because every wall remembers heat.",
            "Red storms carry voices from radios with no batteries.",
            "The green rumor has moved for ten years and still everyone walks toward it.",
        ],
        "stats": ["Grit", "Focus", "Water", "Hope", "Scrap"],
        "items": ["Water Map", "Glass Knife", "Storm Tarp", "Green Seed"],
        "actions": [
            action("Scavenge", "You pry scrap from a bus that still displays route 404.", -4, -5, -3, 4, 6, 18, 1),
            action("Scout", "A ridge line shows shelter, smoke, and one bad shortcut.", -3, -5, -2, 5, 1, 17, 2),
            action("Shelter", "Canvas snaps, grit hisses, and nobody leaves the circle.", 10, 7, -5, 4, -1, 13, 4),
            action("Trade", "The trader swaps fair enough after seeing your crew still laughs.", -1, -3, 3, 7, 3, 16, 8),
        ],
    },
    {
        "slug": "wasteland_guildmaster",
        "title": "Wasteland Guild",
        "role": "Role: job broker, caravan judge, and town peacekeeper.",
        "premise": "Run a dusty guild that brokers jobs between towns, crews, and caravans.",
        "accent": "0xF81F",
        "acts": ["Dust Board", "Crew Test", "Caravan Feud", "Guild Road"],
        "quests": [
            "Post the first jobs without getting the board stolen for firewood.",
            "Vet crews that can shoot straight, speak plain, and come back.",
            "Mediate the caravan feud before both sides hire your worst members.",
            "Open the guild road and make the towns believe in contracts again.",
        ],
        "lore": [
            "The dust board is a sheet of metal from a fallen water tower.",
            "A good crew test includes a broken tire, a locked box, and silence.",
            "Caravan feuds start with prices and end with cousins.",
            "The guild road is not safe. It is just dangerous in documented ways.",
        ],
        "stats": ["Order", "Focus", "Stores", "Cred", "Chits"],
        "items": ["Dust Seal", "Crew Token", "Peace Writ", "Road Bell"],
        "actions": [
            action("Post Job", "A clean job notice draws three crews and only one obvious liar.", 0, -3, -1, 7, 2, 16, 1),
            action("Vet Crew", "The crew passes after fixing the ambush you arranged badly on purpose.", -4, -5, -3, 9, 1, 21, 2),
            action("Restock", "Ammo, filters, and dried fruit make everyone less dramatic.", 5, 4, 11, 2, -5, 13, 4),
            action("Mediate", "You settle the feud with witness marks and one very public apology.", -2, -4, -1, 11, 4, 20, 8),
        ],
    },
]


INO_TEMPLATE = """#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {{
  "{title}",
  "{save_slug}",
  "{role}",
  "{premise}",
  {acts},
  {quests},
  {lore},
  {stats},
  {items},
  {actions},
  {accent}
}};

void setup() {{
  beginStoryGame(profile);
}}

void loop() {{
  loopStoryGame();
}}
"""

DOC_TEMPLATE = """# {title}

Status: `campaign_lite`

Source route: Cardputer Game OS individual game, rebuilt as a Waveshare touch-first campaign RPG.

## Campaign Loop

{premise}

Role: {role}

## Waveshare Controls

- Tap `Play`, `Sheet`, `Quest`, `Lore`, or `Pack` tabs to change screens.
- On `Play`, tap one of the four action cards to advance the campaign day.
- Swipe up/down/left/right to move between screens.
- Hold BOOT to return to the launcher.
- Serial fallback: `act 1`, `act 2`, `act 3`, `act 4`, `screen play`, `screen sheet`, `screen quest`, `screen lore`, `screen pack`, `status`, `save`, `reset`, `home`.

## Campaign Data

- Compact campaign state saves to `/waveshare-os/cardputer-game-os/states/{save_slug}.txt`.
- Journal entries append to `/waveshare-os/cardputer-game-os/saves/{save_slug}.txt`.
- Existing journal files are not deleted by the campaign upgrade.

## Port Notes

This keeps the Game OS catalog entry installable as its own launcher `.bin` while adding stats, chapters, lore, inventory unlocks, and richer narrative outcomes without Cardputer keyboard assumptions.
"""


def dash_slug(slug: str) -> str:
    return "gameos-" + slug.replace("_", "-")


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def cpp_array(values) -> str:
    return "{" + ", ".join(f'"{cpp_string(value)}"' for value in values) + "}"


def cpp_actions(actions) -> str:
    rows = []
    for item in actions:
        rows.append(
            '{{"{label}", "{result}", {hp}, {focus}, {kit}, {standing}, {wealth}, {xp}, {mask}}}'.format(
                label=cpp_string(item["label"]),
                result=cpp_string(item["result"]),
                hp=item["hp"],
                focus=item["focus"],
                kit=item["kit"],
                standing=item["standing"],
                wealth=item["wealth"],
                xp=item["xp"],
                mask=item["item"],
            )
        )
    return "{" + ", ".join(rows) + "}"


def main() -> None:
    DOCS.mkdir(parents=True, exist_ok=True)
    for game in GAMES:
        if game["slug"] in MANUAL_PORT_SLUGS:
            continue
        app_slug = dash_slug(game["slug"])
        app_dir = APPS / app_slug
        app_dir.mkdir(parents=True, exist_ok=True)
        ino = INO_TEMPLATE.format(
            title=cpp_string(game["title"]),
            save_slug=cpp_string(game["slug"]),
            role=cpp_string(game["role"]),
            premise=cpp_string(game["premise"]),
            acts=cpp_array(game["acts"]),
            quests=cpp_array(game["quests"]),
            lore=cpp_array(game["lore"]),
            stats=cpp_array(game["stats"]),
            items=cpp_array(game["items"]),
            actions=cpp_actions(game["actions"]),
            accent=game["accent"],
        )
        (app_dir / f"{app_slug}.ino").write_text(ino, encoding="utf-8")
        doc = DOC_TEMPLATE.format(
            title=game["title"],
            premise=game["premise"],
            role=game["role"].replace("Role: ", ""),
            save_slug=game["slug"],
        )
        (DOCS / f"{app_slug}.md").write_text(doc, encoding="utf-8")
    print(f"generated {len(GAMES)} Game OS campaign ports")


if __name__ == "__main__":
    main()
