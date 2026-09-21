// PCM WAVE reader, reconstructed from WavLoader.cpp (0x0810d6c0-0x0810dab0).
#pragma once

namespace core
{
class FileInputStream;
}

namespace engine
{

class WavLoader
{
  public:
    WavLoader();
    ~WavLoader();
    void loadSound(const char* filename);

    short m_format;
    short m_channels;
    int m_sampleRate;
    int m_byteRate;
    short m_blockAlign;
    short m_bitsPerSample;
    int m_dataSize;
    char* m_pData;

  private:
    void formatChunk(core::FileInputStream& in, int size);
    void dataChunk(core::FileInputStream& in, int size);
};

} // namespace engine
