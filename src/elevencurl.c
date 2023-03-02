#include <stdio.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#include "elevencurl.h"

#ifdef _MSC_VER 
#define ARRAY_SIZE(arr) _ARRAYSIZE(arr)
#endif

char g_apiKey[32];

void elevenlabs_setapikey(const char* pszKey)
{
    strncpy(g_apiKey, pszKey, ARRAY_SIZE(g_apiKey));
}

const char* elevenlabs_getapikey()
{
    return g_apiKey;
}

void build_apikey(char* pszIn)
{
    sprintf(pszIn, "xi-api-key: %s", g_apiKey);
}

int has_apikey(void)
{
    return g_apiKey[0] != '\0';
}

void ThrowJSONError()
{
    const char* error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL)
    {
        fprintf(stderr, "Error before: %s\n", error_ptr);
    }
}

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct elevenlabs_memchunk* mem = (struct elevenlabs_memchunk*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static void ParseJSON_Voice(const cJSON* pJson, struct elevenlabs_voice* pVoice)
{
    cJSON* name = cJSON_GetObjectItem(pJson, "name");
    cJSON* voice_id = cJSON_GetObjectItem(pJson, "voice_id");

    if (name && voice_id)
    {
        strncpy(pVoice->m_szID, voice_id->valuestring, ARRAY_SIZE(pVoice->m_szID));
        strncpy(pVoice->m_szName, name->valuestring, ARRAY_SIZE(pVoice->m_szName));
    }
}

static void ParseJSON_HistoryEntry(const cJSON* pJson, struct elevenlabs_history_entry* pEntry)
{
    cJSON* history_item_id = cJSON_GetObjectItem(pJson, "history_item_id");
    cJSON* voice_name = cJSON_GetObjectItem(pJson, "voice_name");
    cJSON* voice_id = cJSON_GetObjectItem(pJson, "voice_id");
    cJSON* text = cJSON_GetObjectItem(pJson, "text");
    cJSON* date_unix = cJSON_GetObjectItem(pJson, "date_unix");

    if (history_item_id)
        strncpy(pEntry->m_szHistoryID, history_item_id->valuestring, ARRAY_SIZE(pEntry->m_szHistoryID));
    if (voice_name)
        strncpy(pEntry->m_szVoiceName, voice_name->valuestring, ARRAY_SIZE(pEntry->m_szVoiceName));
    if ( voice_id)
        strncpy(pEntry->m_szVoiceID, voice_id->valuestring, ARRAY_SIZE(pEntry->m_szVoiceID));
    if (text)
        strncpy(pEntry->m_szText, text->valuestring, ARRAY_SIZE(pEntry->m_szText));
    if (date_unix)
        pEntry->m_iUnixDate = date_unix->valueint;
}

static int ParseJSON_Voices(const char* pJson, struct elevenlabs_voice* pOutVoices, size_t len)
{
    cJSON* json_root = cJSON_Parse(pJson);
    int i = 0;

    if (!json_root)
    {
        ThrowJSONError();

        goto end;
    }

    const cJSON* pVoices = cJSON_GetObjectItem(json_root, "voices");
    const cJSON* pVoice = NULL;

    if (!pVoices)
    {
        goto end;
    }

    cJSON_ArrayForEach(pVoice, pVoices)
    {
        if (i >= len)
            break;

        ParseJSON_Voice(pVoice, &pOutVoices[i]);
        
        i++;
    }

end:
    cJSON_Delete(json_root);

    return i; // return how many were put into the array
}

static void ParseJSON_VoiceSettings(const char* pJson, struct elevenlabs_voice_settings* pOutSettings)
{
    cJSON* json_root = cJSON_Parse(pJson);

    if (!json_root)
    {
        ThrowJSONError();

        goto end;
    }

    const cJSON* pStability = cJSON_GetObjectItem(json_root, "stability");

    if (pStability)
        pOutSettings->m_fStability = cJSON_GetNumberValue(pStability);

    const cJSON* pSimilarityBoost = cJSON_GetObjectItem(json_root, "similarity_boost");

    if (pSimilarityBoost)
        pOutSettings->m_fSimilarityBoost = cJSON_GetNumberValue(pSimilarityBoost);

end:
    cJSON_Delete(json_root);
}

static int ParseJSON_History(const char* pJson, struct elevenlabs_history_entry* pEntries, size_t len)
{
    cJSON* json_root = cJSON_Parse(pJson);
    int i = 0;

    if (!json_root)
    {
        ThrowJSONError();

        goto end;
    }

    const cJSON* pHistory = cJSON_GetObjectItem(json_root, "history");
    const cJSON* pEntry = NULL;

    if (!pHistory)
        goto end;

    cJSON_ArrayForEach(pHistory, pEntry)
    {
        if (i >= len)
            break;

        ParseJSON_HistoryEntry(pEntry, &pEntries[i]);

        i++;
    }

end:
    cJSON_Delete(json_root);

    return i; // return how many were put into the array
}

int elevenlabs_getvoices(CURL* pCurl, struct elevenlabs_voice* pVoices, size_t len)
{
    if (!pCurl || !has_apikey())
        return 0;

    int inserted = 0;
    struct elevenlabs_memchunk chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");

    char apikeyfmt[45];
    build_apikey(apikeyfmt);

    headers = curl_slist_append(headers, apikeyfmt);
    headers = curl_slist_append(headers, "charset: utf-8");

    curl_easy_setopt(pCurl, CURLOPT_URL, "https://api.elevenlabs.io/v1/voices");
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)&chunk);

    /* get it! */
    CURLcode res = curl_easy_perform(pCurl);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
        inserted = ParseJSON_Voices(chunk.memory, pVoices, len);
    }


    free(chunk.memory);

    curl_easy_reset(pCurl);

    return inserted;
}

void elevenlabs_getvoice(CURL* pCurl, const char* pszVoiceID, struct elevenlabs_voice* pVoice)
{
    if (!pCurl || !has_apikey())
        return;

    struct elevenlabs_memchunk chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");

    char apikeyfmt[45];
    build_apikey(apikeyfmt);

    headers = curl_slist_append(headers, apikeyfmt);
    headers = curl_slist_append(headers, "charset: utf-8");

    char szURL[256];
    snprintf(szURL, ARRAY_SIZE(szURL), "https://api.elevenlabs.io/v1/voices/%s", pszVoiceID);

    curl_easy_setopt(pCurl, CURLOPT_URL, szURL);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)&chunk);

    /* get it! */
    CURLcode res = curl_easy_perform(pCurl);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
        ParseJSON_Voice(chunk.memory, pVoice);
    }


    free(chunk.memory);

    curl_easy_reset(pCurl);
}

void elevenlabs_getsample(CURL* pCurl, const char* pszVoiceID, const char* pszSampleID, struct elevenlabs_memchunk* pChunk)
{
    if (!pCurl || !has_apikey() || !pChunk)
        return;

    pChunk->memory = malloc(1);  /* will be grown as needed by the realloc above */
    pChunk->size = 0;    /* no data at this point */

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "accept: audio/*");

    char apikeyfmt[45];
    build_apikey(apikeyfmt);

    headers = curl_slist_append(headers, apikeyfmt);
    headers = curl_slist_append(headers, "charset: utf-8");

    char szURL[256];
    snprintf(szURL, ARRAY_SIZE(szURL), "https://api.elevenlabs.io/v1/voices/%s/samples/%s/audio", pszVoiceID, pszSampleID);

    curl_easy_setopt(pCurl, CURLOPT_URL, szURL);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)pChunk);

    /* get it! */
    CURLcode res = curl_easy_perform(pCurl);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)pChunk->size);
    }


//    free(pChunk->memory);

    curl_easy_reset(pCurl);
}

void elevenlabs_getvoicesettings(CURL* pCurl, const char* pszVoiceID, struct elevenlabs_voice_settings* pOutSettings)
{
    if (!pCurl || !has_apikey())
        return;

    int inserted = 0;
    struct elevenlabs_memchunk chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");

    char apikeyfmt[45];
    build_apikey(apikeyfmt);

    headers = curl_slist_append(headers, apikeyfmt);
    headers = curl_slist_append(headers, "charset: utf-8");

    char szURL[256];
    snprintf(szURL, ARRAY_SIZE(szURL), "https://api.elevenlabs.io/v1/voices/%s/settings", pszVoiceID);

    curl_easy_setopt(pCurl, CURLOPT_URL, szURL);
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)&chunk);

    /* get it! */
    CURLcode res = curl_easy_perform(pCurl);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
        ParseJSON_VoiceSettings(chunk.memory, pOutSettings);
    }


    free(chunk.memory);

    curl_easy_reset(pCurl);
}

void elevenlabs_gethistory(CURL* pCurl, struct elevenlabs_history_entry* pEntries, size_t len)
{
    if (!pCurl || !has_apikey())
        return 0;

    int inserted = 0;
    struct elevenlabs_memchunk chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");

    char apikeyfmt[45];
    build_apikey(apikeyfmt);

    headers = curl_slist_append(headers, apikeyfmt);
    headers = curl_slist_append(headers, "charset: utf-8");

    curl_easy_setopt(pCurl, CURLOPT_URL, "https://api.elevenlabs.io/v1/history");
    curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)&chunk);

    /* get it! */
    CURLcode res = curl_easy_perform(pCurl);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
        inserted = ParseJSON_History(chunk.memory, pEntries, len);
    }


    free(chunk.memory);

    curl_easy_reset(pCurl);

    return inserted;
}