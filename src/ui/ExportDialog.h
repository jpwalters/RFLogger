#pragma once

#include "data/ScanSession.h"

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(ScanSession* session, QWidget* parent = nullptr);

    QString filePath() const;
    QString format() const;
    QString dataSource() const;

private slots:
    void onBrowse();
    void onExport();
    void updatePreview();

private:
    ScanSession* m_session;

    QComboBox* m_formatCombo;
    QComboBox* m_sourceCombo;
    QLineEdit* m_pathEdit;
    QPushButton* m_browseBtn;
    QPushButton* m_exportBtn;
    QPlainTextEdit* m_preview;
};
