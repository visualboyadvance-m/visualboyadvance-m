#include "gbCartData.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>

#include "../common/Port.h"
#include "../common/sizes.h"

namespace {

#pragma pack(push, 1)

// Packed struct for the ROM header.
struct gbRomHeader {
    uint8_t header[0x100];
    // Entry point starts at 0x100.
    uint8_t entry_point[4];
    uint8_t nintendo_logo[0x30];
    union {
        uint8_t old_title[16];
        struct {
            uint8_t title[11];
            uint8_t manufacturer_code[4];
            uint8_t cgb_flag;
        } new_format;
    };
    uint8_t new_licensee_code[2];
    uint8_t sgb_flag;
    uint8_t cartridge_type;
    uint8_t rom_size;
    uint8_t ram_size;
    uint8_t destination_code;
    uint8_t old_licensee_code;
    uint8_t version_number;
    uint8_t header_checksum;
};

#pragma pack(pop)

char byte_to_char(uint8_t byte) {
    if (byte < 10) {
        return '0' + byte;
    } else if (byte < 16) {
        return 'A' + (byte - 10);
    } else {
        assert(false);
        return '\0';
    }
}

std::string old_licensee_to_string(uint8_t licensee) {
    const std::array<char, 2> result = {
        byte_to_char(licensee / 16),
        byte_to_char(licensee % 16),
    };
    return std::string(result.begin(), result.end());
}

constexpr size_t kHeaderGlobalChecksumAdress = 0x14e;

uint16_t get_rom_checksum(const uint8_t* romData, size_t romDataSize) {
    assert(romData);

    uint16_t checksum = 0;
    for (size_t i = 0; i < romDataSize; i++) {
        // Skip over the global checksum bytes.
        if (i == kHeaderGlobalChecksumAdress ||
            i == kHeaderGlobalChecksumAdress + 1) {
            continue;
        }
        checksum += romData[i];
    }

    return checksum;
}

bool is_game_genie(const gbRomHeader* romHeader) {
    static constexpr std::array<uint8_t, 16> kGameGenieHeader = {
        0x47, 0x61, 0x6d, 0x65, 0x20, 0x47, 0x65, 0x6e,
        0x69, 0x65, 0x28, 0x54, 0x4d, 0x29, 0x20, 0x73,
    };

    return std::equal(romHeader->header,
                      romHeader->header + kGameGenieHeader.size(),
                      kGameGenieHeader.begin(), kGameGenieHeader.end());
}

bool is_game_shark(const gbRomHeader* romHeader) {
    static constexpr std::array<uint8_t, 14> kGameSharkTitle = {
        0x41, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20,
        0x52, 0x65, 0x70, 0x6c, 0x61, 0x79, 0x20};

    return std::equal(romHeader->old_title,
                      romHeader->old_title + kGameSharkTitle.size(),
                      kGameSharkTitle.begin(), kGameSharkTitle.end());
}

bool is_nintendo_logo_valid(const gbRomHeader* romHeader) {
    static constexpr std::array<uint8_t, 0x30> kNintendoLogo = {
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83,
        0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63,
        0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
    };

    return std::equal(romHeader->nintendo_logo,
                      romHeader->nintendo_logo + kNintendoLogo.size(),
                      kNintendoLogo.begin(), kNintendoLogo.end());
}

constexpr uint8_t kNewManufacturerCode = 0x33;
constexpr uint8_t kSgbSetFlag = 0x03;
constexpr uint8_t kCgbSupportedFlag = 0x80;
constexpr uint8_t kCgbRequiredFlag = 0xc0;

// The start and end addresses of the cartridge header. This is used for the
// header checksum calculation.
constexpr size_t kHeaderChecksumStartAdress = 0x134;
constexpr size_t kHeaderChecksumEndAdress = 0x14c;

}  // namespace

gbCartData::gbCartData(const uint8_t* romData, size_t romDataSize) {
    assert(romData);

    if (romDataSize < sizeof(gbRomHeader)) {
        validity_ = Validity::kSizeTooSmall;
        return;
    }

    const gbRomHeader* header = (gbRomHeader*)romData;

    if (is_game_genie(header)) {
        // Game Genie is special.
        title_ = "Game Genie";
        header_type_ = RomHeaderType::kOldLicenseeCode;
        mapper_flag_ = 0x55;
        mapper_type_ = MapperType::kGameGenie;
        rom_size_ = k32KiB;
        rom_mask_ = rom_size_ -1;
        validity_ = Validity::kValid;
        return;
    }

    if (is_game_shark(header)) {
        // Game Shark is special.
        title_ = "Action Replay";
        header_type_ = RomHeaderType::kOldLicenseeCode;
        mapper_flag_ = 0x56;
        mapper_type_ = MapperType::kGameShark;
        rom_size_ = k32KiB;
        rom_mask_ = rom_size_ -1;
        validity_ = Validity::kValid;
        return;
    }


    if (!is_nintendo_logo_valid(header)) {
        validity_ = Validity::kNoNintendoLogo;
        return;
    }

    old_licensee_code_ = header->old_licensee_code;
    if (old_licensee_code_ == kNewManufacturerCode) {
        // New licensee code format.
        header_type_ = RomHeaderType::kNewLicenseeCode;
        title_ =
            std::string(reinterpret_cast<const char*>(header->new_format.title),
                        sizeof(header->new_format.title));
        maker_code_ = std::string(
            reinterpret_cast<const char*>(header->new_licensee_code),
            sizeof(header->new_licensee_code));
        manufacturer_code_ = std::string(
            reinterpret_cast<const char*>(header->new_format.manufacturer_code),
            sizeof(header->new_format.manufacturer_code));

        // CGB support flag.
        cgb_flag_ = header->new_format.cgb_flag;
        if (cgb_flag_ == kCgbSupportedFlag) {
            cgb_support_ = CGBSupport::kSupported;
        } else if (cgb_flag_ == kCgbRequiredFlag) {
            cgb_support_ = CGBSupport::kRequired;
        } else {
            cgb_support_ = CGBSupport::kNone;
        }
    } else {
        // Old licensee code format.
        header_type_ = RomHeaderType::kOldLicenseeCode;

        title_ = std::string(reinterpret_cast<const char*>(header->old_title),
                             sizeof(header->old_title));

        maker_code_ = old_licensee_to_string(header->old_licensee_code);

        cgb_flag_ = 0;
        cgb_support_ = CGBSupport::kNone;
        manufacturer_code_ = "";
    }

    // SGB support flag.
    sgb_flag_ = header->sgb_flag;
    sgb_support_ = header->sgb_flag == kSgbSetFlag;

    // We may have to skip the RAM size calculation for some mappers.
    bool skip_ram = false;
    mapper_flag_ = header->cartridge_type;
    switch (mapper_flag_) {
        case 0x00:
            mapper_type_ = MapperType::kNone;
            break;
        case 0x01:
            mapper_type_ = MapperType::kMbc1;
            break;
        case 0x02:
            mapper_type_ = MapperType::kMbc1;
            break;
        case 0x03:
            mapper_type_ = MapperType::kMbc1;
            has_battery_ = true;
            break;
        case 0x05:
            // MBC2 header does not specify a RAM size so set it here.
            mapper_type_ = MapperType::kMbc2;
            ram_size_ = k512B;
            skip_ram = true;
            break;
        case 0x06:
            mapper_type_ = MapperType::kMbc2;
            ram_size_ = k512B;
            skip_ram = true;
            has_battery_ = true;
            break;
        case 0x08:
            mapper_type_ = MapperType::kNone;
            break;
        case 0x09:
            mapper_type_ = MapperType::kNone;
            has_battery_ = true;
            break;
        case 0x0b:
            mapper_type_ = MapperType::kMmm01;
            break;
        case 0x0c:
            mapper_type_ = MapperType::kMmm01;
            break;
        case 0x0d:
            mapper_type_ = MapperType::kMmm01;
            has_battery_ = true;
            break;
        case 0x0f:
            mapper_type_ = MapperType::kMbc3;
            has_rtc_ = true;
            has_battery_ = true;
            break;
        case 0x10:
            mapper_type_ = MapperType::kMbc3;
            has_rtc_ = true;
            has_battery_ = true;
            break;
        case 0x11:
            mapper_type_ = MapperType::kMbc3;
            break;
        case 0x12:
            mapper_type_ = MapperType::kMbc3;
            break;
        case 0x13:
            mapper_type_ = MapperType::kMbc3;
            has_battery_ = true;
            break;
        case 0x19:
            mapper_type_ = MapperType::kMbc5;
            break;
        case 0x1a:
            mapper_type_ = MapperType::kMbc5;
            break;
        case 0x1b:
            mapper_type_ = MapperType::kMbc5;
            has_battery_ = true;
            break;
        case 0x1c:
            mapper_type_ = MapperType::kMbc5;
            has_rumble_ = true;
            break;
        case 0x1d:
            mapper_type_ = MapperType::kMbc5;
            has_rumble_ = true;
            break;
        case 0x1e:
            mapper_type_ = MapperType::kMbc5;
            has_battery_ = true;
            has_rumble_ = true;
            break;
        case 0x20:
            mapper_type_ = MapperType::kMbc6;
            break;
        case 0x22:
            // MBC7 header does not specify a RAM size so set it here.
            mapper_type_ = MapperType::kMbc7;
            ram_size_ = k512B;
            skip_ram = true;
            has_battery_ = true;
            has_rumble_ = true;
            has_sensor_ = true;
            break;
        case 0x55:
            mapper_type_ = MapperType::kGameGenie;
            break;
        case 0x56:
            mapper_type_ = MapperType::kGameShark;
            break;
        case 0xfc:
            mapper_type_ = MapperType::kPocketCamera;
            break;
        case 0xfd:
            // Tama5 header does not specify a RAM size so set it here.
            mapper_type_ = MapperType::kTama5;
            ram_size_ = k32KiB;
            skip_ram = true;
            has_battery_ = true;
            has_rtc_ = true;
            break;
        case 0xfe:
            mapper_type_ = MapperType::kHuC3;
            has_battery_ = true;
            has_rtc_ = true;
            break;
        case 0xff:
            mapper_type_ = MapperType::kHuC1;
            has_battery_ = true;
            break;
        default:
            validity_ = Validity::kUnknownMapperType;
            return;
    }

    rom_flag_ = header->rom_size;
    switch (rom_flag_) {
        case 0x00:
            rom_size_ = k32KiB;
            break;
        case 0x01:
            rom_size_ = k64KiB;
            break;
        case 0x02:
            rom_size_ = k128KiB;
            break;
        case 0x03:
            rom_size_ = k256KiB;
            break;
        case 0x04:
            rom_size_ = k512KiB;
            break;
        case 0x05:
            rom_size_ = k1MiB;
            break;
        case 0x06:
            rom_size_ = k2MiB;
            break;
        case 0x07:
            rom_size_ = k4MiB;
            break;
        case 0x08:
            rom_size_ = k8MiB;
            break;
        default:
            validity_ = Validity::kUnknownRomSize;
            return;
    }

    // The ROM mask is always rom_size_ - 1.
    rom_mask_ = rom_size_ - 1;

    ram_flag_ = header->ram_size;
    if (!skip_ram) {
        switch (ram_flag_) {
            case 0x00:
                ram_size_ = 0;
                break;
            case 0x01:
                ram_size_ = k2KiB;
                break;
            case 0x02:
                ram_size_ = k8KiB;
                break;
            case 0x03:
                ram_size_ = k32KiB;
                break;
            case 0x04:
                ram_size_ = k128KiB;
                break;
            case 0x05:
                // Technically, this refers to a 1 Mbit (128 KiB) RAM chip, but
                // this is only used in Pocket Monsters Crystal JP, which has a
                // mapper chip referred as "MBC30". This mapper can only address
                // half of the RAM chip. So, we set the size to 64 KiB.
                ram_size_ = k64KiB;
                break;
            default:
                validity_ = Validity::kUnknownRamSize;
                return;
        }
    }

    if (ram_size_ != 0) {
        ram_mask_ = ram_size_ - 1;
    }

    version_flag_ = header->version_number;

    // Calculate the header checksum.
    header_checksum_ = header->header_checksum;
    uint8_t checksum = 0;
    for (uint16_t address = kHeaderChecksumStartAdress;
         address <= kHeaderChecksumEndAdress; address++) {
        checksum = checksum - romData[address] - 1;
    }
    actual_header_checksum_ = checksum;

    if (header_checksum_ != actual_header_checksum_) {
        validity_ = Validity::kInvalidHeaderChecksum;
        return;
    }

    // The global checksum is stored in big endian.
    global_checksum_ = *(romData + kHeaderGlobalChecksumAdress) * 0x100 +
                       *(romData + kHeaderGlobalChecksumAdress + 1);

    // The global checksum value is computed here. This does not fail the header
    // verification as it does not on actual hardware either.
    actual_global_checksum_ = get_rom_checksum(romData, romDataSize);

    validity_ = Validity::kValid;
}
