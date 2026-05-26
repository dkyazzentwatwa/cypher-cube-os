#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Guildmaster",
  "guildmaster_pocket",
  "Role: guildmaster, contract judge, and keeper of the job board.",
  "Assign jobs, keep supplies moving, and make your tiny guild matter.",
  {"Empty Hall", "First Contracts", "Council Trouble", "Guild Banner"},
  {"Turn one rented room and a crooked board into a guild worth joining.", "Match risky contracts with rookies before debt collectors smell weakness.", "Win council recognition without selling the guild's spine.", "Raise the banner and survive the jobs that fame brings."},
  {"The guild hall used to be a bakery. The oven is now the evidence locker.", "Rookies believe contracts. Veterans read who signed them.", "The council hates adventurers until a basement starts whispering.", "A guild banner is cloth, debt, and everyone who slept under it."},
  {"Morale", "Focus", "Stores", "Fame", "Coin"},
  {"Job Seal", "Roster Book", "Council Pin", "Banner Cord"},
  {{"Recruit", "A shield-singer joins after you promise honest pay and dishonest odds.", 0, -4, -2, 8, -3, 18, 2}, {"Quest", "The crew returns muddy, grinning, and carrying proof of useful danger.", -5, -5, -5, 10, 6, 22, 1}, {"Supply", "You stock torches, poultices, and enough rope to sound professional.", 4, 3, 11, 1, -5, 13, 4}, {"Negotiate", "The council grants a license after you name three disasters you prevented.", -1, -3, 0, 9, 3, 17, 8}},
  0xAFE5
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
