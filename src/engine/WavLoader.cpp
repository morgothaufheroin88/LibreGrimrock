// Reconstructed from Grimrock.bin.x86 WavLoader.cpp.
#include "engine/WavLoader.h"
#include "core/Exception.h"
#include "core/FileStream.h"

namespace engine
{

using namespace core;

constexpr unsigned int RiffTag = 0x46464952;        // "RIFF"
constexpr unsigned int WaveTag = 0x45564157;        // "WAVE"
constexpr unsigned int FormatChunkTag = 0x20746d66; // "fmt "
constexpr unsigned int DataChunkTag = 0x61746164;   // "data"

// 0x0810d6c0
WavLoader::WavLoader()
    : m_format(0), m_channels(0), m_sampleRate(0), m_byteRate(0), m_blockAlign(0),
      m_bitsPerSample(0), m_dataSize(0), m_pData(0)
{
}
// 0x0810d760
WavLoader::~WavLoader()
{
    delete[] m_pData;
}
// 0x0810d7c0
void WavLoader::formatChunk(FileInputStream& in, int size)
{
    in.readShort(m_format);
    in.readShort(m_channels);
    in.readInt(m_sampleRate);
    in.readInt(m_byteRate);
    in.readShort(m_blockAlign);
    in.readShort(m_bitsPerSample);
    if (m_format != 1)
        throw Exception("Not a PCM waveform file.");
}
// 0x0810d700
void WavLoader::dataChunk(FileInputStream& in, int size)
{
    delete[] m_pData;
    m_dataSize = size;
    m_pData = new char[size];
    in.readBytes(m_pData, size);
}
// 0x0810d890: walks the RIFF chunks, keeps the last fmt and data chunks.
void WavLoader::loadSound(const char* filename)
{
    delete[] m_pData;
    m_pData = 0;
    FileInputStream stream(filename);
    unsigned int riff, size, wave;
    stream.readInt(riff);
    stream.readInt(size);
    stream.readInt(wave);
    if (riff != RiffTag)
        throw Exception("Not a RIFF file.");
    if (wave != WaveTag)
        throw Exception("Not a WAVE file.");
    size -= 4;
    while (size != 0)
    {
        unsigned int id, chunkSize;
        stream.readInt(id);
        stream.readInt(chunkSize);
        size -= 8;
        int start = stream.getPosition();
        if (id == FormatChunkTag)
            formatChunk(stream, chunkSize);
        else if (id == DataChunkTag)
            dataChunk(stream, chunkSize);
        int consumed = stream.getPosition() - start;
        if (consumed < (int)chunkSize)
            stream.skip(chunkSize - consumed);
        size -= chunkSize;
    }
}

} // namespace engine
