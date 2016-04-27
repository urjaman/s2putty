/*
 * PuTTY version numbering
 */

#define STR1(x) #x
#define STR(x) STR1(x)

#if defined SNAPSHOT

const char ver[] = "Development snapshot " STR(SNAPSHOT);
const char sshver[] = "PuTTY-Snapshot-" STR(SNAPSHOT);

#elif defined RELEASE

const char ver[] = "Release " STR(RELEASE);
const char sshver[] = "PuTTY-Release-" STR(RELEASE);

#else

const char ver[] = "Unidentified build, " __DATE__ " " __TIME__;
const char sshver[] = "PuTTY-Local: " __DATE__ " " __TIME__;

#endif

/*
 * SSH local version string MUST be under 40 characters. Here's a
 * compile time assertion to verify this.
 */
enum { vorpal_sword = 1 / (sizeof(sshver) <= 40) };
