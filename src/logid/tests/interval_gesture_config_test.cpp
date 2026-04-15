#include <Configuration.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace fs = std::filesystem;

namespace {
    struct TempFile {
        fs::path path;

        explicit TempFile(const std::string& name) :
                path(fs::temp_directory_path() / name) {
        }

        ~TempFile() {
            std::error_code ec;
            fs::remove(path, ec);
        }
    };

    void expect(bool condition, const std::string& message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeTextFile(const fs::path& path, const std::string& content) {
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("failed to open " + path.string());
        out << content;
        if (!out.good())
            throw std::runtime_error("failed to write " + path.string());
    }

    std::string readTextFile(const fs::path& path) {
        std::ifstream in(path);
        if (!in)
            throw std::runtime_error("failed to open " + path.string());
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    logid::config::Profile& getProfile(logid::Configuration& config) {
        expect(config.devices.has_value(), "missing devices map");
        auto& devices = config.devices.value();
        auto it = devices.find("Test Device");
        expect(it != devices.end(), "missing Test Device");
        expect(std::holds_alternative<logid::config::Profile>(it->second),
               "expected profile device config");
        return std::get<logid::config::Profile>(it->second);
    }

    logid::config::Gesture& getGesture(logid::config::Profile& profile,
                                       uint16_t cid,
                                       const std::string& direction) {
        expect(profile.buttons.has_value(), "missing buttons");
        auto& buttons = profile.buttons.value();
        auto button_it = buttons.find(cid);
        expect(button_it != buttons.end(), "missing button");
        auto& button = button_it->second;
        expect(button.action.has_value(), "missing button action");
        expect(std::holds_alternative<logid::config::GestureAction>(button.action.value()),
               "button action is not a gesture action");
        auto& gesture_action = std::get<logid::config::GestureAction>(button.action.value());
        expect(gesture_action.gestures.has_value(), "missing gestures map");
        auto& gestures = gesture_action.gestures.value();
        auto gesture_it = gestures.find(direction);
        expect(gesture_it != gestures.end(), "missing gesture direction");
        return gesture_it->second;
    }

    logid::config::IntervalGesture& getIntervalGesture(logid::config::Gesture& gesture) {
        expect(std::holds_alternative<logid::config::IntervalGesture>(gesture)
               || std::holds_alternative<logid::config::FewPixelsGesture>(gesture),
               "gesture is not interval-based");
        if (std::holds_alternative<logid::config::FewPixelsGesture>(gesture))
            return std::get<logid::config::FewPixelsGesture>(gesture);
        return std::get<logid::config::IntervalGesture>(gesture);
    }

    void expectDelayValue(const std::optional<int>& delay,
                          const std::optional<int>& expected,
                          const std::string& label) {
        expect(delay == expected, label + " delay mismatch");
    }

    std::string makeConfig(const std::string& left_delay_line) {
        return R"(devices: (
{
    name: "Test Device";
    buttons: (
        {
            cid: 0xc3;
            action =
            {
                type: "Gestures";
                gestures: (
                    {
                        direction: "Left";
                        mode: "OnInterval";
                        interval: 30;
)" + left_delay_line + R"(
                        action =
                        {
                            type: "Keypress";
                            keys: ["KEY_LEFTCTRL", "KEY_PAGEUP"];
                        };
                    },
                    {
                        direction: "Right";
                        mode: "OnFewPixels";
                        interval: 10;
                        delay: 75;
                        action =
                        {
                            type: "Keypress";
                            keys: ["KEY_LEFTCTRL", "KEY_PAGEDOWN"];
                        };
                    }
                );
            };
        }
    );
}
);
)";
    }

    void runRoundTripCase(const std::string& suffix,
                          const std::string& left_delay_line,
                          const std::optional<int>& expected_left_delay,
                          const std::optional<int>& expected_right_delay) {
        TempFile temp_file("logid_delay_test_" + suffix + ".cfg");
        writeTextFile(temp_file.path, makeConfig(left_delay_line));

        logid::Configuration config(temp_file.path.string());
        auto& profile = getProfile(config);
        auto& left_gesture = getIntervalGesture(getGesture(profile, 0xc3, "Left"));
        auto& right_gesture = getIntervalGesture(getGesture(profile, 0xc3, "Right"));

        expectDelayValue(left_gesture.delay, expected_left_delay, "left parsed");
        expectDelayValue(right_gesture.delay, expected_right_delay, "right parsed");

        config.save();

        logid::Configuration reloaded_config(temp_file.path.string());
        auto& reloaded_profile = getProfile(reloaded_config);
        auto& reloaded_left = getIntervalGesture(getGesture(reloaded_profile, 0xc3, "Left"));
        auto& reloaded_right = getIntervalGesture(getGesture(reloaded_profile, 0xc3, "Right"));

        expectDelayValue(reloaded_left.delay, expected_left_delay, "left reloaded");
        expectDelayValue(reloaded_right.delay, expected_right_delay, "right reloaded");

        const auto saved = readTextFile(temp_file.path);
        if (expected_left_delay.has_value())
            expect(saved.find("delay = " + std::to_string(expected_left_delay.value()))
                   != std::string::npos, "saved config missing left delay");
        else
            expect(saved.find("delay = 75") != std::string::npos,
                   "saved config missing right delay");
    }
}

int main() {
    try {
        runRoundTripCase("omitted", "", std::nullopt, 75);
        runRoundTripCase("zero", "                        delay: 0;", 0, 75);
        runRoundTripCase("negative", "                        delay: -1;", -1, 75);
        runRoundTripCase("positive", "                        delay: 150;", 150, 75);
    } catch (const std::exception& e) {
        std::cerr << "logid_config_test failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
