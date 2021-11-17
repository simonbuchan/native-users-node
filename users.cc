// Code made with reference to https://github.com/mmraff/windows-users
#include <iomanip>
#include <iostream>
#include <sstream>
#include <napi.h>

#include <Windows.h>
#include <tchar.h>
#include <lm.h>        // USER_INFO_xx and various #defines
#include <Sddl.h>      // ConvertSidToStringSid
#include <userenv.h>   // CreateProfile

#include <codecvt>
#include <vector>
#include <fstream>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "userenv.lib")

using namespace Napi;

std::wstring to_wstring(Value value) {
    size_t length;
    napi_status status = napi_get_value_string_utf16(
        value.Env(),
        value,
        nullptr,
        0,
        &length);
    NAPI_THROW_IF_FAILED_VOID(value.Env(), status);

    std::wstring result;
    result.reserve(length + 1);
    result.resize(length);
    status = napi_get_value_string_utf16(
        value.Env(),
        value,
        reinterpret_cast<char16_t*>(result.data()),
        result.capacity(),
        nullptr);
    NAPI_THROW_IF_FAILED_VOID(value.Env(), status);
    return result;
}

Value to_value(Env env, std::wstring_view str) {
    return String::New(
        env,
        reinterpret_cast<const char16_t*>(str.data()),
        str.size());
}

// like unique_ptr, but takes a deleter value, not type.
template <typename T, auto Deleter>
struct Ptr {
    T* value = NULL;

    Ptr() = default;
    // No copies
    Ptr(const Ptr&) = delete;
    Ptr& operator=(const Ptr&) = delete;
    // Moves
    Ptr(Ptr&& other) : value{ other.release() } {}
    Ptr& operator=(Ptr&& other) {
        assign(other.release());
    }

    ~Ptr() { clear(); }

    operator T*() const { return value; }
    T* operator ->() const { return value; }

    T* assign(T* newValue) {
        clear();
        return value = newValue;
    }

    T* release() {
        auto result = value;
        value = nullptr;
        return result;
    }

    void clear() {
        if (value) {
            Deleter(value);
            value = nullptr;
        }
    }
};

template <typename T>
using Win32Local = Ptr<T, LocalFree>;

std::wstring formatSystemError(HRESULT hr) {
    Win32Local<WCHAR> message_ptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        LANG_USER_DEFAULT,
        (LPWSTR)&message_ptr,
        0,
        nullptr);

    if (!message_ptr) {
        return L"Unknown Error";
    }

    std::wstring message(message_ptr);

    // Windows ends it's system messages with "\r\n", which is bad formatting for us.
    if (auto last_not_newline = message.find_last_not_of(L"\r\n");
        last_not_newline != std::wstring::npos) {
        message.erase(last_not_newline + 1);
    }

    return message;
}

Error createWindowsError(napi_env env, HRESULT hr, const char* syscall) {
    napi_value error_value = nullptr;

    napi_status status = napi_create_error(
        env,
        nullptr,
        to_value(env, formatSystemError(hr)),
        &error_value);
    if (status != napi_ok) {
        throw Error::New(env);
    }

    auto error = Error(env, error_value);
    error.Value().DefineProperties({
        PropertyDescriptor::Value("errno", Number::New(env, hr)),
        PropertyDescriptor::Value("name", String::New(env, "WindowsError")),
        PropertyDescriptor::Value("syscall", String::New(env, syscall)),
    });
    return error;
}

template <typename USER_INFO_level, int level>
struct NetUserInfo : Ptr<USER_INFO_level, NetApiBufferFree> {
    bool get(Env env, LPCWSTR username) {
        return get(env, nullptr, username);
    }

    bool get(Env env, LPCWSTR servername, LPCWSTR username) {
        clear();
        auto nerr = NetUserGetInfo(servername, username, level, reinterpret_cast<LPBYTE*>(&value));
        if (nerr == NERR_UserNotFound) {
            return false;
        }
        if (nerr != NERR_Success) {
            throw createWindowsError(env, GetLastError(), "NetUserGetInfo");
        }
        return true;
    }
};

#define NET_USER_INFO(level) NetUserInfo<USER_INFO_ ## level, level>

Win32Local<WCHAR> sid_to_local_string(Env env, PSID sid) {
    Ptr<WCHAR, LocalFree> local;
    if (!ConvertSidToStringSidW(sid, &local.value)) {
        throw createWindowsError(env, GetLastError(), "ConvertSidToStringSidW");
    }
    return local;
}

Value sid_to_value(Env env, PSID sid) {
    auto local = sid_to_local_string(env, sid);
    return to_value(env, local.value);
}

Value get(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);

    NET_USER_INFO(23) user_info;

    if (!user_info.get(env, name.c_str())) {
        return env.Null();
    }

    auto res = Object::New(env);
    res.DefineProperties({
        PropertyDescriptor::Value("name", to_value(env, user_info->usri23_name), napi_enumerable),
        PropertyDescriptor::Value("full_name", to_value(env, user_info->usri23_full_name), napi_enumerable),
        PropertyDescriptor::Value("comment", to_value(env, user_info->usri23_comment), napi_enumerable),
        PropertyDescriptor::Value("flags", Number::New(env, user_info->usri23_flags), napi_enumerable),
        PropertyDescriptor::Value("sid", sid_to_value(env, user_info->usri23_user_sid), napi_enumerable),
    });
    return res;
}

Value add(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);
    auto password = to_wstring(info[1]);
    auto flags = info[2].As<Number>().Uint32Value();

    USER_INFO_1 user = {};

    // The strings are not const, so using .data() instead of .c_str().
    // If this surprises you, basic_string::data() was made non-const in C++17.
    user.usri1_name = name.data();
    user.usri1_password = password.data();
    user.usri1_priv = USER_PRIV_USER;
    user.usri1_flags = flags;

    auto nerr = NetUserAdd(
        NULL, // servername
        1, // level (X in USER_INFO_X)
        (LPBYTE) &user,
        NULL); // parm_err

    if (nerr == NERR_Success) {
        return Boolean::New(env, true);
    } else if (nerr == NERR_UserExists) {
        return Boolean::New(env, false);
    } else {
        throw createWindowsError(env, nerr, "NetUserAdd");
    }
}

Value del(CallbackInfo const& info) {
    auto env = info.Env();
    
    auto name = to_wstring(info[0]);

    auto nerr = NetUserDel(
        NULL, // servername
        name.c_str());

    if (nerr == NERR_Success) {
        return Boolean::New(env, true);
    } else if (nerr == NERR_UserNotFound) {
        return Boolean::New(env, false);
    } else {
        throw createWindowsError(env, nerr, "NetUserAdd");
    }
}

Value createProfile(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);

    NET_USER_INFO(23) user_info;

    if (!user_info.get(env, name.c_str())) {
        // User does not exist
        return Boolean::New(env, false);
    }

    auto sid_local = sid_to_local_string(env, user_info->usri23_user_sid);

    WCHAR profile_path[MAX_PATH];
    auto hr = CreateProfile(sid_local, name.c_str(), profile_path, MAX_PATH);

    if (SUCCEEDED(hr)) {
        return to_value(env, profile_path);
    } else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        return env.Null();
    } else {
        throw createWindowsError(env, hr, "CreateProfile");
    }
}

Value deleteProfile(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);

    NET_USER_INFO(23) user_info;

    if (!user_info.get(env, name.c_str())) {
        // User does not exist
        return Boolean::New(env, false);
    }

    auto sid_local = sid_to_local_string(env, user_info->usri23_user_sid);

    if (!DeleteProfileW(sid_local.value, NULL, NULL)) {
        auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return Boolean::New(env, false);
        } else {
            throw createWindowsError(env, error, "DeleteProfileW");
        }
    } else {
        return Boolean::New(env, true);
    }
}

void changePassword(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);
    auto oldPassword = to_wstring(info[1]);
    auto newPassword = to_wstring(info[2]);

    auto nerr = NetUserChangePassword(
        NULL, // servername,
        name.c_str(),
        oldPassword.c_str(),
        newPassword.c_str());

    if (nerr != NERR_Success) {
        throw createWindowsError(env, nerr, "NetUserChangePassword");
    }
}

#define NET_USER_SET_INFO(level, name, ...) {       \
    USER_INFO_##level user_info { __VA_ARGS__ };    \
    auto nerr = NetUserSetInfo(                     \
        nullptr,                                    \
        name,                                       \
        level,                                      \
        (LPBYTE) &info,                             \
        nullptr);                                   \
    if (nerr != NERR_Success) {                     \
        throw createWindowsError(env, nerr, "NetUserSetInfo"); \
    }                                               \
}

void set(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);
    auto options = info[1].As<Object>();

    if (auto value = options.Get("full_name");
        !value.IsEmpty() && !value.IsUndefined()) {
        auto full_name = to_wstring(value);
        NET_USER_SET_INFO(1011, name.c_str(), full_name.data());
    }

    if (auto value = options.Get("flags");
        !value.IsEmpty() && !value.IsUndefined()) {
        auto flags = value.As<Number>().Uint32Value();
        NET_USER_SET_INFO(1008, name.c_str(), flags);
    }
}

std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

Value logonUser(CallbackInfo const& info) {
    auto env = info.Env();

    /*auto name = info[0].As<Napi::String>().Utf8Value();
    auto domain = info[1].As<Napi::String>().Utf8Value();
    auto password = info[2].As<Napi::String>().Utf8Value();*/
    auto name = to_wstring(info[0]);
    auto domain = to_wstring(info[1]);
    auto password = to_wstring(info[2]);
    auto type = info[3].As<Number>().Uint32Value();
    auto provider = info[4].As<Number>().Uint32Value();

    HANDLE token;

    bool ok = LogonUserW(
        name.c_str(),
        domain.c_str(),
        password.c_str(),
        /*s2ws(name).c_str(),
        s2ws(domain).c_str(),
        s2ws(password).c_str(),*/
        type,
        provider,
        &token);
	
    if (!ok) {
        throw createWindowsError(env, GetLastError(), "LogonUserW");
    }

    return External<void>::New(env, token, [](Env env, HANDLE handle) {
        CloseHandle(handle);
    });
}

HANDLE get_handle(Env env, Value value) {
    if (!value.IsExternal()) {
        throw TypeError::New(env, "'handle' should be an External returned from logonUser()");
    }

    return value.As<External<void>>().Data();
}

void closeHandle(CallbackInfo const& info) {
    auto env = info.Env();

    auto handle = get_handle(env, info[0]);

    if (!CloseHandle(handle)) {
        throw createWindowsError(env, GetLastError(), "CloseHandle");
    }
}

void* s2p(std::string& s) {
  void *result;
  // remove 0x
  std::string str = s.substr(2);
  std::istringstream c(str);
  c >> std::hex >> result;
  return result;
}

std::string p2s(void *ptr) {
  std::stringstream s;
  s << "0x" << std::setfill('0') << std::setw(sizeof(ULONG_PTR) * 2) << std::hex
     << ptr;
  std::string result = s.str();
  return result;
}


void testWrite() {
    // Create and open a text file
    std::ofstream MyFile("ImpersonationTest.txt");

    // Write to the file
    MyFile << "Check the owner of this file! Is the impersonated user?";

    // Close the file
    MyFile.close();
}


Value impersonateLoggedOnUserSSPI(CallbackInfo const& info) {
    auto env = info.Env();
    try
    {
        // return info[0];

        HANDLE token =
            s2p(info[0].As<Napi::String>().Utf8Value());

        Value ret_handle = External<void>::New(env, token, [](Env env, HANDLE handle) {
                CloseHandle(handle);
            });
			
		HANDLE userToken;

    	DWORD flags = MAXIMUM_ALLOWED; //not only TOKEN_QUERY | TOKEN_QUERY_SOURCE;
		std::cout << "test OpenThreadToken";
		BOOL statusOpen = OpenThreadToken(GetCurrentThread(), flags, TRUE, &userToken);
		if (statusOpen == FALSE) {
            return Napi::String::New(env, createWindowsError(env, GetLastError(), "OpenThreadToken").Message());
		}
		std::cout << "test duplicatedToken";
		HANDLE duplicatedToken;
		BOOL statusDupl = DuplicateTokenEx(userToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &duplicatedToken);
		if (statusDupl == FALSE) {
            return Napi::String::New(env, createWindowsError(env, GetLastError(), "DuplicateTokenEx").Message());
		}
		std::cout << "test ImpersonateLoggedOnUser";
        if (!ImpersonateLoggedOnUser(duplicatedToken)) {
            return Napi::String::New(env, createWindowsError(env, GetLastError(), "ImpersonateLoggedOnUser").Message());
        }

        testWrite();

        //std::string str = p2s(handle);
        //return Napi::String::New(env, str);
        return Napi::String::New(env, "original external version"); // ret_handle;
    }
    catch (const std::exception& exc)
    {
        return Napi::String::New(env, exc.what());
    }

}

Value impersonateLoggedOnUser(CallbackInfo const& info) {
    auto env = info.Env();
    try
    {
        // return info[0];

        /*HANDLE token =
            s2p(info[0].As<Napi::String>().Utf8Value());

        Value ret_handle = External<void>::New(env, token, [](Env env, HANDLE handle) {
                CloseHandle(handle);
            });*/

        auto handle = get_handle(env,
            info[0] //ret_handle
        );

        if (!ImpersonateLoggedOnUser(handle)) {
            return Napi::String::New(env, createWindowsError(env, GetLastError(), "ImpersonateLoggedOnUser").Message());
        }

        testWrite();

        //std::string str = p2s(handle);
        //return Napi::String::New(env, str);
        return Napi::String::New(env, "original external version"); // ret_handle;
    }
    catch (const std::exception& exc)
    {
        return Napi::String::New(env, exc.what());
    }

}

void revertToSelf(CallbackInfo const& info) {
    auto env = info.Env();

    if (!RevertToSelf()) {
        throw createWindowsError(env, GetLastError(), "RevertToSelf");
    }
}

Value getUserProfileDirectory(CallbackInfo const& info) {
    auto env = info.Env();

    auto handle = get_handle(env, info[0]);

    DWORD size = 0;
    GetUserProfileDirectoryW(handle, NULL, &size);
    if (!size) {
       throw createWindowsError(env, GetLastError(), "GetUserProfileDirectoryW");
    }

    std::wstring data;
    data.reserve(size + 1);
    data.resize(size);
    if (!GetUserProfileDirectoryW(handle, data.data(), &size)) {
       throw createWindowsError(env, GetLastError(), "GetUserProfileDirectoryW");
    }

    return to_value(env, data);
}

#define EXPORT_FUNCTION(name) \
    PropertyDescriptor::Function(env, exports, #name, name, napi_enumerable)

Object module_init(Env env, Object exports) {
    exports.DefineProperties({
        EXPORT_FUNCTION(get),
        EXPORT_FUNCTION(add),
        EXPORT_FUNCTION(del),
        EXPORT_FUNCTION(createProfile),
        EXPORT_FUNCTION(deleteProfile),
        EXPORT_FUNCTION(changePassword),
        EXPORT_FUNCTION(set),
        EXPORT_FUNCTION(logonUser),
        EXPORT_FUNCTION(closeHandle),
        EXPORT_FUNCTION(impersonateLoggedOnUser),
        EXPORT_FUNCTION(impersonateLoggedOnUserSSPI),
        EXPORT_FUNCTION(revertToSelf),
        EXPORT_FUNCTION(getUserProfileDirectory),
    });
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, module_init);
