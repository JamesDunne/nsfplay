#include <sstream>
#include <string>

#include "nsfplay.h"

int main(int argc, char **argv) {
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

    for (int i = 0; i < songCount; i++) {
        xgm::NSFPlayerConfig pc;
        xgm::NSFPlayer p;

        std::string wavPath;
        {
            std::ostringstream s;
            s << nsfPath.substr(0, lastdot);
            s << ".";
            s << (i+1);
            s << ".pcm";
            wavPath = s.str();
        }

        printf("generating %s\n", wavPath.c_str());
        FILE *wav = fopen(wavPath.c_str(), "wb");
        if (!wav) {
            fprintf(stderr, "error opening wav for writing: %s\n", strerror(ferror(wav)));
            return 2;
        }

        pc.SetValue("PLAY_TIME", (60 * 3 + 30) * 1000);
        p.SetConfig(&pc);

        nsf.SetDefaults(pc["PLAY_TIME"], pc["FADE_TIME"], pc["LOOP_NUM"]);
        if (!p.Load(&nsf)) {
            fclose(wav);
            return 3;
        }

        p.SetChannels(1);
        p.SetPlayFreq(48000);
        p.SetSong(i);
        p.Reset();

        xgm::UINT32 samplesLength = 480;
        auto samples = static_cast<short *>(malloc(sizeof(short) * samplesLength));
        memset(samples, 0, samplesLength * sizeof(short));

        while (!p.IsStopped()) {
            auto len = p.Render(samples, samplesLength);
            fwrite(samples, sizeof(short), len, wav);
        }

        free(samples);
        fclose(wav);
    }

    return 0;
}
