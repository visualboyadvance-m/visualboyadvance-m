#include "wx/config/option.h"

#include <gtest/gtest.h>

#include <wx/log.h>

#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"

// Basic tests for a boolean option.
TEST(OptionTest, Bool) {
    config::Option* option = config::Option::ByID(config::OptionID::kDispBilinear);
    ASSERT_TRUE(option);

    EXPECT_EQ(option->command(), "Bilinear");
    EXPECT_EQ(option->config_name(), "Display/Bilinear");
    EXPECT_EQ(option->id(), config::OptionID::kDispBilinear);
    EXPECT_EQ(option->type(), config::Option::Type::kBool);
    EXPECT_TRUE(option->is_bool());

    EXPECT_TRUE(option->SetBool(false));
    EXPECT_FALSE(option->GetBool());

    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Double) {
    config::Option* option = config::Option::ByID(config::OptionID::kDispScale);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Display/Scale");
    EXPECT_EQ(option->id(), config::OptionID::kDispScale);
    EXPECT_EQ(option->type(), config::Option::Type::kDouble);
    EXPECT_TRUE(option->is_double());

    EXPECT_TRUE(option->SetDouble(2.5));
    EXPECT_DOUBLE_EQ(option->GetDouble(), 2.5);

    // Need to disable logging to test for errors.
    const wxLogNull disable_logging;

    // Test out of bounds values.
    EXPECT_FALSE(option->SetDouble(-1.0));
    EXPECT_FALSE(option->SetDouble(7.0));
    EXPECT_DOUBLE_EQ(option->GetDouble(), 2.5);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Int) {
    config::Option* option = config::Option::ByID(config::OptionID::kSoundBuffers);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Sound/Buffers");
    EXPECT_EQ(option->id(), config::OptionID::kSoundBuffers);
    EXPECT_EQ(option->type(), config::Option::Type::kInt);
    EXPECT_TRUE(option->is_int());

    EXPECT_TRUE(option->SetInt(8));
    EXPECT_EQ(option->GetInt(), 8);

    // Need to disable logging to test for errors.
    const wxLogNull disable_logging;

    // Test out of bounds values.
    EXPECT_FALSE(option->SetInt(-1));
    EXPECT_FALSE(option->SetInt(42));
    EXPECT_EQ(option->GetInt(), 8);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Unsigned) {
    config::Option* option = config::Option::ByID(config::OptionID::kGeomWindowHeight);
    ASSERT_TRUE(option);

    EXPECT_EQ(option->config_name(), "geometry/windowHeight");
    EXPECT_EQ(option->id(), config::OptionID::kGeomWindowHeight);
    EXPECT_EQ(option->type(), config::Option::Type::kUnsigned);
    EXPECT_TRUE(option->is_unsigned());

    EXPECT_TRUE(option->SetUnsigned(100));
    EXPECT_EQ(option->GetUnsigned(), 100);

    // Need to disable logging to test for errors.
    const wxLogNull disable_logging;

    // Test out of bounds values.
    EXPECT_FALSE(option->SetUnsigned(100000));
    EXPECT_EQ(option->GetUnsigned(), 100);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, String) {
    config::Option* option = config::Option::ByID(config::OptionID::kGenStateDir);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "General/StateDir");
    EXPECT_EQ(option->id(), config::OptionID::kGenStateDir);
    EXPECT_EQ(option->type(), config::Option::Type::kString);
    EXPECT_TRUE(option->is_string());

    EXPECT_TRUE(option->SetString("/path/to/sthg"));
    EXPECT_EQ(option->GetString(), "/path/to/sthg");

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Filter) {
    config::Option* option = config::Option::ByID(config::OptionID::kDispFilter);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Display/Filter");
    EXPECT_EQ(option->id(), config::OptionID::kDispFilter);
    EXPECT_EQ(option->type(), config::Option::Type::kFilter);
    EXPECT_TRUE(option->is_filter());

    EXPECT_TRUE(option->SetFilter(config::Filter::kNone));
    EXPECT_EQ(option->GetFilter(), config::Filter::kNone);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Interframe) {
    config::Option* option = config::Option::ByID(config::OptionID::kDispIFB);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Display/IFB");
    EXPECT_EQ(option->id(), config::OptionID::kDispIFB);
    EXPECT_EQ(option->type(), config::Option::Type::kInterframe);
    EXPECT_TRUE(option->is_interframe());

    EXPECT_TRUE(option->SetInterframe(config::Interframe::kNone));
    EXPECT_EQ(option->GetInterframe(), config::Interframe::kNone);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, AudioApi) {
    config::Option* option = config::Option::ByID(config::OptionID::kSoundAudioAPI);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Sound/AudioAPI");
    EXPECT_EQ(option->id(), config::OptionID::kSoundAudioAPI);
    EXPECT_EQ(option->type(), config::Option::Type::kAudioApi);
    EXPECT_TRUE(option->is_audio_api());

    EXPECT_TRUE(option->SetAudioApi(config::AudioApi::kSDL));
    EXPECT_EQ(option->GetAudioApi(), config::AudioApi::kSDL);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, AudioRate) {
    config::Option* option = config::Option::ByID(config::OptionID::kSoundAudioRate);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Sound/Quality");
    EXPECT_EQ(option->id(), config::OptionID::kSoundAudioRate);
    EXPECT_EQ(option->type(), config::Option::Type::kAudioRate);
    EXPECT_TRUE(option->is_audio_rate());

    EXPECT_TRUE(option->SetAudioRate(config::AudioRate::k11kHz));
    EXPECT_EQ(option->GetAudioRate(), config::AudioRate::k11kHz);

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), "is_gb_palette\\(\\)");
}

TEST(OptionTest, Enum) {
    config::Option* option = config::Option::ByID(config::OptionID::kDispRenderMethod);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "Display/RenderMethod");
    EXPECT_EQ(option->id(), config::OptionID::kDispRenderMethod);
    EXPECT_EQ(option->type(), config::Option::Type::kRenderMethod);
    EXPECT_TRUE(option->is_render_method());

    EXPECT_TRUE(option->SetRenderMethod(config::RenderMethod::kSimple));
    EXPECT_EQ(option->GetRenderMethod(), config::RenderMethod::kSimple);

    EXPECT_TRUE(option->SetRenderMethod(config::RenderMethod::kSDL));
    EXPECT_EQ(option->GetRenderMethod(), config::RenderMethod::kSDL);

    EXPECT_TRUE(option->SetEnumString("opengl"));
    EXPECT_EQ(option->GetRenderMethod(), config::RenderMethod::kOpenGL);
}

TEST(Optiontest, GbPalette) {
    config::Option* option = config::Option::ByID(config::OptionID::kGBPalette0);
    ASSERT_TRUE(option);

    EXPECT_TRUE(option->command().empty());
    EXPECT_EQ(option->config_name(), "GB/Palette0");
    EXPECT_EQ(option->id(), config::OptionID::kGBPalette0);
    EXPECT_EQ(option->type(), config::Option::Type::kGbPalette);
    EXPECT_TRUE(option->is_gb_palette());

    const std::array<uint16_t, 8> palette = {0, 1, 2, 3, 4, 5, 6, 7};
    EXPECT_TRUE(option->SetGbPalette(palette));
    EXPECT_EQ(option->GetGbPalette(), palette);

    EXPECT_EQ(option->GetGbPaletteString(), "0000,0001,0002,0003,0004,0005,0006,0007");

    // Need to disable logging to test for errors.
    const wxLogNull disable_logging;

    // Incorrect configuration string tests
    EXPECT_FALSE(option->SetGbPaletteString(""));
    EXPECT_FALSE(option->SetGbPaletteString("0000,0001,0002,0003,0004,0005,0006,000Q"));

    EXPECT_DEATH(option->SetBool(true), "is_bool\\(\\)");
    EXPECT_DEATH(option->SetDouble(2.0), "is_double\\(\\)");
    EXPECT_DEATH(option->SetInt(2), "is_int\\(\\)");
    EXPECT_DEATH(option->SetUnsigned(2), "is_unsigned\\(\\)");
    EXPECT_DEATH(option->SetString("foo"), "is_string\\(\\)");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), "is_filter\\(\\)");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), "is_interframe\\(\\)");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), "is_render_method\\(\\)");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), "is_audio_api\\(\\)");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), "is_audio_rate\\(\\)");
}

TEST(OptionObserverTest, Basic) {
    class TestOptionsObserver : public config::Option::Observer {
    public:
        TestOptionsObserver(config::OptionID id) : config::Option::Observer(id), changed_(false) {}
        ~TestOptionsObserver() override = default;

        void OnValueChanged() override { changed_ = true; }

        bool changed() const { return changed_; }
        void ResetChanged() { changed_ = false; }

    private:
        bool changed_;
    };

    config::Option* option = config::Option::ByID(config::OptionID::kDispKeepOnTop);
    ASSERT_TRUE(option);
    const bool initial_value = option->GetBool();

    TestOptionsObserver observer(config::OptionID::kDispKeepOnTop);

    // Test that the observer is not notified when the value does not change.
    EXPECT_TRUE(option->SetBool(initial_value));
    EXPECT_FALSE(observer.changed());
    observer.ResetChanged();

    // Test that the observer is notified when the value changes.
    EXPECT_TRUE(option->SetBool(!initial_value));
    EXPECT_TRUE(observer.changed());
    observer.ResetChanged();

    EXPECT_TRUE(option->SetBool(initial_value));
    EXPECT_TRUE(observer.changed());
}

// Tests that the option proxy correctly matches the types of the options.
TEST(OptionProxyTest, MatchingTypes) {
    for (size_t i = 0; i < config::kNbOptions; i++) {
        const config::OptionID id = static_cast<config::OptionID>(i);
        const config::Option::Type proxy_type = config::kOptionsTypes[i];
        const config::Option* option = config::Option::ByID(id);

        ASSERT_TRUE(option);
        EXPECT_EQ(option->type(), proxy_type);
    }
}

TEST(OptionProxyTest, NumericOperators) {
    wxLogNull disable_logging;

    int32_t int_opt = OPTION(kDispMaxThreads);

    int_opt++;
    OPTION(kDispMaxThreads)++;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    int_opt--;
    OPTION(kDispMaxThreads)--;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    ++int_opt;
    OPTION(kDispMaxThreads)++;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    --int_opt;
    OPTION(kDispMaxThreads)--;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    int_opt += 2;
    OPTION(kDispMaxThreads) += 2;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    int_opt -= 2;
    OPTION(kDispMaxThreads) -= 2;
    EXPECT_EQ(int_opt, OPTION(kDispMaxThreads));

    OPTION(kDispMaxThreads) = OPTION(kDispMaxThreads).Max();
    OPTION(kDispMaxThreads)++;
    EXPECT_EQ(OPTION(kDispMaxThreads), OPTION(kDispMaxThreads).Max());

    OPTION(kDispMaxThreads) = OPTION(kDispMaxThreads).Min();
    OPTION(kDispMaxThreads)--;
    EXPECT_EQ(OPTION(kDispMaxThreads), OPTION(kDispMaxThreads).Min());

    uint32_t unsigned_opt = OPTION(kJoyDefault);

    unsigned_opt++;
    OPTION(kJoyDefault)++;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    unsigned_opt--;
    OPTION(kJoyDefault)--;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    ++unsigned_opt;
    OPTION(kJoyDefault)++;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    --unsigned_opt;
    OPTION(kJoyDefault)--;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    unsigned_opt += 2;
    OPTION(kJoyDefault) += 2;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    unsigned_opt -= 2;
    OPTION(kJoyDefault) -= 2;
    EXPECT_EQ(unsigned_opt, OPTION(kJoyDefault));

    OPTION(kJoyDefault) = OPTION(kJoyDefault).Max();
    OPTION(kJoyDefault)++;
    EXPECT_EQ(OPTION(kJoyDefault), OPTION(kJoyDefault).Max());

    OPTION(kJoyDefault) = OPTION(kJoyDefault).Min();
    OPTION(kJoyDefault)--;
    EXPECT_EQ(OPTION(kJoyDefault), OPTION(kJoyDefault).Min());

    double double_opt = OPTION(kDispScale);

    double_opt++;
    OPTION(kDispScale)++;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    double_opt--;
    OPTION(kDispScale)--;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    ++double_opt;
    OPTION(kDispScale)++;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    --double_opt;
    OPTION(kDispScale)--;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    double_opt += 2;
    OPTION(kDispScale) += 2;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    double_opt -= 2;
    OPTION(kDispScale) -= 2;
    EXPECT_EQ(double_opt, OPTION(kDispScale));

    OPTION(kDispScale) = OPTION(kDispScale).Max();
    OPTION(kDispScale)++;
    EXPECT_EQ(OPTION(kDispScale), OPTION(kDispScale).Max());

    OPTION(kDispScale) = OPTION(kDispScale).Min();
    OPTION(kDispScale)--;
    EXPECT_EQ(OPTION(kDispScale), OPTION(kDispScale).Min());
}
