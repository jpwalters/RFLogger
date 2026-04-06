#include "AboutDialog.h"
#include "Version.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About RF Logger"));
    setFixedSize(400, 420);

    auto* layout = new QVBoxLayout(this);

    auto* logoLabel = new QLabel();
    QPixmap logoPixmap(":/icons/rflogger.png");
    logoLabel->setPixmap(logoPixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);

    auto* title = new QLabel(tr("<h2>RF Logger</h2>"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* version = new QLabel(tr("Version %1").arg(RFLOGGER_VERSION_STRING));
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    auto* desc = new QLabel(tr(
        "<p>Cross-platform RF spectrum analyzer for pro audio.</p>"
        "<p>Capture, visualize, and export RF spectrum data<br>"
        "for use with Shure Wireless Workbench.</p>"
        "<p>Supports RF Explorer, TinySA, and RTL-SDR devices.</p>"
    ));
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    auto* link = new QLabel(
        tr("<p><a href=\"https://github.com/jpwalters/RFLogger\" style=\"color: #2a82da;\">github.com/jpwalters/RFLogger</a></p>"));
    link->setAlignment(Qt::AlignCenter);
    link->setOpenExternalLinks(true);
    layout->addWidget(link);

    auto* license = new QLabel(tr("<p style=\"color: #888;\">MIT License &copy; 2026 James P. Walters</p>"));
    license->setAlignment(Qt::AlignCenter);
    layout->addWidget(license);

    layout->addStretch();

    auto* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
}
