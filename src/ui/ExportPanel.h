#pragma once

#include <QWidget>

class QComboBox;
class QPushButton;

class ExportPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExportPanel(QWidget* parent = nullptr);

public slots:
    void setExportEnabled(bool enabled);

signals:
    void exportRequested(const QString& format, const QString& dataSource, const QString& filePath);

private slots:
    void onExport();

private:
    QComboBox* m_formatCombo;
    QComboBox* m_sourceCombo;
    QPushButton* m_exportBtn;
};
