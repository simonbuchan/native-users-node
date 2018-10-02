# `@simonbuchan/users`

Node addon package for exposing native Windows user management APIs.

Not terribly complete as of yet, for example, it does not support listing, groups, updating existing users,
or accessing global options. Feature requests welcome!

If you can think of a good package name that isn't taken or ugly, I'm listening! 😉

## APIs

These APIs are described using their TypeScript definition. You might need to know:
- `UserInfo | null` means a value that can match either the type `UserInfo` or `null`, but not, e.g., `undefined`.
- `interface UserInfo { ... }` describes any value that has the properties described. As a type, it does not have
  any runtime representation (that is, importing `UserInfo` will get you `undefined` in JavaScript)
- `enum LogonType { ... }` implements a two-way mapping between string names and number values, that is, for
  any member in the enum, for example `INTERACTIVE = 2`, `LogonType.INTERACTIVE` gives `2`, and `LogonType[2]` gives
  `"INTERACTIVE"`.

### `get`

Wraps [`NetGetUserInfo`](https://docs.microsoft.com/en-nz/windows/desktop/api/lmaccess/nf-lmaccess-netusergetinfo)
using [`USER_INFO_23`](https://docs.microsoft.com/en-us/windows/desktop/api/lmaccess/ns-lmaccess-_user_info_23)

This level contains most of the relevant metadata that is still exposed by current Windows.

Returns `null` if the user does not exist.

```ts
export function get(name: string): UserInfo | null;

export interface UserInfo {
  name: string;
  full_name: string;
  comment: string;
  flags: Flags;
  sid: string;
}
```

### `add`

Wraps [`NetUserAdd`](https://docs.microsoft.com/en-nz/windows/desktop/api/lmaccess/nf-lmaccess-netuseradd)
with some minimal options.

This is similar to `net user add` on the command line, but also allows
setting certain flags such as `Flags.DONT_EXPIRE_PASSWORD`.

If the user already existed, this will return `false`, and may have
a different password or flags than provided here.

This does not also create the user's profile (e.g. `C:\Users\some-user`),
see [`createProfile`](#createprofile).

```ts
export function add(
    name: string,
    password: string,
    flags: Flags = 0,
): boolean;
```

### `del`

Wraps [`NetUserDel`](https://docs.microsoft.com/en-nz/windows/desktop/api/lmaccess/nf-lmaccess-netuserdel)

Returns `false` if the user did not exist previously.

```ts
export function del(name: string): boolean;
```

### `createProfile`

Wraps [`CreateProfile`](https://docs.microsoft.com/en-us/windows/desktop/api/userenv/nf-userenv-createprofile)

Returns the path to the newly created profile, or `null` if the profile already exists.

```ts
export function createProfile(name: string): string | null;
```

### `deleteProfile`

Wraps [`DeleteProfileW`](https://docs.microsoft.com/en-us/windows/desktop/api/userenv/nf-userenv-deleteprofilew)

Returns `false` if the user does not exist or has a profile. You must call this before [`del`](#del) to
successfully clean up the profile for an existing user.

```ts
export function deleteProfile(name: string): boolean;
```

### `changePassword`

Wraps [`NetUserChangePassword`](https://docs.microsoft.com/en-nz/windows/desktop/api/lmaccess/nf-lmaccess-netuserchangepassword)

```ts
export function changePassword(
  name: string,
  oldPassword: string,
  newPassword: string,
): void;
```

### `logonUser`

Wraps [`LogonUserW`](https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-logonuserw)

> The LogonUser function attempts to log a user on to the local computer.

Returns an `External` value for the granted access token. Use [`closeHandle`](#closehandle) when you no longer need
the token to clean up.

```ts
export function logonUser(
  name: string,
  password: string,
  domain: string = ".",
  type: LogonType = LogonType.NETWORK,
  provider: LogonProvider = LogonProvider.DEFAULT,
): unknown;

export enum LogonType {
  INTERACTIVE = 2,
  NETWORK = 3,
  BATCH = 4,
  SERVICE = 5,
  UNLOCK = 7,
  NETWORK_CLEARTEXT = 8,
  NEW_CREDENTIALS = 9,
}

export enum LogonProvider {
  DEFAULT = 0,
  WINNT35 = 1,
  WINNT40 = 2,
  WINNT50 = 3,
  VIRTUAL = 4,
}
```

### `impersonateLoggedOnUser`

Wraps [`ImpersonateLoggedOnUser`](https://docs.microsoft.com/en-us/windows/desktop/api/securitybaseapi/nf-securitybaseapi-impersonateloggedonuser)

> The ImpersonateLoggedOnUser function lets the calling thread impersonate the security context of a logged-on user.

```ts
export function impersonateLoggedOnUser(handle: unknown): void;
```

### `revertToSelf`

Wraps [`RevertToSelf`](https://docs.microsoft.com/en-us/windows/desktop/api/securitybaseapi/nf-securitybaseapi-reverttoself)

> The RevertToSelf function terminates the impersonation of a client application.

```ts
export function revertToSelf(): void;
```

### `getUserProfileDirectory`

Wraps [`GetUserProfileDirectoryW`](https://docs.microsoft.com/en-us/windows/desktop/api/userenv/nf-userenv-getuserprofiledirectoryw)

> Retrieves the path to the root directory of the specified user's profile.

```ts
export function getUserProfileDirectory(handle: unknown): string;
```

### `closeHandle`

Wraps [`CloseHandle`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms724211.aspx)

> Closes an open object handle.

Closes handles returned by [`logonUser`](#logonuser), or many other Windows APIs, so long as they are wrapped
in an `External`.

```ts
export function closeHandle(handle: unknown): void;
```

## Flags

The `UF_*` constants used for user flags exposed as a TypeScript enum.

Multiple values can be combined with the `|` (bitwise or) operator, for example
`Flags.NORMAL_ACCOUNT | Flags.DONT_EXPIRE_PASSWORD`.

This is used as a parameter to [`add`](#add) and returned in the `flags` property of the result of [`get`](#get).

Check the documentation for the flags member of [`USER_INFO_23`](https://docs.microsoft.com/en-us/windows/desktop/api/lmaccess/ns-lmaccess-_user_info_23)
(or several of the other "levels" of `USER_INFO`). Note that most of these values do not have documentation,
and probably shouldn't be used, (these values are directly taken from `lmaccess.h`)

```ts
export enum Flags {
  SCRIPT = 0x0001,
  ACCOUNTDISABLE = 0x0002,
  HOMEDIR_REQUIRED = 0x0008,
  LOCKOUT = 0x0010,
  PASSWD_NOTREQD = 0x0020,
  PASSWD_CANT_CHANGE = 0x0040,
  ENCRYPTED_TEXT_PASSWORD_ALLOWED = 0x0080,

  //
  // Account type bits as part of usri_flags.
  //

  TEMP_DUPLICATE_ACCOUNT = 0x0100,
  NORMAL_ACCOUNT = 0x0200,
  INTERDOMAIN_TRUST_ACCOUNT = 0x0800,
  WORKSTATION_TRUST_ACCOUNT = 0x1000,
  SERVER_TRUST_ACCOUNT = 0x2000,

  MACHINE_ACCOUNT_MASK = INTERDOMAIN_TRUST_ACCOUNT |
    WORKSTATION_TRUST_ACCOUNT |
    SERVER_TRUST_ACCOUNT,

  ACCOUNT_TYPE_MASK = TEMP_DUPLICATE_ACCOUNT |
    NORMAL_ACCOUNT |
    INTERDOMAIN_TRUST_ACCOUNT |
    WORKSTATION_TRUST_ACCOUNT |
    SERVER_TRUST_ACCOUNT,

  DONT_EXPIRE_PASSWD = 0x10000,
  MNS_LOGON_ACCOUNT = 0x20000,
  SMARTCARD_REQUIRED = 0x40000,
  TRUSTED_FOR_DELEGATION = 0x80000,
  NOT_DELEGATED = 0x100000,
  USE_DES_KEY_ONLY = 0x200000,
  DONT_REQUIRE_PREAUTH = 0x400000,
  PASSWORD_EXPIRED = 0x800000,
  TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION = 0x1000000,
  NO_AUTH_DATA_REQUIRED = 0x2000000,
  PARTIAL_SECRETS_ACCOUNT = 0x4000000,
  USE_AES_KEYS = 0x8000000,

  SETTABLE_BITS = SCRIPT |
    ACCOUNTDISABLE |
    LOCKOUT |
    HOMEDIR_REQUIRED |
    PASSWD_NOTREQD |
    PASSWD_CANT_CHANGE |
    ACCOUNT_TYPE_MASK |
    DONT_EXPIRE_PASSWD |
    MNS_LOGON_ACCOUNT |
    ENCRYPTED_TEXT_PASSWORD_ALLOWED |
    SMARTCARD_REQUIRED |
    TRUSTED_FOR_DELEGATION |
    NOT_DELEGATED |
    USE_DES_KEY_ONLY |
    DONT_REQUIRE_PREAUTH |
    PASSWORD_EXPIRED |
    TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION |
    NO_AUTH_DATA_REQUIRED |
    USE_AES_KEYS |
    PARTIAL_SECRETS_ACCOUNT,
}
```
