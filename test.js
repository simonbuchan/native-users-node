const assert = require("assert").strict;
const users = require("./lib");

const flagBits = Object.keys(users.Flags)
  .map(key => parseInt(key, 10))
  .filter(bit => !!bit)
  .sort((a, b) => a - b);

function formatFlags(flags) {
  return (
    flagBits
      .filter(bit => {
        if ((flags & bit) !== bit) {
          return false;
        }
        flags &= ~bit;
        return true;
      })
      .map(bit => users.Flags[bit])
      .join(" | ") || "0"
  );
}

try {
  const currentUser = users.get(process.env.USERNAME);
  console.log(
    "Current user: %O\nFormatted flags: %O",
    currentUser,
    formatFlags(currentUser.flags),
  );

  const testName = "autologon";
  const testPassword = "";

  // Clean up a failed previous run.
  // Delete profile first, as it needs to look up the SID for the user.
  users.deleteProfile(testName);
  users.del(testName);

  // Should create the first time, and not the second
  assert.equal(
    users.add(testName, testPassword, users.Flags.DONT_EXPIRE_PASSWD),
    true,
  );
  assert.equal(
    users.add(testName, testPassword, users.Flags.DONT_EXPIRE_PASSWD),
    false,
  );

  // Likewise for creating the profile, but it returns the profile path.
  const testProfile = users.createProfile(testName);
  assert.equal(typeof testProfile, "string");
  assert.equal(users.createProfile(testName), null);

  const testUser = users.get(testName);
  console.log(
    "Test user: %O\nFormatted flags: %O\nCreated profile: %O",
    testUser,
    formatFlags(testUser.flags),
    testProfile,
  );

  assert.throws(
    () => {
      const token = users.logonUser(testName, testPassword);
      console.log("Logon Token: %O", token);
      users.closeHandle(token);
    },
    error => error.errno === 1327, // can't logonUser for users without a password.
  );

  users.set(testName, {});
  const testUser2 = users.get(testName);
  assert.deepEqual(testUser2, testUser);
  const testSetOptions = {
    full_name: "The Test User",
    flags: users.Flags.DONT_EXPIRE_PASSWD,
  };
  users.set(testName, testSetOptions);
  const testUser3 = users.get(testName);
  assert.deepEqual(testUser3, {
    ...testUser,
    ...testSetOptions,
    // Some flags are required:
    flags:
      testSetOptions.flags | users.Flags.NORMAL_ACCOUNT | users.Flags.SCRIPT,
  });

  const testPassword2 = "Pa$$w0rd!";
  users.changePassword(testName, testPassword, testPassword2);

  const handle = users.logonUser(testName, testPassword2);
  console.log("Logon Token: %O", handle);
  const getProfile = users.getUserProfileDirectory(handle);
  assert.equal(getProfile, testProfile);
  users.impersonateLoggedOnUser(handle);
  users.revertToSelf();
  users.closeHandle(handle);

  // Should delete the first time, and not the second.
  assert.equal(users.deleteProfile(testName), true);
  assert.equal(users.deleteProfile(testName), false);

  // Likewise for deleting the user
  assert.equal(users.del(testName), true);
  assert.equal(users.del(testName), false);
} catch (e) {
  if (e.errno) {
    console.error("Failed with errno %d", e.errno);
  }
  if (e.syscall) {
    console.error("Failed with syscall %s", e.syscall);
  }
  throw e;
}
