#ifndef VBAM_CORE_RUST_H_
#define VBAM_CORE_RUST_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
namespace core {
#endif // __cplusplus

typedef enum CheatsDecodeError {
  UnsupportedType,
  InvalidLength,
  InvalidCharacter,
  AddressNotHex,
  ValueNotHex,
} CheatsDecodeError;

typedef enum GbaCheatsType {
  Generic,
  GameSharkV2,
  CodeBreaker,
  ActionReplay,
} GbaCheatsType;

typedef struct CheatsData {
  int code;
  int size;
  int status;
  bool enabled;
  uint32_t rawaddress;
  uint32_t address;
  uint32_t value;
  uint32_t old_value;
  char codestring[20];
  char desc[32];
} CheatsData;

typedef enum Result_CheatsData__CheatsDecodeError_Tag {
  Ok_CheatsData__CheatsDecodeError,
  Err_CheatsData__CheatsDecodeError,
} Result_CheatsData__CheatsDecodeError_Tag;

typedef struct Result_CheatsData__CheatsDecodeError {
  Result_CheatsData__CheatsDecodeError_Tag tag;
  union {
    struct {
      struct CheatsData ok;
    };
    struct {
      enum CheatsDecodeError err;
    };
  };
} Result_CheatsData__CheatsDecodeError;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum GbaCheatsType some_type(void);

struct Result_CheatsData__CheatsDecodeError decode_code(enum GbaCheatsType cheat_type,
                                                        const char *code_string,
                                                        const char *description);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#ifdef __cplusplus
} // namespace core
#endif // __cplusplus

#endif /* VBAM_CORE_RUST_H_ */
