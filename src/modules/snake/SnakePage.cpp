// Snake game page: a native Canvas board driven by a DispatcherTimer, with arrow
// / WASD steering. All game rules live in SnakeLogic (superwin_core, unit-tested);
// this file only renders the state and forwards input + ticks.
#include <Windows.h>

#include <chrono>
#include <memory>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include "app/Ui.h"
#include "modules/snake/SnakeLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Shapes;
}  // namespace winrt

namespace superwin {
namespace {

constexpr int kCols = 24;
constexpr int kRows = 16;
constexpr double kCell = 24.0;

class SnakePage : public IModulePage {
public:
    SnakePage() : game_(kCols, kRows) { Build(); }
    winrt::UIElement Root() override { return root_; }
    void OnShown() override {
        Restart();
        if (host_) host_.Focus(winrt::FocusState::Programmatic);
    }
    void OnHidden() override { if (timer_) timer_.Stop(); }

private:
    winrt::SolidColorBrush Rgb(uint8_t r, uint8_t g, uint8_t b) {
        return winrt::SolidColorBrush{winrt::Windows::UI::Color{255, r, g, b}};
    }

    void Build() {
        canvas_ = winrt::Canvas();
        canvas_.Width(kCols * kCell);
        canvas_.Height(kRows * kCell);

        const bool dark = true;  // refined again on Render via theme
        boardBorder_ = winrt::Border();
        boardBorder_.CornerRadius(winrt::CornerRadius{12, 12, 12, 12});
        boardBorder_.Width(kCols * kCell);
        boardBorder_.Height(kRows * kCell);
        boardBorder_.Child(canvas_);
        (void)dark;

        overlay_ = ui::VStack(10);
        overlay_.HorizontalAlignment(winrt::HorizontalAlignment::Center);
        overlay_.VerticalAlignment(winrt::VerticalAlignment::Center);
        overlay_.Visibility(winrt::Visibility::Collapsed);
        overlayText_ = ui::Text(L"Game Over", 28, true);
        overlayText_.Foreground(Rgb(255, 255, 255));
        overlayText_.HorizontalAlignment(winrt::HorizontalAlignment::Center);
        auto restart = winrt::Button();
        restart.Content(winrt::box_value(winrt::hstring(L"Play again")));
        if (auto st = winrt::Application::Current().Resources()
                          .TryLookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle"))).try_as<winrt::Style>())
            restart.Style(st);
        restart.HorizontalAlignment(winrt::HorizontalAlignment::Center);
        restart.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Restart(); });
        overlay_.Children().Append(overlayText_);
        overlay_.Children().Append(restart);

        auto boardStack = winrt::Grid();
        boardStack.Children().Append(boardBorder_);
        boardStack.Children().Append(overlay_);
        boardStack.HorizontalAlignment(winrt::HorizontalAlignment::Left);

        // A focusable host so arrow / WASD keys reach the game.
        host_ = winrt::ContentControl();
        host_.IsTabStop(true);
        host_.UseSystemFocusVisuals(false);
        host_.Content(boardStack);
        host_.KeyDown([this](winrt::IInspectable const&, winrt::KeyRoutedEventArgs const& e) { OnKey(e); });

        score_ = ui::Text(L"Score: 0", 16, true);

        auto col = ui::VStack(12);
        col.Children().Append(score_);
        col.Children().Append(host_);
        col.Children().Append(ui::Caption(L"Steer with the arrow keys or WASD. Eat the food to grow."));

        timer_ = winrt::DispatcherTimer();
        timer_.Interval(std::chrono::milliseconds(110));
        timer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { Step(); });

        root_ = ui::Page(L"Snake", ui::Card(col));
    }

    void Restart() {
        game_.Reset();
        overlay_.Visibility(winrt::Visibility::Collapsed);
        score_.Text(L"Score: 0");
        Render();
        if (timer_) timer_.Start();
        if (host_) host_.Focus(winrt::FocusState::Programmatic);
    }

    void Step() {
        if (!game_.Tick()) {
            timer_.Stop();
            overlayText_.Text(L"Game Over  \x2022  Score " + winrt::to_hstring(game_.score()));
            overlay_.Visibility(winrt::Visibility::Visible);
            return;
        }
        score_.Text(L"Score: " + winrt::to_hstring(game_.score()));
        Render();
    }

    void OnKey(winrt::KeyRoutedEventArgs const& e) {
        using winrt::Windows::System::VirtualKey;
        const auto k = e.Key();
        bool handled = true;
        if (k == VirtualKey::Up || k == VirtualKey::W) game_.SetDirection(Dir::Up);
        else if (k == VirtualKey::Down || k == VirtualKey::S) game_.SetDirection(Dir::Down);
        else if (k == VirtualKey::Left || k == VirtualKey::A) game_.SetDirection(Dir::Left);
        else if (k == VirtualKey::Right || k == VirtualKey::D) game_.SetDirection(Dir::Right);
        else if ((k == VirtualKey::Space || k == VirtualKey::Enter) && game_.gameOver()) Restart();
        else handled = false;
        if (handled) e.Handled(true);
    }

    void Render() {
        if (!canvas_) return;
        const bool dark = canvas_.ActualTheme() == winrt::ElementTheme::Dark;
        boardBorder_.Background(dark ? Rgb(18, 20, 26) : Rgb(244, 245, 248));
        canvas_.Children().Clear();

        auto place = [&](int gx, int gy, winrt::Shape s, double inset) {
            winrt::Canvas::SetLeft(s, gx * kCell + inset);
            winrt::Canvas::SetTop(s, gy * kCell + inset);
            canvas_.Children().Append(s);
        };

        // Food: a round red dot.
        {
            winrt::Ellipse food;
            food.Width(kCell - 8);
            food.Height(kCell - 8);
            food.Fill(Rgb(0xff, 0x45, 0x3a));
            place(game_.food().x, game_.food().y, food, 4);
        }

        // Snake: rounded squares, head brighter.
        const auto& body = game_.body();
        for (size_t i = 0; i < body.size(); ++i) {
            winrt::Rectangle seg;
            seg.Width(kCell - 4);
            seg.Height(kCell - 4);
            seg.RadiusX(6);
            seg.RadiusY(6);
            seg.Fill(i == 0 ? Rgb(0x4f, 0x8e, 0xf7) : Rgb(0x34, 0xc7, 0x59));
            place(body[i].x, body[i].y, seg, 2);
        }
    }

    SnakeGame game_;
    winrt::UIElement root_{nullptr};
    winrt::ContentControl host_{nullptr};
    winrt::Border boardBorder_{nullptr};
    winrt::Canvas canvas_{nullptr};
    winrt::StackPanel overlay_{nullptr};
    winrt::TextBlock overlayText_{nullptr};
    winrt::TextBlock score_{nullptr};
    winrt::DispatcherTimer timer_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeSnakePage() {
    return std::make_unique<SnakePage>();
}

}  // namespace superwin
