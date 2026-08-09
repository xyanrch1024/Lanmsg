#include "ui/SettingsDialog.h"

#include "common/Config.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);

    auto *form = new QFormLayout;
    m_nickname = new QLineEdit(Config::instance().nickname(), this);
    m_password = new QLineEdit(Config::instance().remotePassword(), this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(QStringLiteral("留空则每次远程控制都需确认"));
    form->addRow(QStringLiteral("昵称"), m_nickname);
    form->addRow(QStringLiteral("远程控制密码"), m_password);

    auto *note = new QLabel(QStringLiteral("设置密码后，对方发起远程控制时需输入正确密码才能连接。"), this);
    note->setWordWrap(true);
    note->setStyleSheet("color:gray;");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(note);
    lay->addWidget(buttons);
}

void SettingsDialog::save() {
    Config::instance().setNickname(m_nickname->text().trimmed());
    Config::instance().setRemotePassword(m_password->text());
    accept();
}
