const assert = require("assert");
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

  let testName = "Kiosk";

  // Clean up a failed previous run.
  // Delete profile first, as it needs to look up the SID for the user.
  users.deleteProfile(testName);
  users.del(testName);

  // Should create the first time, and not the second
  assert(users.add(testName, "", users.Flags.DONT_EXPIRE_PASSWD));
  assert(!users.add(testName, "", users.Flags.DONT_EXPIRE_PASSWD));

  // Likewise for creating the profile
  assert(users.createProfile(testName));
  assert(!users.createProfile(testName));

  const testUser = users.get(testName);
  console.log(
    "Test user: %O\nFormatted flags: %O",
    testUser,
    formatFlags(testUser.flags),
  );

  // Should delete the first time, and not the second.
  assert(users.deleteProfile(testName));
  assert(!users.deleteProfile(testName));

  // Likewise for deleting the user
  assert(users.del(testName));
  assert(!users.del(testName));
} catch (e) {
  if (e.errno) {
    console.error('Failed with errno %d', e.errno);
  }
  if (e.syscall) {
    console.error('Failed with syscall %s', e.syscall);
  }
  throw e;
}
