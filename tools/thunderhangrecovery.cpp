#include <iostream>
#include <curl/curl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <rbus/rbus.h>
#include <fstream>
#include <signal.h>
#include <atomic>
#include <cstdarg>
#include <cjson/cJSON.h>

#define NUM_ELEMENTS                  3
#define PROCESS_NAME                  "WPEFramework"
#define CONFIG_FILE                   "/opt/thunderHangDetector/thunderHangDetector.json"

const char* HANG_DETECTOR_ENABLE = "Device.Thunder.HangDetector.Enable";
const char* HANG_DETECTOR_COREFILE_ENABLE = "Device.Thunder.HangDetector.CoreFile.Enable";
const char* HANG_DETECTOR_POLLING_COUNT = "Device.Thunder.HangDetector.Polling.Count";
std::atomic<bool> hangDetetectorEnable{false};
std::atomic<bool> coreFileEnable{false};
std::atomic<unsigned int> pollingCount{5};
cJSON* json = nullptr;

class CurlObject
{
    private:
        CURL* m_curlHandle;
        std::string m_curlDataBuffer;
        long m_httpcode;

    public:
        CurlObject(const std::string& url, const std::string data, const struct curl_slist *headers);
        ~CurlObject();
        static int curlwritefunc(const char *data, size_t size, size_t nmemb, std::string *buffer);
        std::string getCurlData();
        long gethttpcode();
};

const char* trimPath(const char* s)
{
  if (!s)
    return s;

  const char* t = strrchr(s, (int) '/');
  if (t) t++;
  if (!t) t = s;

  return t;
}

void logPrint(const char* file, const char* func, int line, const char* format, ...)
{
    size_t n = 0;
    const short kFormatMessageSize = 1024;
    char formattedLog[kFormatMessageSize] = {0};

    va_list args;

    va_start(args, format);
    n = vsnprintf(formattedLog, (kFormatMessageSize - 1), format, args);
    va_end(args);

    if (n > (kFormatMessageSize - 1))
    {
        formattedLog[kFormatMessageSize - 4] = '.';
        formattedLog[kFormatMessageSize - 3] = '.';
        formattedLog[kFormatMessageSize - 2] = '.';
    }
    formattedLog[kFormatMessageSize - 1] = '\0';

    struct timeval tv;
    struct tm* lt;
    const char* fileName = trimPath(file);

    gettimeofday(&tv, NULL);
    lt = localtime(&tv.tv_sec);

    printf("%.2d:%.2d:%.2d.%.6lld [PID=%d] [TID=%d] [%s +%d] %s : %s\n", lt->tm_hour, lt->tm_min, lt->tm_sec, (long long int)tv.tv_usec, getpid(), gettid(), fileName, line, func, formattedLog);
    fflush(stdout);
}

#define LOG_MSG(FMT, ...)   logPrint(__FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)

static void writeJsonToFile()
{
    if (json == nullptr)
    {
        LOG_MSG("JSON object is null");
        return;
    }

    char* jsonString = cJSON_Print(json);
    if (jsonString == nullptr)
    {
        LOG_MSG("Failed to print JSON object.");
        return;
    }

    std::ofstream jsonFile(CONFIG_FILE);
    if (!jsonFile.is_open())
    {
        LOG_MSG("Could not open the json file for writing!");
        cJSON_free(jsonString);
        return;
    }

    jsonFile << jsonString;
    jsonFile.close();

    cJSON_free(jsonString);
}

static rbusError_t hangDetectorEnableGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName)
    {
        LOG_MSG("Unable to handle get request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_ENABLE, strlen(HANG_DETECTOR_ENABLE)) == 0)
    {
        rbusValue_t value;
        rbusValue_Init(&value);
        rbusValue_SetBoolean(value, hangDetetectorEnable);
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);
    }
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t hangDetectorEnableSetHandler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;
    std::string updatedJsonContent;
    bool updatedHangDetectorValue;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName)
    {
        LOG_MSG("Unable to handle set request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    rbusValueType_t type;
    rbusValue_t paramValue = rbusProperty_GetValue(property);
    if(paramValue)
        type = rbusValue_GetType(paramValue);
    else
    {
        LOG_MSG("Invalid input to set");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_ENABLE, strlen(HANG_DETECTOR_ENABLE)) == 0)
    {
        if(type == RBUS_BOOLEAN)
            hangDetetectorEnable = rbusValue_GetBoolean(paramValue);
        else
        {
            LOG_MSG("set value type is invalid");
            return RBUS_ERROR_INVALID_INPUT;
        }
        updatedHangDetectorValue = hangDetetectorEnable.load();
        cJSON* newHangDetectorEnable = cJSON_CreateBool(updatedHangDetectorValue);
        if (newHangDetectorEnable != nullptr) {
            cJSON_ReplaceItemInObjectCaseSensitive(json, "hangDetectorEnable", newHangDetectorEnable);
        }
        writeJsonToFile();
    }
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t hangDetectorCoreFileGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName)
    {
        LOG_MSG("Unable to handle get request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_COREFILE_ENABLE, strlen(HANG_DETECTOR_COREFILE_ENABLE)) == 0)
    {
        rbusValue_t value;
        rbusValue_Init(&value);
        rbusValue_SetBoolean(value, coreFileEnable);
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);
    }
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t hangDetectorCoreFileSetHandler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;
    std::string updatedJsonContent;
    bool updatedCoreFileValue;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName)
    {
        LOG_MSG("Unable to handle set request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    rbusValueType_t type;
    rbusValue_t paramValue = rbusProperty_GetValue(property);
    if(paramValue)
        type = rbusValue_GetType(paramValue);
    else
    {
        LOG_MSG("Invalid input to set");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_COREFILE_ENABLE, strlen(HANG_DETECTOR_COREFILE_ENABLE)) == 0)
    {
        if(type == RBUS_BOOLEAN)
            coreFileEnable = rbusValue_GetBoolean(paramValue);
        else
        {
            LOG_MSG("set value type is invalid");
            return RBUS_ERROR_INVALID_INPUT;
        }
        updatedCoreFileValue = coreFileEnable.load();
        cJSON* newCoreFileValue = cJSON_CreateBool(updatedCoreFileValue);
        if (newCoreFileValue != nullptr && json != nullptr) {
            cJSON_ReplaceItemInObjectCaseSensitive(json, "coreFileEnable", newCoreFileValue);
        }
        writeJsonToFile();
    }
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t hangDetectorPollingCountGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName) {
        LOG_MSG("Unable to handle get request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_POLLING_COUNT, strlen(HANG_DETECTOR_POLLING_COUNT)) == 0)
    {
        rbusValue_t value;
        rbusValue_Init(&value);
        rbusValue_SetUInt32(value, pollingCount);
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);
    }
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t hangDetectorPollingCountSetHandler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts)
{
    (void) handle;
    (void) opts;
    const char* propertyName;
    std::string updatedJsonContent;
    unsigned int updatedPollingCountValue;

    propertyName = rbusProperty_GetName(property);
    if(!propertyName) {
        LOG_MSG("Unable to handle set request for property");
        return RBUS_ERROR_INVALID_INPUT;
    }
    rbusValueType_t type;
    rbusValue_t paramValue = rbusProperty_GetValue(property);
    if(paramValue)
        type = rbusValue_GetType(paramValue);
    else
    {
        LOG_MSG("Invalid input to set");
        return RBUS_ERROR_INVALID_INPUT;
    }
    if(strncmp(propertyName, HANG_DETECTOR_POLLING_COUNT, strlen(HANG_DETECTOR_POLLING_COUNT)) == 0)
    {
        if(type == RBUS_UINT32)
            pollingCount = rbusValue_GetUInt32(paramValue);
        else
        {
            LOG_MSG("set value type is invalid");
            return RBUS_ERROR_INVALID_INPUT;
        }
        updatedPollingCountValue = pollingCount.load();
        cJSON* newPollingCountValue = cJSON_CreateNumber(updatedPollingCountValue);
        if (newPollingCountValue != nullptr) {
            cJSON_ReplaceItemInObjectCaseSensitive(json, "pollingCount", newPollingCountValue);
        }
        writeJsonToFile();
    }
    return RBUS_ERROR_SUCCESS;
}

CurlObject::CurlObject(const std::string& url, const std::string data, const struct curl_slist *headers) {
    CURLcode res;
    char errbuf[CURL_ERROR_SIZE];
    long httpCode;
    m_curlHandle = curl_easy_init();
    if (!m_curlHandle) {
        LOG_MSG("curl failed in init");
    }

    errbuf[0] = 0;

    res = curl_easy_setopt(m_curlHandle, CURLOPT_URL, url.c_str());
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_URL set failed with curl error %d", res);

    res = curl_easy_setopt(m_curlHandle, CURLOPT_TIMEOUT, 10L);  // Timeout for entire request (in seconds)
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_TIMEOUT set failed with curl error %d", res);

    // Set headers
    res = curl_easy_setopt(m_curlHandle, CURLOPT_HTTPHEADER, headers);
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_URL set failed with curl error %d", res);

    res = curl_easy_setopt(m_curlHandle, CURLOPT_POSTFIELDS, data.c_str());
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_POSTFIELDS set failed with curl error %d", res);

    res = curl_easy_setopt(m_curlHandle, CURLOPT_ERRORBUFFER, errbuf);
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_ERRORBUFFER set failed with curl error %d", res);

    errbuf[0] = 0;

    res = curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, &CurlObject::curlwritefunc);
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_WRITEFUNCTION set failed with curl error %d", res);

    res = curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &m_curlDataBuffer);
    if (CURLE_OK != res)
        LOG_MSG("CURLOPT_WRITEDATA set failed with curl error %d", res);

    res = curl_easy_perform(m_curlHandle);
    if (CURLE_OK != res) {
        LOG_MSG("curl failed with curl error %d", res);
        size_t len = strlen(errbuf);
        if (len)
            LOG_MSG("errbuf content = %s%s", errbuf, ((errbuf[len - 1] != '\n') ? "\n" : ""));
        else
            LOG_MSG("curl_easy_perform failed: %s", curl_easy_strerror(res));
    }

    res = curl_easy_getinfo(m_curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
    if (CURLE_OK != res) {
        LOG_MSG("Failed to get response code");
    }

    if (httpCode != 200)
        LOG_MSG("curl failed with http error %d", httpCode);

    m_httpcode = httpCode;
    curl_easy_cleanup(m_curlHandle);
}

int CurlObject::curlwritefunc(const char *data, size_t size, size_t nmemb, std::string *buffer) {
    int result = 0;
    if (buffer != nullptr) {
        buffer->append(data, size * nmemb);
        result = size * nmemb;
    } else {
        LOG_MSG("curl buffer NULL");
    }
    return result;
}

std::string CurlObject::getCurlData() {
    LOG_MSG("Received data: %s", m_curlDataBuffer.c_str());
    return m_curlDataBuffer;
}

long CurlObject::gethttpcode() {
    return m_httpcode;
}

CurlObject::~CurlObject() {}

int main() {

    const std::string url = "http://127.0.0.1:9998/jsonrpc";
    std::string jsonData;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    unsigned int failureCount = 0;
    unsigned int successCount = 0;
    unsigned int iterationCount = 1;
    unsigned int timer = 30;
    rbusHandle_t rbus_handle;
    char command[256];

    int ret = RBUS_ERROR_SUCCESS;

    // Open and read the JSON file
    std::ifstream file(CONFIG_FILE);
    if (!file.is_open())
    {
        LOG_MSG("Unable to open file: %s", CONFIG_FILE);
        json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "hangDetectorEnable", hangDetetectorEnable);
        cJSON_AddBoolToObject(json, "coreFileEnable", coreFileEnable);
        cJSON_AddNumberToObject(json, "pollingCount", pollingCount);

        char *jsonString = cJSON_Print(json);
        if (jsonString == nullptr) {
            LOG_MSG("Failed to print JSON string");
            cJSON_Delete(json);
            json = nullptr;
        }
        cJSON_free(jsonString);
        writeJsonToFile();
    }
    else
    {
        // Read entire file content into a string
        std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Parse the JSON content using cJSON
        json = cJSON_Parse(jsonContent.c_str());
        if (json == nullptr) {
            const char* errorPtr = cJSON_GetErrorPtr();
            if (errorPtr != nullptr) {
                LOG_MSG("Error to parse json file: %s", errorPtr);
            }
        }
        else
        {
            cJSON *hangDetectorEnableValue = cJSON_GetObjectItemCaseSensitive(json, "hangDetectorEnable");
            if (cJSON_IsBool(hangDetectorEnableValue))
                hangDetetectorEnable = cJSON_IsTrue(hangDetectorEnableValue) ? true : false;
            cJSON *coreFileEnableValue = cJSON_GetObjectItemCaseSensitive(json, "coreFileEnable");
            if (cJSON_IsBool(coreFileEnableValue))
                coreFileEnable = cJSON_IsTrue(coreFileEnableValue) ? true : false;
            cJSON *pollingCountValue = cJSON_GetObjectItemCaseSensitive(json, "pollingCount");
            if (cJSON_IsNumber(pollingCountValue))
                pollingCount = pollingCountValue->valueint;
        }
    }

    ret = rbus_open(&rbus_handle, "thunderHangDetector");
    if(ret != RBUS_ERROR_SUCCESS)
        LOG_MSG("rbus open failed with error code %d", ret);

    rbusDataElement_t dataElements[NUM_ELEMENTS] = {

        {(char*)HANG_DETECTOR_ENABLE, RBUS_ELEMENT_TYPE_PROPERTY, {hangDetectorEnableGetHandler, hangDetectorEnableSetHandler, NULL, NULL, NULL, NULL}},
        {(char*)HANG_DETECTOR_COREFILE_ENABLE, RBUS_ELEMENT_TYPE_PROPERTY, {hangDetectorCoreFileGetHandler, hangDetectorCoreFileSetHandler, NULL, NULL, NULL, NULL}},
        {(char*)HANG_DETECTOR_POLLING_COUNT, RBUS_ELEMENT_TYPE_PROPERTY, {hangDetectorPollingCountGetHandler, hangDetectorPollingCountSetHandler, NULL, NULL, NULL, NULL}},
    };

    ret = rbus_regDataElements(rbus_handle, NUM_ELEMENTS, dataElements);

    sleep(180);
    while (true) {
        std::ostringstream jsonStream;
        char buffer[128] = {};
        std::string killCmd;
        int pid = -1;
        std::string signal;
        jsonStream << R"json({"jsonrpc": "2.0", "id": )json" << iterationCount << R"json(, "method": "Controller.1.status"})json";
        jsonData = jsonStream.str();
        snprintf(command, sizeof(command), "pidof %s", PROCESS_NAME);
        FILE *fp = popen(command, "r");
        if (!fp)
            LOG_MSG("popen for pidof WPEFramework failed");

        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            pid = atoi(buffer);
        }
        pclose(fp);
        int rc = kill(pid, 0);
        if(rc == 0 && pid != -1)
        {
            CurlObject curlObj(url, jsonData, headers);
            long httpCode = curlObj.gethttpcode();

            if (httpCode != 200) {
                failureCount++;
                LOG_MSG("External JSONRPC failed for %d retry", failureCount);
                timer = 5;
                if (failureCount >= pollingCount) {
                    LOG_MSG("Number of external JSONRPC request successfully executed before thunder hang: %d ", successCount);
                    LOG_MSG("Thunder is not responding to the 3 consecutive external JSONRPC request, so killing the WPEFramework process");
                    if(hangDetetectorEnable)
                    {
                        if(coreFileEnable)
                            signal = "SIGSEGV";
                        else
                            signal = "SIGHUP";
                        killCmd = "killall -s " + signal + " " + PROCESS_NAME;
                        int result = system(killCmd.c_str());
                        if (result != 0)
                        {
                            LOG_MSG("Failed to execute command: %s", killCmd);
                        }
                    }
                    failureCount = 0;
                    successCount = 0;
                }
            } else {
                if(failureCount < pollingCount && failureCount != 0)
                    LOG_MSG("External JSONRPC recovered after %d retry", failureCount);
                timer = 30;
                failureCount = 0;
                successCount++;
            }
        }
        else
            LOG_MSG("%s is not running", PROCESS_NAME);
        iterationCount++;
        sleep(timer);
    }
    curl_slist_free_all(headers);

    return 0;
}

