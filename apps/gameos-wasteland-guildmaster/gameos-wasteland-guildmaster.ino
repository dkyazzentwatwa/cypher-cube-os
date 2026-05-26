#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Wasteland Guild",
  "wasteland_guildmaster",
  "Role: job broker, caravan judge, and town peacekeeper.",
  "Run a dusty guild that brokers jobs between towns, crews, and caravans.",
  {"Dust Board", "Crew Test", "Caravan Feud", "Guild Road"},
  {"Post the first jobs without getting the board stolen for firewood.", "Vet crews that can shoot straight, speak plain, and come back.", "Mediate the caravan feud before both sides hire your worst members.", "Open the guild road and make the towns believe in contracts again."},
  {"The dust board is a sheet of metal from a fallen water tower.", "A good crew test includes a broken tire, a locked box, and silence.", "Caravan feuds start with prices and end with cousins.", "The guild road is not safe. It is just dangerous in documented ways."},
  {"Order", "Focus", "Stores", "Cred", "Chits"},
  {"Dust Seal", "Crew Token", "Peace Writ", "Road Bell"},
  {{"Post Job", "A clean job notice draws three crews and only one obvious liar.", 0, -3, -1, 7, 2, 16, 1}, {"Vet Crew", "The crew passes after fixing the ambush you arranged badly on purpose.", -4, -5, -3, 9, 1, 21, 2}, {"Restock", "Ammo, filters, and dried fruit make everyone less dramatic.", 5, 4, 11, 2, -5, 13, 4}, {"Mediate", "You settle the feud with witness marks and one very public apology.", -2, -4, -1, 11, 4, 20, 8}},
  0xF81F
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
