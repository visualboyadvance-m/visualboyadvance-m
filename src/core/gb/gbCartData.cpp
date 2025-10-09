#include "core/gb/gbCartData.h"

#include <algorithm>
#include <array>

#include "core/base/check.h"
#include "core/base/sizes.h"

namespace {

#pragma pack(push, 1)

// Packed struct for the ROM header.
struct gbRomHeader {
    uint8_t header[0x100];
    // Entry point starts at 0x100.
    uint8_t entry_point[4];
    uint8_t nintendo_logo[0x30];
    union {
        // The original format.
        struct {
            uint8_t title[18];
        } dmg;
        // Some homebrew use this format. We also use this to workaround the
        // manufacturer code being incorrectly set in some commercial carts.
        struct {
            uint8_t title[15];
            uint8_t cgb_flag;
            uint8_t padding[2];
        } hb;
        // The SGB format. The title can be up to 16 bytes long, since the last
        // byte was not (yet) used for CGB identification.
        struct {
            uint8_t title[16];
            uint8_t new_licensee_code[2];
        } sgb;
        // The final-ish CGB format. Some homebrew and commecial carts do not
        // actually set the manufacturer code field properly so we revert to
        // the "homebrew format" for these.
        struct {
            uint8_t title[11];
            uint8_t manufacturer_code[4];
            uint8_t cgb_flag;
            uint8_t new_licensee_code[2];
        } cgb;
    };
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
        VBAM_NOTREACHED();
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

template <typename T, std::size_t N>
constexpr std::size_t array_size(const T (&)[N]) {
    return N;
}

// Constructs a "real" string from a raw buffer. The buffer could have a null
// value. If it does, the string will be truncated at that point.
template <typename T>
std::string from_buffer(const T& buffer) {
    // The string is null terminated, so we can use std::find to find the end.
    return std::string(buffer,
                       std::find(buffer, buffer + array_size(buffer), '\0'));
}

bool is_valid_manufacturer_code(const std::string& manufacturer_code) {
    return manufacturer_code.size() == 4 &&
           std::all_of(manufacturer_code.begin(), manufacturer_code.end(),
                       [](char c) { return std::isalnum(c); });
}

constexpr size_t kHeaderGlobalChecksumAdress = 0x14e;

uint16_t get_rom_checksum(const uint8_t* romData, size_t romDataSize) {
    VBAM_CHECK(romData);

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

    return std::equal(romHeader->dmg.title,
                      romHeader->dmg.title + kGameSharkTitle.size(),
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
    VBAM_CHECK(romData);

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

    // SGB support flag.
    sgb_flag_ = header->sgb_flag;
    sgb_support_ = (sgb_flag_ == kSgbSetFlag);

    // CGB support flag. This needs to be parsed now to work around some
    // incorrect homebrew ROMs.
    cgb_flag_ = header->cgb.cgb_flag;
    if (cgb_flag_ == kCgbSupportedFlag) {
        cgb_support_ = CGBSupport::kSupported;
    } else if (cgb_flag_ == kCgbRequiredFlag) {
        cgb_support_ = CGBSupport::kRequired;
    } else {
        // Any other value must be interpreted as being part of the title,
        // with the exception of some homebrew.
        cgb_support_ = CGBSupport::kNone;
    }

    old_licensee_code_ = header->old_licensee_code;
    if (old_licensee_code_ == kNewManufacturerCode) {
        // New licensee code format.
        header_type_ = RomHeaderType::kNewLicenseeCode;
        maker_code_ = from_buffer(header->cgb.new_licensee_code);

        if (cgb_support_ == CGBSupport::kNone) {
            title_ = from_buffer(header->sgb.title);
        } else {
            manufacturer_code_ = from_buffer(header->cgb.manufacturer_code);
            if (is_valid_manufacturer_code(manufacturer_code_)) {
                title_ = from_buffer(header->cgb.title);
            } else {
                // Some ROMS and homebrew do not actually set this field.
                // Interpret the title as a homebrew title instead.
                manufacturer_code_.clear();
                title_ = from_buffer(header->hb.title);
            }
        }
    } else {
        // Old licensee code format.
        header_type_ = RomHeaderType::kOldLicenseeCode;

        if (cgb_support_ == CGBSupport::kNone) {
            // Rese the CGB flag here. This is part of the title.
            cgb_flag_ = 0;
            title_ = from_buffer(header->dmg.title);
        } else {
            // This is (incorrectly) used by some homebrew.
            title_ = from_buffer(header->hb.title);
        }

        maker_code_ = old_licensee_to_string(header->old_licensee_code);
    }

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
            // This specific flag does not seem to exist in any cart.
            mapper_type_ = MapperType::kMbc2;
            ram_size_ = k512B;
            skip_ram = true;
            break;
        case 0x06:
            // MBC2 header does not specify a RAM size so set it here.
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
            // No cart seems to have a proper dump for this.
            mapper_type_ = MapperType::kMmm01;
            break;
        case 0x0c:
            // No cart seems to have a proper dump for this.
            mapper_type_ = MapperType::kMmm01;
            break;
        case 0x0d:
            // No cart seems to have a proper dump for this.
            mapper_type_ = MapperType::kMmm01;
            has_battery_ = true;
            break;
        case 0x0f:
            // This specific flag does not seem to exist in any cart.
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
            // This specific flag does not seem to exist in any cart.
            mapper_type_ = MapperType::kMbc3;
            break;
        case 0x12:
            // This specific flag does not seem to exist in any cart.
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
            // This specific flag does not seem to exist in any cart.
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
            if (header->rom_size == 0x05) {
                // Kirby Tilt 'n' Tumble / Korokoro Kirby.
                ram_size_ = k256B;
            } else if (header->rom_size == 0x06) {
                // Command Master.
                ram_size_ = k512B;
            } else {
                // Not a licensed MBC7 cart. Default to the larger size and hope
                // for the best.
                ram_size_ = k512B;
            }
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

    destination_code_flag_ = header->destination_code;
    if (destination_code_flag_ == 0x00) {
        destination_code_ = DestinationCode::kJapanese;
    } else if (destination_code_flag_ == 0x01) {
        destination_code_ = DestinationCode::kWorldwide;
    }

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

    if (has_battery_ && !has_rtc_ && ram_size_ == 0) {
        // Some homebrew ROMs have the battery flag set but no RAM or RTC. This
        // is probably a mistake, so we ignore the battery flag.
        has_battery_ = false;
    }

    validity_ = Validity::kValid;
}
