#include "ExportDialog.h"
#include "export/WwbExporter.h"
#include "export/GenericCsvExporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>

ExportDialog::ExportDialog(ScanSession* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
{
    setWindowTitle(tr("Export Scan Data"));
    setMinimumSize(500, 400);

    auto* layout = new QVBoxLayout(this);

    // Settings
    auto* settingsGroup = new QGroupBox(tr("Export Settings"));
    auto* formLayout = new QFormLayout(settingsGroup);

    m_formatCombo = new QComboBox;
    m_formatCombo->addItem(tr("Shure WWB CSV"), "wwb");
    m_formatCombo->addItem(tr("Generic CSV"), "csv");
    formLayout->addRow(tr("Format:"), m_formatCombo);

    m_sourceCombo = new QComboBox;
    m_sourceCombo->addItem(tr("Max Hold"), "maxhold");
    m_sourceCombo->addItem(tr("Average"), "average");
    m_sourceCombo->addItem(tr("Last Sweep"), "last");
    formLayout->addRow(tr("Data:"), m_sourceCombo);

    auto* pathLayout = new QHBoxLayout;
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText(tr("Select output file..."));
    m_browseBtn = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseBtn);
    formLayout->addRow(tr("File:"), pathLayout);

    layout->addWidget(settingsGroup);

    // Preview
    auto* previewGroup = new QGroupBox(tr("Preview (first 10 lines)"));
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_preview = new QPlainTextEdit;
    m_preview->setReadOnly(true);
    m_preview->setMaximumHeight(160);
    m_preview->setFont(QFont("Courier New", 9));
    previewLayout->addWidget(m_preview);
    layout->addWidget(previewGroup);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setDefault(true);
    auto* cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // Connections
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowse);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::onExport);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::updatePreview);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::updatePreview);

    updatePreview();
}

QString ExportDialog::filePath() const { return m_pathEdit->text(); }
QString ExportDialog::format() const { return m_formatCombo->currentData().toString(); }
QString ExportDialog::dataSource() const { return m_sourceCombo->currentData().toString(); }

void ExportDialog::onBrowse()
{
    QString filter;
    if (format() == "wwb")
        filter = tr("CSV Files (*.csv);;All Files (*)");
    else
        filter = tr("CSV Files (*.csv);;All Files (*)");

    QString path = QFileDialog::getSaveFileName(this, tr("Export Scan Data"), {}, filter);
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

void ExportDialog::onExport()
{
    if (m_pathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("Please select an output file."));
        return;
    }

    if (m_session->isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("No scan data to export."));
        return;
    }

    SweepData data;
    QString source = dataSource();
    if (source == "maxhold")
        data = m_session->maxHold();
    else if (source == "average")
        data = m_session->average();
    else
        data = m_session->latestSweep();

    bool success = false;
    if (format() == "wwb") {
        success = WwbExporter::exportToFile(m_pathEdit->text(), data);
    } else {
        success = GenericCsvExporter::exportToFile(m_pathEdit->text(), data, m_session->deviceName());
    }

    if (success) {
        QMessageBox::information(this, tr("Export"), tr("Scan data exported successfully."));
        accept();
    } else {
        QMessageBox::critical(this, tr("Export"), tr("Failed to write file."));
    }
}

void ExportDialog::updatePreview()
{
    if (m_session->isEmpty()) {
        m_preview->setPlainText(tr("(no scan data)"));
        return;
    }

    SweepData data;
    QString source = dataSource();
    if (source == "maxhold")
        data = m_session->maxHold();
    else if (source == "average")
        data = m_session->average();
    else
        data = m_session->latestSweep();

    if (format() == "wwb") {
        QString full = WwbExporter::formatSweep(data);
        QStringList lines = full.split('\n', Qt::SkipEmptyParts);
        QStringList preview = lines.mid(0, 10);
        if (lines.size() > 10)
            preview << QString("... (%1 more lines)").arg(lines.size() - 10);
        m_preview->setPlainText(preview.join('\n'));
    } else {
        m_preview->setPlainText(tr("(preview not available for this format)"));
    }
}
