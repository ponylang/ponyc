// This file defines the standard Posix type to make FFI calls easier
// to manage and more consistent.

type BlkCnt is I64 "blkcnt_t - Used for file block counts."

// not consistent across OSX (always 32 bit int)/Linux (Long)
type BlkSize is ILong "blksize_t - Used for block sizes."
type OSXBlkSize is I32 "blksize_t - Used for block sizes. (OSX variant)"

type Cc is U8 "cc_t - Type used for terminal special characters."

// not consistent across OSX (Unsigned Long)/Linux (Long)
type Clock is ILong "clock_t - Used for system times in clock ticks or CLOCKS_PER_SEC."
type OSXClock is ULong "clock_t - Used for system times in clock ticks or CLOCKS_PER_SEC. (OSX variant)"

// not defined on OSX
type ClockId is I32 "clockid_t - Used for clock ID type in the clock and timer functions."

// not consistent across OSX (always 32 bit int)/Linux (always 64 bit)
type Dev is I64 "dev_t - Used for device IDs."
type OSXDev is I32 "dev_t - Used for device IDs. (OSX variant)"

// not consistent across OSX (always unsigned int)/Linux (always unsigned long)
type FsBlkCnt is ULong "fsblkcnt_t - Used for file system block counts."
type OSXFsBlkCnt is U32 "fsblkcnt_t - Used for file system block counts. (OSX variant)"

// not consistent across OSX (always unsigned int)/Linux (always unsigned long)
type FsFilCnt is ULong "fsfilcnt_t - Used for file system file counts."
type OSXFsFilCnt is U32 "fsfilcnt_t - Used for file system file counts. (OSX variant)"

type Gid is U32 "gid_t - Used for group IDs."

type Id is U32 "id_t - Used as a general identifier; can be used to contain at least a pid_t, uid_t, or gid_t."

type INo is U64 "ino_t - Used for file serial numbers."

type Key is I32 "key_t - Used for XSI interprocess communication."

// not consistent across OSX (always unsigned 16 bit int)/Linux (always unsigned int)
type Mode is ULong "mode_t - Used for some file attributes."
type OSXMode is U16 "mode_t - Used for some file attributes. (OSX variant)"

type Mqd is I32 "mqd_t - Used for message queue descriptors."

// not consistent across OSX (always unsigned int)/Linux (always unsigned Long)
type Nfds is ULong "nfds_t - Integral type used for the number of file descriptors."
type OSXNfds is U32 "nfds_t - Integral type used for the number of file descriptors. (OSX variant)"

// not consistent across OSX (always unsigned 16 bit int)/Linux (unsigned Long)
type NLink is ULong "nlink_t - Used for link counts."
type OSXNLink is U16 "nlink_t - Used for link counts. (OSX variant)"

type Off is I64 "off_t - Used for file sizes."

type Pid is I32 "pid_t - Used for process IDs and process group IDs."

// not defined on linux
type PtrDiff is ISize "ptrdiff_t - Signed integral type of the result of subtracting two pointers."

type RLim is I64 "rlim_t - Unsigned arithmetic type used for limit values, to which objects of type int and off_t can be cast without loss of value."

type SigAtomic is I32 "sig_atomic_t - Integral type of an object that can be accessed as an atomic entity, even in the presence of asynchronous interrupts."

type Size is USize "size_t - Used for sizes of objects."

// not consistent across OSX (always unsigned long)/Linux (always unsigned int)
type Speed is ULong "speed_t - Type used for terminal baud rates."

type SSize is ISize "ssize_t - Used for a count of bytes or an error indication."

// not consistent across OSX (always int)/Linux (always long)
type SuSeconds is ILong "suseconds_t - Used for time in microseconds."

// not consistent across OSX (always unsigned long)/Linux (always unsigned int)
type TcFlag is U32 "tcflag_t - Type used for terminal modes."
type OSXTcFlag is ULong "tcflag_t - Type used for terminal modes. (OSX variant)"

type Time is ILong "time_t - Used for time in seconds."

// not defined on OSX/void* on Linux
type Timer is Pointer[None] "timer_t - Used for timer ID returned by timer_create()."

type Uid is U32 "uid_t - Used for user IDs."

type USeconds is U32 "useconds_t - Used for time in microseconds."

type Wchar is I32 "wchar_t - Integral type whose range of values can represent distinct codes for all members of the largest extended character set specified by the supported locales."

// not consistent across OSX (always unsigned 32 bit)/Linux (always unsigned long)
type WCType is ULong "wctype_t - Scalar type which represents a character class descriptor."
type OSXWCType is U32 "wctype_t - Scalar type which represents a character class descriptor. (OSX variant)"

// not consistent across OSX (always signed 32 bit)/Linux (always unsigned 32 bit)
type WInt is U32 "wint_t - An integral type capable of storing any valid value of wchar_t, or WEOF."
type OSXWInt is I32 "wint_t - An integral type capable of storing any valid value of wchar_t, or WEOF. (OSX variant)"
