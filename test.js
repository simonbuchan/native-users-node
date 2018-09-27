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

let simon = users.get("simon");
console.log(simon);
console.log(formatFlags(simon.flags));
console.log(users.Flags[simon.flags & users.Flags.ACCOUNT_TYPE_MASK]);

try {
  users.del('Kiosk');
} catch (e) {
  if (e.errno === 2221) { // The user name could not be found.
    console.log('User "Kiosk" does not exist');
  } else {
    console.error(e);
  }
}

users.add('Kiosk', '', users.Flags.DONT_EXPIRE_PASSWD);

try {
  users.add('Kiosk', '');
} catch (e) {
  if (e.errno !== 2224) { // account already exists.
    throw e;
  }
}

let kiosk = users.get("Kiosk");
console.log(kiosk);
console.log(formatFlags(kiosk.flags));
console.log(users.Flags[kiosk.flags & users.Flags.ACCOUNT_TYPE_MASK]);
