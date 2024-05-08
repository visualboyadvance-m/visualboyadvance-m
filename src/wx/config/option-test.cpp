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

#if !defined(NDEBUG)
    EXPECT_DEATH(option->SetDouble(2.0), ".*");
    EXPECT_DEATH(option->SetInt(2), ".*");
    EXPECT_DEATH(option->SetUnsigned(2), ".*");
    EXPECT_DEATH(option->SetString("foo"), ".*");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), ".*");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), ".*");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), ".*");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), ".*");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), ".*");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), ".*");
#endif  // !defined(NDEBUG)
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

#if !defined(NDEBUG)
    EXPECT_DEATH(option->SetBool(true), ".*");
    EXPECT_DEATH(option->SetInt(2), ".*");
    EXPECT_DEATH(option->SetUnsigned(2), ".*");
    EXPECT_DEATH(option->SetString("foo"), ".*");
    EXPECT_DEATH(option->SetFilter(config::Filter::kNone), ".*");
    EXPECT_DEATH(option->SetInterframe(config::Interframe::kNone), ".*");
    EXPECT_DEATH(option->SetRenderMethod(config::RenderMethod::kSimple), ".*");
    EXPECT_DEATH(option->SetAudioApi(config::AudioApi::kOpenAL), ".*");
    EXPECT_DEATH(option->SetAudioRate(config::AudioRate::k11kHz), ".*");
    EXPECT_DEATH(option->SetGbPalette({0, 1, 2, 3, 4, 5, 6, 7}), ".*");
#endif  // !defined(NDEBUG)
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
