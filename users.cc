// Code made with reference to https://github.com/mmraff/windows-users

#include <nan.h>
#include <Windows.h>
#include <lm.h>        // USER_INFO_xx and various #defines
#include <Sddl.h>      // ConvertSidToStringSid

#include <vector>

#pragma comment(lib, "netapi32.lib")

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
    auto user_name = get_ucs2(info[0]);

    LPBYTE pbuf = NULL;

    auto status = NetUserGetInfo(
        NULL, // servername
        user_name.data(),
        23, // info level (X in USER_INFO_X)
        &pbuf);
    if (status != ERROR_SUCCESS) {
        return ThrowWin32Error(status, "NetUserGetInfo");
    }

    auto user = (USER_INFO_23*) pbuf;

    auto res = Nan::New<v8::Object>();

#define NEW_VALUE(arg) Nan::New(arg).ToLocalChecked().As<v8::Object>()
#define SET(name, value) Nan::Set(res, NEW_VALUE(#name), value);
#define SET_STR(name) SET(name, NEW_VALUE((uint16_t*) user->usri23_##name))

    SET_STR(name);
    SET_STR(full_name);
    SET_STR(comment);
    SET(flags, Nan::New((uint32_t)user->usri23_flags));

    LPWSTR sid = NULL;
    if (!ConvertSidToStringSidW(user->usri23_user_sid, &sid)) {
        NetApiBufferFree(pbuf);
        return ThrowWin32Error(GetLastError(), "ConvertSidToStringSidW");
    }

    SET(sid, NEW_VALUE((uint16_t*)sid));
#undef NEW_VALUE
#undef SET
#undef SET_STR

    LocalFree(sid);
    NetApiBufferFree(pbuf);

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

    auto status = NetUserAdd(
        NULL, // servername
        1, // level (X in USER_INFO_X)
        (LPBYTE) &user,
        NULL); // parm_err
    if (status != ERROR_SUCCESS) {
        return ThrowWin32Error(status, "NetUserAdd");
    }
}

NAN_METHOD(del) {
    auto name = get_ucs2(info[0]);

    auto status = NetUserDel(
        NULL, // servername
        name.data());

    if (status != ERROR_SUCCESS) {
        return ThrowWin32Error(status, "NetUserAdd");
    }
}

NAN_MODULE_INIT(Init) {
    NAN_EXPORT(target, get);
    NAN_EXPORT(target, add);
    NAN_EXPORT(target, del);
}

NODE_MODULE(users, Init);
