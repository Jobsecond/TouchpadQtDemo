#include <QApplication>
#include <QFont>
#include <QTextEdit>
#include <QVBoxLayout>
#include <vector>

#include "MainWindow.h"
#include "TouchpadHelper.h"

using RawInput::TouchpadHelper;
using RawInput::TouchpadContact;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("RawInput Touchpad");

    auto currentFont = font();
    currentFont.setHintingPreference(QFont::PreferNoHinting);
    setFont(currentFont);

    textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);

    auto centralWidget = new QWidget(this);
    auto layout = new QVBoxLayout(centralWidget);
    layout->addWidget(textEdit);
    setLayout(layout);
    setCentralWidget(centralWidget);

    TouchpadHelper::registerInput(reinterpret_cast<HWND>(winId()));

    QString displayText = QString("Precision touchpad exists: ") + (TouchpadHelper::exists() ? "Yes" : "No");
    textEdit->setPlainText(displayText);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    auto msg = static_cast<MSG *>(message);
    if (msg->message == WM_INPUT) {
        const auto contacts = TouchpadHelper::parseInput(reinterpret_cast<HRAWINPUT>(msg->lParam));
        QString displayText = QString("Precision touchpad exists: ") + (TouchpadHelper::exists() ? "Yes" : "No") + "\n";
        displayText += QString("Points count: %1\n").arg(contacts.size());
        for (const auto &contact : std::as_const(contacts)) {
            displayText += QString("Contact ID: %1 Point: (%2, %3)\n").arg(contact.contactId).arg(contact.x).arg(contact.y);
        }
        textEdit->setPlainText(displayText);
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
