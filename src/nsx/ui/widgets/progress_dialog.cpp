// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/widgets/progress_dialog.hpp"

#include <utility>

namespace nsx::ui {

using namespace brls::i18n::literals;

namespace {

brls::BoxLayout* createLayout(brls::Label*& titleLabel, brls::Label*& stageLabel,
                              brls::ProgressDisplay*& progressDisplay, brls::Label*& detailLabel,
                              const std::string& title)
{
    auto* layout = new brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL);
    layout->setSpacing(14);
    layout->setResize(true);

    titleLabel = new brls::Label(brls::LabelStyle::DIALOG, title, true);
    titleLabel->setHorizontalAlign(NVG_ALIGN_CENTER);
    layout->addView(titleLabel);

    stageLabel = new brls::Label(brls::LabelStyle::REGULAR, "", true);
    stageLabel->setHorizontalAlign(NVG_ALIGN_CENTER);
    layout->addView(stageLabel);

    progressDisplay = new brls::ProgressDisplay();
    progressDisplay->setHeight(36);
    layout->addView(progressDisplay);

    detailLabel = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    detailLabel->setHorizontalAlign(NVG_ALIGN_CENTER);
    layout->addView(detailLabel);

    return layout;
}

}  // namespace

ProgressDialog::ProgressDialog(const std::string& title, std::function<void()> onCancelCallback)
    : Dialog(createLayout(m_titleLabel, m_stageLabel, m_progressDisplay, m_detailLabel, title)),
      m_onCancelCallback(std::move(onCancelCallback))
{
    addButton("nsx/actions/cancel"_i18n, [this](brls::View*) { (void)this->onCancel(); });

    setCancelable(true);
}

void ProgressDialog::setStage(const std::string& stage)
{
    if (m_stageLabel && !m_cancelling) {
        m_stageLabel->setText(stage);
    }
}

void ProgressDialog::setProgress(int current, int total)
{
    if (m_progressDisplay && total > 0 && current >= 0) {
        m_progressDisplay->setProgress(current, total);
    }
}

void ProgressDialog::setDetail(const std::string& detail)
{
    if (m_detailLabel && !m_cancelling) {
        m_detailLabel->setText(detail);
    }
}

void ProgressDialog::setCancelling()
{
    m_cancelling = true;
    if (m_stageLabel) {
        m_stageLabel->setText("catalog/cancelling"_i18n);
    }
}

bool ProgressDialog::onCancel()
{
    if (m_cancelling) {
        return false;
    }
    setCancelling();
    if (m_onCancelCallback) {
        m_onCancelCallback();
    }
    return false;
}

}  // namespace nsx::ui
