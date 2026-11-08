// BrassCue - GPL-3.0. A changed route always requires explicit reactivation.
#include "network_panel.hpp"
#include "dark_theme.hpp"
#include <algorithm>
NetworkPanel::NetworkPanel(wxWindow *parent, std::function<void()> changed,
                           std::function<void(bool)> requested)
    : wxPanel(parent), timer_(this), changed_(std::move(changed)),
      requested_(std::move(requested)) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *top = new wxBoxSizer(wxHORIZONTAL);
    top->Add(new wxStaticText(this, wxID_ANY, "Network interface"), 0,
             wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    interfaces_ = new wxChoice(this, wxID_ANY);
    top->Add(interfaces_, 1, wxEXPAND | wxRIGHT, 8);
    auto *refresh = new wxButton(this, wxID_REFRESH, "Refresh");
    top->Add(refresh);
    root->Add(top, 0, wxEXPAND | wxALL, 6);
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    const wxString modes[]{"Unicast", "Broadcast"};
    mode_ = new wxRadioBox(this, wxID_ANY, "Destination", wxDefaultPosition, wxDefaultSize, 2,
                           modes, 2, wxRA_SPECIFY_COLS);
    row->Add(mode_, 0, wxRIGHT, 8);
    auto *addresses = new wxBoxSizer(wxVERTICAL);
    addresses->Add(new wxStaticText(this, wxID_ANY, "Manual unicast IPv4"));
    ip_ = new wxTextCtrl(this, wxID_ANY, "127.0.0.1");
    addresses->Add(ip_, 0, wxEXPAND);
    addresses->Add(new wxStaticText(this, wxID_ANY, "Effective destination"));
    target_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    addresses->Add(target_, 0, wxEXPAND);
    row->Add(addresses, 1, wxRIGHT, 8);
    enable_ = new wxCheckBox(this, wxID_ANY, "Enable Art-Net");
    row->Add(enable_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(row, 0, wxEXPAND | wxALL, 6);
    hint_ = new wxStaticText(this, wxID_ANY, "");
    root->Add(hint_, 0, wxEXPAND | wxALL, 6);
    SetSizer(root);
    ApplyDarkTheme(*this);
    RefreshInterfaces(false);
    UpdateDestination();
    interfaces_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
        wanted_ = SelectedId();
        Changed();
    });
    mode_->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent &) { Changed(); });
    ip_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { Changed(); });
    enable_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) {
        if (requested_)
            requested_(enable_->GetValue());
    });
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { RefreshInterfaces(true); });
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { RefreshInterfaces(true); });
    timer_.Start(5000);
}
std::string NetworkPanel::SelectedId() const {
    auto i = interfaces_->GetSelection();
    return i >= 0 && std::size_t(i) < ids_.size() ? ids_[i] : std::string{};
}
void NetworkPanel::Load(const brasscue::ControlProfile &p) {
    wanted_ = p.network_interface_id;
    ip_->ChangeValue(wxString::FromUTF8(p.target_ip));
    mode_->SetSelection(p.allow_broadcast ? 1 : 0);
    enable_->SetValue(false);
    fingerprint_ = "\x01";
    RefreshInterfaces(false);
    UpdateDestination();
}
void NetworkPanel::RefreshInterfaces(bool notify) {
    try {
        auto adapters = brasscue::network_interfaces();
        std::string fingerprint;
        for (const auto &n : adapters)
            fingerprint += n.id + "|" + n.name + "|" + n.address + "|" + n.netmask + "|" +
                           std::to_string(n.index) + ";";
        if (fingerprint == fingerprint_)
            return;
        const bool changed = fingerprint_ != "\x01";
        fingerprint_ = fingerprint;
        adapters_ = std::move(adapters);
        interfaces_->Clear();
        ids_.clear();
        interfaces_->Append("Auto / default (unicast)");
        ids_.push_back({});
        int selected = 0;
        for (const auto &n : adapters_) {
            interfaces_->Append(wxString::FromUTF8(n.name + " — " + n.address + " / " + n.netmask +
                                                   (n.loopback ? " (loopback)" : "")));
            ids_.push_back(n.id);
            if (n.id == wanted_)
                selected = int(ids_.size() - 1);
        }
        if (!wanted_.empty() && selected == 0) {
            interfaces_->Append("Saved interface unavailable — choose a current route");
            ids_.push_back(wanted_);
            selected = int(ids_.size() - 1);
        }
        interfaces_->SetSelection(selected);
        UpdateDestination();
        if (notify && changed) {
            SetOutput(false);
            if (changed_)
                changed_();
        }
    } catch (const std::exception &e) {
        SetOutput(false);
        enable_->Disable();
        hint_->SetLabel(wxString::FromUTF8(e.what()));
        if (notify && changed_)
            changed_();
    }
}
void NetworkPanel::UpdateDestination() {
    bool can_broadcast = false;
    for (const auto &n : adapters_)
        if (n.id == SelectedId())
            try {
                (void)brasscue::resolve_network(ip_->GetValue().ToStdString(), true, n.id,
                                                adapters_);
                can_broadcast = true;
            } catch (const std::exception &) {
            }
    mode_->Enable(1, can_broadcast);
    ip_->Enable(mode_->GetSelection() == 0);
    try {
        auto route = brasscue::resolve_network(ip_->GetValue().ToStdString(),
                                               mode_->GetSelection() == 1, SelectedId(), adapters_);
        target_->ChangeValue(wxString::FromUTF8(route.target));
        enable_->Enable();
        auto message =
            route.source.empty()
                ? "Auto selects the system unicast route. Choose an interface to enable broadcast."
                : "Source " + route.source + ". Route changes require output to be enabled again.";
        if (hint_->GetLabel() != wxString::FromUTF8(message))
            hint_->SetLabel(wxString::FromUTF8(message));
    } catch (const std::exception &e) {
        target_->ChangeValue("Unavailable");
        enable_->SetValue(false);
        enable_->Disable();
        hint_->SetLabel(wxString::FromUTF8(e.what()));
    }
}
void NetworkPanel::Changed() {
    SetOutput(false);
    UpdateDestination();
    if (changed_)
        changed_();
}
void NetworkPanel::SetOutput(bool enabled) {
    if (enable_->GetValue() != enabled)
        enable_->SetValue(enabled);
}
void NetworkPanel::WritePreferences(brasscue::ControllerSession &session) const {
    session.network(ip_->GetValue().ToStdString(), mode_->GetSelection() == 1, SelectedId());
}
brasscue::ResolvedNetwork NetworkPanel::Resolve() const {
    // Re-enumerate on enable: a stale dropdown can never silently choose another NIC.
    return brasscue::resolve_network(ip_->GetValue().ToStdString(), mode_->GetSelection() == 1,
                                     SelectedId(), brasscue::network_interfaces());
}
