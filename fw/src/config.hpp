// config.hpp
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <Preferences.h>

class Config {
private:
    Preferences preferences;
    static constexpr const char* NAMESPACE = "config";

public:
    void begin() {
        preferences.begin(NAMESPACE, false);
    }

    void load() {
        // 기본값들을 설정 (없으면 자동으로 기본값 사용)
        // 실제로는 get() 호출 시 기본값이 적용되므로 여기서는 특별한 작업 불필요
    }

    void save() {
        // NVS는 set() 호출 시 자동으로 저장되므로 별도 save() 불필요
        // 하지만 호환성을 위해 남겨둠
    }

    // 타입별 set 함수들
    void set(const char* key, const String& value) {
        preferences.putString(key, value);
    }
    
    void set(const char* key, const char* value) {
        preferences.putString(key, String(value));
    }
    
    void set(const char* key, int value) {
        preferences.putInt(key, value);
    }
    
    void set(const char* key, uint32_t value) {
        preferences.putUInt(key, value);
    }
    
    void set(const char* key, bool value) {
        preferences.putBool(key, value);
    }
    
    void set(const char* key, float value) {
        preferences.putFloat(key, value);
    }
    
    void set(const char* key, double value) {
        preferences.putDouble(key, value);
    }

    // 타입별 get 함수들
    String getString(const char* key, const String& defaultValue = "") {
        return preferences.getString(key, defaultValue);
    }
    
    int getInt(const char* key, int defaultValue = 0) {
        return preferences.getInt(key, defaultValue);
    }
    
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
        return preferences.getUInt(key, defaultValue);
    }
    
    bool getBool(const char* key, bool defaultValue = false) {
        return preferences.getBool(key, defaultValue);
    }
    
    float getFloat(const char* key, float defaultValue = 0.0f) {
        return preferences.getFloat(key, defaultValue);
    }
    
    double getDouble(const char* key, double defaultValue = 0.0) {
        return preferences.getDouble(key, defaultValue);
    }

    // 템플릿 함수 (하위 호환성을 위해 유지)
    template <typename T>
    T get(const char* key, T defaultValue = T()) {
        // 컴파일러가 타입을 추론할 수 있도록 명시적 특수화 사용
        return getTypedValue<T>(key, defaultValue);
    }

private:
    // 내부 템플릿 함수들
    template <typename T>
    T getTypedValue(const char* key, T defaultValue);
    
public:

    bool hasKey(const char* key) {
        return preferences.isKey(key);
    }

    String dump() {
        // 간단한 key=value 형태로 덤프 (const 제거)
        String result = "Config dump:\n";
        result += "password=" + preferences.getString("password", "1111") + "\n";
        result += "debounceDelay=" + String(preferences.getUInt("debounceDelay", 50)) + "\n";
        result += "maxAmmoCount=" + String(preferences.getInt("maxAmmoCount", 30)) + "\n";
        result += "currentAmmo=" + String(preferences.getInt("currentAmmo", 30)) + "\n";
        return result;
    }

    void clear() {
        preferences.clear();
    }

    // 설정 초기화 (기본값 설정)
    void initDefaults() {
        if (!hasKey("password")) {
            set("password", "1111");
        }
        if (!hasKey("debounceDelay")) {
            set("debounceDelay", uint32_t(50));
        }
        if (!hasKey("maxAmmoCount")) {
            set("maxAmmoCount", 30);
        }
        if (!hasKey("currentAmmo")) {
            set("currentAmmo", 30);
        }
    }
};

// 템플릿 특수화 정의
template <>
inline String Config::getTypedValue<String>(const char* key, String defaultValue) {
    return getString(key, defaultValue);
}

template <>
inline int Config::getTypedValue<int>(const char* key, int defaultValue) {
    return getInt(key, defaultValue);
}

template <>
inline uint32_t Config::getTypedValue<uint32_t>(const char* key, uint32_t defaultValue) {
    return getUInt(key, defaultValue);
}

template <>
inline bool Config::getTypedValue<bool>(const char* key, bool defaultValue) {
    return getBool(key, defaultValue);
}

template <>
inline float Config::getTypedValue<float>(const char* key, float defaultValue) {
    return getFloat(key, defaultValue);
}

template <>
inline double Config::getTypedValue<double>(const char* key, double defaultValue) {
    return getDouble(key, defaultValue);
};

#endif // CONFIG_HPP