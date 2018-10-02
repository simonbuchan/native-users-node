// Code made with reference to https://github.com/mmraff/windows-users

#include <nan.h>
#include <Windows.h>
#include <lm.h>        // USER_INFO_xx and various #defines
#include <Sddl.h>      // ConvertSidToStringSid
#include <userenv.h>   // CreateProfile

#include <vector>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "userenv.lib")

struct string_value : v8::String::Value {
    explicit string_value(v8::Local<v8::Value> value)
#if NODE_MODULE_VERSION >= NODE_8_0_NODE_MODULE_VERSION
        : v8::String::Value(v8::Isolate::GetCurrent(), value) {}
#else
        : v8::String::Value(value) {}
#endif

    LPWSTR operator*() { return reinterpret_cast<LPWSTR>(this->v8::String::Value::operator *()); }
};

void ThrowWin32Error(LSTATUS status, const char* syscall) {
    Nan::ThrowError(node::WinapiErrnoException(v8::Isolate::GetCurrent(), status, syscall));;
}

v8::Local<v8::Object> new_str_obj(LPCWSTR value) {
    if (value == NULL) {
        return Nan::Null().As<v8::Object>();
    }
    return Nan::New<v8::String>((uint16_t*) value).ToLocalChecked().As<v8::Object>();
}

template <typename USER_INFO_level, int level>
struct net_user_info {
    USER_INFO_level* user = NULL;

    net_user_info() = default;

    ~net_user_info() { clear(); }

    auto get_info(LPCWSTR username) {
        return get_info(NULL, username);
    }

    auto get_info(LPCWSTR servername, LPCWSTR username) {
        clear();
        return NetUserGetInfo(servername, username, level, reinterpret_cast<LPBYTE*>(&user));
    }

    void clear() {
        if (user) {
            NetApiBufferFree(user);
            user = NULL;
        }
    }

    USER_INFO_level* operator->() { return user; }

    net_user_info(const net_user_info&) = delete;
    net_user_info& operator=(const net_user_info&) = delete;
};

#define NET_USER_INFO(level) net_user_info<USER_INFO_ ## level, level>

struct sid_string {
    LPWSTR str = NULL;

    sid_string() = default;

    ~sid_string() { clear(); }

    auto convert(PSID sid) {
        clear();
        return ConvertSidToStringSidW(sid, &str);
    }

    bool convert_or_js_throw(PSID sid) {
        if (!convert(sid)) {
            ThrowWin32Error(GetLastError(), "ConvertSidToStringSidW");
            return false;
        }
        return true;
    }

    void clear() {
        if (str) {
            LocalFree(str);
            str = NULL;
        }
    }

    sid_string(const sid_string&) = delete;
    sid_string& operator=(const sid_string&) = delete;
};

NAN_METHOD(get) {
    string_value name(info[0]);

    auto res = Nan::New<v8::Object>();
    info.GetReturnValue().Set(res);

#define SET(name, value) Nan::Set(res, new_str_obj(L ## #name), value)
#define SET_STR(name) SET(name, new_str_obj(user->usri23_##name))
#define SET_UINT(name) SET(name, Nan::New((uint32_t) user->usri23_##name).As<v8::Object>())

    NET_USER_INFO(23) user;

    auto nerr = user.get_info(*name);
    if (nerr == NERR_UserNotFound) {
        return info.GetReturnValue().SetNull();
    } else if (nerr != NERR_Success) {
        return ThrowWin32Error(nerr, "NetUserGetInfo");
    }

    SET_STR(name);
    SET_STR(full_name);
    SET_STR(comment);
    SET_UINT(flags);

    sid_string sid;
    if (!sid.convert_or_js_throw(user->usri23_user_sid)) {
        return;
    }

    SET(sid, new_str_obj(sid.str));
#undef SET_STR
#undef SET_UINT
#undef SET
}

NAN_METHOD(add) {
    string_value name(info[0]);
    string_value password(info[1]);
    auto flags = info[2]->Uint32Value();

    USER_INFO_1 user = {};

    user.usri1_name = *name;
    user.usri1_password = *password;
    user.usri1_priv = USER_PRIV_USER;
    user.usri1_flags = flags;

    auto nerr = NetUserAdd(
        NULL, // servername
        1, // level (X in USER_INFO_X)
        (LPBYTE) &user,
        NULL); // parm_err

    if (nerr == NERR_Success) {
        info.GetReturnValue().Set(true);
    } else if (nerr == NERR_UserExists) {
        info.GetReturnValue().Set(false);
    } else {
        return ThrowWin32Error(nerr, "NetUserAdd");
    }
}

NAN_METHOD(del) {
    string_value name(info[0]);

    auto nerr = NetUserDel(
        NULL, // servername
        *name);

    if (nerr == NERR_Success) {
        info.GetReturnValue().Set(true);
    } else if (nerr == NERR_UserNotFound) {
        info.GetReturnValue().Set(false);
    } else {
        return ThrowWin32Error(nerr, "NetUserDel");
    }
}

enum class user_status_e { not_exists, exists, other_error };

user_status_e get_user_sid(LPWSTR name, sid_string* sid) {
    NET_USER_INFO(23) user;

    auto status = user.get_info(name);
    if (status != NERR_Success) {
        if (status == NERR_UserNotFound) {
            return user_status_e::not_exists;
        }
        ThrowWin32Error(status, "NetUserGetInfo");
        return user_status_e::other_error;
    }

    if (!sid->convert_or_js_throw(user->usri23_user_sid)) {
        return user_status_e::other_error;
    }

    return user_status_e::exists;
}

NAN_METHOD(changePassword) {
    string_value name(info[0]);
    string_value oldPassword(info[1]);
    string_value newPassword(info[2]);

    auto nerr = NetUserChangePassword(
        NULL, // servername,
        *name,
        *oldPassword,
        *newPassword);

    if (nerr != NERR_Success) {
        return ThrowWin32Error(nerr, "NetUserChangePassword");
    }
}

NAN_METHOD(createProfile) {
    string_value name(info[0]);

    sid_string sid;
    switch (get_user_sid(*name, &sid)) {
        case user_status_e::other_error: return; // already called Nan::ThrowError()
        case user_status_e::not_exists:
            info.GetReturnValue().Set(false);
            return;
        case user_status_e::exists: break;
    }

    WCHAR profile_path[MAX_PATH];
    auto hr = CreateProfile(sid.str, *name, profile_path, MAX_PATH);

    if (SUCCEEDED(hr)) {
        info.GetReturnValue().Set(Nan::New((uint16_t*) profile_path).ToLocalChecked());
    } else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        info.GetReturnValue().SetNull();
    } else {
        return ThrowWin32Error(hr, "CreateProfile");
    }
}

NAN_METHOD(deleteProfile) {
    string_value name(info[0]);

    sid_string sid;
    switch (get_user_sid(*name, &sid)) {
        case user_status_e::other_error: return; // already called Nan::ThrowError()
        case user_status_e::not_exists:
            info.GetReturnValue().Set(false);
            return;
        case user_status_e::exists: break;
    }

    auto ok = DeleteProfileW(sid.str, NULL, NULL);

    if (ok) {
        info.GetReturnValue().Set(true);
    } else {
        auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            info.GetReturnValue().Set(false);
        } else {
            return ThrowWin32Error(error, "DeleteProfileW");
        }
    }
}

NAN_METHOD(logonUser) {
    string_value name(info[0]);
    string_value domain(info[1]);
    string_value password(info[2]);
    auto type = info[3]->Uint32Value();
    auto provider = info[4]->Uint32Value();

    HANDLE token;

    auto ok = LogonUserW(
        *name,
        *domain,
        *password,
        type,
        provider,
        &token);

    if (!ok) {
        return ThrowWin32Error(GetLastError(), "LogonUserW");
    }

    info.GetReturnValue().Set(Nan::New<v8::External>(token));
}

HANDLE get_handle(v8::Local<v8::Value> value) {
    if (!value->IsExternal()) {
        Nan::ThrowTypeError("'handle' should be an External returned from logonUser()");
        return NULL;
    }
    return value.As<v8::External>()->Value();
}

NAN_METHOD(closeHandle) {
    auto handle = get_handle(info[0]);
    if (!handle) return;

    auto ok = CloseHandle(handle);

    if (!ok) {
        return ThrowWin32Error(GetLastError(), "CloseHandle");
    }
}

NAN_METHOD(impersonateLoggedOnUser) {
    auto handle = get_handle(info[0]);
    if (!handle) return;

    auto ok = ImpersonateLoggedOnUser(handle);

    if (!ok) {
        return ThrowWin32Error(GetLastError(), "ImpersonateLoggedOnUser");
    }
}

NAN_METHOD(revertToSelf) {
    auto ok = RevertToSelf();

    if (!ok) {
        return ThrowWin32Error(GetLastError(), "RevertToSelf");
    }
}

NAN_METHOD(getUserProfileDirectory) {
    auto handle = get_handle(info[0]);
    if (!handle) return;

    DWORD size = 0;
    GetUserProfileDirectoryW(handle, NULL, &size);
    if (!size) {
       return ThrowWin32Error(GetLastError(), "GetUserProfileDirectoryW");
    }
    std::vector<WCHAR> data(size);
    if (!GetUserProfileDirectoryW(handle, data.data(), &size)) {
       return ThrowWin32Error(GetLastError(), "GetUserProfileDirectoryW");
    }

    auto str = Nan::New<v8::String>((uint16_t*) data.data()).ToLocalChecked();
    info.GetReturnValue().Set(str);
}

NAN_MODULE_INIT(Init) {
    NAN_EXPORT(target, get);
    NAN_EXPORT(target, add);
    NAN_EXPORT(target, del);
    NAN_EXPORT(target, changePassword);
    NAN_EXPORT(target, createProfile);
    NAN_EXPORT(target, deleteProfile);
    NAN_EXPORT(target, logonUser);
    NAN_EXPORT(target, closeHandle);
    NAN_EXPORT(target, impersonateLoggedOnUser);
    NAN_EXPORT(target, revertToSelf);
    NAN_EXPORT(target, getUserProfileDirectory);
}

NODE_MODULE(users, Init);
