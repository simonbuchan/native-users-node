const assert = require("assert");
const native = require("bindings")("users.node");

// lmaccess.h UF_*
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

export interface UserInfo {
  name: string;
  full_name: string;
  comment: string;
  flags: Flags;
  sid: string;
}

export function get(name: string): UserInfo {
  assert(typeof name === "string");
  return native.get(name);
}

export function add(name: string, password: string, flags: Flags = 0): void {
  assert(typeof name === "string");
  assert(typeof password === "string");
  assert(typeof flags === "number");
  // flags |= Flags.SCRIPT;
  // if (!(flags & Flags.ACCOUNT_TYPE_MASK)) {
  //   flags |= Flags.NORMAL_ACCOUNT;
  // }
  native.add(name, password, flags);
}

export function del(name: string): void {
  assert(typeof name === "string");
  native.del(name);
}
