#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include <array>
#include <memory>

namespace f64 {

class Forge64Processor;

// Responsive 4x4 grid showing the 16 pads of the selected bank (A/B/C/D).
// Click zooms into the pad dashboard; ALT-drag copies a pad onto another;
// dropping audio files loads them as the pad's sample.
class PadGridView : public juce::Component
{
public:
    struct Host
    {
        virtual ~Host() = default;
        virtual void padClicked(int globalPad) = 0;
    };

    PadGridView(Forge64Processor& p, Host& h);
    ~PadGridView() override;

    void setBank(int b);
    int  getBank() const { return bank; }
    void setSelected(int globalPad);
    void resized() override;
    void tick();          // hit-flash animation, driven by the editor timer
    void refreshPads();   // full repaint after state changes
    void chooseSampleFor(int pad);

    Forge64Processor& proc() { return processor; }

private:
    class PadCell;
    Forge64Processor& processor;
    Host& host;
    int bank = 0;
    int selectedPad = -1;
    std::array<std::unique_ptr<PadCell>, kPadsPerBank> cells;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace f64
