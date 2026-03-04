#ifndef PARAMS_HPP
#define PARAMS_HPP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

namespace utils
{

class Params
{
public:
    static constexpr int MAX_PARAMS = 512;
    static constexpr int MAX_KEY_LEN = 32;
    static constexpr int MAX_VAL_LEN = 256;
    static constexpr int MAX_PATH_LEN = 256;
    static constexpr int MAX_LINE_LEN = MAX_KEY_LEN + MAX_VAL_LEN + 32; // Padding for spaces, =, newline

    Params(const char* filePath, const char* defaultParams)
        : m_count(0)
    {
        strncpy(m_filePath, filePath, MAX_PATH_LEN - 1);
        m_filePath[MAX_PATH_LEN - 1] = '\0';

        // Parse defaults to populate keys
        parseAndSet(defaultParams, true);

        // Load from file to override defaults
        load();
    }

    void load()
    {
        FILE* f = fopen(m_filePath, "r");
        if (!f)
        {
            // File doesn't exist, create it with current defaults
            save();
            return;
        }

        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), f))
        {
            parseLine(line, false);
        }
        fclose(f);
    }

    void save()
    {
        FILE* f = fopen(m_filePath, "w");
        if (!f)
        {
            fprintf(stderr, "Failed to open config file for writing: %s\n", m_filePath);
            return;
        }

        for (int i = 0; i < m_count; ++i)
        {
            fprintf(f, "%s=%s\n", m_keys[i], m_values[i]);
        }
        fclose(f);
    }

    void update(const char* newParams)
    {
        parseAndSet(newParams, false);
        save();
    }

    const char* get(const char* key)
    {
        for (int i = 0; i < m_count; ++i)
        {
            if (strcmp(m_keys[i], key) == 0)
            {
                return m_values[i];
            }
        }
        return nullptr;
    }

    int getInt(const char* key, int defaultVal)
    {
        const char* val = get(key);
        if (val)
        {
            return atoi(val);
        }
        return defaultVal;
    }

private:
    char m_filePath[MAX_PATH_LEN];
    char m_keys[MAX_PARAMS][MAX_KEY_LEN];
    char m_values[MAX_PARAMS][MAX_VAL_LEN];
    int m_count;

    void parseAndSet(const char* input, bool createNew)
    {
        if (!input) return;

        const char* ptr = input;
        char lineBuffer[MAX_LINE_LEN];

        while (*ptr)
        {
            // Find end of line
            const char* end = strchr(ptr, '\n');
            size_t len = end ? (size_t)(end - ptr) : strlen(ptr);

            if (len >= sizeof(lineBuffer)) len = sizeof(lineBuffer) - 1;

            memcpy(lineBuffer, ptr, len);
            lineBuffer[len] = '\0';

            parseLine(lineBuffer, createNew);

            if (!end) break;
            ptr = end + 1;
        }
    }

    void parseLine(char* line, bool createNew)
    {
        char* eq = strchr(line, '=');
        if (!eq) return;

        *eq = '\0';
        char* key = trim(line);
        char* val = trim(eq + 1);

        // Handle quotes
        if (val[0] == '"')
        {
            size_t len = strlen(val);
            if (len > 1 && val[len - 1] == '"')
            {
                val[len - 1] = '\0';
                val++;
            }
        }

        if (strlen(key) == 0) return;

        // Find existing
        int idx = -1;
        for (int i = 0; i < m_count; ++i)
        {
            if (strcmp(m_keys[i], key) == 0)
            {
                idx = i;
                break;
            }
        }

        if (idx >= 0)
        {
            // Update existing
            strncpy(m_values[idx], val, MAX_VAL_LEN - 1);
            m_values[idx][MAX_VAL_LEN - 1] = '\0';
        }
        else if (createNew)
        {
            // Add new
            if (m_count < MAX_PARAMS)
            {
                strncpy(m_keys[m_count], key, MAX_KEY_LEN - 1);
                m_keys[m_count][MAX_KEY_LEN - 1] = '\0';

                strncpy(m_values[m_count], val, MAX_VAL_LEN - 1);
                m_values[m_count][MAX_VAL_LEN - 1] = '\0';

                m_count++;
            }
            else
            {
                fprintf(stderr, "Max params limit reached\n");
            }
        }
        else
        {
            fprintf(stderr, "Unknown parameter: %s\n", key);
        }
    }

    char* trim(char* str)
    {
        while (isspace((unsigned char)*str)) str++;
        if (*str == 0) return str;

        char* end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end)) end--;
        *(end + 1) = 0;

        return str;
    }
};

} // namespace utils

#endif // PARAMS_HPP
