#include "TextToSpeech.h"
#include <sapi.h>
#include <objbase.h>
#include <string>

TextToSpeech::TextToSpeech(QObject* parent)
    : QObject(parent)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_comInitialized = SUCCEEDED(hr);

    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
        IID_ISpVoice, reinterpret_cast<void**>(&m_voice));
    if (FAILED(hr))
        m_voice = nullptr;
}

TextToSpeech::~TextToSpeech()
{
    if (m_voice) {
        m_voice->Release();
        m_voice = nullptr;
    }
    if (m_comInitialized)
        CoUninitialize();
}

bool TextToSpeech::isAvailable() const
{
    return m_voice != nullptr;
}

void TextToSpeech::speak(const QString& text)
{
    if (!m_voice || text.isEmpty())
        return;
    m_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    std::wstring w = text.toStdWString();
    m_voice->Speak(w.c_str(), SPF_ASYNC, nullptr);
}

void TextToSpeech::stop()
{
    if (m_voice)
        m_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
}

void TextToSpeech::setRate(double rate)
{
    if (!m_voice)
        return;
    long r = qBound(-10L, static_cast<long>(qRound((rate - 1.0) * 10.0)), 10L);
    m_voice->SetRate(r);
}

void TextToSpeech::setVolume(int volume)
{
    if (!m_voice)
        return;
    m_voice->SetVolume(static_cast<USHORT>(qBound(0, volume, 100)));
}
