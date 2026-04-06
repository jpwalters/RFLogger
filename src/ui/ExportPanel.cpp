#include "ExportPanel.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include "app/SettingsManager.h"

ExportPanel::ExportPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* form = new QFormLayout;

    m_formatCombo = new QComboBox;
    m_formatCombo->addItem(tr("Shure WWB CSV"), "wwb");
    m_formatCombo->addItem(tr("Generic CSV"), "csv");
    form->addRow(tr("Format:"), m_formatCombo);

    m_sourceCombo = new QComboBox;
    m_sourceCombo->addItem(tr("Max Hold"), "maxhold");
    m_sourceCombo->addItem(tr("Average"), "average");
    m_sourceCombo->addItem(tr("Last Sweep"), "last");
    form->addRow(tr("Data:"), m_sourceCombo);

    layout->addLayout(form);

    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setMinimumHeight(36);
    m_exportBtn->setEnabled(false);
    m_exportBtn->setStyleSheet(
        "QPushButton { background-color: #1a5fb4; border: 1px solid #2a7fd4; font-size: 13px; font-weight: bold; color: white; }"
        "QPushButton:hover { background-color: #2a7fd4; }"
        "QPushButton:pressed { background-color: #144a8a; }"
        "QPushButton:disabled { background-color: #555; border: 1px solid #666; color: #999; }");
    layout->addWidget(m_exportBtn);

    layout->addStretch();

    connect(m_exportBtn, &QPushButton::clicked, this, &ExportPanel::onExport);
}

void ExportPanel::setExportEnabled(bool enabled)
{
    m_exportBtn->setEnabled(enabled);
}

void ExportPanel::onExport()
{
    QString folder = SettingsManager::value("export/lastFolder").toString();
    if (folder.isEmpty() || !QDir(folder).exists())
        folder = QDir::homePath();

    QString defaultName = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm") + ".csv";
    QString defaultPath = QDir(folder).filePath(defaultName);

    QString filter = tr("CSV Files (*.csv);;All Files (*)");
    QString path = QFileDialog::getSaveFileName(this, tr("Export Scan Data"), defaultPath, filter);
    if (!path.isEmpty()) {
        SettingsManager::setValue("export/lastFolder", QFileInfo(path).absolutePath());
        emit exportRequested(
            m_formatCombo->currentData().toString(),
            m_sourceCombo->currentData().toString(),
            path
        );
    }
}
