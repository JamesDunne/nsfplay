#include <sstream>
#include <string>
#include <cstdint>
#include <cstdio>

#include "nsfplay.h"

typedef struct {
    char ChunkID[4];
    uint32_t ChunkSize;
    char Format[4];
    char Subchunk1ID[4];
    uint32_t Subchunk1Size;
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    char Subchunk2ID[4];
    uint32_t Subchunk2Size;
} WavHeader;

typedef enum {
    W_INT16 = 2,  // two byte signed integer
    W_FLOAT32 = 4 // four byte IEEE float
} WavSampleFormat;

typedef struct {
    FILE *f;
    WavHeader h;
    int16_t numChannels;
    uint32_t dataBytesWritten;
    uint32_t extraRiffBytesWritten;
    WavSampleFormat sampFmt;
} Wav;

int wav_open_write(
    Wav *w,
    int16_t numChannels,
    int32_t samplerate,
    WavSampleFormat sampFmt,
    const char *path
) {
    assert(w);
    assert(path);
    assert(numChannels >= 1);
    assert(samplerate >= 1);

#if _WIN32
    errno_t err = fopen_s(&w->f, path, "wb");
    if (err != 0) { w->f == nullptr; }
#else
    w->f = fopen(path, "wb");
#endif

    if (w->f == nullptr) {
        return -1;
    }

    w->numChannels = numChannels;
    w->dataBytesWritten = 0;
    w->extraRiffBytesWritten = 0;
    w->sampFmt = sampFmt;

    // prepare WAV header
    w->h.ChunkID[0] = 'R';
    w->h.ChunkID[1] = 'I';
    w->h.ChunkID[2] = 'F';
    w->h.ChunkID[3] = 'F';
    // fill this in on file-close
    w->h.ChunkSize = 0;
    w->h.Format[0] = 'W';
    w->h.Format[1] = 'A';
    w->h.Format[2] = 'V';
    w->h.Format[3] = 'E';
    w->h.Subchunk1ID[0] = 'f';
    w->h.Subchunk1ID[1] = 'm';
    w->h.Subchunk1ID[2] = 't';
    w->h.Subchunk1ID[3] = ' ';
    w->h.Subchunk1Size = 16; // PCM
    w->h.AudioFormat = (w->sampFmt - 1); // 1 PCM, 3 IEEE float
    w->h.NumChannels = numChannels;
    w->h.SampleRate = samplerate;
    w->h.ByteRate = samplerate * numChannels * w->sampFmt;
    w->h.BlockAlign = numChannels * w->sampFmt;
    w->h.BitsPerSample = 8 * w->sampFmt;
    w->h.Subchunk2ID[0] = 'd';
    w->h.Subchunk2ID[1] = 'a';
    w->h.Subchunk2ID[2] = 't';
    w->h.Subchunk2ID[3] = 'a';
    // fill this in on file-close
    w->h.Subchunk2Size = 0;

    // write WAV header:
    size_t elementCount = fwrite(w->h.ChunkID, sizeof(char), 4, w->f);
    elementCount += fwrite(&w->h.ChunkSize, sizeof(uint32_t), 1, w->f);
    elementCount += fwrite(w->h.Format, sizeof(char), 4, w->f);
    elementCount += fwrite(w->h.Subchunk1ID, sizeof(char), 4, w->f);
    elementCount += fwrite(&w->h.Subchunk1Size, sizeof(uint32_t), 1, w->f);
    elementCount += fwrite(&w->h.AudioFormat, sizeof(uint16_t), 1, w->f);
    elementCount += fwrite(&w->h.NumChannels, sizeof(uint16_t), 1, w->f);
    elementCount += fwrite(&w->h.SampleRate, sizeof(uint32_t), 1, w->f);
    elementCount += fwrite(&w->h.ByteRate, sizeof(uint32_t), 1, w->f);
    elementCount += fwrite(&w->h.BlockAlign, sizeof(uint16_t), 1, w->f);
    elementCount += fwrite(&w->h.BitsPerSample, sizeof(uint16_t), 1, w->f);
    elementCount += fwrite(w->h.Subchunk2ID, sizeof(char), 4, w->f);
    elementCount += fwrite(&w->h.Subchunk2Size, sizeof(uint32_t), 1, w->f);
    if (elementCount != 25) {
        return -1;
    }

    return 0;
}

void wav_write_f(Wav *w, void *f, int len) {
    assert(w != nullptr);
    assert(f != nullptr);
    assert(len >= 0);

    switch (w->sampFmt) {
        case W_INT16: {
            int16_t *z = (int16_t *) alloca(w->numChannels * len * sizeof(int16_t));
            const auto *const x = (const int16_t *const) f;
            for (int i = 0; i < w->numChannels * len; ++i) {
                z[i] = (int16_t) (x[i]);
            }

            size_t samples_written = fwrite(z, sizeof(int16_t), w->numChannels * len, w->f);
            w->dataBytesWritten += samples_written * sizeof(int16_t);
            return;
        }
        case W_FLOAT32: {
            float *z = (float *) alloca(w->numChannels * len * sizeof(float));
            const auto *const x = (const float *const) f;
            for (int i = 0; i < w->numChannels * len; ++i) {
                z[i] = x[i];
            }

            size_t samples_written = fwrite(z, sizeof(float), w->numChannels * len, w->f);
            w->dataBytesWritten += samples_written * sizeof(float);
            return;
        }
        default: return;
    }
}

void wav_write_subchunk(Wav *w, const char id[4], uint32_t size, void *data) {
    assert(w);
    assert(w->f);

    // TODO: 32-bit overflow and splitting RIFF chunks
    w->extraRiffBytesWritten += 4 + 4 + size;
    fwrite(id, 1, 4, w->f);
    fwrite(&size, 1, 4, w->f);
    fwrite(data, 1, size, w->f);
}

void wav_close_write(Wav *w) {
    assert(w);
    assert(w->f);

    uint32_t data_len = w->dataBytesWritten;
    // 36 is size of header minus 8 (RIFF + this field)
    uint32_t chunkSize_len = 36 + data_len + w->extraRiffBytesWritten;

    // update header struct as well
    w->h.ChunkSize = chunkSize_len;
    w->h.Subchunk2Size = data_len;

    // set length of data
    fseek(w->f, 4, SEEK_SET); // offset of ChunkSize
    fwrite(&chunkSize_len, sizeof(uint32_t), 1, w->f); // write ChunkSize

    fseek(w->f, 40, SEEK_SET); // offset Subchunk2Size
    fwrite(&data_len, sizeof(uint32_t), 1, w->f); // write Subchunk2Size

    fclose(w->f);
    w->f = nullptr;
}

void id3_synchsafe_u32(uint8_t *id3, uint32_t &p, uint32_t v) {
    id3[p+3] = (v & 0x7F);
    v = v >> 7;
    id3[p+2] = (v & 0x7F);
    v = v >> 7;
    id3[p+1] = (v & 0x7F);
    v = v >> 7;
    id3[p+0] = (v & 0x7F);

    p += 4;
}

void id3_frame(uint8_t *id3, uint32_t &id3Size, const char id[4], const char *text) {
    auto len = strlen(text);

    id3[id3Size++] = id[0];
    id3[id3Size++] = id[1];
    id3[id3Size++] = id[2];
    id3[id3Size++] = id[3];
    // length = strlen(text) + 1 byte NUL terminator + 1 byte text encoding
    id3_synchsafe_u32(id3, id3Size, len+2);
    // flags:
    id3[id3Size++] = 0;
    id3[id3Size++] = 0;
    // text encoding:
    id3[id3Size++] = 0; // ISO-8859-1, terminated with $00
    // text:
    for (size_t i = 0; i < len+1; i++) {
        id3[id3Size++] = text[i];
    }
    //printf("%4s: %s\n", id, text);
}

int main(int argc, char **argv) {
    xgm::NSFPlayerConfig pc;
    xgm::NSFPlayer p;
    xgm::NSF nsf;

    argc--;
    argv++;

    if (argc < 1) {
        fprintf(stderr, "required input NSF file\n");
        return 1;
    }

    std::string nsfPath = argv[0];

    printf("loading %s\n", nsfPath.c_str());
    if (!nsf.LoadFile(nsfPath.c_str())) {
        fprintf(stderr, "%s\n", nsf.LoadError());
        return 1;
    }

    auto lastdot = nsfPath.find_last_of('.');

    auto songCount = nsf.GetSongNum();
    printf("%d songs\n", songCount);

    pc["PLAY_TIME"] = (60 * 3 + 30) * 1000;
    pc["AUTO_DETECT"] = 1;
    pc["QUALITY"] = 40;
    pc["MASTER_VOLUME"] = 224;
    pc["APU2_OPTION5"] = 0; // disable randomized noise phase at reset
    pc["APU2_OPTION7"] = 0; // disable randomized tri phase at reset

    // disable DMC pop at start:
    pc.GetDeviceOption(xgm::DMC,xgm::NES_DMC::OPT_ENABLE_4011) = 0;

    p.SetConfig(&pc);

    if (!p.Load(&nsf)) {
        return 3;
    }

    p.SetChannels(1);

    for (int i = 0; i < songCount; i++) {
        std::string wavPath;
        {
            std::ostringstream s;
            s << nsfPath.substr(0, lastdot);
            s << ".";
            s << (i+1);
            s << ".wav";
            wavPath = s.str();
        }

        printf("generating %s\n", wavPath.c_str());

        Wav wav;
        if (wav_open_write(
            &wav,
            1,
            48000,
            W_INT16,
            wavPath.c_str()
        )) {
            fprintf(stderr, "unable to open file for writing: %s\n", strerror(ferror(wav.f)));
            return 2;
        }

        nsf.SetDefaults(pc["PLAY_TIME"], pc["FADE_TIME"], pc["LOOP_NUM"]);
        nsf.ClearLength();
        p.SetPlayFreq(48000);
        p.SetSong(i);
        p.Reset();

        // ID3v2:
        uint8_t id3[512];
        uint32_t id3Size = 0;
        uint32_t id3LengthPtr = 0;
        {
            // ID3v2 identifier:
            id3[id3Size++] = 'I';
            id3[id3Size++] = 'D';
            id3[id3Size++] = '3';
            // version
            id3[id3Size++] = 4;
            id3[id3Size++] = 0;
            // flags:
            id3[id3Size++] = 0;
            // length:
            id3LengthPtr = id3Size;
            id3[id3Size++] = 0;
            id3[id3Size++] = 0;
            id3[id3Size++] = 0;
            id3[id3Size++] = 0;
        }

        // artist:
        id3_frame(id3, id3Size, "TPE1", nsf.GetTitleString("%t"));
        // album:
        id3_frame(id3, id3Size, "TALB", "Tracks and Effects");
        // composer:
        id3_frame(id3, id3Size, "TCOM", nsf.GetTitleString("%a"));
        // title:
        id3_frame(id3, id3Size, "TIT2", nsf.GetTitleString("Track %n"));
        // track number / total tracks:
        id3_frame(id3, id3Size, "TRCK", nsf.GetTitleString("%n/%e"));
        // copyright:
        id3_frame(id3, id3Size, "TCOP", nsf.GetTitleString("%c"));

        // encode ID3 length:
        id3_synchsafe_u32(id3, id3LengthPtr, id3Size - 10);

        // render WAV data:
        short samples[480];
        memset(samples, 0, 480 * sizeof(short));
        while (!p.IsStopped()) {
            auto len = p.Render(samples, 480);
            wav_write_f(&wav, samples, len);
        }

        // write id3 RIFF chunk:
        wav_write_subchunk(&wav, "id3 ", id3Size, id3);

        // close out file and write back lengths:
        wav_close_write(&wav);
    }

    return 0;
}
