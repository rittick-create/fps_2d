#pragma once

#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <memory>
#include <vector>

// Short original WAV effects; Qt Multimedia is intentionally not required.
class SoundBank {
public:
    SoundBank();
    ~SoundBank();
    void play(const QString& name);
    void setMuted(bool muted);
    bool muted() const { return m_muted; }
    void stopAll();

private:
    struct Voice {
        QString name;
        QString path;
        std::unique_ptr<QProcess> process;
    };
    QTemporaryDir m_directory;
    QString m_player;
    std::vector<Voice> m_voices;
    bool m_muted = false;
    bool m_suppressed = false;
};
