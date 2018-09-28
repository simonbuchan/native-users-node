// Code made with reference to https://github.com/mmraff/windows-users

#include <nan.h>
#include <Windows.h>
#include <lm.h>        // USER_INFO_xx and various #defines
#include <Sddl.h>      // ConvertSidToStringSid
#include <userenv.h>   // CreateProfile

#include <vector>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "userenv.lib")

auto get_ucs2(v8::Local<v8::Value> value) {
    auto encoding = Nan::Encoding::UCS2;
    auto size = Nan::DecodeBytes(value, encoding);
    std::vector<WCHAR> data(size / 2 + 1);
    Nan::DecodeWrite((char*) data.data(), size, value, encoding);
    return data;
}

void ThrowWin32Error(LSTATUS status, const char* syscall) {
    Nan::ThrowError(node::WinapiErrnoException(v8::Isolate::GetCurrent(), status, syscall));;
}

NAN_METHOD(get) {
    auto name = get_ucs2(info[0]);

    LPBYTE pbuf = NULL;

    auto nerr = NetUserGetInfo(
        NULL, // servername
        name.data(),
        23, // info level (X in USER_INFO_X)
        &pbuf);

    if (nerr == NERR_UserNotFound) {
        return info.GetReturnValue().SetNull();
    } else if (nerr != NERR_Success) {
        return ThrowWin32Error(nerr, "NetUserGetInfo");
    }

    auto user = (USER_INFO_23*) pbuf;

    auto res = Nan::New<v8::Object>();

#define NEW_VALUE(arg) Nan::New(arg).ToLocalChecked().As<v8::Object>()
#define SET(name, value) Nan::Set(res, NEW_VALUE(#name), value);
#define SET_STR(name) SET(name, NEW_VALUE((uint16_t*) user->usri23_##name))
    SET_STR(name);
    SET_STR(full_name);
    SET_STR(comment);
#undef SET_STR
    SET(flags, Nan::New((uint32_t)user->usri23_flags));

    LPWSTR sid = NULL;
    auto sid_ok = ConvertSidToStringSidW(user->usri23_user_sid, &sid);
    NetApiBufferFree(pbuf);

    if (!sid_ok) {
        return ThrowWin32Error(GetLastError(), "ConvertSidToStringSidW");
    }

    SET(sid, NEW_VALUE((uint16_t*)sid));
#undef NEW_VALUE
#undef SET

    LocalFree(sid);

    info.GetReturnValue().Set(res);
}

NAN_METHOD(add) {
    auto name = get_ucs2(info[0]);
    auto password = get_ucs2(info[1]);
    auto flags = info[2]->Uint32Value();

    USER_INFO_1 user = {};

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
        info.GetReturnValue().Set(true);
    } else if (nerr == NERR_UserExists) {
        info.GetReturnValue().Set(false);
    } else {
        return ThrowWin32Error(nerr, "NetUserAdd");
    }
}

NAN_METHOD(del) {
    auto name = get_ucs2(info[0]);

    auto nerr = NetUserDel(
        NULL, // servername
        name.data());

    if (nerr == NERR_Success) {
        info.GetReturnValue().Set(true);
    } else if (nerr == NERR_UserNotFound) {
        info.GetReturnValue().Set(false);
    } else {
        return ThrowWin32Error(nerr, "NetUserDel");
    }
}

enum class user_status_e { not_exists, exists, other_error };

user_status_e get_user_sid(LPWSTR name, LPWSTR* psid) {
    LPBYTE pbuf = NULL;

    auto status = NetUserGetInfo(
        NULL, // servername
        name,
        23, // info level (X in USER_INFO_X)
        &pbuf);
    if (status != NERR_Success) {
        if (status == NERR_UserNotFound) {
            return user_status_e::not_exists;
        }
        ThrowWin32Error(status, "NetUserGetInfo");
        return user_status_e::other_error;
    }

    auto user = (USER_INFO_23*) pbuf;

    LPWSTR sid = NULL;
    auto ok = ConvertSidToStringSidW(user->usri23_user_sid, psid);
    NetApiBufferFree(pbuf);

    if (!ok) {
        ThrowWin32Error(GetLastError(), "ConvertSidToStringSidW");
        return user_status_e::other_error;
    }

    return user_status_e::exists;
}

NAN_METHOD(createProfile) {
    auto name = get_ucs2(info[0]);

    LPWSTR sid = NULL;
    switch (get_user_sid(name.data(), &sid)) {
        case user_status_e::other_error: return; // already called Nan::ThrowError()
        case user_status_e::not_exists:
            info.GetReturnValue().Set(false);
            return;
        case user_status_e::exists: break;
    }

    WCHAR profile_path[MAX_PATH];
    auto hr = CreateProfile(sid, name.data(), profile_path, MAX_PATH);
    LocalFree(sid);

    if (SUCCEEDED(hr)) {
        info.GetReturnValue().Set(true);
    } else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        info.GetReturnValue().Set(false);
    } else {
        return ThrowWin32Error(hr, "CreateProfile");
    }
}

NAN_METHOD(deleteProfile) {
    auto name = get_ucs2(info[0]);

    LPWSTR sid = NULL;
    switch (get_user_sid(name.data(), &sid)) {
        case user_status_e::other_error: return; // already called Nan::ThrowError()
        case user_status_e::not_exists:
            info.GetReturnValue().Set(false);
            return;
        case user_status_e::exists: break;
    }

    auto ok = DeleteProfileW(sid, NULL, NULL);
    LocalFree(sid);

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

NAN_MODULE_INIT(Init) {
    NAN_EXPORT(target, get);
    NAN_EXPORT(target, add);
    NAN_EXPORT(target, del);
    NAN_EXPORT(target, createProfile);
    NAN_EXPORT(target, deleteProfile);
}

NODE_MODULE(users, Init);
