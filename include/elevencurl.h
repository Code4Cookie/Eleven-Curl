#ifndef ELEVENLIB_H
#define ELEVENLIB_H


struct elevenlabs_memchunk {
    char* memory;
    size_t size;
};

struct elevenlabs_voice {
    char m_szID[21];
    char m_szName[256];
};

struct elevenlabs_history_entry
{
    int m_iUnixDate;

    char m_szHistoryID[21];
    char m_szVoiceID[21];
    char m_szVoiceName[128];
    char m_szText[8192];
};

struct elevenlabs_voice_settings {
    float m_fStability, m_fSimilarityBoost;
};


#ifdef __cplusplus
extern "C"
{
#endif

int                 elevenlabs_getvoices(CURL* pCurl, struct elevenlabs_voice* pVoices, size_t len);
void                elevenlabs_getvoice(CURL* pCurl, const char* pszVoiceID, struct elevenlabs_voice* pVoice);
void                elevenlabs_getvoicesettings(CURL* pCurl, const char* pszVoiceID, struct elevenlabs_voice_settings* pSetting);
void                elevenlabs_gethistory(CURL* pCurl, struct elevenlabs_history_entry* pEntries, size_t len);

// You are responsible for freeing the memory inside the chunk after calling this!
void                elevenlabs_getsample(CURL* pCurl, const char* pszVoiceID, const char* pszSampleID, struct elevenlabs_memchunk* pChunk);

void                elevenlabs_setapikey(const char* pszKey);
const char*         elevenlabs_getapikey();

#ifdef __cplusplus
}
#endif

#endif // ELEVENLIB_H