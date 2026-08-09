#pragma once

#include <QDialog>

class QLineEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void save();

private:
    QLineEdit *m_nickname = nullptr;
    QLineEdit *m_password = nullptr;
};
