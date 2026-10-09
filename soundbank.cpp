#include "soundbank.h"

#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QStringList>

SoundBank::SoundBank()
{
    m_suppressed = qEnvironmentVariableIsSet("GRIDBREACH_TEST")
        || QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal");
    if (m_suppressed || !m_directory.isValid()) return;

#ifdef Q_OS_MACOS
    m_player = QStandardPaths::findExecutable(QStringLiteral("afplay"));
#else
    m_player = QStandardPaths::findExecutable(QStringLiteral("paplay"));
    if (m_player.isEmpty())
        m_player = QStandardPaths::findExecutable(QStringLiteral("aplay"));
#endif
    if (m_player.isEmpty()) return;

    const QStringList effects {"rifle", "shotgun", "hit", "explosion", "hurt",
                               "pickup", "reload", "wave", "victory", "defeat"};
    for (const QString& name : effects) {
        const QString path = m_directory.filePath(name + ".wav");
        if (!QFile::copy(":/sounds/" + name + ".wav", path)) continue;
        const int voiceCount = (name == "rifle" || name == "shotgun") ? 2 : 1;
        for (int i = 0; i < voiceCount; ++i) {
            auto process = std::make_unique<QProcess>();
            process->setStandardOutputFile(QProcess::nullDevice());
            process->setStandardErrorFile(QProcess::nullDevice());
            m_voices.push_back({name, path, std::move(process)});
        }
    }
}

SoundBank::~SoundBank()
{
    stopAll();
}

void SoundBank::play(const QString& name)
{
    if (m_muted || m_suppressed || m_player.isEmpty()) return;
    for (Voice& voice : m_voices) {
        if (voice.name != name || voice.process->state() != QProcess::NotRunning)
            continue;
        voice.process->start(m_player, QStringList {voice.path});
        return;
    }
    // All voices busy: skip this effect instead of building an audio backlog.
}

void SoundBank::setMuted(bool muted)
{
    m_muted = muted;
    if (muted) stopAll();
}

void SoundBank::stopAll()
{
    // Signal every active voice first, then reap. No player outlives this bank.
    for (Voice& voice : m_voices)
        if (voice.process->state() != QProcess::NotRunning) voice.process->kill();
    for (Voice& voice : m_voices)
        if (voice.process->state() != QProcess::NotRunning)
            voice.process->waitForFinished(100);
}
