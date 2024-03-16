#ifndef VBAM_CORE_GB_GBCARTDATA_H_
#define VBAM_CORE_GB_GBCARTDATA_H_

#include <cstdint>
#include <string>

class gbCartData final {
public:
    enum class RomHeaderType {
        kOldLicenseeCode,
        kNewLicenseeCode,
        kInvalid,
    };

    enum class MapperType {
        kNone,
        kMbc1,
        kMbc2,
        kMbc3,
        kMbc5,
        kMbc6,
        kMbc7,
        kPocketCamera,
        kMmm01,
        kHuC1,
        kHuC3,
        kTama5,
        kGameGenie,
        kGameShark,
        kUnknown,
    };

    enum class CGBSupport {
        kNone,
        kSupported,
        kRequired,
    };

    enum class DestinationCode {
        kJapanese,
        kWorldwide,
        kUnknown,
    };

    enum class Validity {
        kValid,
        // This object is uninitialized (default state).
        kUninitialized,
        // The provided size is too small to contain a valid header.
        kSizeTooSmall,
        // The mapper code is unknown. mapper_flag() will be set.
        kUnknownMapperType,
        // The ROM size byte is unknown. rom_flag() will be set.
        kUnknownRomSize,
        // The RAM size byte is unknown. ram_flag() will be set.
        kUnknownRamSize,
        // The Nintendo logo is invalid.
        kNoNintendoLogo,
        // The header checksum is invalid. header_checksum() and
        // actual_header_checksum() will be set.
        kInvalidHeaderChecksum,
    };

    gbCartData() = default;
    gbCartData(const uint8_t* romData, size_t romDataSize);
    ~gbCartData() = default;

    // Whether CGB is supported in any form.
    bool SupportsCGB() const {
        return cgb_support_ == CGBSupport::kSupported ||
               cgb_support_ == CGBSupport::kRequired;
    };

    // Whether CGB support is required.
    bool RequiresCGB() const { return cgb_support_ == CGBSupport::kRequired; };

    // Whether this ROM is valid.
    bool IsValid() const { return validity_ == Validity::kValid; }
    operator bool() const { return IsValid(); }

    // Whether this cartridge has RAM.
    bool HasRam() const { return ram_size() != 0; }

    // The Validity of this object. Doubles as an error code. If this does not
    // return kValid, this object may be in a half-initialized state and no
    // other accessor should be considered to return valid data.
    Validity validity() const { return validity_; }

    // The Game Boy cartridge header type.
    RomHeaderType header_type() const { return header_type_; }

    // The old licensee code in the ROM header (0x014b). Set to 0x33 if
    // header_type() is kNewLicenseeCode.
    uint8_t old_licensee_code() const { return old_licensee_code_; }
    // The ROM title.
    const std::string& title() const { return title_; }
    // The 2-characters Maker code.
    const std::string& maker_code() const { return maker_code_; }
    // The 4-characters manufacturer code.
    // Only set if header_type() is kNewLicenseeCode. Empty string otherwise.
    const std::string& manufacturer_code() const { return manufacturer_code_; }

    // The mapper flag in the ROM header (0x0147).
    uint8_t mapper_flag() const { return mapper_flag_; }
    // The cartridge mapper type.
    MapperType mapper_type() const { return mapper_type_; }

    // Whether the cartridge has battery support.
    bool has_battery() const { return has_battery_; }
    // Whether the cartridge has RTC support.
    bool has_rtc() const { return has_rtc_; }
    // Whether the cartridge has rumble support.
    bool has_rumble() const { return has_rumble_; }
    // Whether the cartridge has sensor support (accelerometer).
    bool has_sensor() const { return has_sensor_; }

    // The CGB flag in the ROM header (0x0143).
    uint8_t cgb_flag() const { return cgb_flag_; }
    // Whether the ROM supports or requires CGB.
    CGBSupport cgb_support() const { return cgb_support_; }

    // The SGB flag in the ROM header (0x0146).
    uint8_t sgb_flag() const { return sgb_flag_; }
    // Whether the ROM supports SGB.
    bool sgb_support() const { return sgb_support_; }

    // The ROM size flag in the ROM header (0x0148).
    uint8_t rom_flag() const { return rom_flag_; }
    // The ROM size, in bytes.
    size_t rom_size() const { return rom_size_; }
    // The ROM mask.
    size_t rom_mask() const { return rom_mask_; }

    // The RAM size flag in the ROM header (0x0149).
    uint8_t ram_flag() const { return ram_flag_; }
    // The RAM size, in bytes.
    size_t ram_size() const { return ram_size_; }
    // The RAM mask.
    size_t ram_mask() const { return ram_mask_; }

    // The destination code flag in the ROM header (0x014a).
    uint8_t destination_code_flag() const { return destination_code_flag_; }
    // The destination code.
    DestinationCode destination_code() const { return destination_code_; }

    // The version flag in the ROM header (0x014c).
    uint8_t version_flag() const { return version_flag_; }

    // The advertised header checksum in the ROM header (0x014d).
    uint8_t header_checksum() const { return header_checksum_; }
    // The actual header checksum.
    uint8_t actual_header_checksum() const { return actual_header_checksum_; }

    // The advertised global checksum in the ROM header (0x014e-0x014f).
    uint16_t global_checksum() const { return global_checksum_; }
    // The actual global checksum. This is never checked on real hardware.
    uint16_t actual_global_checksum() const { return actual_global_checksum_; }

private:
    Validity validity_ = Validity::kUninitialized;

    RomHeaderType header_type_ = RomHeaderType::kInvalid;

    uint8_t old_licensee_code_ = 0;
    std::string title_ = "";
    std::string maker_code_ = "";
    std::string manufacturer_code_ = "";

    uint8_t mapper_flag_ = 0;
    MapperType mapper_type_ = MapperType::kUnknown;

    bool has_battery_ = false;
    bool has_rtc_ = false;
    bool has_rumble_ = false;
    bool has_sensor_ = false;

    uint8_t cgb_flag_ = 0;
    CGBSupport cgb_support_ = CGBSupport::kNone;

    uint8_t sgb_flag_ = 0;
    bool sgb_support_ = false;

    uint8_t rom_flag_ = 0;
    size_t rom_size_ = 0;
    size_t rom_mask_ = 0;

    uint8_t ram_flag_ = 0;
    size_t ram_size_ = 0;
    size_t ram_mask_ = 0;

    uint8_t destination_code_flag_ = 0;
    DestinationCode destination_code_ = DestinationCode::kUnknown;

    uint8_t version_flag_ = 0;

    uint8_t header_checksum_ = 0;
    uint8_t actual_header_checksum_ = 0;

    uint16_t global_checksum_ = 0;
    uint16_t actual_global_checksum_ = 0;
};

#endif  // VBAM_CORE_GB_GBCARTDATA_H_
