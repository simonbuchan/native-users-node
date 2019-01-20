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

// lmaccess.h USER_INFO_23
export interface UserInfo {
  name: string;
  full_name: string;
  comment: string;
  flags: Flags;
  sid: string;
}

// from winbase.h
export enum LogonType {
  INTERACTIVE = 2,
  NETWORK = 3,
  BATCH = 4,
  SERVICE = 5,
  UNLOCK = 7,
  NETWORK_CLEARTEXT = 8,
  NEW_CREDENTIALS = 9,
}

// from winbase.h
export enum LogonProvider {
  DEFAULT = 0,
  WINNT35 = 1,
  WINNT40 = 2,
  WINNT50 = 3,
  VIRTUAL = 4,
}

export function get(name: string): UserInfo | null {
  assert(typeof name === "string");
  return native.get(name);
}

export function add(name: string, password: string, flags: Flags = 0): boolean {
  assert(typeof name === "string");
  assert(typeof password === "string");
  assert(typeof flags === "number");
  return native.add(name, password, flags);
}

export function del(name: string): boolean {
  assert(typeof name === "string");
  return native.del(name);
}

export function changePassword(
  name: string,
  oldPassword: string,
  newPassword: string,
): void {
  return native.changePassword(name, oldPassword, newPassword);
}

export interface SetOptions {
  full_name?: string;
  flags?: number;
}

export function set(name: string, options: SetOptions): void {
  assert(typeof name === "string");
  assert(typeof options === "object" && options !== null);
  return native.set(name, options);
}

export function createProfile(name: string): string | null {
  assert(typeof name === "string");
  return native.createProfile(name);
}

export function deleteProfile(name: string): boolean {
  assert(typeof name === "string");
  return native.deleteProfile(name);
}

export function logonUser(
  name: string,
  password: string,
  domain: string = ".",
  type: LogonType = LogonType.NETWORK,
  provider: LogonProvider = LogonProvider.DEFAULT,
): unknown {
  assert(typeof name === "string");
  assert(typeof domain === "string");
  assert(typeof password === "string");
  assert(typeof type === "number");
  assert(typeof provider === "number");
  return native.logonUser(name, domain, password, type, provider);
}

export function impersonateLoggedOnUser(handle: unknown): void {
  return native.impersonateLoggedOnUser(handle);
}

export function revertToSelf(): void {
  return native.revertToSelf();
}

export function getUserProfileDirectory(handle: unknown): string {
  return native.getUserProfileDirectory(handle);
}

export function closeHandle(handle: unknown): void {
  return native.revertToSelf();
}
