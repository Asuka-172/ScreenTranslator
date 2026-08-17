#ifndef TEXT_TO_SPEECH_H
#define TEXT_TO_SPEECH_H

#include <QObject>
#include <QString>

struct ISpVoice;

// Windows SAPI 语音合成封装（COM ISpVoice），零额外依赖。
class TextToSpeech : public QObject
{
    Q_OBJECT
public:
    explicit TextToSpeech(QObject* parent = nullptr);
    ~TextToSpeech() override;

    bool isAvailable() const;
    void speak(const QString& text);
    void stop();
    void setRate(double rate);    // 0.5 - 2.0
    void setVolume(int volume);   // 0 - 100

private:
    ISpVoice* m_voice = nullptr;
    bool m_comInitialized = false;
};

#endif
