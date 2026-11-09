// BrassCue - GPL-3.0. Never redirect live output without confirmation; stop clears old routes.
#include "profile_commit.hpp"
namespace {
bool Same(const std::vector<brasscue::Assignment> &a, const std::vector<brasscue::Assignment> &b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].address.universe != b[i].address.universe ||
            a[i].address.channel != b[i].address.channel || a[i].value != b[i].value)
            return false;
    return true;
}
bool SameRouting(const brasscue::ControlProfile &a, const brasscue::ControlProfile &b) {
    const auto left = brasscue::effective_encoder_patch(a),
               right = brasscue::effective_encoder_patch(b);
    for (unsigned i = 0; i < 4; ++i)
        if (!Same(left[i], right[i]))
            return false;
    if (a.fader.grandmaster != b.fader.grandmaster ||
        !Same(a.fader.assignments, b.fader.assignments))
        return false;
    for (unsigned i = 0; i < 3; ++i)
        if (a.buttons[i].action != b.buttons[i].action ||
            !Same(a.buttons[i].assignments, b.buttons[i].assignments))
            return false;
    return true;
}
} // namespace
bool CommitProfileEdit(wxWindow *parent, brasscue::ControllerSession &session,
                       brasscue::OutputSession &output, brasscue::ControlProfile candidate) {
    brasscue::validate(candidate);
    if (output.enabled() && !SameRouting(session.profile(), candidate)) {
        if (wxMessageBox(
                "Art-Net is ON. Stop output and apply the new patch?\n"
                "Old routes will receive a best-effort zero frame. Art-Net will remain OFF; "
                "re-enable it explicitly when ready.",
                "Confirm live repatch", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, parent) != wxYES)
            return false;
        output.stop();
    }
    session.configure(std::move(candidate));
    return true;
}
