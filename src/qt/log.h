#ifndef VBAM_QT_LOG_H_
#define VBAM_QT_LOG_H_

// Logging and string helpers for the Qt frontend. Replaces wxLogError /
// wxLogWarning / wxLogMessage / wxLogDebug and the wx string conversions.
//
// Semantics mirror the wx port:
//  - LogError / LogWarning show a modal message box (unless suppressed with a
//    LogNull scope) and append to the application log (see VbamApp::log).
//  - LogMessage / LogInfo only append to the application log.
//  - LogDebug prints to stderr in Debug builds only.

#include <functional>
#include <string>

#include <QString>

namespace vbam {

void LogError(const QString& msg);
void LogWarning(const QString& msg);
void LogMessage(const QString& msg);
void LogInfo(const QString& msg);
void LogDebug(const QString& msg);

// Suppresses the message boxes of LogError/LogWarning while alive (they are
// still appended to the application log). Equivalent of wxLogNull.
class LogNull {
public:
    LogNull();
    ~LogNull();
    LogNull(const LogNull&) = delete;
    LogNull& operator=(const LogNull&) = delete;
};

// True while at least one LogNull is alive.
bool LogSuppressed();

// The accumulated log text (every Log* call, timestamped). This is the store
// behind the Logging dialog; VbamApp::log is not used by the logging
// functions themselves.
const QString& LogText();
void ClearLogText();

// Callback invoked (on the calling thread) every time the log text changes.
// The main window installs one that refreshes the Logging dialog if it is up.
void SetLogUpdateCallback(std::function<void()> callback);

// UTF-8 conversions at the core boundary.
inline std::string ToStd(const QString& s) { return s.toStdString(); }
inline QString FromStd(const std::string& s) { return QString::fromStdString(s); }
inline QString FromUtf8(const char* s) { return s ? QString::fromUtf8(s) : QString(); }

// Local 8-bit filesystem path for fopen()/core APIs. On Windows the core uses
// UTF-8 aware wrappers, so UTF-8 is used everywhere.
inline std::string ToPath(const QString& s) { return s.toUtf8().toStdString(); }

}  // namespace vbam

// Convenience: translated string in non-QObject code.
#define VBAM_TR(s) QCoreApplication::translate("vbam", s)

#endif  // VBAM_QT_LOG_H_
