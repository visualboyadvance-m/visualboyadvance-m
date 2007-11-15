///////  Rom Info
//////   Included by SDL.m


/*struct WinGBACompanyName {
  char *code;
  char *name;
};

static WinGBACompanyName winGBARomInfoCompanies[] = {
  { "01", "Nintendo" },
  { "02", "Rocket Games" },
  { "08", "Capcom" },
  { "09", "Hot B Co." },
  { "0A", "Jaleco" },
  { "0B", "Coconuts" },
  { "0C", "Coconuts Japan/Elite" },
  { "0H", "Starfish" },
  { "0L", "Warashi Inc." },
  { "13", "Electronic Arts Japan" },
  { "18", "Hudson Soft Japan" },
  { "1A", "Yonoman/Japan Art Media" },
  { "1P", "Creatures Inc." },
  { "20", "Destination Software" },
  { "22", "VR 1 Japan" },
  { "25", "San-X" },
  { "28", "Kemco Japan" },
  { "29", "Seta" },
  { "2K", "NEC InterChannel" },
  { "2L", "Tam" },
  { "2M", "GU Inc/Gajin/Jordan" },
  { "2N", "Smilesoft" },
  { "2Q", "Mediakite/Systemsoft Alpha Corp" },
  { "36", "Codemasters" },
  { "37", "GAGA Communications" },
  { "38", "Laguna" },
  { "39", "Telstar Fun and Games" },
  { "41", "Ubi Soft Entertainment" },
  { "47", "Spectrum Holobyte" },
  { "49", "IREM" },
  { "4D", "Malibu Games" },
  { "4F", "U.S. Gold" },
  { "4J", "Fox Interactive" },
  { "4K", "Time Warner Interactive" },
  { "4Q", "Disney" },
  { "4S", "EA SPORTS/THQ" },
  { "4X", "GT Interactive" },
  { "4Y", "RARE" },
  { "4Z", "Crave Entertainment" },
  { "50", "Absolute Entertainment" },
  { "51", "Acclaim" },
  { "52", "Activision" },
  { "53", "American Sammy Corp." },
  { "54", "Take 2 Interactive" },
  { "55", "Hi Tech" },
  { "56", "LJN LTD." },
  { "58", "Mattel" },
  { "5A", "Red Orb Entertainment" },
  { "5C", "Taxan" },
  { "5D", "Midway" },
  { "5G", "Majesco Sales Inc" },
  { "5H", "3DO" },
  { "5K", "Hasbro" },
  { "5L", "NewKidCo" },
  { "5M", "Telegames" },
  { "5N", "Metro3D" },
  { "5P", "Vatical Entertainment" },
  { "5Q", "LEGO Media" },
  { "5S", "Xicat Interactive" },
  { "5T", "Cryo Interactive" },
  { "5X", "Microids" },
  { "5W", "BKN Ent./Red Storm Ent." },
  { "5Z", "Conspiracy Entertainment Corp." },
  { "60", "Titus Interactive Studios" },
  { "61", "Virgin Interactive" },
  { "64", "LucasArts Entertainment" },
  { "67", "Ocean" },
  { "69", "Electronic Arts" },
  { "6E", "Elite Systems Ltd." },
  { "6F", "Electro Brain" },
  { "6H", "Crawfish" },
  { "6L", "BAM! Entertainment" },
  { "6M", "Studio 3" },
  { "6Q", "Classified Games" },
  { "6S", "TDK Mediactive" },
  { "6U", "DreamCatcher" },
  { "6V", "JoWood Productions" },
  { "6W", "SEGA" },
  { "6Y", "LSP" },
  { "70", "Infogrames" },
  { "71", "Interplay" },
  { "72", "JVC Musical Industries Inc" },
  { "75", "SCI" },
  { "78", "THQ" },
  { "79", "Accolade" },
  { "7A", "Triffix Ent. Inc." },
  { "7C", "Microprose Software" },
  { "7D", "Universal Interactive Studios" },
  { "7F", "Kemco" },
  { "7G", "Rage Software" },
  { "7M", "Asmik Ace Entertainment Inc./AIA" },
  { "83", "LOZC/G.Amusements" },
  { "8B", "Bulletproof Software" },
  { "8C", "Vic Tokai Inc." },
  { "8J", "General Entertainment" },
  { "8N", "Success" },
  { "8P", "SEGA Japan" },
  { "91", "Chun Soft" },
  { "93", "BEC" },
  { "97", "Kaneko" },
  { "99", "Victor Interactive Software" },
  { "9B", "Tecmo" },
  { "9C", "Imagineer" },
  { "9H", "Bottom Up" },
  { "9N", "Marvelous Entertainment" },
  { "9P", "Keynet Inc." },
  { "9Q", "Hands-On Entertainment" },
  { "A0", "Telenet/Olympia" },
  { "A1", "Hori" },
  { "A4", "Konami" },
  { "A6", "Kawada" },
  { "A7", "Takara" },
  { "A9", "Technos Japan Corp." },
  { "AC", "Toei Animation" },
  { "AD", "Toho" },
  { "AF", "Namco" },
  { "AG", "Media Rings Corporation/Amedio/Playmates" },
  { "AH", "J-Wing" },
  { "AK", "KID" },
  { "AL", "MediaFactory" },
  { "B0", "Acclaim Japan" },
  { "B1", "Nexoft" },
  { "B2", "Bandai" },
  { "B4", "Enix" },
  { "B6", "HAL Laboratory" },
  { "B7", "SNK" },
  { "B9", "Pony Canyon" },
  { "BA", "Culture Brain" },
  { "BB", "Sunsoft" },
  { "BD", "Sony Imagesoft" },
  { "BF", "Sammy" },
  { "BG", "Magical" },
  { "BJ", "Compile" },
  { "BL", "MTO Inc." },
  { "BN", "Sunrise Interactive" },
  { "BP", "Global A Entertainment" },
  { "BQ", "Fuuki" },
  { "C0", "Taito" },
  { "C2", "Kemco" },
  { "C3", "Square Soft" },
  { "C5", "Data East" },
  { "C6", "Broderbund Japan" },
  { "C8", "Koei" },
  { "CA", "Ultra Games" },
  { "CB", "Vapinc/NTVIC" },
  { "CC", "Use Co.,Ltd." },
  { "CE", "FCI" },
  { "CF", "Angel" },
  { "CM", "Konami Computer Enterteinment Osaka" },
  { "D1", "Sofel" },
  { "D3", "Sigma Enterprises" },
  { "D4", "Ask Kodansa/Lenar" },
  { "D7", "Copya System" },
  { "D9", "Banpresto" },
  { "DA", "TOMY" },
  { "DD", "NCS" },
  { "DF", "Altron Corporation" },
  { "E2", "Lad/Shogakukan.Nas/Yutaka" },
  { "E3", "Varie" },
  { "E5", "Epoch" },
  { "E7", "Athena" },
  { "E8", "Asmik Ace Entertainment Inc." },
  { "E9", "Natsume" },
  { "EA", "King Records" },
  { "EB", "Atlus" },
  { "EC", "Epic/Sony/Ocean" },
  { "EE", "IGS" },
  { "EL", "Vaill" },
  { "EM", "Konami Computer Entertainment Tokyo" },
  { "EN", "Alphadream Corporation" },
  { "F0", "A Wave" },
  { "HY", "Sachen" },
  { NULL, NULL }
};

static char *winGBARomInfoFindMakerCode(char *code)
{
  int i = 0;
  while(winGBARomInfoCompanies[i].code) {
    if(!strcmp(winGBARomInfoCompanies[i].code, code))
      return winGBARomInfoCompanies[i].name;
    i++;
  }
  return 0;
}*/

if (showInfo)
        {
        char buffer[13];
        char maker[100];
        strncpy(buffer, (const char *)&rom[0xa0], 12);
        buffer[12] = 0;
        NSString * name = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoName:name];

        strncpy(buffer, (const char *)&rom[0xac], 4);
        buffer[4] = 0;
        NSString * gameCode = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoGameCode:gameCode];

        strncpy(buffer, (const char *)&rom[0xb0],2);
        buffer[2] = 0;
        NSString * makerCode = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoMakerCode:makerCode];

        strcpy(maker, winGBARomInfoFindMakerCode(buffer));
        NSString * makerString = [[NSString alloc] initWithCString:maker];
        [gSDLMain romInfoMaker:makerString];

        sprintf(buffer, "%02x", rom[0xb3]);
        NSString * unitCode = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoUnitCode:unitCode];

        sprintf(buffer, "%02x", rom[0xb4]);
        NSString * deviceType = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoDeviceType:deviceType];

        sprintf(buffer, "%02x", rom[0xbc]);
        NSString * version = [[NSString alloc] initWithCString:buffer];
        [gSDLMain romInfoVersion:version];
        }


///////
