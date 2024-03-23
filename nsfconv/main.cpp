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
    int32_t numFramesInHeader; ///< number of samples per channel declared in wav header (only populated when reading)
    uint32_t totalFramesReadWritten; ///< total numSamples per channel which have been read or written
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
    w->numFramesInHeader = -1; // not used for writer
    w->totalFramesReadWritten = 0;
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

int wav_write_f(Wav *w, void *f, int len) {
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
            size_t frames_written = samples_written / w->numChannels;
            w->totalFramesReadWritten += frames_written;
            return (int) frames_written;
        }
        case W_FLOAT32: {
            float *z = (float *) alloca(w->numChannels * len * sizeof(float));
            const auto *const x = (const float *const) f;
            for (int i = 0; i < w->numChannels * len; ++i) {
                z[i] = x[i];
            }

            size_t samples_written = fwrite(z, sizeof(float), w->numChannels * len, w->f);
            size_t frames_written = samples_written / w->numChannels;
            w->totalFramesReadWritten += frames_written;
            return (int) frames_written;
        }
        default: return 0;
    }
}

void wav_close_write(Wav *w) {
    assert(w);
    assert(w->f);

    uint32_t data_len = w->totalFramesReadWritten * w->numChannels * w->sampFmt;
    uint32_t chunkSize_len = 36 + data_len; // 36 is size of header minus 8 (RIFF + this field)

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

    pc.SetValue("PLAY_TIME", (60 * 3 + 30) * 1000);
    pc.SetValue("AUTO_DETECT", 1);
    pc.SetValue("QUALITY", 40);
    p.SetConfig(&pc);

    if (!p.Load(&nsf)) {
        return 3;
    }

    p.SetChannels(1);

    const float multiplier = 1.0f / (float) INT16_MAX;

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
        nsf.SetLength(-1);
        p.SetPlayFreq(48000);
        p.SetSong(i);
        p.Reset();

        short samples[480];
        memset(samples, 0, 480 * sizeof(short));

        while (!p.IsStopped()) {
            auto len = p.Render(samples, 480);
            wav_write_f(&wav, samples, len);
        }

        wav_close_write(&wav);
    }

    return 0;
}
