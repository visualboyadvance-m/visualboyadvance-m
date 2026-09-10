#include "qt/log.h"

#include <cstdio>
#include <functional>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QWidget>

namespace vbam {

namespace {

int g_log_null_depth = 0;
QString g_log_text;
std::function<void()> g_log_update_callback;

void Append(const QString& msg) {
    g_log_text += QDateTime::currentDateTime().toString("HH:mm:ss");
    g_log_text += ": ";
    g_log_text += msg;
    g_log_text += '\n';
    if (g_log_update_callback) {
        g_log_update_callback();
    }
}

// Parent for the message boxes: the active window if a QApplication (GUI) is
// running, otherwise nothing can be shown at all (console tools, tests).
bool GuiAvailable() {
    return qobject_cast<QApplication*>(QCoreApplication::instance()) != nullptr;
}

void ShowBox(QMessageBox::Icon icon, const QString& title, const QString& msg) {
    if (LogSuppressed()) {
        return;
    }
    if (!GuiAvailable()) {
        fprintf(stderr, "%s: %s\n", title.toUtf8().constData(), msg.toUtf8().constData());
        return;
    }
    QMessageBox box(icon, title, msg, QMessageBox::Ok, QApplication::activeWindow());
    box.exec();
}

}  // namespace

void LogError(const QString& msg) {
    Append(msg);
    ShowBox(QMessageBox::Critical, QCoreApplication::translate("vbam", "Error"), msg);
}

void LogWarning(const QString& msg) {
    Append(msg);
    ShowBox(QMessageBox::Warning, QCoreApplication::translate("vbam", "Warning"), msg);
}

void LogMessage(const QString& msg) {
    Append(msg);
}

void LogInfo(const QString& msg) {
    Append(msg);
}

void LogDebug(const QString& msg) {
    // Always on in debug builds; in release builds only when VBAM_DEBUG is set
    // in the environment (renderer/audio init diagnostics).
#if defined(DEBUG) || !defined(NDEBUG)
    const bool enabled = true;
#else
    static const bool enabled = qEnvironmentVariableIsSet("VBAM_DEBUG");
#endif
    if (enabled)
        fprintf(stderr, "%s\n", msg.toUtf8().constData());
}

LogNull::LogNull() {
    g_log_null_depth++;
}

LogNull::~LogNull() {
    g_log_null_depth--;
}

bool LogSuppressed() {
    return g_log_null_depth > 0;
}

const QString& LogText() {
    return g_log_text;
}

void ClearLogText() {
    g_log_text.clear();
    if (g_log_update_callback) {
        g_log_update_callback();
    }
}

void SetLogUpdateCallback(std::function<void()> callback) {
    g_log_update_callback = std::move(callback);
}

}  // namespace vbam
